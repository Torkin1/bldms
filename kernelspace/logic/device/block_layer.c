#include <linux/slab.h>
#include <linux/rculist.h>
#include <linux/types.h>

#include "device/block_layer.h"
#include "device/device.h"

/************** Block manipulation stuff*/

int bldms_block_memcpy(struct bldms_block *block, void *data, size_t size){
    
    int copied_size = 0;
        
    copied_size = size > block->header.data_capacity ? block->header.data_capacity : size;
    memcpy(block->data, data, copied_size);
    block->header.data_size = copied_size;

    return copied_size;
}

/************** Block allocation stuff*/

static size_t bldms_calc_block_header_size(struct bldms_block_header header){
    return sizeof(header.data_size) + sizeof(header.header_size) + 
        sizeof(header.index) + sizeof(header.size);
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
    block->header.size = block_size;
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

/************** Block Serialization stuff*/

void bldms_memcpy_stateful(void * dest, void * src, size_t size,
 int *offset_p)
{
    memcpy(dest, src, size);
    *offset_p += size;
}

void bldms_serialize(void *dest, void *src, size_t size, int *offset_p)
{
    bldms_memcpy_stateful(dest + *offset_p, src, size, offset_p);
}

void bldms_deserialize(void *dest, void *src, size_t size, int *offset_p)
{
    bldms_memcpy_stateful(dest, src + *offset_p, size, offset_p);
}

#define bldms_block_serialize_header_field(dest_, block_, field_, offset_p_)\
{\
    bldms_serialize(dest_, &block_->header.field_, \
     sizeof(block->header.field_), offset_p_); \
}

#define bldms_block_deserialize_header_field(block_, src_, field_, offset_p_)\
{\
    bldms_deserialize(&block_->header.field_, src_, \
     sizeof(block->header.field_), offset_p_); \
} 

void bldms_block_serialize_header(struct bldms_block *block, u8 *buffer,
 int *offset_p)
{
    bldms_block_serialize_header_field(buffer, block, data_size, offset_p);
    bldms_block_serialize_header_field(buffer, block, data_capacity, offset_p);
    bldms_block_serialize_header_field(buffer, block, header_size, offset_p);
    bldms_block_serialize_header_field(buffer, block, size, offset_p);
    bldms_block_serialize_header_field(buffer, block, index, offset_p);
}

void bldms_block_deserialize_header(struct bldms_block *block, u8 *buffer, 
 int *offset_p)
{
    bldms_block_deserialize_header_field(block, buffer, data_size, offset_p);
    bldms_block_deserialize_header_field(block, buffer, data_capacity, offset_p);
    bldms_block_deserialize_header_field(block, buffer, header_size, offset_p);
    bldms_block_deserialize_header_field(block, buffer, size, offset_p);
    bldms_block_deserialize_header_field(block, buffer, index, offset_p);
}

void bldms_block_serialize_data(struct bldms_block *block, u8 *buffer,
 int *offset_p)
{
    bldms_serialize(buffer, block->data,
     block->header.data_size, offset_p);
}

void bldms_block_deserialize_data(struct bldms_block *block, u8 *buffer,
 int *offset_p)
{
    bldms_deserialize(block->data, buffer,
     block->header.data_size, offset_p);
}

void bldms_block_serialize(struct bldms_block *block, u8 *buffer)
{
    int offset = 0;
    bldms_block_serialize_header(block, buffer, &offset);
    bldms_block_serialize_data(block, buffer, &offset);
}

void bldms_block_deserialize(struct bldms_block *block, u8 *buffer)
{
    int cursor = 0;
    bldms_block_deserialize_header(block, buffer, &cursor);
    bldms_block_deserialize_data(block, buffer, &cursor);
}

/************** Block layer interactions with the block device*/

/**
 * Translates a block index to a sector index in the device
*/
static sector_t bldms_block_to_sector(struct bldms_device *dev, 
    int block_index){
    
    return block_index * (dev->block_size / dev->sector_size);    
}

/** Moves one block of data*/
int bldms_move_block(struct bldms_device *dev,
    struct bldms_block *block, enum req_opf op){
    
    struct bio *bio;
    int res;
    sector_t start_sector;
    int buffer_order;
    struct page *start_page;
    u8 *buffer;
    struct block_device *bdev;

    /**
     * We need to translate the block index to a sector index in the device
    */
    start_sector = bldms_block_to_sector(dev, block->header.index);
    
    /**
     * Open the bldms device as a block device
     * FIXME: we should open the device before calling this function,
     * and close it when we did all the operations we needed
    */
    bdev = blkdev_get_by_path(dev->path, FMODE_READ | FMODE_WRITE | FMODE_EXCL, 
        dev->driver->device_ops.owner);
    if (IS_ERR(bdev)){
        pr_err("%s: failed to open device at %s as a block device\n", __func__, dev->path);
        return -1;
    }
    
    /**
     * Create a struct bio describing the operations on block
    */
    bio = bio_alloc(GFP_KERNEL, 1);
    if(!bio){
        pr_err("%s: failed to allocate bio\n", __func__);
        return -1;
    }
    bio ->bi_bdev = bdev;
    bio ->bi_iter.bi_sector = start_sector;
    bio ->bi_opf = op;

    /**
     * Allocates pages to hold serialized block and adds them to the bio 
    */
    buffer_order = order_base_2(dev ->block_size / PAGE_SIZE);
    start_page = alloc_pages(GFP_KERNEL | __GFP_ZERO, buffer_order);
    if(!start_page){
        pr_err("%s: failed to allocate pages of order %d\n", __func__, buffer_order);
        return -1;
    }
    bio_add_page(bio, start_page, dev->block_size, 0);

    /**
     * If op is a write, we need to serialize the block and copy it to the buffer
    */
    if(op == REQ_OP_WRITE){
        buffer = kmap_local_page(start_page);
        bldms_block_serialize(block, buffer);
        kunmap_local(buffer);
    }
        
    // submits the bio to the I/O subsystem in order to add it to a request
    res = submit_bio_wait(bio);
    if (res < 0){
        pr_err("%s: failed to submit bio with error %d\n", __func__, res);
        return -1;
    }

    /**
     * if op is a read, we need to deserialize the block from the buffer
     * and copy it to the block struct
    */
    if (op == REQ_OP_READ){
        buffer = kmap_local_page(start_page);
        bldms_block_deserialize(block, buffer);
        kunmap_local(buffer);
    }
    
    bio_put(bio);
    __free_pages(start_page, buffer_order);
    blkdev_put(bdev, FMODE_READ | FMODE_WRITE | FMODE_EXCL);

    return 0;
    
}

/************** Free blocks list allocation and manipulation*/

struct bldms_free_blocks_list *bldms_create_free_blocks_list(int nr_blocks){
    struct bldms_free_blocks_list *list;
    struct bldms_free_blocks_entry *entry;
    struct bldms_free_blocks_entry *head;
    int i;

    list = kzalloc(sizeof(struct bldms_free_blocks_list), GFP_KERNEL);
    spin_lock_init(&list->write_lock);
    for (i = 0; i < nr_blocks; i ++){
        entry = kzalloc(sizeof(struct bldms_free_blocks_entry), GFP_KERNEL);
        entry->block_index = i;
        if (i == 0){
            INIT_LIST_HEAD(&entry->list_head);
            head = entry;
        }
        else {
            list_add(&entry->list_head, &head->list_head);
        }
    }
    list->head = head;
    return list;
}

void bldms_destroy_free_blocks_list(struct bldms_free_blocks_list *list){

    struct bldms_free_blocks_entry *pos;
    
    spin_lock(&list->write_lock);
    list_for_each_entry(pos, &list->head->list_head, list_head){
        list_del_rcu(&pos->list_head);
        synchronize_rcu();
        kfree(pos);
    }
    spin_unlock(&list->write_lock);
}