#ifndef VFS_SUPPORTED_H_INCLUDED
#define VFS_SUPPORTED_H_INCLUDED

#include <linux/types.h>

#include "block_layer/block_layer.h"

ssize_t bldms_read(struct bldms_block_layer *b_layer, char *buf, size_t len,
loff_t *off);

#endif // VFS_SUPPORTED_H_INCLUDED