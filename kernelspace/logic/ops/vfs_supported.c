#include <linux/types.h>
#include <linux/minmax.h>

#include "vfs_supported.h"
#include "block_layer/block_layer.h"

#define bldms_skip_block(b_layer_, b_i_){\
    bldms_end_op_on_block(b_layer_, b_i_);\
    continue;\
}

ssize_t bldms_read(struct bldms_block_layer *b_layer, char *buf, size_t len,
 loff_t *off) {
    
    ssize_t read;
    int b_valid_indexes[b_layer->nr_blocks];
    int be_i;   // block entry index, used to traverse b_valid_indexes
    int b_i;    // block index
    loff_t b_start;    // where do we need to start reading data from block
    size_t b_len;   // how much data do we need to read from block
    struct bldms_block *b;  // block buffer
    char *buf_cursor;    // where are we in the caller's buffer
    loff_t stream_cursor;   // where are we in the stream
    loff_t stream_cursor_old;
    /**
     * true if we read the first block containing valid data with a stream position
     * after the desired offset.
    */
    bool first_block_read;
    
    read = 0;
    memset(b_valid_indexes, -1, b_layer->nr_blocks * sizeof(int));
    buf_cursor = buf;
    stream_cursor = 0;

    /**
     * We cannot sleep/block while traversing a rcu list, so we need to get all 
     * indexes of blocks containing valid data before starting to read the blocks.
     * This means that we lose updates to the list happening after we get the
     * indexes.
    */
    bldms_get_valid_block_indexes(b_layer, b_valid_indexes, b_layer->nr_blocks);

    // at worst case we need to read nr_blocks block entries
    // (all blocks contain valid data)
    for (be_i = 0; be_i < b_layer->nr_blocks; be_i ++){

        if (read == len) break; // we read all the data requested by the caller
        
        /**
         * Chooses the current block with valid data to work with, locking it from other
         * ops
        */
        b_i = b_valid_indexes[be_i];
        pr_debug("%s: b_i: %d\n", __func__, b_i);
        if(b_i < 0) break; // we reached end of valid blocks.
        bldms_start_op_on_block(b_layer, b_i);

        /**
         * Now that we have chosen the block, we can work with it, right?
         * WRONG! >:(
         * 
         * Consider the following race condition:
         * read():                      invalidate():
         * get_valid_block_indexes()
         *                              start_op_on_block()
         *                              invalidate_block()
         *                              end_op_on_block()
         * start_op_on_block()
         * read_block()
         * end_op_on_block()
         * 
         * The read() op will read invalidated data. This happens because
         * after we get all valid block indexes, we lose the read lock on rcu, thus
         * allowing invalidate() to run after read() knows the index of the block to
         * read and invalidate it before we actually can read it.
         * To solve this, we must check if the block is still valid before we try to
         * read it.
        */
        if(!bldms_block_contains_valid_data(b_layer, b_i)){
            
            // block has been invalidated in the meanwhile, we skip it
            bldms_skip_block(b_layer, b_i);
        }

        /**
         * now we can read the block
        */
        b = bldms_block_alloc(b_layer ->block_size);
        b->header.index = b_i;
        if (!b){
            pr_err("%s: failed to allocate block buffer for block %d, skipping\n",
            __func__, b_i);
            bldms_skip_block(b_layer, b_i);
        }
        if (bldms_move_block(b_layer, b, READ) < 0){
            pr_err("%s: failed to read block %d, skipping\n", __func__, b_i);
            bldms_block_free(b);
            bldms_skip_block(b_layer, b_i);
        }
        bldms_end_op_on_block(b_layer, b_i);
        pr_debug("%s: data in block %d is %s\n", __func__, b_i, b->data);

        /**
         * Where are we in the stream?
         * 
         * Stream cursor is updated in such a way that it is always pointing the end
         * of data of a block.
         * 
         * TODO: Block data size is written in header block on disk, so we need to
         *  read it
         * before doing stream cursor updates. This can slow down perfomances.
         * Enforcing to read entire block capacity may improve perfomances.
        */
        stream_cursor_old = stream_cursor;
        stream_cursor += b->header.data_size;
        pr_debug("%s: stream_cursor: %lld\n", __func__, stream_cursor);
        pr_debug("%s: *off is %lld\n", __func__, *off);
        pr_debug("%s: *off+len is %lld\n", __func__, *off+len);   
        if (stream_cursor < *off){
            /**
             * b_data: [--------][---------------][---]
             *                  ^                    ^
             *                  stream_cursor        *off
             * 
             * we are before the desired stream offset, we skip this valid block
            */
            pr_debug("%s: before desired offset");
            bldms_block_free(b);
            continue;
        }
        else if(stream_cursor >= *off && !first_block_read){     
            /**
             * b_data: [------------------]
             *               ^            ^
             *               *off         stream_cursor
             * 
             * We have passed the desired offset, but we are still in the block where
             * such offset falls, so we need to start reading from the offset until
             * we reach the end of the block.
            */
            pr_debug("%s: first block read\n", __func__);
            first_block_read = true;
            b_start = stream_cursor - *off;
            b_len = min((size_t)(b->header.data_size - b_start), (size_t)(len - read));
        }
        else if(stream_cursor >= *off && first_block_read && stream_cursor < *off + len){
            /**
             * b_data: [-------][--][----------------------]
             *              ^      ^                  ^
             *              *off   stream_cursor      *off + len
             * 
             * We are reading blocks in range between the desired offset and the
             * requested length. We can read all data from the block. 
             */
            pr_debug("%s: in range\n", __func__);
            b_start = 0;
            b_len = b->header.data_size;
        }
        else if(stream_cursor >= *off && first_block_read && stream_cursor >= *off + len){
            /**
             * b_data: [------------------]
             *               ^            ^
             *               *off + len   stream_cursor
             * 
             * We reached the final block. We start reading data from the start of the
             * block until we consume all the remaining bytes to read.
            */
            pr_debug("%s: last block read\n", __func__);
            b_start = 0;
            b_len = *off + len - stream_cursor_old;        
        }
        else {
            // We should never reach this block since all prevoius guards should
            // exhaust the possible cases
            pr_err("%s: invalid stream cursor position, this should never happen!\n\
             b_i: %d, first_block_read: %d, stream_cursor_old: %lld,\
              stream_cursor: %lld, *off: %lld, len: %lu, *off+len: %lld\n",
              __func__, b_i, first_block_read, stream_cursor_old, stream_cursor, *off,
               len, *off+len);
            return -1;
        }

        // we copy the data in caller's buffer and update cursors
        memcpy(buf_cursor, b->data + b_start, b_len);
        pr_debug("%s: data copied is %s\n", __func__, buf_cursor);
        buf_cursor += b_len;
        read += b_len;
        bldms_block_free(b);
    }

    pr_debug("%s: read %ld bytes\n", __func__, read);
    return read;

}