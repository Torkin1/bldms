#include <linux/types.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>
#include <linux/blk_types.h>
#include <linux/genhd.h>
#include <linux/vmalloc.h>

#include "block_serialization.h"
#include "blocks_list.h"
#include "block_layer.h"

/************** Block layer management **************/

void bldms_block_layer_put(struct bldms_block_layer *b_layer_){
    atomic_sub(1, &b_layer_->users);
}

/**
 * Initializes the block layer. The is no need to know which device will be used yet.
 * @param b_layer: the block layer to initialize
 * @param block_size: the size of a block in bytes
 * @param nr_blocks: the number of blocks in the block layer
 * 
*/
int bldms_block_layer_init(struct bldms_block_layer *b_layer,
 size_t block_size, int nr_blocks){

    int i;

    spin_lock_init(&b_layer->mounted_lock);

    b_layer->block_size = block_size;
    b_layer->nr_blocks = nr_blocks;

    // flags all blocks as ready to access from ops
    b_layer->in_progress_ops = vzalloc(nr_blocks * sizeof(struct completion));
    if(!b_layer->in_progress_ops){
        pr_err("failed to allocate in_progress_ops array\n");
        kfree(b_layer);
        return -1;
    }
    for (i = 0; i < nr_blocks; i ++){
        init_completion(b_layer ->in_progress_ops + i);
        complete(b_layer ->in_progress_ops + i);
    }

    // initializes blocks lists
    b_layer ->free_blocks = bldms_create_blocks_list(nr_blocks);
    b_layer ->used_blocks = bldms_create_blocks_list(0);
    b_layer ->prepared_for_write_blocks = bldms_create_blocks_list(0);
    b_layer ->reserved_blocks = bldms_create_blocks_list(0);

    return 0;

}

int bldms_block_layer_register_sb(struct bldms_block_layer *b_layer,
 struct super_block *sb){

    b_layer->sb = sb;

    spin_lock(&b_layer->mounted_lock);
    b_layer->mounted = true;
    spin_unlock(&b_layer->mounted_lock);

    return 0;    
}

void bldms_block_layer_clean(struct bldms_block_layer *b_layer){

    bldms_destroy_blocks_list(b_layer ->free_blocks);
    bldms_destroy_blocks_list(b_layer ->used_blocks);
    bldms_destroy_blocks_list(b_layer ->prepared_for_write_blocks);
    vfree(b_layer ->in_progress_ops);

}

/************** Block layer interactions ******************/

/**
 * Gets a snapshot of all valid block indexes at the moment of the calling
*/
int bldms_get_valid_block_indexes(struct bldms_block_layer *b_layer,
 int *block_indexes, int max_blocks){
    return bldms_blocks_snapshot(b_layer->used_blocks, block_indexes, max_blocks);
 }

/**
 * @return true if the block contains valid data, false otherwise
*/
bool bldms_block_contains_valid_data(struct bldms_block_layer *b_layer, int block_index){
    return bldms_blocks_contains(b_layer->used_blocks, block_index);
}

/**
 * Reserves a free block, hiding it from most of ops
*/
int bldms_reserve_block(struct bldms_block_layer *b_layer, int block_index){
    int res = 0;
    res = bldms_blocks_move_block(b_layer->reserved_blocks, b_layer->free_blocks,
     block_index);
    if (res < 0){
        pr_err("%s: failed to move block %d in reserved_blocks\n", __func__, block_index);
    }
    return res;
}

/**
 * Reserves the desired block for writing
*/
int bldms_prepare_write_on_block(struct bldms_block_layer *b_layer, int block_index){
    int res = 0;
    res = bldms_blocks_move_block(b_layer->prepared_for_write_blocks, b_layer->free_blocks,
     block_index);
    if (res < 0){
        pr_err("%s: failed to move block %d in in_use_by_write_bocks\n", __func__, block_index);
        return -1;
    }
    return res;
}

/**
 * Commits the write to the desired block, marking it as occupied by valid data
*/
int bldms_commit_write_on_block(struct bldms_block_layer *b_layer, int block_index){
    int res = 0;
    res = bldms_blocks_move_block(b_layer->used_blocks, b_layer ->prepared_for_write_blocks,
     block_index);
    if (res < 0){
        pr_err("%s: failed to move block %d in used_blocks\n", __func__, block_index);
        return -1;
    }
    return res;
}

/**
 * Marks the desired block as free to use
*/
int bldms_invalidate_block(struct bldms_block_layer *b_layer, int block_index){
    int res = 0;
    res = bldms_blocks_move_block(b_layer->free_blocks, b_layer->used_blocks, block_index);
    if (res < 0){
        pr_err("%s: failed to release block %d\n", __func__, block_index);
        return -1;
    }
    return res;
}

/**
 * Prepares a block for writing
*/
int bldms_prepare_write_on_block_any(struct bldms_block_layer *b_layer){

    int block_index;

    block_index = bldms_prepare_write_on_block(b_layer, BLDMS_ANY_BLOCK_INDEX);
    if (block_index < 0){
        pr_err("%s: no free blocks available in device\n", __func__);
        return -1;
    }
    return block_index;    

}

/**
 * Undoes the reserving of a block from writing
*/
int bldms_undo_write_on_block(struct bldms_block_layer *b_layer, int block_index){

    int res = 0;
    res = bldms_blocks_move_block(b_layer->free_blocks, b_layer->prepared_for_write_blocks,
     block_index);
    if (res){
        pr_err("%s: failed to move block %d in free_blocks\n", __func__, block_index);
        return -1;
    }
    return res;

}

/**
 * Signals that an operation is in progress on a block
 * This function sleeps until the underway operation is completed or a signal is caught
*/
int bldms_start_op_on_block(struct bldms_block_layer *b_layer, int block_index){
    
    int res_wait;
    
    might_sleep();
    res_wait = wait_for_completion_interruptible(b_layer->in_progress_ops + block_index);
    if (res_wait == -ERESTARTSYS){
        pr_err("%s: interrupted while waiting pending op on block %d\n", __func__, block_index);
        return -1;
    }
    return 0;
}

/**
 * Call this when your operation has completed on a block
*/
void bldms_end_op_on_block(struct bldms_block_layer *b_layer, int block_index){
    complete(b_layer->in_progress_ops + block_index);
}

/**
 * Moves one block of data to/from the device.
 * Blocks are abstracted using the buffer_head api
*/
int bldms_move_block(struct bldms_block_layer *b_layer,
 struct bldms_block *block, int direction){
    
    struct buffer_head *bh;
    int res;

    might_sleep();
    res = 0;

    /**
     * Get buffer head correspoding to given block
    */
    bh = sb_bread(b_layer->sb, block->header.index);
    if (!bh){
        pr_err("%s: failed to read block %d from disk %s\n", __func__,
         block->header.index, b_layer->sb->s_bdev->bd_disk->disk_name);
        return -1;
    }

    switch(direction){
        case READ:
            bldms_block_deserialize(block, bh->b_data);
            break;
        case WRITE:
            bldms_block_serialize(block, bh->b_data);
            mark_buffer_dirty(bh);
            break;
        default:
            pr_err("%s: unsupported data direction %d\n", __func__, direction);
            goto bldms_move_block_exit;
    }
    
bldms_move_block_exit:
    brelse(bh);
    return res;
}