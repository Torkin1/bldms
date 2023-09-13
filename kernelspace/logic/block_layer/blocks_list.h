#ifndef BLOCKS_LIST_H_INCLUDED
#define BLOCKS_LIST_H_INCLUDED

#include <linux/rculist.h>

struct bldms_blocks_entry{

    struct list_head list_head;
    struct rcu_head _rcu_head;
    int block_index;    // index of the block in the device
};

struct bldms_blocks_list{

    struct bldms_blocks_entry *head;
    spinlock_t write_lock;
};

int bldms_blocks_move_block(struct bldms_blocks_list *to,
struct bldms_blocks_list *from, int from_block_index);
struct bldms_blocks_entry *bldms_blocks_get_entry_from_block_index(
    struct bldms_blocks_list *list, int block_index);

struct bldms_blocks_list *bldms_create_blocks_list(int nr_blocks);
void bldms_destroy_blocks_list(struct bldms_blocks_list *head);
void bldms_destroy_blocks_entry(struct rcu_head *entry_list);
bool bldms_blocks_contains(struct bldms_blocks_list *list, int block_index);

#endif // BLOCKS_LIST_H_INCLUDED

