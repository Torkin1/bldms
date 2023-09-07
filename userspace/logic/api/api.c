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

/**
 * Reads syscall desc from corresponding pseudo file in syscall_descs_folder
*/
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

int call_kernelspace_test(int test_index){

    int test_driver_desc = get_syscall_desc("test_driver");
    ON_ERROR_LOG_AND_RETURN((test_driver_desc < 0), -1, "Failed to get test_driver syscall descriptor\n");
    
    return syscall(test_driver_desc, test_index);
}

int put_data(char * source, size_t size){

    int put_data_desc = get_syscall_desc("put_data");
    ON_ERROR_LOG_AND_RETURN((put_data_desc < 0), -1, "Failed to get put_data syscall descriptor\n");
    
    return syscall(put_data_desc, source, size);
}

int get_data(int offset, char * destination, size_t size){
    
    int get_data_desc = get_syscall_desc("get_data");
    ON_ERROR_LOG_AND_RETURN((get_data_desc < 0), -1, "Failed to get get_data syscall descriptor\n");
    
    return syscall(get_data_desc, offset, destination, size);
}

int invalidate_data(int offset){
    
    int invalidate_data_desc = get_syscall_desc("invalidate_data");
    ON_ERROR_LOG_AND_RETURN((invalidate_data_desc < 0), -1, "Failed to get invalidate_data syscall descriptor\n");
    
    return syscall(invalidate_data_desc, offset);
}