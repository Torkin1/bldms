#include <linux/types.h>
#include <linux/minmax.h>
#include <linux/slab.h>

#include "vfs_supported.h"
#include "block_layer/block_layer.h"

ssize_t bldms_read(struct bldms_block_layer *b_layer, char *buf, size_t len,
 loff_t *off) {
    
    ssize_t read;
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
    int reader_idx;
    
    read = 0;
    buf_cursor = buf;
    stream_cursor = 0;
    first_block_read = false;

    bldms_start_read(b_layer, &reader_idx);

    // at worst case we need to read nr_blocks block entries
    // (all blocks contain valid data)
    b = bldms_block_alloc(b_layer->block_size);
    b->header.index = b_layer->used_blocks.first_bi;
    bldms_blocks_foreach_index(b){

        if (read == len) break; // we read all the data requested by the caller
        
        /**
         * Chooses the current block with valid data to work with, locking it from other
         * ops
        */
        if (bldms_move_block(b_layer, b, READ) < 0){
            pr_err("%s: failed to read block %d\n", __func__, b->header.index);
            read = -1;
            goto bldms_read_exit;
        }
        pr_debug("%s: b_i: %d\n", __func__, b->header.index);
        /**
         * Consider the following race condition:
         * read():                      invalidate_data():
         *                              invalidate_block()                 
         * read_block()
         * load_next_block() --> will fetch a block from the free or the used list?
         * 
         * RCU allows a writer to modify block state while a reader is traversing it.
         * A writer will wait a grace period to expire before modifying next and prev
         * of this block,
         * so old readers can safely traverse it and get back on valid blocks list.
         * New readers will not see the invalidated block, since "neighbour" blocks
         * next and prev indexes are updated before starting the grace period.
         * See the code
         * of the block layer function bldms_blocks_move_block() to grasp the details.
         * 
         * So, here we can just care to skip the block if it does not contain valid
         * data.
         * We do not account for state changes happening after the following check
        */
        if(!bldms_block_contains_valid_data(b_layer, b)) continue;

        pr_debug("%s: data in block %d is %s\n", __func__, b->header.index,
         (char *)b->data);

        /**
         * Where are we in the stream?
         * 
         * Stream cursor is updated in such a way that it is always pointing the end
         * of data of a block.
         * 
         * TODO: Block data size is written in header block on disk, so we need to
         *  read it
         * before doing stream cursor updates. This can slow down perfomances.
         * Enforcing to read entire block capacity may improve perfomances, since
         * we can skip entire blocks in used list while knowing how much we must
         * progress the stream cursor to keep it consistent.
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
            pr_debug("%s: before desired offset", __func__);
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
            b_start = b->header.data_size - stream_cursor + *off;
            b_len = min((size_t)(b->header.data_size - b_start), len);
            pr_debug("%s: b_start: %lld, b_len: %lu\n", __func__, b_start, b_len);
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
              __func__, b->header.index, first_block_read, stream_cursor_old, stream_cursor, *off,
               len, *off+len);
            return -1;
        }

        // we copy the data in caller's buffer and update cursors
        memcpy(buf_cursor, b->data + b_start, b_len);
        pr_debug("%s: data copied is %s\n", __func__, buf_cursor);
        buf_cursor += b_len;
        read += b_len;
    }
    
    /**
     * We waited the very last moment to update the file seek to keep it consistent
     * in case of errors
    */
    *off += read; 
bldms_read_exit:
    pr_debug("%s: read %ld bytes\n", __func__, read);
    bldms_end_read(b_layer, reader_idx);
    bldms_block_free(b);
    return read;

}