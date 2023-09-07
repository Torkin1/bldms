#include <linux/syscalls.h>

#include "usctm/usctm.h"
#include "device/device.h"
#include "device/block_serialization.h"
#include "tests.h"

static int test_syscall_desc;
static const int test_block_size = 32;
static struct bldms_device *test_dev;

/***************** test implementations*/

static int test_hello(void){
    
    pr_info("%s: Hello from test driver :)", __func__);
    return 0;
}

static int test_block_serialize(void){
    struct bldms_block *block_expected;
    struct bldms_block *block_actual;
    u8 buffer[test_block_size];

    block_expected = bldms_block_alloc(test_block_size);
    if (block_expected == NULL){
        pr_err("%s: failed to allocate expected block\n", __func__);
        return -1;
    }
    block_actual = bldms_block_alloc(test_block_size);
    if (block_actual == NULL){
        pr_err("%s: failed to allocate actual block\n", __func__);
        return -1;
    }
    bldms_block_memset(block_expected, 'a',
     block_expected->header.data_capacity - 1);
    
    bldms_block_serialize(block_expected, buffer);
    bldms_block_deserialize(block_actual, buffer);

    if (strcmp(block_expected->data, block_actual->data) != 0){
        pr_err("%s: expected: %s, actual: %s\n", __func__, (char *)block_expected->data,
         (char *)block_actual->data);
        return -1;
    }

    return 0;
}

static int test_block_move(void){
    struct bldms_block *block_expected;
    struct bldms_block *block_actual;
    int res = 0;

    block_expected = bldms_block_alloc(test_block_size);
    block_actual = bldms_block_alloc(test_block_size);
    bldms_block_memset(block_expected, 'a',
     block_expected->header.data_capacity - 1);

    block_expected->header.index = 0;
    block_actual->header.index = 0;

    pr_debug("%s: test blocks prepared\n", __func__);
    
    if (bldms_blocks_get_entry_from_block_index(test_dev ->used_blocks, 0) == NULL){
        pr_err("%s: failed to get entry for block %d in used blocks list\n",
         __func__, 0);
        res = -1;
        goto test_block_move_exit; 
    }
    pr_debug("%s: got entry for block %d in used blocks list\n", __func__, 0);
    
    // test write to block 0
    if (bldms_move_block(test_dev, block_expected, REQ_OP_WRITE)){
        pr_err("%s: failed to write block\n", __func__);
        res = -1;
        goto test_block_move_exit;
    }
    pr_debug("%s: wrote block %d\n", __func__, 0);

    // test read of block 0
    if (bldms_move_block(test_dev, block_actual, REQ_OP_READ)){
        pr_err("%s: failed to read block\n", __func__);
        res = -1;
        goto test_block_move_exit;
    }
    pr_debug("%s: read block %d\n", __func__, 0);

    if (strcmp(block_expected->data, block_actual->data)){
        pr_err("%s: expected: %s, actual: %s\n", __func__, (char *)block_expected->data,
         (char *)block_actual->data);
        res = -1;
        goto test_block_move_exit;
    }

test_block_move_exit:
    bldms_block_free(block_expected);
    bldms_block_free(block_actual);
    return res;   
}

static int (*tests[])(void) = {
    test_hello,
    test_block_serialize,
    test_block_move,
    NULL
};

/************** test driver */

__SYSCALL_DEFINEx(1, _test_driver, int, test_index){

    int tests_nr = sizeof(tests) / sizeof(tests[0]) - 1;
    
    if (test_index < 0 || test_index >= tests_nr){
        pr_err("%s: index %d is out of range [0, %d)\n", __func__, test_index,
         tests_nr);
         return -1;
    }
    return tests[test_index]();
    
}

static unsigned long test_syscall = (unsigned long) usctm_get_syscall_symbol(test_driver);

int bldms_tests_init(struct bldms_device *device){

    struct usctm_syscall_tbl *syscall_tbl;
    
    syscall_tbl = usctm_get_syscall_tbl_ref();
    if (!syscall_tbl){
        pr_err("%s: failed to get syscall table reference\n", __func__);
        return -1;
    }

    test_syscall_desc = usctm_register_syscall(syscall_tbl, test_syscall, usctm_get_string_from_symbol(test_driver));
    if (test_syscall_desc < 0){
        pr_err("%s: failed to register test syscall\n", __func__);
        return -1;
    }

    test_dev = device;

    return 0;
}

void bldms_tests_cleanup(void){
    struct usctm_syscall_tbl *syscall_tbl;

    syscall_tbl = usctm_get_syscall_tbl_ref();
    if (!syscall_tbl){
        pr_err("%s: failed to get syscall table reference\n", __func__);
        return;
    }

    usctm_unregister_syscall(syscall_tbl, test_syscall_desc);
}