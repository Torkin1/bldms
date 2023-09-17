#ifndef BLDMS_CONFIG_H
#define BLDMS_CONFIG_H

#include <linux/moduleparam.h>
#include <linux/module.h>

/**
 * TODO: use module params and use the following defines as default values
*/

#define BLDMS_NAME_DEFAULT THIS_MODULE->name
#define BLDMS_MINORS_DEFAULT 1
#define BLDMS_NBLOCKS_DEFAULT 16
#define BLDMS_KERNEL_SECTOR_SIZE_DEFAULT 512
#define BLDMS_BLOCKSIZE_DEFAULT 4096

#define BLDMS_SYSCALL_DESCS_DIRNAME_DEFAULT "bldms_syscalls"

#define BLDMS_DEV_NAME_DEFAULT "bldmsdisk"

extern char *BLDMS_NAME;
extern int BLDMS_MINORS;
extern int BLDMS_NBLOCKS;
extern int BLDMS_KERNEL_SECTOR_SIZE;
extern int BLDMS_NR_SECTORS_IN_BLOCK;
extern int BLDMS_BLOCKSIZE;
extern char *BLDMS_SYSCALL_DESCS_DIRNAME;
extern char *BLDMS_DEV_NAME;

/**
 * Default page cache policy is write-back.
 * To change to write-through, uncomment the following line
 * or define BLDMS_BLOCK_SYNC_IO when compiling.
*/
// #define BLDMS_BLOCK_SYNC_IO

#endif // BLDMS_CONFIG_H