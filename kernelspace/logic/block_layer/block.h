#ifndef BLOCK_H_INCLUDED
#define BLOCK_H_INCLUDED

#include <linux/types.h>

static const int BLDMS_ANY_BLOCK_INDEX = -1;

enum bldms_block_memcpy_dir{
    BLDMS_BLOCK_MEMCPY_TO_BLOCK,
    BLDMS_BLOCK_MEMCPY_FROM_BLOCK
};

enum bldms_block_state{
    BLDMS_BLOCK_STATE_VALID,    // block contains valid data
    BLDMS_BLOCK_STATE_INVALID,  // block contains invalid data
    BLDMS_BLOCK_STATE_NR_STATES
};

struct bldms_block_header{

    size_t data_size;  // size of data in bytes
    size_t data_capacity; // max bytes of data that can be stored
    size_t header_size; // size of header in bytes
    int index;  // index of the block in the device
    enum bldms_block_state state;
    int next; // index of the next block in the device with same state
    int prev;// index of the prev block in the device with same statet
};

struct bldms_block{

    struct bldms_block_header header;
    void *data;
};

struct bldms_block *bldms_block_alloc(size_t block_size);
void bldms_block_free(struct bldms_block *block);

int bldms_block_memcpy(struct bldms_block *block, void *data, size_t size,
 enum bldms_block_memcpy_dir dir);
int bldms_block_memset(struct bldms_block *block_, int value_, size_t size_);

#endif // BLOCK_H_INCLUDED