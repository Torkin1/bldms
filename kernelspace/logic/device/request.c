#include "device/request.h"
#include "device/device.h"

/**
 * Moves data between the device buffer and the data buffer;
 * The caller must know the position of the starting sector in the device
 * and the linear addr of the data buffer, plus size of data in sectors
*/
static blk_status_t bldms_move_data(
    struct bldms_device *dev,
    unsigned long pos_first_sector, int nr_sectors, size_t sector_size,
    void *buffer_data, enum req_opf direction){
    
    unsigned long offset, nr_bytes;
    u8 *start;
    bool is_write;

    // calculate starting position in device buffer and how many bytes to move 
    offset = pos_first_sector * sector_size;
    nr_bytes = nr_sectors * sector_size;
    start = dev->data + offset;

    // check if out of bounds
    if((offset + nr_bytes) > dev->data_size){
        pr_err("%s: out of bounds access\n", __func__);
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

/**
 * A read/write requests handler for the bldms block device.
 * NOTE: runs in interrupt context
*/
blk_status_t bldms_request(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd){

    blk_status_t res = BLK_STS_OK;
    struct request *req = bd->rq;
    struct bldms_device *device = req->q->queuedata;
    struct bio_vec bvec;
    struct req_iterator iter;
    sector_t pos_first_sector;
    size_t nr_sectors;
    void *buffer_data;
    enum req_opf direction;

    blk_mq_start_request(req);
    pos_first_sector = blk_rq_pos(req);
    if (rq_data_dir(req))
        direction = REQ_OP_WRITE; 
    else
        direction = REQ_OP_READ;

    /**
     * skip requests which do not involve data blocks transfer
    */
    if (blk_rq_is_passthrough(req)){
        pr_notice("%s: skipping non-data block transfer request\n", __func__);
        res = BLK_STS_IOERR;
    }
    else {
        /**
         * For each bio_vec in the request, we map the data pages to linear
         * addrs and move the data to/from the device 
        */
        rq_for_each_segment(bvec, req, iter){
            nr_sectors = blk_rq_cur_sectors(req);
            pr_debug("%s: direction: %d, starting sector: %lld, num of sectors %ld\n",
                        __func__,
                        direction,
                        pos_first_sector,
                        nr_sectors);
            buffer_data = page_address(bvec.bv_page) + bvec.bv_offset;
            res = bldms_move_data(device, pos_first_sector,
             nr_sectors, device->sector_size, buffer_data, direction);
             if (res != BLK_STS_OK){
                 pr_err("%s: failed to move data\n", __func__);
                 break;
             }
            pos_first_sector += nr_sectors;
        }
    }
    blk_mq_end_request(req, res);
    return res;
}
