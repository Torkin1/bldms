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

#include "singlefilefs.h"

static int dev_sb_block_size;

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

    if (!sb_set_blocksize(sb, dev_sb_block_size)) {
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
    brelse(bh); // discards sb_disk

    //check on the expected magic number
    if(magic != sb->s_magic){
	    pr_err("%s: magic number mismatch: %llu != %lu\n",__func__, magic, sb->s_magic);
        return -EBADF;
    }

    sb->s_fs_info = NULL; //FS specific data (the magic number) already reported into the generic superblock
    sb->s_op = &singlefilefs_super_ops;//set our own operations


    root_inode = iget_locked(sb, 0);//get a root inode indexed with 0 from cache
    if (!root_inode){
        pr_err("%s: error getting root inode\n",__func__);
        return -ENOMEM;
    }

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

    pr_debug("%s: singlefilefs superblock loaded: magic is %llu\n",__func__, magic);

    return 0;
}

static void singlefilefs_kill_superblock(struct super_block *s) {
    kill_block_super(s);
    printk(KERN_INFO "%s: singlefilefs unmount succesful.\n",SINGLEFILEFS_NAME);
    return;
}

//called on file system mounting 
struct dentry *singlefilefs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {

    struct dentry *ret;

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


int singlefilefs_init(size_t dev_sb_block_size_) {

    int ret;

    dev_sb_block_size = dev_sb_block_size_;

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

    //unregister filesystem
    ret = unregister_filesystem(&onefilefs_type);

    if (likely(ret == 0))
        printk("%s: sucessfully unregistered file system driver\n",SINGLEFILEFS_NAME);
    else
        printk("%s: failed to unregister singlefilefs driver - error %d", SINGLEFILEFS_NAME, ret);
}