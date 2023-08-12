#define __KERNEL__
#define MODULE

#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/genhd.h>

/** Name of driver*/
const char *bldms_name = "bldms";

enum bldms_errors {
    BLDMS_E_OK = 0,
    BLDMS_E_NOMAJOR = -1, 
} bldms_errors;

/**
 * Gets a Major number selected by the kernel
 * among the available ones
*/
static int bldms_getMajorNumber(void){
    int major = register_blkdev(0, bldms_name);
    if(major < 0){
        pr_err("%s: register_blkdev returned error code %d\n", __func__, major);
    }
    return major;
}

static int bldms_init(void){

    int major;
    
    pr_devel("Loading bldms module\n");

    /** Register driver by acquiring a major*/
    major = bldms_getMajorNumber();
    if (major < 0){
        pr_err("unable to get major number\n");
        return major;
    }
    pr_devel("Got major number %d\n", major);
    
    return 0;
}

static void bldms_exit(void){

    pr_devel("Goodbye, world\n");
}

module_init(bldms_init);
module_exit(bldms_exit);

MODULE_AUTHOR("Torkin");
MODULE_DESCRIPTION("Block-level data management service ");
MODULE_LICENSE("GPL");