#include "device/device_core.h"
#include "device/device.h"

/**
 * Moves data between the device buffer and the data buffer;
 * The caller must know the position of the starting sector in the device
 * and the linear addr of the data buffer, plus size of data in sectors
*/
blk_status_t bldms_move_data(
    struct bldms_device *dev,
    unsigned long pos_first_sector, int nr_sectors,
    void *buffer_data, enum req_opf direction){
    
    unsigned long offset, nr_bytes;
    u8 *start;
    bool is_write;

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

    // choose data direction according to the request being a read or a write
    switch(direction){
        case REQ_OP_READ:
            is_write = false;
            break;
        case REQ_OP_WRITE:
            is_write = true;
            break;
        default:
            pr_err("%s: unsupported data direction %d\n", __func__, direction);
            return BLK_STS_IOERR;
    }
    
    is_write? memcpy(start, buffer_data, nr_bytes) : memcpy(buffer_data, start, nr_bytes);

    return BLK_STS_OK;
}

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
