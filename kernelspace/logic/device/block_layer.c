#include <linux/types.h>

#include "device/device.h"
#include "device/block_serialization.h"
#include "device/device_core.h"

/************** Block layer interactions with the block device*/

int bldms_reserve_block(struct bldms_device *dev, int block_index){
    int res = 0;
    pr_debug("%s: dev is %p\n", __func__, dev);
    res = bldms_blocks_move_block(dev->used_blocks, dev->free_blocks, block_index);
    if (res){
        pr_err("%s: failed to reserve block %d\n", __func__, block_index);
        return -1;
    }
    return res;
}

int bldms_release_block(struct bldms_device *dev, int block_index){
    int res = 0;
    res = bldms_blocks_move_block(dev->free_blocks, dev->used_blocks, block_index);
    if (res){
        pr_err("%s: failed to release block %d\n", __func__, block_index);
        return -1;
    }
    return res;
}

/**
 * Translates a block index to a sector index in the device
*/
sector_t bldms_block_to_sector(struct bldms_device *dev, 
    int block_index){
    
    return block_index * (dev->block_size / dev->sector_size);    
}

/**
 * Translates a sector index in the device to a block index
*/
int bldms_sector_to_block(struct bldms_device *dev, 
    sector_t sector_index){
    
    // TODO: implement
    return -1;    
}

/** Moves one block of data*/
int bldms_move_block(struct bldms_device *dev,
    struct bldms_block *block, enum req_opf op){
    
    struct bio *bio;
    int res;
    sector_t start_sector;
    int buffer_order;
    struct page *start_page;
    u8 *buffer;
    struct block_device *bdev;
    int added_pages_len;

    /**
     * We need to translate the block index to a sector index in the device
    */
    start_sector = bldms_block_to_sector(dev, block->header.index);
    pr_debug("%s: start sector: %llu\n", __func__, start_sector);
    
    /**
     * Open the bldms device as a block device
     * FIXME: we should open the device before calling this function,
     * and close it when we did all the operations we needed
    */
    bdev = blkdev_get_by_path(dev->path, FMODE_READ | FMODE_WRITE | FMODE_EXCL, 
        dev->driver->device_ops.owner);
    if (IS_ERR(bdev)){
        pr_err("%s: failed to open device at %s as a block device\n", __func__, dev->path);
        return -1;
    }
    pr_debug("%s: bdev opened\n", __func__);
    /**
     * Create a struct bio describing the operations on block
    */
    bio = bio_alloc(GFP_KERNEL, 1);
    if(!bio){
        pr_err("%s: failed to allocate bio\n", __func__);
        blkdev_put(bdev, FMODE_READ | FMODE_WRITE | FMODE_EXCL);
        return -1;
    }
    bio ->bi_bdev = bdev;
    bio ->bi_iter.bi_sector = start_sector;
    bio ->bi_opf = op;
    pr_debug("%s: bio created\n", __func__);

    /**
     * Allocates pages to hold serialized block and adds them to the bio 
    */
    buffer_order = order_base_2(dev ->block_size / PAGE_SIZE);
    start_page = alloc_pages(GFP_KERNEL | __GFP_ZERO, buffer_order);
    if(!start_page){
        pr_err("%s: failed to allocate pages of order %d\n", __func__, buffer_order);
        bio_put(bio);
        blkdev_put(bdev, FMODE_READ | FMODE_WRITE | FMODE_EXCL);
        return -1;
    }
    added_pages_len = bio_add_page(bio, start_page, dev->block_size, 0);
    if (!added_pages_len){
        pr_err("%s: failed to add page to bio with error %d\n", __func__, res);
        bio_put(bio);
        __free_pages(start_page, buffer_order);
        blkdev_put(bdev, FMODE_READ | FMODE_WRITE | FMODE_EXCL);
        return -1;
    }   
    pr_debug("%s: pages added to bio\n", __func__);

    /**
     * If it is a write, we fill the pages with the serialized block
    */
    if(op == REQ_OP_WRITE){
        pr_debug("%s: op is a write\n", __func__);
        buffer = kmap_local_page(start_page);
        bldms_block_serialize(block, buffer);
    }

    // submits the bio to the I/O subsystem in order to add it to a request
    /** FIXME: submit_bio_wat() chrashes with null pointer exception*/
    /*
    res = submit_bio_wait(bio);
    if (res < 0){
        pr_err("%s: failed to submit bio with error %d\n", __func__, res);
        bio_put(bio);
        __free_pages(start_page, buffer_order);
        blkdev_put(bdev, FMODE_READ | FMODE_WRITE | FMODE_EXCL);
        return -1;
    }
    pr_debug("%s: bio submitted\n", __func__);
*/
    
    if (!bldms_move_bio(dev, bio)){
        pr_err("%s: failed to move bio\n", __func__);
        kunmap_local(buffer);
        bio_put(bio);
        __free_pages(start_page, buffer_order);
        blkdev_put(bdev, FMODE_READ | FMODE_WRITE | FMODE_EXCL);
        return -1;
    }

    /**
     * If it is a read, we deserialize the block from the pages
    */
    if (op == REQ_OP_READ){
        pr_debug("%s: op is a read\n", __func__);
        buffer = kmap_local_page(start_page);
        bldms_block_deserialize(block, buffer);
    }

    pr_debug("%s: exiting\n", __func__);
    kunmap_local(buffer);
    bio_put(bio);
    __free_pages(start_page, buffer_order);
    blkdev_put(bdev, FMODE_READ | FMODE_WRITE | FMODE_EXCL);

    return 0;
    
}