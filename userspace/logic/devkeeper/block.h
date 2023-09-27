#ifndef BLOCK_H_INCLUDED
#define BLOCK_H_INCLUDED

#include <stddef.h>

enum bldms_block_state{
    BLDMS_BLOCK_STATE_INVALID,  // block contains invalid data
    BLDMS_BLOCK_STATE_VALID,    // block contains valid data
    BLDMS_BLOCK_STATE_NR_STATES
};

struct bldms_block_header{

    size_t data_size;  // size of data in bytes
    size_t data_capacity; // max bytes of data that can be stored
    size_t header_size; // size of header in bytes
    int index;  // index of the block in the device
    enum bldms_block_state state;
    int next; // index of the next block in the device with same state
    int prev; // index of the prev block in the device with same statet
};

struct bldms_block{

    struct bldms_block_header header;
    void *data;
};

#endif // BLOCK_H_INCLUDED