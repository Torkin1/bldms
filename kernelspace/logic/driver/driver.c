#include <linux/genhd.h>

#include "driver/driver.h"
#include "driver/ops/vfs_supported.h"
#include "device/request.h"

/**
 * Gets a Major number selected by the kernel
 * among the available ones
*/
static int bldms_get_major(struct bldms_driver *driver){
    int major = register_blkdev(0, driver->name);
    if(major < 0){
        pr_err("%s: register_blkdev returned error code %d\n", __func__, major);
    }
    return major;
}

/**
 * Initializes driver informations
*/
int bldms_init_driver(struct bldms_driver *driver, 
 const char *driver_name, struct module *owner, int minors){
    
    memset(driver, 0, sizeof(struct bldms_driver));
    
    // basic driver informations
    driver->name = driver_name;
    driver->minors = minors;

    // Register driver by acquiring a major
    driver->major = bldms_get_major(driver);
    if(driver->major < 0){
        pr_err("%s: unable to get major number\n", __func__);
        return -1;
    }
    
    // fill block device operations
    driver->device_ops.owner = owner;
    driver->device_ops.open = bldms_open;
    driver->device_ops.release = bldms_release;

    // fill queue operations
    driver->queue_ops.queue_rq = bldms_request;

    return 0;
}

void bldms_invalidate_driver(struct bldms_driver *driver){
    if(driver->major > 0){
        unregister_blkdev(driver->major, driver->name);
    }
}
