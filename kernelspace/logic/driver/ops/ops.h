#ifndef OPS_H_INCLUDED
#define OPS_H_INCLUDED

#include "driver/ops/vfs_supported.h"
#include "driver/ops/vfs_unsupported.h"
#include "block_layer/block_layer.h"

#include <linux/spinlock.h>
#include <linux/atomic.h>

#define bldms_abort_op_if(cond_, msg_, ...) {\
    if (cond_)\
    {\
        pr_err(msg_, ##__VA_ARGS__);\
        return -1;\
    }\
}

#endif // OPS_H_INCLUDED