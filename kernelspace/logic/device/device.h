#ifndef DEVICE_H_INCLUDED
#define DEVICE_H_INCLUDED

#include <linux/blk-mq.h>

#include "driver/driver.h"
#include "device/blocks_list.h"
#include "device/block.h"

/**
 * Struct representing a bldms device in memory
*/
struct bldms_device{
    struct bldms_driver *driver; // the driver that works with the device
    char path[32]; // path to the device
    int data_size; // size of the data in the device in bytes
    u8 *data; // the data itself
    int users; // how many users are using the device right now
    bool online; // true if the device can be accessed by users
    int nr_blocks; // how many blocks the device has
    size_t sector_size; // size of a sector in bytes
    size_t block_size; // size of a block in bytes
    struct gendisk *gd; // the block device itself
    struct request_queue *queue; // the queue of requests of read/write
    spinlock_t lock;  // lock to synchronize access to the device
    struct blk_mq_tag_set tag_set;  // tag set of the device
    struct bldms_blocks_list *free_blocks; // list of free blocks
    struct bldms_blocks_list *used_blocks; // list of used blocks
};

void bldms_invalidate_device(struct bldms_device *dev);
int bldms_init_device(struct bldms_device *dev,
 int nr_blocks, size_t block_size, size_t sector_size,
 struct bldms_driver *driver);

sector_t bldms_block_to_sector(struct bldms_device *dev, int block_index);
int bldms_sector_to_block(struct bldms_device *dev, sector_t sector_index);
int bldms_move_block(struct bldms_device *dev,
    struct bldms_block *block, enum req_opf op);
int bldms_reserve_block(struct bldms_device *dev, int block_index);
int bldms_release_block(struct bldms_device *dev, int block_index);


#endif // DEVICE_H_INCLUDED