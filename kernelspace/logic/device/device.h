#ifndef DEVICE_H_INCLUDED
#define DEVICE_H_INCLUDED

#include <linux/blk-mq.h>
#include <linux/rculist.h>

#include "driver/driver.h"
#include "device/block_layer.h"

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
    struct bldms_free_blocks_list *free_blocks; // list of free blocks
};

void bldms_invalidate_device(struct bldms_device *dev);
int bldms_init_device(struct bldms_device *dev,
 int nr_blocks, size_t block_size, size_t sector_size,
 struct bldms_driver *driver);

#endif // DEVICE_H_INCLUDED