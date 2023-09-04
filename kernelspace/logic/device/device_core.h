#ifndef DEVICE_CORE_H
#define DEVICE_CORE_H

#include <linux/blkdev.h>
#include "device/device.h"

blk_status_t bldms_move_data(
    struct bldms_device *dev,
    unsigned long pos_first_sector, int nr_sectors,
    void *buffer_data, enum req_opf direction);

int bldms_move_bio(struct bldms_device *dev,
 struct bio *bio);

#endif // DEVICE_CORE_H