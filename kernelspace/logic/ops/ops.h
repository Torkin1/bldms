#ifndef OPS_H_INCLUDED
#define OPS_H_INCLUDED

//#include "vfs_supported.h"
#include "vfs_unsupported.h"
#include "block_layer/block_layer.h"

#define bldms_abort_op_if(cond_, msg_, ...) {\
    if (cond_)\
    {\
        pr_err(msg_, ##__VA_ARGS__);\
        return -1;\
    }\
}

#endif // OPS_H_INCLUDED