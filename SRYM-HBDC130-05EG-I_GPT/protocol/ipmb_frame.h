/**
 * @file    ipmb_frame.h
 * @brief   IPMB frame parse/build interface.
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#ifndef IPMB_FRAME_H
#define IPMB_FRAME_H

#include "app_config.h"
#include <stdint.h>
#include <stdbool.h>

/** @brief Parsed IPMI request. */
typedef struct {
    uint8_t rs_sa;          /* responder address */
    uint8_t rq_sa;          /* requester address */
    uint8_t netfn;          /* request NetFn */
    uint8_t rq_lun;         /* requester LUN */
    uint8_t rs_lun;         /* responder LUN */
    uint8_t seq;            /* request sequence */
    uint8_t cmd;            /* IPMI command */
    const uint8_t *data;    /* request data */
    uint8_t data_len;       /* request data length */
} ipmi_request_t;

/** @brief IPMI response before IPMB encoding. */
typedef struct {
    uint8_t netfn;                            /* response NetFn */
    uint8_t cmd;                              /* response command */
    uint8_t completion_code;                  /* IPMI completion code */
    uint8_t data[IPMB_MAX_RSP_DATA_LEN];      /* response data */
    uint8_t data_len;                         /* response data length */
} ipmi_response_t;

uint8_t ipmb_checksum(const uint8_t *buf, uint8_t len);
bool ipmb_verify_checksum(const uint8_t *buf, uint8_t len);
bool ipmb_parse_request(const uint8_t *frame, uint8_t len, uint8_t own_addr_8bit, ipmi_request_t *req);
uint8_t ipmb_build_response(const ipmi_request_t *req, const ipmi_response_t *rsp, uint8_t own_addr_8bit, uint8_t *out);

/* NetFn/LUN/Seq helpers. */
#define IPMB_GET_NETFN(x)               ((uint8_t)((x) >> 2))
#define IPMB_GET_LUN(x)                 ((uint8_t)((x) & 0x03u))
#define IPMB_MAKE_NETFN_LUN(netfn,lun)  ((uint8_t)(((netfn) << 2) | ((lun) & 0x03u)))
#define IPMB_GET_SEQ(x)                 ((uint8_t)((x) >> 2))
#define IPMB_MAKE_SEQ_LUN(seq,lun)      ((uint8_t)(((seq) << 2) | ((lun) & 0x03u)))

#endif
