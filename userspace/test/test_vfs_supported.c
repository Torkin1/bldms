#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include "test_suites.h"
#include "logger/logger.h"
#include "api/api.h"

static int count_msgs(const char **msgs){
    int count = 0;
    while(msgs[count] != NULL) count ++;
    return count;
}

static int size_msgs(const char **msgs){
    int size = 0;
    for(int i = 0; msgs[i] != NULL; i ++){
        size += strlen(msgs[i]);
    }
    return size;
}

int test_vfs_read(){

    const char *msgs[] = {
        "message 1-",
        "mess2-",
        "m3",
        NULL,
    };
    const int nr_msgs = count_msgs(msgs);
    int b_indexes[nr_msgs];    
    const char *the_file = "./test_mount/the-file";
    char expected[size_msgs(msgs) + 1];
    char actual[size_msgs(msgs) + 1];
    int fd;
    int res = 0;
    int expected_i = 0;

    memset(expected, 0, size_msgs(msgs) + 1);
    memset(actual, 0, size_msgs(msgs) + 1);

    for(int i = 0; i < nr_msgs; i ++){
        memcpy(expected + expected_i, msgs[i], strlen(msgs[i]));
        expected_i += strlen(msgs[i]);
        b_indexes[i] = put_data((char *)msgs[i], strlen(msgs[i]));
    }

    fd = open(the_file, O_RDONLY);

    read(fd, actual, size_msgs(msgs));
    if (memcmp(expected, actual, size_msgs(msgs)) != 0){
        printf("expected: %s\n", expected);
        printf("actual: %s\n", actual);
        res = -1;
    }

    for (int i = 0; i < nr_msgs; i ++){
        invalidate_data(b_indexes[i]);
    }
    close(fd);
    return res;
}