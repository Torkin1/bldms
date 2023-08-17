#ifndef _BLDMS_H_
#define _BLDMS_H_

#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>

/**
 * Data relevant to the bldms driver
*/
struct bldms_driver{
    const char *name;   // human-readable name of the driver
    int major;  // Major number of registered driver
    int minors; // how many minors we expect each device to support
    struct block_device_operations device_ops;  // functions to operate with the bldms block device (VFS aware)
    struct blk_mq_ops queue_ops; // fuctions implementing R-W ops
};

/**
 * Struct representing a bldms device in memory
*/
struct bldms_device{
    int data_size; // size of the data in the device in bytes
    u8 *data; // the data itself
    int users; // how many users are using the device right now
    bool online; // true if the device can be accessed by users
    struct gendisk *gd; // the block device itself
    struct request_queue *queue; // the queue of requests of read/write
    spinlock_t lock;  // lock to synchronize access to the device
    struct blk_mq_tag_set tag_set;  // tag set of the device
};

#endif // _BLDMS_H_