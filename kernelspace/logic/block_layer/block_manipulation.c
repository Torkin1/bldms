#include <linux/slab.h>
#include <linux/string.h>
#include <linux/minmax.h>

#include "block.h"

/************** Block manipulation stuff*/

/**
 * Copies data safely to/from a struct block updating its header
*/
int bldms_block_memcpy(struct bldms_block *block, void *buffer, size_t buffer_size,
 enum bldms_block_memcpy_dir dir){
    
    int copied_size = 0;
        
    switch(dir){
        case BLDMS_BLOCK_MEMCPY_TO_BLOCK:{
            copied_size = min(buffer_size, block->header.data_capacity);
            memcpy(block->data, buffer, copied_size);
            block->header.data_size = copied_size;
            break;
        }
        case BLDMS_BLOCK_MEMCPY_FROM_BLOCK:{
            copied_size = min(buffer_size, block->header.data_size);
            memcpy(buffer, block->data, copied_size);
            break;
        }
        default:{
            pr_err("%s: invalid data direction\n", __func__);
            copied_size = -1;
        }
    }

    return copied_size;
}

/**
 * Sets all block's data bytes to a given value
 */
int bldms_block_memset(struct bldms_block *block, int value, size_t size){
    int copied_size;

    copied_size = min(block->header.data_capacity, size);
    memset(block->data, value, copied_size); 
    block->header.data_size = copied_size; 

    return copied_size;
}

/************** Block allocation stuff*/

static size_t bldms_calc_block_header_size(struct bldms_block_header header){
    return sizeof(header.data_size) + sizeof(header.header_size) + 
        sizeof(header.index) + sizeof(header.data_capacity) +
        sizeof(header.state) + sizeof(header.next) + sizeof(header.prev);
}

/**
 * Allocs and inits a block of given size.
 * Size must account for the block header size
*/
struct bldms_block *bldms_block_alloc(size_t block_size){
    struct bldms_block *block;
    block = kzalloc(sizeof(struct bldms_block), GFP_KERNEL);
    if(!block){
        pr_err("%s: failed to allocate block\n", __func__);
        return NULL;
    }
    block->header.data_size = 0;
    block->header.header_size = bldms_calc_block_header_size(block->header);
    block->header.index = -1;
    block->header.data_capacity = block_size - block->header.header_size;
    block->data = kzalloc(block->header.data_capacity, GFP_KERNEL);
    if(!block->data){
        pr_err("%s: failed to allocate block data buffer\n", __func__);
        kfree(block);
        return NULL;
    }
    block->header.state = BLDMS_BLOCK_STATE_NR_STATES;
    block->header.next = -1;
    block->header.prev = -1;

    return block;
}

void bldms_block_free(struct bldms_block *block){
    kfree(block->data);
    kfree(block);
}
