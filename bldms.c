// TODO: write a makefile to compile this baby

#define __KERNEL__
#define MODULE

#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

static int bldms_init(void){

    pr_info("Hello, world\n");
    
    return 0;
}

static void bldms_exit(void){

    pr_info("Goodbye, world\n");
}

module_init(bldms_init);
module_exit(bldms_exit);

MODULE_AUTHOR("Torkin");
MODULE_DESCRIPTION("Block-level data management service ");
MODULE_LICENSE("GPL");