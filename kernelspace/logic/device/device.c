#include <linux/vmalloc.h>

#include "device/device.h"

void bldms_invalidate_device(struct bldms_device *dev){
    
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
    if(dev->free_blocks){
        bldms_destroy_blocks_list(dev->free_blocks);
    }
    spin_unlock(&dev->lock);
}

/**
 * Initializes the block device in memory
 * TODO: implement multi-device support
*/
int bldms_init_device(struct bldms_device *dev,
 int nr_blocks, size_t block_size, size_t sector_size,
 struct bldms_driver *driver){
    
    int err = 0;
    int i;
    
    memset(dev, 0, sizeof(struct bldms_device));

    // write some read only info + keep a reference to the driver
    dev->sector_size = sector_size;
    dev->block_size = block_size;
    dev->nr_blocks = nr_blocks;
    dev->driver = driver;

    // flags all blocks as ready to access from ops
    dev->in_progress_ops = vzalloc(nr_blocks * sizeof(struct completion));
    if(!dev->in_progress_ops){
        pr_err("failed to allocate in_progress_ops array\n");
        return -1;
    }
    for (i = 0; i < nr_blocks; i ++){
        init_completion(dev ->in_progress_ops + i);
        complete(dev ->in_progress_ops + i);
    }
    
    // initializes data buffer
    dev->data_size = nr_blocks * block_size;
    dev->data = vzalloc(dev->data_size);
    if(!dev->data){
        pr_err("vzalloc failed to allocate %d bytes of data buffer\n", dev->data_size);
        return -1;
    }

    // initializes synchronization facilities used by ops
    spin_lock_init(&dev->lock);
    
    /**
     *  fill tag set for request queue
        TODO: make queue params configurable from caller or module params
    */
    // 
    dev->tag_set.ops = &driver->queue_ops;
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
    blk_queue_logical_block_size(dev->queue, sector_size);
    
    // initializes gendisk struct
    dev->gd = blk_alloc_disk(NUMA_NO_NODE);
    if (!dev->gd){
        pr_err("%s: blk_alloc_disk failed\n", __func__);
        return -1;
    }
    dev->gd->major = driver->major;
    dev->gd->minors = 1;
    dev->gd->first_minor = 0;
    dev->gd->fops = &driver->device_ops;
    dev->gd->queue = dev->queue;
    dev->gd->private_data = dev;
    snprintf(dev->gd->disk_name, DISK_NAME_LEN, "%sdisk", driver->name);
    set_capacity(dev->gd, nr_blocks * (block_size / sector_size));

    // initializes blocks lists
    dev ->free_blocks = bldms_create_blocks_list(nr_blocks);
    dev ->used_blocks = bldms_create_blocks_list(0);
    dev ->prepared_for_write_blocks = bldms_create_blocks_list(0);

    // set path name
    snprintf(dev->path, 32, "/dev/%s", dev->gd->disk_name);
    
    // we are ready to open to the world!
    dev->online = true;
    err = add_disk(dev->gd);
    if(err){
        pr_err("%s: add_disk failed with error code %d\n", __func__, err);
        return -1;
    }
    return 0;
}


