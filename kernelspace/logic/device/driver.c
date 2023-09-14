#include <linux/genhd.h>

#include "driver.h"
#include "device.h"

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

int bldms_open(struct block_device *bdev, fmode_t mode){

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

void bldms_release(struct gendisk *disk, fmode_t mode){

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
 * There is no need to implement read() and write() functions.
 * When read() and write() are called from VFS, the requested
 * operations are bundled in a struct request and passed to
 * the request handler.
*/

/**
 * A read/write requests handler for the bldms block device.
 * NOTE: runs in interrupt context
*/
blk_status_t bldms_request(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd){

    blk_status_t res = BLK_STS_OK;
    struct request *req = bd->rq;
    struct bldms_device *device = req->q->queuedata;
    struct bio_vec bvec;
    struct req_iterator iter;
    sector_t pos_first_sector;
    size_t nr_sectors;
    void *buffer_data;
    int direction;

    blk_mq_start_request(req);
    pos_first_sector = blk_rq_pos(req);
    direction = rq_data_dir(req);
    
    /**
     * skip requests which do not involve data blocks transfer
    */
    if (blk_rq_is_passthrough(req)){
        pr_notice("%s: skipping non-data block transfer request\n", __func__);
        res = BLK_STS_IOERR;
        goto bldms_request_exit;
    }
    
    /**
     * For each bio_vec in the request, we map the data pages to linear
     * addrs and move the data to/from the device 
    */
    rq_for_each_segment(bvec, req, iter){
        nr_sectors = blk_rq_cur_sectors(req);
        pr_debug("%s: direction: %d, starting sector: %lld, num of sectors %ld\n",
                    __func__,
                    direction,
                    pos_first_sector,
                    nr_sectors);
        buffer_data = page_address(bvec.bv_page) + bvec.bv_offset;
        res = bldms_move_data(device, pos_first_sector,
            nr_sectors, buffer_data, direction);
            if (res != BLK_STS_OK){
                pr_err("%s: failed to move data\n", __func__);
                break;
            }
        pos_first_sector += nr_sectors;
    }
    
bldms_request_exit:    
    blk_mq_end_request(req, res);
    return res;
}

/**
 * Gets a Major number selected by the kernel
 * among the available ones
*/
static int bldms_get_major(struct bldms_driver *driver){
    int major = register_blkdev(0, driver->name);
    if(major < 0){
        pr_err("%s: register_blkdev returned error code %d\n", __func__, major);
    }
    return major;
}

/**
 * Initializes driver informations
*/
int bldms_init_driver(struct bldms_driver *driver, 
 const char *driver_name, struct module *owner, int minors){
    
    memset(driver, 0, sizeof(struct bldms_driver));
    
    // basic driver informations
    driver->name = driver_name;
    driver->minors = minors;

    // Register driver by acquiring a major
    driver->major = bldms_get_major(driver);
    if(driver->major < 0){
        pr_err("%s: unable to get major number\n", __func__);
        return -1;
    }
    
    // fill block device operations
    driver->device_ops.owner = owner;
    driver->device_ops.open = bldms_open;
    driver->device_ops.release = bldms_release;
    driver->device_ops.submit_bio = bldms_submit_bio;

    // fill queue operations
    driver->queue_ops.queue_rq = bldms_request;

    return 0;
}

void bldms_invalidate_driver(struct bldms_driver *driver){
    if(driver->major > 0){
        unregister_blkdev(driver->major, driver->name);
    }
}
