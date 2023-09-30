#include <linux/types.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>
#include <linux/blk_types.h>
#include <linux/genhd.h>
#include <linux/vmalloc.h>
#include <linux/srcu.h>
#include <linux/mutex.h>
#include <linux/fs.h>

#include "block_serialization.h"
#include "block_layer.h"
#include "ops/vfs_supported.h"

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

    INIT_LIST_HEAD(&b_layer->read_states.head);
    mutex_init(&b_layer->read_states.w_lock);
    init_srcu_struct(&b_layer->read_states.srcu);

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

    struct bldms_read_state *pos;
    
    mutex_lock(&b_layer->read_states.w_lock);
    // we need to free all read states
    list_for_each_entry(pos, &b_layer->read_states.head, list_node){
        list_del(&pos->list_node);
        synchronize_srcu(&b_layer->read_states.srcu);
        bldms_read_state_free(pos);
    }
    mutex_unlock(&b_layer->read_states.w_lock);

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

    /**
     * Saving b_layer state to disk at every write guarantees to remember changes
     * in case of sudden disk unavailability.
    */
    b_layer->save_state(b_layer);
}

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
 * Moves one entry from a blocks list at the end of another one, performing needed
 * updates to blocks in device.
 * @param b_layer: the block layer
 * @param to: the receiving list
 * @param from: the donating list
 * @param block: the block to move, must be in the receiving list 
 * @return -1 if error, else 0
*/
int bldms_blocks_move_block(struct bldms_block_layer *b_layer, 
struct bldms_blocks_head *to, struct bldms_blocks_head *from,
struct bldms_block *block){

    int res = 0;
    struct bldms_block *from_block_prev, *from_block_next, *to_last_old;
    int block_old_prev_i, block_old_next_i;

    might_sleep();

    block_old_prev_i = block->header.prev;
    block_old_next_i = block->header.next;
    pr_debug("%s: moving block %d has prev %d and next %d\n", __func__,
     block->header.index, block->header.prev, block->header.next);

    /**
     * If there is a previous block, we update their next pointer, else
     * we update the first_bi pointer of the from list
    */
    if (block->header.prev != -1){
        from_block_prev = bldms_block_alloc(b_layer ->block_size);
        from_block_prev ->header.index = block ->header.prev;
        res = bldms_move_block(b_layer, from_block_prev, READ);
        if (res < 0){
            pr_err("%s: failed to read block %d, previous of %d\n", __func__,
             block->header.prev, block->header.index);
            bldms_block_free(from_block_prev);
            return -1;
        }
        from_block_prev ->header.next = block ->header.next;
        pr_debug("%s: updating next of from block prev %d to %d\n", __func__,
         from_block_prev->header.index, block->header.next);
        res = bldms_move_block(b_layer, from_block_prev, WRITE);
        if (res < 0){
            pr_err("%s: failed to write block %d, previous of %d\n", __func__,
             block->header.prev, block->header.index);
            bldms_block_free(from_block_prev);
            return -1;
        }
        bldms_block_free(from_block_prev);
    }
    /**
     * If there is a next block, we update their prev pointer, else
     * we update the last_bi pointer of the from list
    */
    if (block->header.next != -1){
        from_block_next = bldms_block_alloc(b_layer ->block_size);
        from_block_next ->header.index = block ->header.next;
        res = bldms_move_block(b_layer, from_block_next, READ);
        if (res < 0){
            pr_err("%s: failed to read block %d, next of %d\n", __func__,
             block->header.next, block->header.index);
            bldms_block_free(from_block_next);
            return -1;
        }
        from_block_next ->header.prev = block ->header.prev;
        pr_debug("%s: updating prev of from block next %d to %d\n", __func__,
         from_block_next->header.index, block->header.prev);
        bldms_move_block(b_layer, from_block_next, WRITE);
        if (res < 0){
            pr_err("%s: failed to write block %d, next of %d\n", __func__,
             block->header.next, block->header.index);
            bldms_block_free(from_block_next);
            return -1;
        }
        bldms_block_free(from_block_next);
    }
    /**
     * Give remaining readers time to exit from block to move.
     * NOTE: we cannot use a srcu callback since it is stated that it cannot block, but
     * we must update the block to move which is a blocking operation
    */
    synchronize_srcu(&b_layer->srcu);
    /**
     * Append block to receiving list
    */
    if (to ->last_bi == -1){
        // we put the block at the beginning of the receiving list
        block ->header.prev = -1;
        pr_debug("%s: block %d is the first of to list\n", __func__,
         block->header.index);
    }
    else {
        // we put the block after the last block of receiving list
        to_last_old = bldms_block_alloc(b_layer ->block_size);
        to_last_old ->header.index = to ->last_bi;
        pr_debug("%s: to last old has index %d and next %d\n", __func__,
         to_last_old->header.index, to_last_old->header.next);
        res = bldms_move_block(b_layer, to_last_old, READ);
        if (res < 0){
            pr_err("%s: failed to read block %d, last of to list\n", __func__,
            to->last_bi);
            bldms_block_free(to_last_old);
            return -1;
        }
        block ->header.prev = to_last_old ->header.index;
    }
    block ->header.next = -1;
    pr_debug("%s: moved block %d has prev %d next %d\n", __func__,
     block->header.index, block->header.prev, block->header.next);
    // we publish block updates on disk
    res = bldms_move_block(b_layer, block, WRITE);
    if (res < 0){
        pr_err("%s: failed to write block to move %d\n", __func__,
        block->header.index);
        return -1;
    }

