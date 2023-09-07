#include "logger/logger.h"
#include "api/api.h"

#include "test_suites.h"

int test_syscall(void){

    return call_kernelspace_test(0);
}