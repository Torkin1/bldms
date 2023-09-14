#ifndef DRIVER_H_INCLUDED
#define DRIVER_H_INCLUDED

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

int bldms_init_driver(struct bldms_driver *driver, 
 const char *driver_name, struct module *owner, int minors);
void bldms_invalidate_driver(struct bldms_driver *driver);

blk_status_t bldms_request(struct blk_mq_hw_ctx *hctx,
 const struct blk_mq_queue_data *bd);
void bldms_release(struct gendisk *disk, fmode_t mode);
int bldms_open(struct block_device *bdev, fmode_t mode);

#endif // DRIVER_H_INCLUDED