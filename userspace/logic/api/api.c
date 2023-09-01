#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "logger/logger.h"

static const char *syscall_descs_folder = "/sys/kernel/bldms_syscalls";

int build_syscall_desc_path(char *syscall_name, char *syscall_desc_path){

    strcpy(syscall_desc_path, syscall_descs_folder);
    strcat(syscall_desc_path, "/");
    strcat(syscall_desc_path, syscall_name);
    
    return 0;
} 

int get_syscall_desc(char *syscall_name){

    FILE *syscall_desc_file;
    int syscall_desc;
    char syscall_desc_path[256];

    memset(syscall_desc_path, 0, 256);
    build_syscall_desc_path(syscall_name, syscall_desc_path);
    
    syscall_desc_file = fopen(syscall_desc_path, "r");
    ON_ERROR_LOG_AND_RETURN((syscall_desc_file == NULL), -1, "Failed to open syscall descriptor\n");
    fscanf(syscall_desc_file, "%d", &syscall_desc);
    fclose(syscall_desc_file);
    ON_ERROR_LOG_AND_RETURN((syscall_desc < 0), -1, "Invalid syscall descriptor\n");

    return syscall_desc;
}

int call_test(int test_index){

    int test_driver_desc = get_syscall_desc("test_driver");
    ON_ERROR_LOG_AND_RETURN((test_driver_desc < 0), -1, "Failed to get test_driver syscall descriptor\n");

    logMsg(LOG_TAG_D, "%s: calling test driver with index %d\n", __func__, test_index);
    
    return syscall(test_driver_desc, test_index);
}