#ifndef BLOCK_LAYER_H
#define BLOCK_LAYER_H

#include <linux/types.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/srcu.h>

#include "block.h"

/**
 * Blocks are double-linked to each other according to their data state
 * (valid or invalid). This struct is used to keep track of the first and last
 * block in each list.
 * 
 * To traverse the list, one can use the bldms_blocks_foreach_index() macro to iterate
 * over the indexes, combined with reading the block at the current index from the 
 * device with bldms_move_block().
 * 
 * Examples can be found in bldms_read() and invalidate_data() implementations. 
*/
struct bldms_blocks_head{

    int first_bi;
    int last_bi;
};

#define bldms_blocks_foreach_index(block_)\
    for (; block_->header.index != -1;\
     block_->header.index = block_->header.next)

struct bldms_block_layer {

    struct super_block *sb; // superblock of the fs owning the block layer
    bool mounted; // true if the fs owning the block layer is mounted
    spinlock_t mounted_lock;
    atomic_t users; // number of users of the block layer
    size_t block_size; // size of a block in bytes
    int nr_blocks; // number of blocks in the device
    struct bldms_blocks_head free_blocks; // list of blocks containing invalid data
    struct bldms_blocks_head used_blocks; // list of blocks containing valid data
    struct srcu_struct srcu;
    struct completion in_progress_write;
    int start_data_index; // index of the first block containing data 
};

int bldms_block_layer_init(struct bldms_block_layer *b_layer,
 size_t block_size, int nr_blocks);
void bldms_block_layer_clean(struct bldms_block_layer *b_layer);
int bldms_block_layer_register_sb(struct bldms_block_layer *b_layer,
 struct super_block *sb);

int bldms_move_block(struct bldms_block_layer *b_layer,
 struct bldms_block *block, int direction);
bool bldms_block_contains_valid_data(struct bldms_block_layer *b_layer, 
 struct bldms_block *block);
void bldms_reserve_first_blocks(struct bldms_block_layer *b_layer, int nr_blocks);
void bldms_start_read(struct bldms_block_layer *b_layer, int *reader_id);
void bldms_end_read(struct bldms_block_layer *b_layer, int reader_id);
void bldms_start_write(struct bldms_block_layer *b_layer);
void bldms_end_write(struct bldms_block_layer *b_layer);
int bldms_invalidate_block(struct bldms_block_layer *b_layer,
 struct bldms_block *block);
int bldms_validate_block(struct bldms_block_layer *b_layer,
 struct bldms_block *block);
int bldms_get_free_block_any_index(struct bldms_block_layer *b_layer, 
 struct bldms_block **block);

#define bldms_if_mounted(b_layer__, do_){\
    spin_lock(&b_layer__->mounted_lock);\
    if (!b_layer__->mounted){\
        spin_unlock(&b_layer__->mounted_lock);\
        pr_err("%s: device is not mounted\n", __func__);\
        return -ENODEV;\
    }\
    do_;\
    spin_unlock(&b_layer__->mounted_lock);\
}

#define bldms_block_layer_use(b_layer_) bldms_if_mounted(b_layer_, atomic_add(1, &b_layer_->users));

void bldms_block_layer_put(struct bldms_block_layer *b_layer_);

#endif // BLOCK_LAYER_H