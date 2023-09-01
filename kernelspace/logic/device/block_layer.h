#ifndef BLOCK_LAYER_H_INCLUDED
#define BLOCK_LAYER_H_INCLUDED

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

struct bldms_free_blocks_entry{

    struct list_head list_head;
    int block_index;    // index of the block in the device
};

struct bldms_free_blocks_list{

    struct bldms_free_blocks_entry *head;
    spinlock_t write_lock;
};

struct bldms_block *bldms_block_alloc(size_t block_size);
void bldms_block_free(struct bldms_block *block);

int bldms_block_memcpy(struct bldms_block *block, void *data, size_t size);
int bldms_block_memset(struct bldms_block *block_, int value_, size_t size_);

/**
 * Translates the block into a byte array which can be stored on disk.
 * Buffer must be big enough to hold data and header sizes.
*/
void bldms_block_serialize(struct bldms_block *block, u8 *buffer);

/**
 * Reads block data and header from a byte array.
*/
void bldms_block_deserialize(struct bldms_block *block, u8 *buffer);
size_t bldms_get_serialized_block_size(struct bldms_block *block);

struct bldms_free_blocks_list *bldms_create_free_blocks_list(int nr_blocks);
void bldms_destroy_free_blocks_list(struct bldms_free_blocks_list *head);


#endif // BLOCK_LAYER_H_INCLUDED