#ifndef VFS_UNSUPPORTED_H
#define VFS_UNSUPPORTED_H

#include <linux/types.h>
#include "block_layer/block_layer.h"

int bldms_vfs_unsupported_init(struct bldms_block_layer *b_layer_ref);
void bldms_vfs_unsupported_cleanup(void);

#endif // VFS_UNSUPPORTED_H