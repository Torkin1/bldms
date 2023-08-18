#define __KERNEL__
#define MODULE

#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/string.h>

#include "device/device.h"
#include "driver/driver.h"

MODULE_AUTHOR("Torkin");
MODULE_DESCRIPTION("Block-level data management service ");
MODULE_LICENSE("GPL");

// TODO: move the following defines to module params
#define BLDMS_NAME THIS_MODULE->name
#define BLDMS_MINORS 1
#define BLDMS_NBLOCKS 16
#define BLDMS_KERNEL_SECTOR_SIZE 512
#define BLDMS_BLOCKSIZE 4096
#define BLDMS_NR_SECTORS_IN_BLOCK BLDMS_BLOCKSIZE / BLDMS_KERNEL_SECTOR_SIZE

static struct bldms_driver driver;  // driver related data
static struct bldms_device device;  // represents the device in memory

static int bldms_init(void){
    
    pr_info("%s: Loading bldms module\n", BLDMS_NAME);

    // Initializes driver struct
    if (bldms_init_driver(&driver, BLDMS_NAME, THIS_MODULE,
     BLDMS_MINORS) < 0){
        pr_err("%s: unable to initialize driver\n", __func__);
        return -1;
    }
    pr_info("%s: Initialized driver registered with major %d\n",
     BLDMS_NAME, driver.major);

    // creates needed devices (default is one)
    if(bldms_init_device(&device, BLDMS_NBLOCKS, BLDMS_BLOCKSIZE,
     BLDMS_KERNEL_SECTOR_SIZE, &driver) < 0){
        pr_err("%s: unable to initialize device\n", __func__);
        return -1;
    }
    pr_info("%s: Initialized device /dev/%s\n", BLDMS_NAME,
     device.gd->disk_name);
    
    return 0;
}

static void bldms_exit(void){

    bldms_invalidate_device(&device);
    pr_debug("%s: bldms device deleted\n", __func__);
    bldms_invalidate_driver(&driver);
    pr_debug("%s: bldms driver succesfully unregistered\n", __func__);

    pr_info("%s: module unload completed\n", BLDMS_NAME);
}

module_init(bldms_init);
module_exit(bldms_exit);