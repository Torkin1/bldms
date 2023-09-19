#ifndef BLOCK_LAYER_H
#define BLOCK_LAYER_H

#include <linux/types.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/atomic.h>

#include "block.h"
#include "blocks_list.h"

struct bldms_block_layer {

    struct super_block *sb; // superblock of the fs owning the block layer
    bool mounted; // true if the fs owning the block layer is mounted
    spinlock_t mounted_lock;
    atomic_t users; // number of users of the block layer
    size_t block_size; // size of a block in bytes
    int nr_blocks; // number of blocks in the device
    struct bldms_blocks_list *free_blocks; // list of blocks containing invalid data
    struct bldms_blocks_list *used_blocks; // list of blocks containing valid data
    /**
     * Userspace cannot invalidate or put data in blocks of reserved list
    */
    struct bldms_blocks_list *reserved_blocks; 
    /**
      * There is a possible race condition between get_data() and put_data():
      * get_data():                 put_data():
      *                             reserve_block()
      * block_contains_valid_data()
      * read_block()
      *                             write_block()
      * 
      * get_data() returns invalidated data. This happens because the choosing of the block
      * and the actual writing are not done atomically togheter.
      * We can solve this by putting entries
      * of blocks to be written in a temporary list and move it to the used_blocks
      * only when we finish to write the block.
     * 
    */
    struct bldms_blocks_list *prepared_for_write_blocks;
    
    /**
     * Another race condition can be this:
     * get_data():                  invalidate_data():
     * block_contains_valid_data()       
     *                              block_contains_valid_data()
     *                              invalidate_block()
     * read_block()
     * 
     * get_data() returns invalidated data. This could be solved using the same
     * approach as in_use_by_write_blocks, but it would mean that readers must
     * wait each one every time there are conflicting ops on same block. This
     * could negate the perfomance improvements brought by using RCU lists.
     * Here another approach is used: keep an array of copletion vars to signal
     * whenever an op is currently in progress on a block.
     * For an op to start accessing the block, it must first wait that the
     * currenttly running op on the block finishes, if there is one.
    */
    struct completion *in_progress_ops;
};

int bldms_block_layer_init(struct bldms_block_layer *b_layer,
 size_t block_size, int nr_blocks);
void bldms_block_layer_clean(struct bldms_block_layer *b_layer);
int bldms_block_layer_register_sb(struct bldms_block_layer *b_layer,
 struct super_block *sb);

int bldms_move_block(struct bldms_block_layer *b_layer,
 struct bldms_block *block, int direction);
bool bldms_block_contains_valid_data(struct bldms_block_layer *b_layer,
 int block_index);
int bldms_get_valid_block_indexes(struct bldms_block_layer *b_layer,
 int *block_indexes, int max_blocks);
int bldms_prepare_write_on_block(struct bldms_block_layer *b_layer, int block_index);
int bldms_prepare_write_on_block_any(struct bldms_block_layer *b_layer);
int bldms_commit_write_on_block(struct bldms_block_layer *b_layer, int block_index);
int bldms_undo_write_on_block(struct bldms_block_layer *b_layer, int block_index);
int bldms_start_op_on_block(struct bldms_block_layer *b_layer, int block_index);
void bldms_end_op_on_block(struct bldms_block_layer *b_layer, int block_index);
int bldms_invalidate_block(struct bldms_block_layer *b_layer, int block_index);
int bldms_reserve_block(struct bldms_block_layer *b_layer, int block_index);

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

#define bldms_block_layer_use(b_layer_) bldms_if_mounted(b_layer_, atomic_add(1, &b_layer_->users));\


void bldms_block_layer_put(struct bldms_block_layer *b_layer_);

#endif // BLOCK_LAYER_H