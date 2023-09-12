#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "logger/logger.h"
#include "devkeeper.h"
#include "singlefilefs.h"
#include "../../kernelspace/logic/config.h"

/**
 * Formats a device with the singlefilefs filesystem
*/
int devkeeper_format_device(char * dev_path){

    int fd;
    struct singlefilefs_sb_info sb_info;
    int written;
	struct singlefilefs_inode root_inode;
	struct singlefilefs_inode file_inode;
    
    sb_info.magic = SINGLEFILEFS_MAGIC;
    
    // prepare disk
    fd = open(dev_path, O_TRUNC | O_WRONLY);
    ON_ERROR_LOG_ERRNO_AND_RETURN(fd < 0, -1, "Failed to open device at %s", dev_path);
    lseek(fd, SINGLEFILEFS_SB_BLOCK_NUMBER * BLDMS_BLOCKSIZE, SEEK_SET);

    // write serialized sb_info to disk
    written = write(fd, &sb_info, sizeof(sb_info));
    if(written != sizeof(sb_info)){
        LOG_ERROR("Failed to write superblock to device %s\n", dev_path);
        close(fd);
        return -1;
    }
    logMsg(LOG_TAG_D, "Super block written succesfully\n");

    // write file inode
    lseek(fd, SINGLEFILEFS_FILE_INODE_BLOCK * BLDMS_BLOCKSIZE, SEEK_SET);
	file_inode.mode = S_IFREG;
	file_inode.inode_no = SINGLEFILEFS_FILE_INODE_NUMBER;
	file_inode.file_size = 0; // empty file
	written = write(fd, (char *)&file_inode, sizeof(file_inode));

	if (written != sizeof(root_inode)) {
		LOG_ERROR("The file inode was not written properly.\n");
		close(fd);
		return -1;
	}

    // we write no data in the file since we initialize it as an empty file

    return 0;

}