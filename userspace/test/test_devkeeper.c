#include <stdio.h>
#include <sys/stat.h>

#include "test_suites.h"
#include "devkeeper/devkeeper.h"
#include "logger/logger.h"
#include "../../kernelspace/logic/config.h"

int test_devkeeper(){

    char dev_path[32];
    char *mount_point = "./test_mount";

    sprintf(dev_path, "/dev/%s", BLDMS_DEV_NAME);
    ON_ERROR_LOG_AND_RETURN(devkeeper_format_device(dev_path), -1,
     "Failed to format device at %s", dev_path);
    
    //getchar();  // press a button to continue

    ON_ERROR_LOG_AND_RETURN(devkeeper_create_mountpoint(mount_point, 0777), -1, 
     "Failed to create mount point at %s", mount_point);
    //getchar();
    ON_ERROR_LOG_AND_RETURN(devkeeper_mount_device(dev_path, mount_point), -1,
     "Failed to mount device at %s", dev_path);
    
    return 0;
}