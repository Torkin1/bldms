#include <stdio.h>

#include "logger/logger.h"
#include "api/api.h"
#include "test_suites.h"

int test_block_serialize(void){

    return call_kernelspace_test(1);
}