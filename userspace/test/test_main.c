#include "logger/logger.h"
#include "test_suites.h"

int main(){

    //ON_ERROR_LOG_AND_RETURN(test_open_read_write_close(), EXIT_FAILURE, "Test failed\n");
    ON_ERROR_LOG_AND_RETURN(test_syscall(), EXIT_FAILURE, "Test failed\n");

}