    // we update the donating list head if the moving block is the first of its list
    if(from->first_bi == block->header.index){
        from->first_bi = block_old_next_i;
    }
    if(from->last_bi == block->header.index){
        from->last_bi = block_old_prev_i;
    }

    // we update the receiving list head and last block, if there is one
    if(to->last_bi == -1){
        to ->first_bi = block ->header.index;
        to ->last_bi = block ->header.index;
    }
    else{
        to_last_old ->header.next = block ->header.index;
        // after the following write, new readers can land on the block to move
        // by traversing the receiving list
        res = bldms_move_block(b_layer, to_last_old, WRITE);
        if (res < 0){
            pr_err("%s: failed to write block %d, last of to list\n", __func__,
            to->last_bi);
            bldms_block_free(to_last_old);
            return -1;
        }
        to ->last_bi = block ->header.index;
        pr_debug("%s: to_last_old block %d has prev %d and next %d\n", __func__,
         to_last_old->header.index, to_last_old->header.prev, to_last_old->header.next);

    }
    pr_debug("%s: from list start and end: %d %d\n", __func__, from->first_bi,
     from->last_bi);
    pr_debug("%s: to list start and end: %d %d\n", __func__, to->first_bi,
        to->last_bi);

    bldms_block_free(to_last_old);

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
 * Marks the desired block as free to use, updating block in device
*/
int bldms_invalidate_block(struct bldms_block_layer *b_layer, struct bldms_block *block){
    
    int res = 0;
    struct bldms_read_state *cur_read_state;
    int reader_idx;
    
    /**
     * We need to update states of all sessions of bldms_read() which stream offset
     * currently points to some data in the block to invalidate.
     * 
     * block_data_stream: [-------][xxxxxxxxxxxxxxxxxx][--]
     *                                  ^             ^
     *                                  off  -------> stream_cursor
     * 
     * If the block containing off is invalidated, we simply progress *off to match
     * stream_cursor, and we annotate that the block to start the next read is the
     * next one. In other words, we behave as the last read consumed all data bytes
     * in the invalidated block.
    */
    reader_idx = srcu_read_lock(&b_layer->read_states.srcu);
    list_for_each_entry(cur_read_state, &b_layer->read_states.head, list_node){
        mutex_lock(&cur_read_state->lock);
        if(cur_read_state->b_i_start == block->header.index){
            mutex_lock(&cur_read_state->filp->f_pos_lock);
            cur_read_state->filp->f_pos = cur_read_state->stream_cursor;
            mutex_unlock(&cur_read_state->filp->f_pos_lock);
            cur_read_state->b_i_start = block->header.next;
        }
        mutex_unlock(&cur_read_state->lock);
    }
    srcu_read_unlock(&b_layer->read_states.srcu, reader_idx);

    /**
     * We update block metadata in device to reflect the invalidation
    */
    block ->header.state = BLDMS_BLOCK_STATE_INVALID;
    res = bldms_blocks_move_block(b_layer, &b_layer->free_blocks, &b_layer->used_blocks,
        block);

    return res;
}

/**
 * Marks the desired block as containing valid data, updating block in device
*/
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