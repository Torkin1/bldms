#include <linux/syscalls.h>

#include "usctm/usctm.h"
#include "device/block_layer.h"

static const int block_size = 4096;
static int test_syscall_desc;

/***************** test implementations*/

int test_hello(void){
    
    pr_info("%s: Hello from test driver :)", __func__);
    return 0;
}

int test_block_serialize(void){
    struct bldms_block *block_expected;
    struct bldms_block *block_actual;
    size_t copied_size = 0;
    u8 buffer[block_size];

    block_expected = bldms_block_alloc(block_size);
    if (block_expected == NULL){
        pr_err("%s: failed to allocate expected block\n", __func__);
        return -1;
    }
    block_actual = bldms_block_alloc(block_size);
    if (block_actual == NULL){
        pr_err("%s: failed to allocate actual block\n", __func__);
        return -1;
    }
    bldms_block_memset(block_expected, 'a',
     block_expected->header.data_capacity - 1, copied_size);
    bldms_block_serialize(block_expected, buffer);
    bldms_block_deserialize(block_actual, buffer);

    if (memcmp(block_expected->data, block_actual->data,
     block_expected->header.data_size) != 0){
        pr_err("%s: expected: %s, actual: %s\n", __func__, (char *)block_expected->data,
         (char *)block_actual->data);
        return -1;
    }

    return 0;
}

static int (*tests[])(void) = {
    test_hello,
    test_block_serialize,
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
#define bldms_get_string_from_symbol(symbol) #symbol

int bldms_tests_init(void){

    struct usctm_syscall_tbl *syscall_tbl;
    
    syscall_tbl = usctm_get_syscall_tbl_ref();
    if (!syscall_tbl){
        pr_err("%s: failed to get syscall table reference\n", __func__);
        return -1;
    }

    test_syscall_desc = usctm_register_syscall(syscall_tbl, test_syscall, bldms_get_string_from_symbol(test_driver));
    if (test_syscall_desc < 0){
        pr_err("%s: failed to register test syscall\n", __func__);
        return -1;
    }

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