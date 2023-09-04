#include <linux/slab.h>

#include "device/device.h"

/************** Block manipulation stuff*/

int bldms_block_fit_size_to_cap(struct bldms_block *block, size_t size){

    int trimmed_size = 0;
    trimmed_size = size > block->header.data_capacity ? 
        block->header.data_capacity : size;
    return trimmed_size;
}

int bldms_block_memcpy(struct bldms_block *block, void *data, size_t size){
    
    int copied_size = 0;
        
    copied_size = bldms_block_fit_size_to_cap(block, size);
    memcpy(block->data, data, copied_size);
    block->header.data_size = copied_size;

    return copied_size;
}

int bldms_block_memset(struct bldms_block *block_, int value_, size_t size_){
    int copied_size_;

    copied_size_ = bldms_block_fit_size_to_cap(block_, size_);
    memset(block_->data, value_, copied_size_); 
    block_->header.data_size = copied_size_; 

    return copied_size_;
}

/************** Block allocation stuff*/

static size_t bldms_calc_block_header_size(struct bldms_block_header header){
    return sizeof(header.data_size) + sizeof(header.header_size) + 
        sizeof(header.index) + sizeof(header.data_capacity);
}

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
        pr_err("%s: failed to allocate block data\n", __func__);
        kfree(block);
        return NULL;
    }

    return block;
}

void bldms_block_free(struct bldms_block *block){
    kfree(block->data);
    kfree(block);
}
