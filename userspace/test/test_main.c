#include "logger/logger.h"
#include "test_suites.h"

int (*tests[])() = {
    test_open_read_write_close,
    NULL
};

int main(){

    int i;
    int (*cur_test)();

    // do tests ...
    for (i = 0, cur_test=tests[i] ; cur_test != NULL; i++, cur_test = tests[i]){
        ON_ERROR_LOG_AND_RETURN(cur_test(), EXIT_FAILURE, "Test %d failed\n", i);
    }

}
