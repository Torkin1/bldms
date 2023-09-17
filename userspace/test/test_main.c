#include "logger/logger.h"
#include "test_suites.h"

int main(){

    //ON_ERROR_LOG_AND_RETURN(test_open_read_write_close(), EXIT_FAILURE, "Test failed\n");
    //ON_ERROR_LOG_AND_RETURN(test_syscall(), EXIT_FAILURE, "Test failed\n");
    //ON_ERROR_LOG_AND_RETURN(test_block_serialize(), EXIT_FAILURE, "Test failed\n");
    //ON_ERROR_LOG_AND_RETURN(test_block_move(), EXIT_FAILURE, "Test failed\n");
    //ON_ERROR_LOG_AND_RETURN(test_devkeeper(), EXIT_FAILURE, "Test failed\n");
    test_devkeeper();
    //ON_ERROR_LOG_AND_RETURN(test_invalidate(), EXIT_FAILURE, "Test failed\n");
    //ON_ERROR_LOG_AND_RETURN(test_put_get(), EXIT_FAILURE, "Test failed\n");
    //ON_ERROR_LOG_AND_RETURN(test_mount_twice(), EXIT_FAILURE, "Test failed\n");
    ON_ERROR_LOG_AND_RETURN(test_vfs_read(), EXIT_FAILURE, "Test failed\n");
}
