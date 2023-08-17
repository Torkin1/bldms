#define __KERNEL__
#define MODULE

//#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "bldms.h"

// TODO: move the following defines to module params
#define BLDMS_NAME THIS_MODULE->name
#define BLDMS_MINORS 1
#define BLDMS_NBLOCKS 16
#define BLDMS_KERNEL_SECTOR_SIZE 512
#define BLDMS_BLOCKSIZE 4096
#define BLDMS_NR_SECTORS_IN_BLOCK BLDMS_BLOCKSIZE / BLDMS_KERNEL_SECTOR_SIZE

static struct bldms_driver driver;  // driver related data
static struct bldms_device device;  // represents the device in memory

static int bldms_modify_device_user_count(struct bldms_device *dev, int howmany)
{
    int res =0;
    if (!dev -> online){
        pr_err("%s: device is not user accessible\n", __func__);
        res = -ENODEV;
    }
    else {
        dev -> users = dev->users + howmany;
    }
    return res;
}

static int bldms_open(struct block_device *bdev, fmode_t mode){

    // TODO: must fail with ENODEV if not mounted
    
    struct bldms_device *dev = bdev->bd_disk->private_data;
    int res;

    spin_lock(&dev->lock);
    res = bldms_modify_device_user_count(dev, 1);
    spin_unlock(&dev->lock);

    if (res){
        pr_err("%s: failed to open device %s\n", __func__, bdev->bd_disk->disk_name);
    }   
    return res;
}

static void bldms_release(struct gendisk *disk, fmode_t mode){

    // TODO: must fail with ENODEV if not mounted
    
    struct bldms_device *dev = disk->private_data;
    int res = 0;

    spin_lock(&dev->lock);
    res = bldms_modify_device_user_count(dev, -1);
    spin_unlock(&dev->lock);

    if (res){
        pr_err("%s: failed to release device %s\n", __func__, disk->disk_name);
    }   
}

/**
 * Moves data between the device buffer and the data buffer;
 * The caller must know the position of the starting sector in the device
 * and the linear addr of the data buffer, plus size of data in sectors
*/
static blk_status_t bldms_move_data(
    struct bldms_device *dev,
    unsigned long pos_first_sector, size_t nr_sectors,
    void *buffer_data, bool is_write){
    
    unsigned long offset, nr_bytes;
    u8 *start;

    // calculate starting position in device buffer and how many bytes to move 
    offset = pos_first_sector * BLDMS_KERNEL_SECTOR_SIZE;
    nr_bytes = nr_sectors * BLDMS_KERNEL_SECTOR_SIZE;
    start = dev->data + offset;

    // check if out of bounds
    if((offset + nr_bytes) > dev->data_size){
        pr_err("%s: out of bounds access\n", __func__);
        return BLK_STS_IOERR;
    }

    // choose data direction according to the request being a read or a write
    is_write? memcpy(start, buffer_data, nr_bytes) : memcpy(buffer_data, start, nr_bytes);

    return BLK_STS_OK;
}

/**
 * A read/write requests handler for the bldms block device.
 * NOTE: runs in interrupt context
*/
static blk_status_t bldms_request(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd){

    blk_status_t res = BLK_STS_OK;
    struct request *req = bd->rq;
    struct bio_vec bvec;
    struct req_iterator iter;
    sector_t pos_first_sector;
    size_t nr_sectors;
    void *buffer_data;
    bool is_write;

    blk_mq_start_request(req);
    pos_first_sector = blk_rq_pos(req);
    is_write = rq_data_dir(req);

    /**
     * skip requests which do not involve data blocks transfer
    */
    if (blk_rq_is_passthrough(req)){
        pr_notice("%s: skipping non-data block transfer request\n", __func__);
        res = BLK_STS_IOERR;
    }
    else {
        /**
         * For each bio_vec in the request, we map the data pages to linear
         * addrs and move the data to/from the device 
        */
        rq_for_each_segment(bvec, req, iter){
            nr_sectors = blk_rq_cur_sectors(req);
            pr_debug("%s: isWrite? %d, starting sector: %lld, num of sectors %ld\n",
                        __func__,
                        is_write,
                        pos_first_sector,
                        nr_sectors);
            buffer_data = page_address(bvec.bv_page) + bvec.bv_offset;
            bldms_move_data(&device, pos_first_sector, nr_sectors, buffer_data, is_write);
            pos_first_sector += nr_sectors;
        }
    }
    blk_mq_end_request(req, res);
    return res;
}

/**
 * Gets a Major number selected by the kernel
 * among the available ones
*/
static int bldms_get_major(void){
    int major = register_blkdev(0, driver.name);
    if(major < 0){
        pr_err("%s: register_blkdev returned error code %d\n", __func__, major);
    }
    return major;
}

