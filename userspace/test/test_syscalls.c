#include <unistd.h>
#include <stdio.h>

#include "logger/logger.h"
#include "test_suites.h"

int test_syscall(void){

    FILE *syscall_desc_holder;
    int syscall_desc;
    
    syscall_desc_holder = fopen("/sys/kernel/bldms_syscalls/test_driver", "r");
    ON_ERROR_LOG_AND_RETURN((syscall_desc_holder == NULL), -1, "Failed to open syscall descriptor\n");
    fscanf(syscall_desc_holder, "%d", &syscall_desc);
    fclose(syscall_desc_holder);
    ON_ERROR_LOG_AND_RETURN((syscall_desc < 0), -1, "Invalid syscall descriptor\n");

    syscall(syscall_desc, 0);
    

    return 0;
}