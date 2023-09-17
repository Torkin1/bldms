#include <stdio.h>
#include <sys/stat.h>

#include "test_suites.h"
#include "devkeeper/devkeeper.h"
#include "logger/logger.h"
#include "../../kernelspace/logic/config.h"
#include "api/api.h"

int test_devkeeper(){

    char dev_path[64];
    char *mount_point = "./test_mount";
    char BLDMS_DEV_NAME[32];

    memset(BLDMS_DEV_NAME, 0, 32);

    get_string_param("BLDMS_DEV_NAME", BLDMS_DEV_NAME);

    sprintf(dev_path, "/dev/%s", BLDMS_DEV_NAME);
    ON_ERROR_LOG_AND_RETURN(devkeeper_format_device(dev_path), -1,
     "Failed to format device at %s", dev_path);
    ON_ERROR_LOG_AND_RETURN(devkeeper_create_mountpoint(mount_point, 0777), -1, 
     "Failed to create mount point at %s", mount_point);
    ON_ERROR_LOG_AND_RETURN(devkeeper_mount_device(dev_path, mount_point), -1,
     "Failed to mount device at %s", dev_path);
    
    return 0;
}

int test_mount_twice(){
    char dev_path[64];
    char *mount_point_1 = "./test_mount_1";
    char *mount_point_2 = "./test_mount_2";
    char BLDMS_DEV_NAME[32];

    memset(BLDMS_DEV_NAME, 0, 32);
    
    get_string_param("BLDMS_DEV_NAME", BLDMS_DEV_NAME);


    sprintf(dev_path, "/dev/%s", BLDMS_DEV_NAME);
    ON_ERROR_LOG_AND_RETURN(devkeeper_format_device(dev_path), -1,
     "Failed to format device at %s", dev_path);
    ON_ERROR_LOG_AND_RETURN(devkeeper_create_mountpoint(mount_point_1, 0777), -1, 
     "Failed to create mount point at %s", mount_point_1);
        ON_ERROR_LOG_AND_RETURN(devkeeper_create_mountpoint(mount_point_2, 0777), -1, 
     "Failed to create mount point at %s", mount_point_2);
    ON_ERROR_LOG_AND_RETURN(devkeeper_mount_device(dev_path, mount_point_1), -1,
     "Failed to mount device at %s", dev_path);
    
    if (devkeeper_mount_device(dev_path, mount_point_2) == 0){
        LOG_ERROR("Mounting device at %s to %s should have failed", dev_path, mount_point_2);
        return -1;
    } 

    return 0;
    
}