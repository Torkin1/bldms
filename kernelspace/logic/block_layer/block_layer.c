#include <linux/types.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>
#include <linux/blk_types.h>
#include <linux/genhd.h>
#include <linux/vmalloc.h>
#include <linux/srcu.h>

#include "block_serialization.h"
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

    spin_lock_init(&b_layer->mounted_lock);

    b_layer->block_size = block_size;
    b_layer->nr_blocks = nr_blocks;

    init_srcu_struct(&b_layer->srcu);
    init_completion(&b_layer->in_progress_write);
    complete(&b_layer->in_progress_write);

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


}

/************** Block layer interactions ******************/

void bldms_reserve_first_blocks(struct bldms_block_layer *b_layer, int nr_blocks){
    b_layer->start_data_index = nr_blocks;
}

void bldms_start_read(struct bldms_block_layer *b_layer, int *reader_id){

    *reader_id = srcu_read_lock(&b_layer->srcu);
}

void bldms_end_read(struct bldms_block_layer *b_layer, int reader_id){

    srcu_read_unlock(&b_layer->srcu, reader_id);
}

void bldms_start_write(struct bldms_block_layer *b_layer){

    might_sleep();
    wait_for_completion(&b_layer->in_progress_write);
        
}

void bldms_end_write(struct bldms_block_layer *b_layer){

    complete(&b_layer->in_progress_write);
}

#define bldms_blocks_foreach_index(block_)\
    for (; block_->header.index != -1;\
     block_->header.index = block_->header.next)

struct bldms_block *bldms_blocks_get_block(struct bldms_block_layer *b_layer,
 struct bldms_blocks_head *list, int block_index){

    struct bldms_block *block;
    pr_debug("%s: list starts at block %d\n", __func__, list->first_bi);

    block = bldms_block_alloc(b_layer->block_size);
    block->header.index = list->first_bi;

    bldms_blocks_foreach_index(block){
        
        bldms_move_block(b_layer, block, READ);
        if (block->header.index == block_index || block_index == BLDMS_ANY_BLOCK_INDEX){
            pr_debug("%s: found block %d\n", __func__, block->header.index);
            break;
        }
    }
    
    return block;
 }

/**
 * @return true if the block contains valid data, false otherwise
*/
bool bldms_block_contains_valid_data(struct bldms_block_layer *b_layer, 
 struct bldms_block *block){
    return block->header.state == BLDMS_BLOCK_STATE_VALID;
}
bool bldms_block_contains_invalid_data(struct bldms_block_layer *b_layer, 
 struct bldms_block *block){
    return block->header.state == BLDMS_BLOCK_STATE_INVALID;
 }

/**
 * Marks some blocks in device head as reserved, hiding them from operations
*/
void blmds_reserve_first_blocks(struct bldms_block_layer *b_layer, int nr_blocks){

    b_layer->start_data_index = nr_blocks;
}

/**
 * Delivers a free block to the caller.
*/
int bldms_get_free_block_any_index(struct bldms_block_layer *b_layer, 
 struct bldms_block **block){
    int res = 0;
    struct bldms_block *b;

    b = bldms_blocks_get_block(b_layer, &b_layer->free_blocks, BLDMS_ANY_BLOCK_INDEX);
    if (b->header.index < 0){
        pr_err("%s: no free blocks available\n", __func__);
        bldms_block_free(b);
        return -1;
    }

    *block = b;
    return res;
}

/**
 * Moves one entry from a blocks list after another one;
 * @return -1 if error, else the index of the moved block
*/
int bldms_blocks_move_block(struct bldms_block_layer *b_layer, 
struct bldms_blocks_head *to, struct bldms_blocks_head *from,
struct bldms_block *block){

    int res = 0;
    struct bldms_block *b_prev, *b_next, *to_last;

