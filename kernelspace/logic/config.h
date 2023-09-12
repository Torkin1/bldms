#ifndef BLDMS_CONFIG_H
#define BLDMS_CONFIG_H

/**
 * TODO: use module params and use the following defines as default values
*/
#define BLDMS_NAME THIS_MODULE->name
#define BLDMS_MINORS 1
#define BLDMS_NBLOCKS 16
#define BLDMS_KERNEL_SECTOR_SIZE 512
#define BLDMS_BLOCKSIZE 4096
#define BLDMS_NR_SECTORS_IN_BLOCK BLDMS_BLOCKSIZE / BLDMS_KERNEL_SECTOR_SIZE

#define BLDMS_SYSCALL_DESCS_DIRNAME "bldms_syscalls"

#define BLDMS_DEV_NAME "bldmsdisk"

#endif // BLDMS_CONFIG_H