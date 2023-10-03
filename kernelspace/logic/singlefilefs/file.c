#include <linux/fs.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include "singlefilefs.h"
#include "block_layer/block_layer.h"
#include "ops/vfs_supported.h"

ssize_t onefilefs_write(struct file *f, const char __user *u, size_t s, loff_t *o){

    // unsupported
    return -ENOTSUPP;
}

int onefilefs_open(struct inode *inode, struct file *filp){
    
    struct bldms_block_layer *b_layer;
    struct bldms_read_state *read_state;
    
    b_layer = inode->i_sb->s_fs_info;
    read_state = NULL;
    bldms_block_layer_use(b_layer);
    pr_debug("%s: open operation called\n",SINGLEFILEFS_NAME);

    // store the read state in file session if there isn't any assigned yet
    // and registers the session in the b_layer to make it visible to
    // invalidate ops
    if (!filp->private_data){
        pr_debug("%s: creating read state\n",SINGLEFILEFS_NAME);
        mutex_lock(&b_layer->read_states.w_lock);
        read_state = bldms_read_state_alloc();
        bldms_read_state_init(b_layer, read_state, filp);
        list_add_tail(&read_state->list_node, &b_layer->read_states.head);
        mutex_unlock(&b_layer->read_states.w_lock);
        filp->private_data = (void*)read_state;
    }

    return 0;
    
}

void onefilefs_release_read_state(struct rcu_head *rcu){
    struct bldms_read_state *read_state = container_of(rcu, struct bldms_read_state, rcu);
    bldms_read_state_free(read_state);
}

int onefilefs_release(struct inode *inode, struct file *filp){
    struct bldms_block_layer *b_layer;
    struct bldms_read_state *read_state;

    b_layer = inode->i_sb->s_fs_info;
    bldms_if_mounted(b_layer, bldms_block_layer_put(b_layer));
    pr_debug("%s: release operation called\n",SINGLEFILEFS_NAME);
    
    // free the read state after non-blockingly waiting for a grace period
    // to expire
    if (filp->private_data){
        mutex_lock(&b_layer->read_states.w_lock);
        read_state = (struct bldms_read_state*)filp->private_data;
        list_del(&read_state->list_node);
        mutex_unlock(&b_layer->read_states.w_lock);
        call_srcu(&b_layer->srcu, &read_state->rcu, onefilefs_release_read_state);
    }

    return 0;
}

ssize_t onefilefs_read(struct file * filp, char __user * buf, size_t len, loff_t * off) {

    struct inode * the_inode = filp->f_inode;
    uint64_t file_size = the_inode->i_size;
    struct bldms_block_layer *b_layer = the_inode->i_sb->s_fs_info;
    ssize_t read = 0;
    char *my_buffer;
    struct bldms_read_state *read_state;
    int reader_idx;
    
    printk("%s: read operation called with len %ld - and offset %lld (the current file size is %lld)",SINGLEFILEFS_NAME, len, *off, file_size);

    might_sleep();
    my_buffer = kzalloc(len * sizeof(char), GFP_KERNEL);

    // lock the file position to avoid threads sharing same fd to concurrently 
    // corrupt it
    if (mutex_lock_interruptible(&filp ->f_pos_lock)){
        pr_err("%s: interrupted while waiting to lock the file position\n",__func__);
        read = -EINTR;
        goto onefilefs_read_exit;
    }

    // check if we are reading in range
    // FIXME: use file_size
    ////if (*off >= file_size || *off < 0) return 0;
    if (*off < 0) {
        mutex_unlock(&filp ->f_pos_lock);
        read = -EINVAL;
        goto onefilefs_read_exit;
    }
        
    /**
     * Perform actual read with corresponding read state
    */
    reader_idx = srcu_read_lock(&b_layer->read_states.srcu);
    // read_state pointer should be safe to dereference until we exit from the reader
    // critical section
    read_state = (struct bldms_read_state*)filp->private_data;
    if (!read_state){
        pr_err("%s: read state is NULL\n",__func__);
        read = -EFAULT;
        goto onefilefs_read_exit;
    }
    mutex_lock(&read_state->lock);
    read = bldms_read(b_layer, my_buffer, len, off, read_state);
    mutex_unlock(&read_state->lock);
    srcu_read_unlock(&b_layer->read_states.srcu, reader_idx);

    mutex_unlock(&filp ->f_pos_lock);

    if (read > 0 && copy_to_user(buf, my_buffer, read)){       
        pr_err("%s: failed to copy data to user\n",__func__);
        read = -EFAULT;
        goto onefilefs_read_exit;   
    }

onefilefs_read_exit:
    kfree(my_buffer);
    return read;
}


struct dentry *onefilefs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags) {
    
    struct singlefilefs_inode *FS_specific_inode;
    struct super_block *sb = parent_inode->i_sb;
    struct buffer_head *bh = NULL;
    struct inode *the_inode = NULL;

    printk("%s: running the lookup inode-function for name %s",SINGLEFILEFS_NAME,child_dentry->d_name.name);

    if(!strcmp(child_dentry->d_name.name, SINGLEFILEFS_UNIQUE_FILE_NAME)){

	
	//get a locked inode from the cache 
        the_inode = iget_locked(sb, 1);
        if (!the_inode)
       		 return ERR_PTR(-ENOMEM);

	//already cached inode - simply return successfully
	if(!(the_inode->i_state & I_NEW)){
		return child_dentry;
	}


	//this work is done if the inode was not already cached
	inode_init_owner(&init_user_ns, the_inode, NULL, S_IFREG );
	the_inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP | S_IXOTH;
        the_inode->i_fop = &singlefilefs_file_operations;
	the_inode->i_op = &singlefilefs_inode_ops;

	//just one link for this file
	set_nlink(the_inode,1);

	//now we retrieve the file size via the FS specific inode, putting it into the generic inode
    	bh = (struct buffer_head *)sb_bread(sb, SINGLEFILEFS_INODES_BLOCK_NUMBER );
    	if(!bh){
		return ERR_PTR(-EIO);
    	}
	FS_specific_inode = (struct singlefilefs_inode*)bh->b_data;
	the_inode->i_size = FS_specific_inode->file_size;
        brelse(bh);

        d_add(child_dentry, the_inode);
	dget(child_dentry);

	//unlock the inode to make it usable 
    	unlock_new_inode(the_inode);

	return child_dentry;
    }

    return NULL;

}

//look up goes in the inode operations
const struct inode_operations singlefilefs_inode_ops = {
    .lookup = onefilefs_lookup,
};

const struct file_operations singlefilefs_file_operations = {
    .owner = THIS_MODULE,
    .read = onefilefs_read,
    .open = onefilefs_open,
    .release = onefilefs_release,
    .write = onefilefs_write,
};
