#ifndef IPMB_FRAME_H
#define IPMB_FRAME_H

#include "app_config.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t rs_sa;
    uint8_t rq_sa;
    uint8_t netfn;
    uint8_t rq_lun;
    uint8_t rs_lun;
    uint8_t seq;
    uint8_t cmd;
    const uint8_t *data;
    uint8_t data_len;
} ipmi_request_t;

typedef struct {
    uint8_t netfn;
    uint8_t cmd;
    uint8_t completion_code;
    uint8_t data[IPMB_MAX_RSP_DATA_LEN];
    uint8_t data_len;
} ipmi_response_t;

uint8_t ipmb_checksum(const uint8_t *buf, uint8_t len);
bool ipmb_verify_checksum(const uint8_t *buf, uint8_t len);
bool ipmb_parse_request(const uint8_t *frame, uint8_t len, uint8_t own_addr_8bit, ipmi_request_t *req);
uint8_t ipmb_build_response(const ipmi_request_t *req, const ipmi_response_t *rsp, uint8_t own_addr_8bit, uint8_t *out);

#define IPMB_GET_NETFN(x)               ((uint8_t)((x) >> 2))
#define IPMB_GET_LUN(x)                 ((uint8_t)((x) & 0x03u))
#define IPMB_MAKE_NETFN_LUN(netfn,lun)  ((uint8_t)(((netfn) << 2) | ((lun) & 0x03u)))
#define IPMB_GET_SEQ(x)                 ((uint8_t)((x) >> 2))
#define IPMB_MAKE_SEQ_LUN(seq,lun)      ((uint8_t)(((seq) << 2) | ((lun) & 0x03u)))

#endif
