#ifndef SRCU_LIST_H
#define SRCU_LIST_H

#include <linux/mutex.h>
#include <linux/srcu.h>

/**
 * sRCU protected list
*/
struct srcu_list{

    struct mutex w_lock;
    struct list_head head;
    struct srcu_struct srcu;
};

#endif // SRCU_LIST_H