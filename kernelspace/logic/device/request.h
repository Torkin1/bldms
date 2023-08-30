#ifndef REQUEST_H_INCLUDED
#define REQUEST_H_INCLUDED

#include <linux/blk-mq.h>

blk_status_t bldms_request(struct blk_mq_hw_ctx *hctx,
 const struct blk_mq_queue_data *bd);

#endif // REQUEST_H_INCLUDED