/**
 * Initializes the block device in memory
 * TODO: implement multi-device support
*/
static int bldms_init_device(struct bldms_device *dev){
    
    int err = 0;
    
    memset(dev, 0, sizeof(struct bldms_device));
    
    // initializes data buffer
    dev->data_size = BLDMS_NBLOCKS * BLDMS_BLOCKSIZE;
    dev->data = vzalloc(dev->data_size);
    if(!dev->data){
        pr_err("vzalloc failed to allocate %d bytes of memory\n", dev->data_size);
        return -1;
    }

    // initializes synchronization facilities used by ops
    spin_lock_init(&dev->lock);
    
    /**
     *  fill tag set for request queue
        TODO: make queue params configurable from caller or module params
    */
    // 
    dev->tag_set.ops = &driver.queue_ops;
    dev->tag_set.nr_hw_queues = 1;
    dev->tag_set.queue_depth = 128;
    /** 
     * we do not care about which node is the closest to
     *  the memory allocated for the bldms device
     * TODO: we SHOULD care lol
     */
    dev->tag_set.numa_node = NUMA_NO_NODE;
    /**
     * Multiple requests are combined at an higher level if they involve
     * a set of consecutive blocks on device.
     * Kernel will not combine write requests with read requests.
     * Source: https://linux-kernel-labs.github.io/refs/heads/master/labs/block_device_drivers.html#struct-bio-structure
     * */    
    dev->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
    err = blk_mq_alloc_tag_set(&dev->tag_set);
    if(err){
        pr_err("%s: blk_mq_alloc_tag_set failed and returned %d\n", __func__, err);
        return -1;
    }

    // initializes request queue
    dev->queue = blk_mq_init_queue(&dev->tag_set);
    if(!dev->queue){
        pr_err("%s: blk_mq_init_queue failed\n", __func__);
        return -1;
    }
    dev->queue->queuedata = dev;
    blk_queue_logical_block_size(dev->queue, BLDMS_KERNEL_SECTOR_SIZE);
    
    // initializes gendisk struct
    dev->gd = blk_alloc_disk(NUMA_NO_NODE);
    if (!dev->gd){
        pr_err("%s: blk_alloc_disk failed\n", __func__);
        return -1;
    }
    dev->gd->major = driver.major;
    dev->gd->minors = 1;
    dev->gd->first_minor = 0;
    dev->gd->fops = &driver.device_ops;
    dev->gd->queue = dev->queue;
    dev->gd->private_data = &device;
    snprintf(dev->gd->disk_name, DISK_NAME_LEN, "%sdisk", driver.name);
    set_capacity(dev->gd, BLDMS_NBLOCKS * BLDMS_NR_SECTORS_IN_BLOCK);
    
    dev->online = true;

    err = add_disk(dev->gd);
    if(err){
        pr_err("%s: add_disk failed with error code %d\n", __func__, err);
        return -1;
    }
    return 0;
}

/**
 * Initializes driver informations
*/
static int bldms_init_driver(const char *driver_name){
    
    memset(&driver, 0, sizeof(struct bldms_driver));
    
    // basic driver informations
    driver.name = driver_name;
    driver.minors = BLDMS_MINORS;

    // Register driver by acquiring a major
    driver.major = bldms_get_major();
    if(driver.major < 0){
        pr_err("%s: unable to get major number\n", __func__);
        return -1;
    }
    
    // fill block device operations
    driver.device_ops.owner = THIS_MODULE;
    driver.device_ops.open = bldms_open;
    driver.device_ops.release = bldms_release;

    // fill queue operations
    driver.queue_ops.queue_rq = bldms_request;

    return 0;
}

static int bldms_init(void){
    
    pr_info("%s: Loading bldms module\n", BLDMS_NAME);

    // Initializes driver struct
    if (bldms_init_driver(BLDMS_NAME) < 0){
        pr_err("%s: unable to initialize driver\n", __func__);
        return -1;
    }
    pr_info("%s: Initialized driver registered with major %d\n", BLDMS_NAME, driver.major);

    // creates needed devices (default is one)
    if(bldms_init_device(&device) < 0){
        pr_err("%s: unable to initialize device\n", __func__);
        return -1;
    }
    pr_info("%s: Initialized device /dev/%s\n", BLDMS_NAME, device.gd->disk_name);
    
    return 0;
}

static void bldms_disable_device(struct bldms_device *dev){
    
    spin_lock(&dev->lock);
    if(dev ->users){
        pr_warn("%s: device %s is still in use by %d users\n", __func__, dev->gd->disk_name, dev->users);
    }
    dev ->online = false;
    if(dev->gd){
        del_gendisk(dev->gd);
    }
    if(dev->queue){
        blk_cleanup_queue(dev->queue);
    }
    if (dev->tag_set.tags)
		blk_mq_free_tag_set(&dev->tag_set);
    if(dev->data){
        vfree(dev->data);
    }
    spin_unlock(&dev->lock);
}

static void bldms_exit(void){

    bldms_disable_device(&device);
    pr_debug("%s: bldms device deleted\n", __func__);
    unregister_blkdev(driver.major, driver.name);
    pr_debug("%s: bldms driver succesfully unregistered\n", __func__);

    pr_info("%s: bldms module unloaded\n", BLDMS_NAME);
}

module_init(bldms_init);
module_exit(bldms_exit);

MODULE_AUTHOR("Torkin");
MODULE_DESCRIPTION("Block-level data management service ");
MODULE_LICENSE("GPL");