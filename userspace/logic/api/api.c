#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "logger/logger.h"
#include "api.h"

static const char *syscall_descs_folder = "/sys/kernel/bldms_syscalls";
static const char *parameters_folder = "/sys/module/bldms/parameters";

int build_pseudofile_path(const char * prefix, char *pseudofile_name, char *pseudofile_path){

    strcpy(pseudofile_path, prefix);
    strcat(pseudofile_path, "/");
    strcat(pseudofile_path, pseudofile_name);
    
    return 0;
} 

int get_int_from_pseudofile(char *pseudofile_path){
    FILE *pseudofile;
    int pseudofile_val;
    
    pseudofile = fopen(pseudofile_path, "r");
    ON_ERROR_LOG_AND_RETURN((pseudofile == NULL), -1, "Failed to open pseudofile descriptor\n");
    fscanf(pseudofile, "%d", &pseudofile_val);
    fclose(pseudofile);

    return pseudofile_val;
}

int get_string_from_pseudofile(char *pseudofile_path, char *buf){
    FILE *pseudofile;
    
    pseudofile = fopen(pseudofile_path, "r");
    ON_ERROR_LOG_AND_RETURN((pseudofile == NULL), -1, "Failed to open pseudofile descriptor\n");
    fscanf(pseudofile, "%s", buf);
    fclose(pseudofile);

    return 0;
}

/**
 * Reads syscall desc from corresponding pseudo file in syscall_descs_folder
*/
int get_syscall_desc(char *syscall_name){

    char syscall_desc_path[256];

    memset(syscall_desc_path, 0, 256);
    build_pseudofile_path(syscall_descs_folder, syscall_name, syscall_desc_path);

    return get_int_from_pseudofile(syscall_desc_path);
}

int get_int_param(char *param_name){

    char param_path[256];

    memset(param_path, 0, 256);
    build_pseudofile_path(parameters_folder, param_name, param_path);

    return get_int_from_pseudofile(param_path);

}

int get_string_param(char *param_name, char *buf){

    char param_path[256];

    memset(param_path, 0, 256);
    build_pseudofile_path(parameters_folder, param_name, param_path);

    return get_string_from_pseudofile(param_path, buf);

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