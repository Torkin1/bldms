#ifndef VFS_SUPPORTED_H_INCLUDED
#define VFS_SUPPORTED_H_INCLUDED

#include <linux/types.h>
#include <linux/list.h>
#include <linux/fs.h>
#include "block_layer/block_layer.h"

struct bldms_read_state{

    struct file *filp;  // file session owning this read state
    struct list_head list_node;
    struct rcu_head rcu;
    loff_t stream_cursor;   // should be at the end of the block where off is placed
    loff_t stream_cursor_old; // should be at the start of the block where off is placed
    loff_t off_old; // where the read storing this state stopped reading the stream
    int b_i_start; // read loading this state will start from this block index
    /**
     * Locking at read_state level is useful to synchronize invalidate
     * ops and read op to same file session.
     * 
     * Combined with srcu mechanism of container list, it allows reads of 
     * different sessions to be concurrent. This comes at the expense
     * of invalidate perfomance: an invalidate must wait for all reads to finish
     * before it can update their state.
    */
    struct mutex lock;
};

ssize_t bldms_read(struct bldms_block_layer *b_layer, char *buf, size_t len,
 loff_t *off, struct bldms_read_state *read_state);

struct bldms_read_state *bldms_read_state_alloc(void);
void bldms_read_state_init( struct bldms_block_layer *b_layer,
 struct bldms_read_state *read_state, struct file *filp);
void bldms_read_state_free(struct bldms_read_state *read_state);
void bldms_read_state_update(struct bldms_read_state read_state);

#endif // VFS_SUPPORTED_H_INCLUDED