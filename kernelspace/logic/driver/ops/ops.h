#ifndef OPS_H_INCLUDED
#define OPS_H_INCLUDED

#include "driver/ops/vfs_supported.h"
#include "driver/ops/vfs_unsupported.h"


#define bldms_abort_op_if(cond_, msg_, ...) {\
    if (cond_)\
    {\
        pr_err(msg_, ##__VA_ARGS__);\
        return -1;\
    }\
}

#define bldms_abort_op_if_device_unavailable(dev_){\
    bldms_abort_op_if(!dev_->online, "%s: device %s is unavailable\n", __func__, dev_->path);\
}

#endif // OPS_H_INCLUDED