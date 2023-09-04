#include "device/block_serialization.h"
#include <linux/string.h>

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
    bldms_block_serialize_header_field(buffer, block, index, offset_p);
}

void bldms_block_deserialize_header(struct bldms_block *block, u8 *buffer, 
 int *offset_p)
{
    bldms_block_deserialize_header_field(block, buffer, data_size, offset_p);
    bldms_block_deserialize_header_field(block, buffer, data_capacity, offset_p);
    bldms_block_deserialize_header_field(block, buffer, header_size, offset_p);
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