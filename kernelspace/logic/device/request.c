#include "device/request.h"
#include "device/device.h"
#include "device/device_core.h"

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
    int direction;

    blk_mq_start_request(req);
    pos_first_sector = blk_rq_pos(req);
    direction = rq_data_dir(req);
    /**
     * skip requests which do not involve data blocks transfer
    */
    if (blk_rq_is_passthrough(req)){
        pr_notice("%s: skipping non-data block transfer request\n", __func__);
        res = BLK_STS_IOERR;
        goto bldms_request_exit;
    }
    
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
            nr_sectors, buffer_data, direction);
            if (res != BLK_STS_OK){
                pr_err("%s: failed to move data\n", __func__);
                break;
            }
        pos_first_sector += nr_sectors;
    }
    
bldms_request_exit:    
    blk_mq_end_request(req, res);
    return res;
}
