#include <linux/rcupdate.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "blocks_list.h"
#include "block.h"


struct bldms_blocks_list *bldms_create_blocks_list(int nr_blocks){
    struct bldms_blocks_list *list;
    struct bldms_blocks_entry *entry;
    int i;

    list = kzalloc(sizeof(struct bldms_blocks_list), GFP_KERNEL);
    spin_lock_init(&list->write_lock);
    list ->head = kzalloc(sizeof(struct bldms_blocks_entry), GFP_KERNEL);
    INIT_LIST_HEAD(&list->head->list_head);
    for (i = 0; i < nr_blocks; i ++){
        entry = kzalloc(sizeof(struct bldms_blocks_entry), GFP_KERNEL);
        entry->block_index = i;
        list_add_tail(&entry->list_head, &list->head->list_head);
    }
    return list;
}

void bldms_destroy_blocks_entry(struct rcu_head *entry_rcu){
    
    struct bldms_blocks_entry *blocks_entry;

    blocks_entry = container_of(entry_rcu, struct bldms_blocks_entry, _rcu_head);
    kfree(blocks_entry);
}

void bldms_destroy_blocks_list(struct bldms_blocks_list *list){

    struct bldms_blocks_entry *pos;
    
    spin_lock(&list->write_lock);
    list_for_each_entry(pos, &list->head->list_head, list_head){
        list_del_rcu(&pos->list_head);
        //call_rcu(&pos->_rcu_head, bldms_destroy_blocks_entry);
        synchronize_rcu();
        kfree(pos);
    }
    spin_unlock(&list->write_lock);
    kfree(list->head);
    kfree(list);
}

/**
 * Returns the blocks_entry corresponding to the block
 * with the given index in the given list.
 * Can be called in interrupt context because, even if
 * calls rcu_read_lock(), actual blocking happens only when
 * calling rcu_synchronize()
*/
struct bldms_blocks_entry *bldms_blocks_get_entry_from_block_index(
 struct bldms_blocks_list *list, int block_index){

    struct bldms_blocks_entry *current_block_entry;
    
    rcu_read_lock();
    list_for_each_entry_rcu(current_block_entry, &list->head->list_head, list_head){
        pr_debug("%s: checking entry with block %d\n", __func__, current_block_entry->block_index);  
        if(current_block_entry->block_index == block_index){
            rcu_read_unlock();
            pr_debug("%s: found entry for block %d\n", __func__, block_index);
            return current_block_entry;
        }
     }
    rcu_read_unlock();
    
    pr_debug("%s: entry for block %d not found\n", __func__, block_index);
    return NULL;

}

/**
 * @return true if the given list contains a block entry with the given index
*/
bool bldms_blocks_contains(struct bldms_blocks_list *list, int block_index){
    return bldms_blocks_get_entry_from_block_index(list, block_index) != NULL;
}

/**
 * Same as bldms_blocks_get_entry_from_block_index(), but without
 * calling rcu_read_lock() and rcu_read_unlock()
*/
struct bldms_blocks_entry *bldms_blocks_get_entry_from_block_index_writers(struct bldms_blocks_list *list,
 int block_index){
    struct bldms_blocks_entry *current_block_entry;
    int current_entry_block_index;

    list_for_each_entry(current_block_entry, &list->head->list_head, list_head){
        pr_debug("%s: checking entry with block %d\n", __func__, current_block_entry->block_index);  
        current_entry_block_index = current_block_entry->block_index;
        if(current_entry_block_index == block_index 
         || block_index == BLDMS_ANY_BLOCK_INDEX){
            pr_debug("%s: found entry for block %d\n", __func__, block_index);
            return current_block_entry;
        }
    }
    pr_debug("%s: entry for block %d not found\n", __func__, block_index);
    return NULL;
 }

/**
 * Moves one entry from a blocks list after another one;
 * @return -1 if error, else the index of the moved block
*/
int bldms_blocks_move_block(struct bldms_blocks_list *to,
struct bldms_blocks_list *from, int from_block_index){

    int res = 0;
    struct bldms_blocks_entry *entry;
    
    spin_lock(&from->write_lock);
    spin_lock(&to->write_lock);

    pr_debug("%s: locks acquired\n", __func__);

    entry = bldms_blocks_get_entry_from_block_index_writers(from, from_block_index);
    if(!entry){
        pr_err("%s: block %d not found in list\n", __func__, from_block_index);
        res = -1;
        goto bldms_blocks_move_block_exit;
    }
    pr_debug("%s: entry found with index %d\n", __func__, entry->block_index);
    list_del_rcu(&entry->list_head);
    pr_debug("%s: entry removed from list 1\n", __func__);
    list_add_rcu(&entry->list_head, &to->head->list_head);
    pr_debug("%s: entry added to list 2\n", __func__);
    res = entry ->block_index;

bldms_blocks_move_block_exit:
    spin_unlock(&from->write_lock);
    spin_unlock(&to->write_lock);
    return res;
}