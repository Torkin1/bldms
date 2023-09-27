#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "logger/logger.h"
#include "devkeeper.h"
#include "singlefilefs.h"
#include "../../kernelspace/logic/config.h"
#include "api/api.h"
#include "block.h"
#include "block_serialization.h"

#define BLDMS_BLOCKSIZE get_int_param("BLDMS_BLOCKSIZE")
#define BLDMS_NBLOCKS get_int_param("BLDMS_NBLOCKS")

/**
 * Formats a device with the singlefilefs filesystem
*/
int devkeeper_format_device(char * dev_path, int block_size, int nr_blocks){

    int fd;
    struct singlefilefs_sb_info sb_info;
    size_t written;
	struct singlefilefs_inode root_inode;
	struct singlefilefs_inode file_inode;
    struct bldms_block b;
    uint8_t serialized_buffer[block_size];
    
    b.data = malloc(1);
    
    sb_info.magic = SINGLEFILEFS_MAGIC;
    
    // prepare disk
    fd = open(dev_path, O_TRUNC | O_WRONLY);
    ON_ERROR_LOG_ERRNO_AND_RETURN(fd < 0, -1, "Failed to open device at %s", dev_path);
    lseek(fd, SINGLEFILEFS_SB_BLOCK_NUMBER * block_size, SEEK_SET);

    // write serialized sb_info to disk
    written = write(fd, &sb_info, sizeof(sb_info));
    if(written != sizeof(sb_info)){
        LOG_ERROR("Failed to write superblock to device %s\n", dev_path);
        close(fd);
        return -1;
    }
    logMsg(LOG_TAG_D, "Super block written succesfully\n");

    // write file inode
    lseek(fd, SINGLEFILEFS_FILE_INODE_BLOCK * block_size, SEEK_SET);
	file_inode.mode = S_IFREG;
	file_inode.inode_no = SINGLEFILEFS_FILE_INODE_NUMBER;
	file_inode.file_size = block_size * nr_blocks; // make room for all the blocks
	written = write(fd, (char *)&file_inode, sizeof(file_inode));

	if (written != sizeof(root_inode)) {
		LOG_ERROR("The file inode was not written properly.\n");
		close(fd);
		return -1;
	}

    // initialize each data block as an invalid one

    for (int i = 2; i < nr_blocks; i++){
        lseek(fd, block_size - written, SEEK_CUR);
        memset(&b, 0, sizeof(b));
        memset(serialized_buffer, 0, block_size);
        b.header.state = BLDMS_BLOCK_STATE_INVALID;
        b.header.header_size = bldms_calc_block_header_size(b.header);
        b.header.data_capacity = block_size - b.header.header_size;
        b.header.index = i;
        b.header.prev = (i == 2)? -1 : i - 1;
        b.header.next = (i == nr_blocks - 1)? -1 : i + 1;
        bldms_block_serialize(&b, serialized_buffer);
        written = write(fd, serialized_buffer, b.header.header_size);
        if(written != b.header.header_size){
            LOG_ERROR("Expected to write %d bytes of block header %d, but wrote %d\n", 
                b.header.header_size, i, written);
            close(fd);
            return -1;
        }

    }
    free(b.data);


    return 0;

}