    /**
     * If there is a previous block, we update their next pointer, else
     * we update the first_bi pointer of the from list
    */
    if (block->header.prev != -1){
        b_prev = bldms_block_alloc(b_layer ->block_size);
        b_prev ->header.index = block ->header.prev;
        res = bldms_move_block(b_layer, b_prev, READ);
        if (res < 0){
            pr_err("%s: failed to read block %d, previous of %d\n", __func__,
             block->header.prev, block->header.index);
        }
        b_prev ->header.next = block ->header.next;
        res = bldms_move_block(b_layer, b_prev, WRITE);
        if (res < 0){
            pr_err("%s: failed to write block %d, previous of %d\n", __func__,
             block->header.prev, block->header.index);
        }
        bldms_block_free(b_prev);
    }
    else {
        from ->first_bi = block ->header.next;
    }
    /**
     * If there is a next block, we update their prev pointer, else
     * we update the last_bi pointer of the from list
    */
    if (block->header.next != -1){
        b_next = bldms_block_alloc(b_layer ->block_size);
        b_next ->header.index = block ->header.next;
        res = bldms_move_block(b_layer, b_next, READ);
        if (res < 0){
            pr_err("%s: failed to read block %d, next of %d\n", __func__,
             block->header.next, block->header.index);
        }
        b_next ->header.prev = block ->header.prev;
        bldms_move_block(b_layer, b_next, WRITE);
        if (res < 0){
            pr_err("%s: failed to write block %d, next of %d\n", __func__,
             block->header.next, block->header.index);
        }
        bldms_block_free(b_next);
    }
    else {
        from ->last_bi = block ->header.prev;
    }
    /**
     * give last readers time to exit from block to move
    */
    synchronize_srcu(&b_layer->srcu);
    /**
     * Append block to list
    */
    if (to ->last_bi == -1){
        to ->first_bi = block ->header.index;
        to ->last_bi = block ->header.index;
    }
    else {
        to_last = bldms_block_alloc(b_layer ->block_size);
        to_last ->header.index = to ->last_bi;
        res = bldms_move_block(b_layer, to_last, READ);
        if (res < 0){
            pr_err("%s: failed to read block %d, last of to list\n", __func__,
            to->last_bi);
        }
        to_last ->header.next = block ->header.index;
        block ->header.prev = to_last ->header.index;
        block ->header.next = -1;
        res = bldms_move_block(b_layer, to_last, WRITE);
        if (res < 0){
            pr_err("%s: failed to write block %d, last of to list\n", __func__,
            to->last_bi);
        }
        bldms_block_free(to_last);
    }

    return res;
}

int bldms_blocks_move_block_index(struct bldms_block_layer *b_layer, 
 struct bldms_blocks_head *to, struct bldms_blocks_head *from,
 int block_index){
    struct bldms_block *block;
    int res;

    block = bldms_blocks_get_block(b_layer, from, block_index);
    res = bldms_blocks_move_block(b_layer, to, from, block);
    bldms_block_free(block);
    return res;
}

/**
 * Marks the desired block as free to use
*/
int bldms_invalidate_block(struct bldms_block_layer *b_layer, struct bldms_block *block){
    int res = 0;
    
    block ->header.state = BLDMS_BLOCK_STATE_INVALID;
    res = bldms_blocks_move_block(b_layer, &b_layer->free_blocks, &b_layer->used_blocks,
        block);

    return res;
}
int bldms_validate_block(struct bldms_block_layer *b_layer,
 struct bldms_block *block){

    int res = 0;
    block->header.state = BLDMS_BLOCK_STATE_VALID;
    res = bldms_blocks_move_block(b_layer, &b_layer->used_blocks, &b_layer->free_blocks,
     block);
    if (res < 0){
        pr_err("%s: failed to move block %d from free to used blocks\n", __func__,
         block->header.index);
    }
    return res;
}

/**
 * Syncs the block corresponding to the given buffer_head
 * to the device and waits for the operation to complete
*/
#ifdef BLDMS_BLOCK_SYNC_IO
static int bldms_block_sync_io(struct buffer_head *bh){
    int res;

    might_sleep();
    res = sync_dirty_buffer(bh);
    if (res){
        pr_err("%s: failed to sync buffer head %p\n", __func__, bh);
        return -1;
    }
    return 0;
}
#else
static inline int bldms_block_sync_io(struct buffer_head *bh){
    return 0;
}
#endif

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

    if (block->header.index < 0){
        pr_err("%s: invalid block index %d\n", __func__, block->header.index);
        return -1;
    }

    /**
     * Get buffer head correspoding to given block
    */
    bh = sb_bread(b_layer->sb, block->header.index);
    if (!bh){
        pr_err("%s: failed to read block %d from disk %s\n", __func__,
         block->header.index, b_layer->sb->s_bdev->bd_disk->disk_name);
        return -1;
    }

    // do the read/write
    switch(direction){
        case READ:
            bldms_block_deserialize(block, bh->b_data);
            break;
        case WRITE:
            /**
             * FIXME: there is a race condition between the writer and new readers
             * which have started after last grace period expired. Readers can access
             * the buffer head content while it is being modified by the writer, thus
             * leading to possibily inconsistent reads.
             * This can be solved if we can copy the data in a buffer array and
             * then switch the b_data pointer to such buffer array after a grace period
             * expires, but I do not
             * know the ownership of the original b_data pointer. (Who frees it?)
            */
            bldms_block_serialize(block, bh->b_data);        
            mark_buffer_dirty(bh);
            break;
        default:
            pr_err("%s: unsupported data direction %d\n", __func__, direction);
            res = -1;
            goto bldms_move_block_exit;
    }

    /**
     * wait for changes to propagate to device if compiled with write-through policy
    */
    if (bldms_block_sync_io(bh)){
        pr_err("%s: failed to sync block %d\n", __func__, block->header.index);
        res = -1;
        goto bldms_move_block_exit;
    }
    
bldms_move_block_exit:
    brelse(bh);
    return res;
}