#include "device/device.h"

/**
 * Moves data between the device buffer and the data buffer;
 * The caller must know the position of the starting sector in the device
 * and the linear addr of the data buffer, plus size of data in sectors
*/
blk_status_t bldms_move_data(
    struct bldms_device *dev,
    unsigned long pos_first_sector, int nr_sectors,
    void *buffer_data, int direction){
    
    unsigned long offset, nr_bytes;
    u8 *start;

    // calculate starting position in device buffer and how many bytes to move 
    offset = pos_first_sector * dev->sector_size;
    nr_bytes = nr_sectors * dev->sector_size;
    start = dev->data + offset;

    // check if out of bounds
    if((offset + nr_bytes) > dev->data_size){
        pr_err("%s: out of bounds access: offset + nr_bytes: %lu, dev->data_size: %d\n",
         __func__, offset + nr_bytes, dev->data_size);
        return BLK_STS_IOERR;
    }
    pr_debug("%s: direction is %d\n", __func__, direction);

    direction? memcpy(start, buffer_data, nr_bytes) : memcpy(buffer_data, start, nr_bytes);

    return BLK_STS_OK;
}


/**
 * Moves data between the bio and the device
*/
int bldms_move_bio(struct bldms_device *dev, struct bio *bio){

    struct bio_vec bvec;
    struct bvec_iter iter;
    sector_t start_sector;
    sector_t current_sector;
    int nr_sectors;
    u8 *buffer;
    blk_status_t res;

    start_sector = bio->bi_iter.bi_sector;
    current_sector = start_sector;

    bio_for_each_segment(bvec, bio, iter){
        nr_sectors = bio_cur_bytes(bio) / dev->sector_size;
        buffer = kmap_local_page(bvec.bv_page) + bvec.bv_offset;
        res = bldms_move_data(dev, start_sector, nr_sectors,
         buffer, bio_data_dir(bio));
        current_sector += nr_sectors;
        kunmap_local(buffer);
        if (res != BLK_STS_OK){
            pr_err("%s: failed to move data with error %d\n", __func__, res);
            return -1;
        }
    }

    return 0;
}

/**
 * Submits a bio to the device
*/
blk_qc_t bldms_submit_bio(struct bio *bio){

    struct bldms_device *dev;
    blk_qc_t res;
    
    // we can get the bldms device from the private data of the disk
    dev = (struct bldms_device *) bio->bi_bdev->bd_disk->private_data;

    if(bldms_move_bio(dev, bio)){
        pr_err("%s: failed to move bio\n", __func__);
        res = BLK_QC_T_NONE;
        goto bldms_submit_bio_exit;
    }
    
bldms_submit_bio_exit:
    bio_endio(bio);
    return res;
}
