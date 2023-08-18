#include "vfs_supported.h"
#include "device.h"

static int bldms_modify_device_user_count(struct bldms_device *dev, int howmany)
{
    int res =0;
    if (!dev -> online){
        pr_err("%s: device is not user accessible\n", __func__);
        res = -ENODEV;
    }
    else {
        dev -> users = dev->users + howmany;
    }
    return res;
}

int bldms_open(struct block_device *bdev, fmode_t mode){

    // TODO: must fail with ENODEV if not mounted
    
    struct bldms_device *dev = bdev->bd_disk->private_data;
    int res;

    spin_lock(&dev->lock);
    res = bldms_modify_device_user_count(dev, 1);
    spin_unlock(&dev->lock);

    if (res){
        pr_err("%s: failed to open device %s\n", __func__, bdev->bd_disk->disk_name);
    }   
    return res;
}

void bldms_release(struct gendisk *disk, fmode_t mode){

    // TODO: must fail with ENODEV if not mounted
    
    struct bldms_device *dev = disk->private_data;
    int res = 0;

    spin_lock(&dev->lock);
    res = bldms_modify_device_user_count(dev, -1);
    spin_unlock(&dev->lock);

    if (res){
        pr_err("%s: failed to release device %s\n", __func__, disk->disk_name);
    }   
}
