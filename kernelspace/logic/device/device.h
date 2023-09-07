#ifndef DEVICE_H_INCLUDED
#define DEVICE_H_INCLUDED

#include <linux/blk-mq.h>

#include "driver/driver.h"
#include "device/blocks_list.h"
#include "device/block.h"

/**
 * Struct representing a bldms device in memory
*/
struct bldms_device{
    struct bldms_driver *driver; // the driver that works with the device
    char path[32]; // path to the device
    int data_size; // size of all the data stored in the device in bytes
    u8 *data; // array view of the data stored in the device
    int users; // how many users are using the device right now
    bool online; // true if the device can be accessed by users
    int nr_blocks; // how many blocks the device has
    size_t sector_size; // size of a sector in bytes
    size_t block_size; // size of a block in bytes
    struct gendisk *gd; // the block device itself
    struct request_queue *queue; // the queue of requests of read/write
    spinlock_t lock;  // lock to synchronize access to the device
    struct blk_mq_tag_set tag_set;  // tag set of the device
    struct bldms_blocks_list *free_blocks; // list of blocks containing invalid data
    struct bldms_blocks_list *used_blocks; // list of blocks containing valid data
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
     * get_data():                  put_data():
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

void bldms_invalidate_device(struct bldms_device *dev);
int bldms_init_device(struct bldms_device *dev,
 int nr_blocks, size_t block_size, size_t sector_size,
 struct bldms_driver *driver);

sector_t bldms_block_to_sector(struct bldms_device *dev, int block_index);
int bldms_move_block(struct bldms_device *dev,
    struct bldms_block *block, enum req_opf op);
bool bldms_block_contains_valid_data(struct bldms_device *dev, int block_index);
int bldms_prepare_write_on_block_any(struct bldms_device *dev);
int bldms_commit_write_on_block(struct bldms_device *dev, int block_index);
int bldms_undo_write_on_block(struct bldms_device *dev, int block_index);
int bldms_start_op_on_block(struct bldms_device *dev, int block_index);
void bldms_end_op_on_block(struct bldms_device *dev, int block_index);
int bldms_invalidate_block(struct bldms_device *dev, int block_index);

#endif // DEVICE_H_INCLUDED