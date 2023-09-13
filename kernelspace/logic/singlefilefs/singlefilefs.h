/**
 * NOTE: keep in synch with userspace dual
*/

#ifndef _ONEFILEFS_H
#define _ONEFILEFS_H

#include <linux/types.h>
#include <linux/fs.h>

#define SINGLEFILEFS_NAME "SINGLE FILE FS"

#define SINGLEFILEFS_MAGIC 0x42424242
#define SINGLEFILEFS_SB_BLOCK_NUMBER 0
#define SINGLEFILEFS_FILE_INODE_BLOCK 1

#define SINGLEFILEFS_FILENAME_MAXLEN 255

#define SINGLEFILEFS_ROOT_INODE_NUMBER 10
#define SINGLEFILEFS_FILE_INODE_NUMBER 1

#define SINGLEFILEFS_INODES_BLOCK_NUMBER 1

#define SINGLEFILEFS_UNIQUE_FILE_NAME "the-file"
#define SINGLEFILEFS_FS_NAME "singlefilefs"

//inode definition
struct singlefilefs_inode {
	mode_t mode;//not exploited
	uint64_t inode_no;
	uint64_t data_block_number;//not exploited

	union {
		uint64_t file_size;
		uint64_t dir_children_count;
	};
};

//dir definition (how the dir datablock is organized)
struct singlefilefs_dir_record {
	char filename[SINGLEFILEFS_FILENAME_MAXLEN];
	uint64_t inode_no;
};


//superblock definition on disk
struct singlefilefs_sb_info {
	uint64_t magic;	// magic number to recognize the fs

};

// file.c
extern const struct inode_operations singlefilefs_inode_ops;
extern const struct file_operations singlefilefs_file_operations; 

// dir.c
extern const struct file_operations singlefilefs_dir_operations;

int singlefilefs_init(size_t block_size, int nr_blocks);
void singlefilefs_exit(void);

#endif
