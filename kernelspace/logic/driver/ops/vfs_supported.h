#ifndef VFS_SUPPORTED_H_INCLUDED
#define VFS_SUPPORTED_H_INCLUDED

#include <linux/genhd.h>

int bldms_open(struct block_device *bdev, fmode_t mode);
void bldms_release(struct gendisk *disk, fmode_t mode);

#endif // VFS_SUPPORTED_H_INCLUDED