#include "test_suites.h"
#include "logger/logger.h"
#include "api/api.h"

int test_block_move(void){

    return call_kernelspace_test(2);
}