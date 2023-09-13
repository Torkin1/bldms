#include <sys/mount.h>
#include <sys/stat.h>

#include "singlefilefs.h"
#include "logger/logger.h"
#include "devkeeper.h"

/**
 * Mounts a device containing the singlefilefs filesystem
*/
int devkeeper_mount_device(char *dev_path, char *mount_point){

    unsigned long mount_flags;

    mount_flags = MS_NODEV | MS_NOEXEC | MS_NOSUID;    
    ON_ERROR_LOG_ERRNO_AND_RETURN(mount(dev_path, mount_point, SINGLEFILEFS_FS_NAME,
     mount_flags, NULL), -1, "Failed to mount device at %s:", dev_path);
    
    return 0;

}

/**
 * Creates a dir at mount_point with the permissions provided that can be used as
 * a mount point with devkeeper_mount_device()
*/
int devkeeper_create_mountpoint(char *mount_point, unsigned int mode){
    
    int mkdir_ret;
    struct stat statbuf;
    
    mkdir_ret = mkdir(mount_point, mode);
    if (mkdir_ret < 0){
        ON_ERROR_LOG_ERRNO_AND_RETURN(errno != EEXIST, -1, 
            "Failed to create dir at %s:", mount_point);
        ON_ERROR_LOG_ERRNO_AND_RETURN(stat(mount_point, &statbuf) < 0, -1,
            "Failed to stat mount point at %s:", mount_point);
        ON_ERROR_LOG_AND_RETURN(!S_ISDIR(statbuf.st_mode), -1,
            "Mount point at %s is not a directory\n", mount_point);
        logMsg(LOG_TAG_W, "Mount point at %s already exists\n", mount_point);
    }

    return 0;
}