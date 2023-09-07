#include "test_suites.h"
#include "logger/logger.h"
#include "api/api.h"

static const char *expected = "Hello World!";
static char actual[256];

int test_put_get(){

    int block_index;
    int get_res;
    memset(actual, 0, 256);

    block_index = put_data((char *)expected, strlen(expected));
    ON_ERROR_LOG_AND_RETURN((block_index < 0), -1, "Failed to put data\n");

    get_res = get_data(block_index, actual, strlen(expected));
    ON_ERROR_LOG_AND_RETURN((get_res < 0), -1, "Failed to get data\n");

    ON_ERROR_LOG_AND_RETURN((strcmp(expected, actual) != 0), -1,
     "Expected: %s, Actual: %s\n", expected, actual);

    return 0;

}

int test_invalidate(){

    int block_index;
    int invalidate_res;
    int get_res;
    int get_res_errno;

    memset(actual, 0, 256);

    block_index = put_data((char *)expected, strlen(expected));
    ON_ERROR_LOG_AND_RETURN((block_index < 0), -1, "Failed to put data\n");

    invalidate_res = invalidate_data(block_index);
    ON_ERROR_LOG_AND_RETURN((invalidate_res < 0), -1, "Failed to invalidate data\n");

    get_res = get_data(block_index, actual, strlen(expected));
    get_res_errno = errno;
    ON_ERROR_LOG_AND_RETURN((get_res != -1 || get_res_errno != ENODATA), -1, "Expected: %d, Actual: %d\n",
     ENODATA, get_res_errno);

    return 0;
}