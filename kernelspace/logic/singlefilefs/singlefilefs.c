/**
 * A file system that contains only one file.
 * Heavily based on the code of Francesco QUaglia <francesco.quaglia@uniroma2.it>
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/wait.h>

#include "singlefilefs.h"
#include "config.h"
#include "block_layer/block_layer.h"
#include "ops/vfs_unsupported.h"

/**
 * Provides a block-level view of the single file stored in
 * a device mounted with singlefilefs.
*/
static struct bldms_block_layer b_layer;

/**
 * wait queue to put the thread to wait for remaining operations on device
 * before unmounting it
*/
DECLARE_WAIT_QUEUE_HEAD(unmount_queue);

static struct super_operations singlefilefs_super_ops = {
};


static struct dentry_operations singlefilefs_dentry_ops = {
};

int singlefilefs_fill_super(struct super_block *sb, void *data, int silent) {   

    struct inode *root_inode;
    struct buffer_head *bh;
    struct singlefilefs_sb_info *sb_disk;
    struct timespec64 curr_time;
    uint64_t magic;

    //Unique identifier of the filesystem
    sb->s_magic = SINGLEFILEFS_MAGIC;

    if (!sb_set_blocksize(sb, BLDMS_BLOCKSIZE)) {
        pr_err("%s: error setting blocksize\n",__func__);
        return -1;
    }

    bh = sb_bread(sb, SINGLEFILEFS_SB_BLOCK_NUMBER);
    if(!sb){
        pr_err("%s: error reading superblock from disk\n",__func__);
	    return -EIO;
    }
    sb_disk = (struct singlefilefs_sb_info *)bh->b_data;
    magic = sb_disk->magic;

    //check on the expected magic number
    if(magic != sb->s_magic){
	    pr_err("%s: magic number mismatch: %llu != %lu\n",__func__, magic, sb->s_magic);
        brelse(bh);
        return -EBADF;
    }

    if(sb_disk->nr_blocks > BLDMS_NBLOCKS){
        pr_err("%s: too many blocks in the device: %d > %d\n",__func__, sb_disk->nr_blocks, BLDMS_NBLOCKS);
        brelse(bh);
        return -EBADF;
    }

    b_layer.nr_blocks = sb_disk->nr_blocks;
    b_layer.free_blocks.first_bi = sb_disk->first_free_bi;//2;
    b_layer.free_blocks.last_bi = sb_disk->last_free_bi;//BLDMS_NBLOCKS_DEFAULT - 1;
    b_layer.used_blocks.first_bi = sb_disk->first_used_bi; //-1;
    b_layer.used_blocks.last_bi = sb_disk->last_used_bi; //-1;
    brelse(bh); // discards sb_disk

    pr_debug("%s: singlefilefs superblock loaded: magic is %llx\n",__func__, magic);

    //check on the expected magic number
    if(magic != sb->s_magic){
	    pr_err("%s: magic number mismatch: %llu != %lu\n",__func__, magic, sb->s_magic);
        return -EBADF;
    }

    sb->s_fs_info = &b_layer;    // store a reference to the block layer in the sb 
    sb->s_op = &singlefilefs_super_ops;//set our own operations


    root_inode = iget_locked(sb, 0);//get a root inode indexed with 0 from cache
    if (!root_inode){
        pr_err("%s: error getting root inode\n",__func__);
        return -ENOMEM;
    }

    pr_debug("%s: root inode loaded\n",__func__);

    root_inode->i_ino = SINGLEFILEFS_ROOT_INODE_NUMBER;//this is actually 10
    inode_init_owner(&init_user_ns, root_inode, NULL, S_IFDIR);//set the root user as owned of the FS root
    root_inode->i_sb = sb;
    root_inode->i_op = &singlefilefs_inode_ops;//set our inode operations
    root_inode->i_fop = &singlefilefs_dir_operations;//set our file operations
    
    //update access permission
    root_inode->i_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP | S_IXOTH;

    //baseline alignment of the FS timestamp to the current time
    ktime_get_real_ts64(&curr_time);
    root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = curr_time;

    // no inode from device is needed - the root of our file system is an in memory object
    root_inode->i_private = NULL;

    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root){
        pr_err("%s: error creating root dentry\n",__func__);
        return -ENOMEM;
    }
    sb->s_root->d_op = &singlefilefs_dentry_ops;//set our dentry operations

    //unlock the inode to make it usable
    unlock_new_inode(root_inode);

    // store ref to sb to make it accessible by non-VFS functions
    bldms_block_layer_register_sb(&b_layer, sb);

    return 0;
}

