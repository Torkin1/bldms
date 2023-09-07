#ifndef VFS_UNSUPPORTED_H
#define VFS_UNSUPPORTED_H

#include <linux/types.h>
#include "device/device.h"

int bldms_vfs_unsupported_init(struct bldms_device *dev);
void bldms_vfs_unsupported_cleanup(void);

#endif // VFS_UNSUPPORTED_H