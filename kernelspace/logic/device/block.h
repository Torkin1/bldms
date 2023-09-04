#ifndef BLOCK_H_INCLUDED
#define BLOCK_H_INCLUDED

#include <linux/types.h>

struct bldms_block_header{

    size_t data_size;  // size of data in bytes
    size_t data_capacity; // max bytes of data that can be stored
    size_t header_size; // size of header in bytes
    int index;  // index of the block in the device
};

struct bldms_block{

    struct bldms_block_header header;
    void *data;
};

struct bldms_block *bldms_block_alloc(size_t block_size);
void bldms_block_free(struct bldms_block *block);

int bldms_block_memcpy(struct bldms_block *block, void *data, size_t size);
int bldms_block_memset(struct bldms_block *block_, int value_, size_t size_);

#endif // BLOCK_H_INCLUDED