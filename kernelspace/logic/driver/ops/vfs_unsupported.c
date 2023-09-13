#include <linux/syscalls.h>

#include "driver/ops/ops.h"
#include "usctm/usctm.h"
#include "block_layer/block_layer.h"
#include "block_layer/block.h"

static struct bldms_block_layer *b_layer;

/**
 * int invalidate_data(int offset) used to invalidate data in a block at a given offset;
 * invalidation means that data should logically disappear from the device;
 * this service should
 * return the ENODATA error if no data is currently valid and associated with the offset
 * parameter.
*/
__SYSCALL_DEFINEx(1, _invalidate_data, int, offset){

    int invalidate_result = 0;
    int res;
    
    bldms_block_layer_use(b_layer);
    
    res = bldms_start_op_on_block(b_layer, offset);
    if (res < 0){
        pr_err("%s: failed to start op on block %d\n", __func__, offset);
        return -1;
    }
    
    // can't invalidate a block 
    if (!bldms_block_contains_valid_data(b_layer, offset)){
        pr_err("%s: block %d contains no valid data\n", __func__, offset);
        invalidate_result = -ENODATA;
        goto invalidate_data_exit;
    }

    // release block
    res = bldms_invalidate_block(b_layer, offset);
    if (res < 0){
        pr_err("%s: failed to invalidate block %d\n", __func__, offset);
        invalidate_result = -1;
        goto invalidate_data_exit;
    }

invalidate_data_exit:
    bldms_end_op_on_block(b_layer, offset);
    bldms_block_layer_put(b_layer);
    return invalidate_result;
}

/**
 * int get_data(int offset, char * destination, size_t size) used to read up to
 *  size bytes
 * from the block at a given offset, if it currently keeps data; this system call
 * should return the
 * amount of bytes actually loaded into the destination area or zero if no data
 * is currently kept
 * by the device block; this service should return the ENODATA error if no data is
 * currently
 * valid and associated with the offset parameter.
*/
__SYSCALL_DEFINEx(3, _get_data, int, offset, char *, destination, size_t, size){

    int data_copied;
    struct bldms_block *block;
    int res;

    bldms_block_layer_use(b_layer);
    
    res = bldms_start_op_on_block(b_layer, offset);
    if (res < 0){
        pr_err("%s: failed to start op on block %d\n", __func__, offset);
        return -1;
    }
    
    // check if block contains valid data
    if (!bldms_block_contains_valid_data(b_layer, offset)){
        pr_err("%s: block %d contains no valid data\n", __func__, offset);
        data_copied = -ENODATA;
        goto get_data_exit_no_block_alloc;
    }
    
    // init block hold deserialized data
    block = bldms_block_alloc(b_layer->block_size);
    block->header.index = offset;

    // copy data from block to destination
    res = bldms_move_block(b_layer, block, READ);
    if (res < 0){
        pr_err("%s: failed to read block %d from device\n", __func__, offset);
        data_copied = -1;
        goto get_data_exit;
    }
    data_copied = bldms_block_memcpy(block, destination, size,
     BLDMS_BLOCK_MEMCPY_FROM_BLOCK);

get_data_exit:    
    bldms_block_free(block);
get_data_exit_no_block_alloc:
    bldms_end_op_on_block(b_layer, offset);
    bldms_block_layer_put(b_layer);
    return data_copied;

}

/**
 *  int put_data(char * source, size_t size) used to put into one free block of the
 * block- device size bytes of the user-space data identified by the source pointer,
 * this operation
 * must be executed all or nothing; the system call returns an integer representing
 * the offset
 * of the device (the block index) where data have been put; if there is currently
 * no room
 * available on the device, the service should simply return the ENOMEM error;
*/
__SYSCALL_DEFINEx(2, _put_data, char *, source, size_t, size){
    
    int block_index;
    struct bldms_block *block;
    int res;
    int copied_size;

    bldms_block_layer_use(b_layer);

    // obtain a block index by reserving a free block in device
    block_index = bldms_prepare_write_on_block_any(b_layer);
    if (block_index < 0){
        pr_err("%s: failed to reserve block\n", __func__);
        return -ENOMEM;
    }

    res = bldms_start_op_on_block(b_layer, block_index);
    if (res < 0){
        pr_err("%s: failed to start op on block %d\n", __func__, block_index);
        bldms_undo_write_on_block(b_layer, block_index);
        return -1;
    }

    // allocate block struct with given index
    block = bldms_block_alloc(b_layer->block_size);
    block ->header.index = block_index;

    // write data to block
    copied_size = bldms_block_memcpy(block, source, size, BLDMS_BLOCK_MEMCPY_TO_BLOCK);
    if(copied_size != size){
        pr_err("%s: cannot fit source data of size %lu in a block of size %lu without\
         truncating\n", __func__, size, block->header.data_capacity);
        bldms_invalidate_block(b_layer, block_index);
        block_index = -1;
        goto put_data_exit;
    }

    // write block to device
    res = bldms_move_block(b_layer, block, REQ_OP_WRITE);
    if (res < 0){
        pr_err("%s: failed to write block %d to device\n", __func__, block_index);
        bldms_invalidate_block(b_layer, block_index);
        block_index = -1;
        goto put_data_exit;
    }
    
    bldms_commit_write_on_block(b_layer, block_index);
    
put_data_exit:
    bldms_end_op_on_block(b_layer, block_index);
    bldms_block_layer_put(b_layer);
    bldms_block_free(block);
    return block_index;
}

int bldms_vfs_unsupported_init(struct bldms_block_layer *b_layer_ref){
    
    struct usctm_syscall_tbl *syscall_tbl;

    b_layer = b_layer_ref;
    
    // register syscalls implementing vfs unsupported ops
    syscall_tbl = usctm_get_syscall_tbl_ref();
    if (!syscall_tbl){
        pr_err("%s: failed to get syscall table reference\n", __func__);
        return -1;
    }
    usctm_register_syscall(syscall_tbl,
     (unsigned long) usctm_get_syscall_symbol(put_data),
     usctm_get_string_from_symbol(put_data));
    usctm_register_syscall(syscall_tbl,
     (unsigned long) usctm_get_syscall_symbol(get_data),
     usctm_get_string_from_symbol(get_data));
    usctm_register_syscall(syscall_tbl,
     (unsigned long) usctm_get_syscall_symbol(invalidate_data),
     usctm_get_string_from_symbol(invalidate_data));
    
    return 0;
}

void bldms_vfs_unsupported_cleanup(void){
    // we let the usctm module unregister the syscalls (I'm so lazy)
}
