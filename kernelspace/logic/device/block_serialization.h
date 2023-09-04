#ifndef BLOCK_SERIALIZATION_H_INCLUDED
#define BLOCK_SERIALIZATION_H_INCLUDED

#include <linux/types.h>
#include "device/block.h"

/**
 * Reads block data and header from a byte array.
*/
void bldms_block_deserialize(struct bldms_block *block, u8 *buffer);
size_t bldms_get_serialized_block_size(struct bldms_block *block);
/**
 * Translates the block into a byte array which can be stored on disk.
 * Buffer must be big enough to hold data and header sizes.
*/
void bldms_block_serialize(struct bldms_block *block, u8 *buffer);

#endif // BLOCK_SERIALIZATION_H_INCLUDED