int singlefilefs_blayer_save_state(struct bldms_block_layer *b_layer){
    
    struct buffer_head *sb_disk_bh;
    struct singlefilefs_sb_info *sb_disk;

    might_sleep();

    if(!b_layer ->sb){
        pr_err("%s: error saving block layer state: superblock not found\n",__func__);
        return -EIO;
    }

    sb_disk_bh = sb_bread(b_layer->sb, SINGLEFILEFS_SB_BLOCK_NUMBER);
    if(!sb_disk_bh){
        pr_err("%s: error reading superblock from disk\n",__func__);
        return -EIO;
    }
    sb_disk = (struct singlefilefs_sb_info *)sb_disk_bh->b_data;

    sb_disk->first_free_bi = b_layer->free_blocks.first_bi;
    sb_disk->last_free_bi = b_layer->free_blocks.last_bi;
    sb_disk->first_used_bi = b_layer->used_blocks.first_bi;
    sb_disk->last_used_bi = b_layer->used_blocks.last_bi;

    mark_buffer_dirty(sb_disk_bh);
    brelse(sb_disk_bh);
    
    return 0;
}

static void singlefilefs_kill_superblock(struct super_block *s) {
    
    might_sleep();

    spin_lock(&b_layer.mounted_lock);
    b_layer.mounted = false;
    spin_unlock(&b_layer.mounted_lock);

    // wait for all operations on the device to finish
    wait_event_interruptible(unmount_queue, atomic_read(&b_layer.users) == 0);

    // save b_layer state to device
    if(b_layer.save_state(&b_layer)){
        pr_err("%s: error saving block layer state\n",__func__);
    }
    
    bldms_block_layer_clean(&b_layer);
    kill_block_super(s);
    printk(KERN_INFO "%s: singlefilefs unmount succesful.\n",SINGLEFILEFS_NAME);
    return;
}

//called on file system mounting 
struct dentry *singlefilefs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {

    struct dentry *ret;

    // only one mount is supported at any time
    spin_lock(&b_layer.mounted_lock);
    if (b_layer.mounted){
        pr_err("%s: error mounting singlefilefs: already mounted\n",__func__);
        spin_unlock(&b_layer.mounted_lock);
        return ERR_PTR(-EEXIST);
    }
    spin_unlock(&b_layer.mounted_lock);

    // mounts singlefilefs from the provided device
    ret = mount_bdev(fs_type, flags, dev_name, data, singlefilefs_fill_super);

    if (unlikely(IS_ERR(ret)))
        printk("%s: error mounting onefilefs",SINGLEFILEFS_NAME);
    else
        printk("%s: singlefilefs is succesfully mounted on from device %s\n",SINGLEFILEFS_NAME,dev_name);

    return ret;
}

//file system structure
static struct file_system_type onefilefs_type = {
	.owner = THIS_MODULE,
    .name           = SINGLEFILEFS_FS_NAME,
    .mount          = singlefilefs_mount,
    .kill_sb        = singlefilefs_kill_superblock,
    .fs_flags       = FS_REQUIRES_DEV,
};


int singlefilefs_init(size_t block_size, int nr_blocks) {

    int ret;

    //init block layer
    bldms_block_layer_init(&b_layer, block_size, nr_blocks);
    b_layer.save_state = singlefilefs_blayer_save_state;

    // reserves superblock and inode blocks
    bldms_reserve_first_blocks(&b_layer, 2);

    // initializes vfs unsupported operations
    if (bldms_vfs_unsupported_init(&b_layer) < 0){
        pr_err("%s: unable to initialize vfs unsupported operations\n", __func__);
        return -1;
    }
    pr_info("%s: vfs unsupported operations initialized\n", BLDMS_NAME);


    //register filesystem
    ret = register_filesystem(&onefilefs_type);
    if (likely(ret == 0))
        printk("%s: sucessfully registered singlefilefs\n",SINGLEFILEFS_NAME);
    else
        printk("%s: failed to register singlefilefs - error %d", SINGLEFILEFS_NAME,ret);

    return ret;
}

void singlefilefs_exit(void) {

    int ret;

    bldms_vfs_unsupported_cleanup();
    pr_debug("%s: vfs unsupported operations cleaned up\n", __func__);

    //unregister filesystem
    ret = unregister_filesystem(&onefilefs_type);

    if (likely(ret == 0))
        printk("%s: sucessfully unregistered file system driver\n",SINGLEFILEFS_NAME);
    else
        printk("%s: failed to unregister singlefilefs driver - error %d", SINGLEFILEFS_NAME, ret);
}