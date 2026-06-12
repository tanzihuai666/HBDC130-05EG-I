/**
 * @file    ipmb_frame.c
 * @brief   IPMB frame checksum, request parser and response builder.
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#include "ipmb_frame.h"

/** @brief Calculate IPMB checksum. */
uint8_t ipmb_checksum(const uint8_t *buf, uint8_t len)
{
    uint8_t sum = 0u;
    uint8_t i;
    for (i = 0u; i < len; i++) sum = (uint8_t)(sum + buf[i]);
    return (uint8_t)(0u - sum);
}

/** @brief Verify checksum by checking whether byte sum is zero. */
bool ipmb_verify_checksum(const uint8_t *buf, uint8_t len)
{
    uint8_t sum = 0u;
    uint8_t i;
    for (i = 0u; i < len; i++) sum = (uint8_t)(sum + buf[i]);
    return (sum == 0u) ? true : false;
}

/** @brief Parse a standard IPMB request frame. */
bool ipmb_parse_request(const uint8_t *frame, uint8_t len, uint8_t own_addr_8bit, ipmi_request_t *req)
{
    if ((frame == 0) || (req == 0)) return false;
    if (len < 7u) return false;
    if (frame[0] != own_addr_8bit) return false;
    if (!ipmb_verify_checksum(&frame[0], 3u)) return false;
    if (!ipmb_verify_checksum(&frame[3], (uint8_t)(len - 3u))) return false;

    req->rs_sa = frame[0];
    req->netfn = IPMB_GET_NETFN(frame[1]);
    req->rs_lun = IPMB_GET_LUN(frame[1]);
    req->rq_sa = frame[3];
    req->seq = IPMB_GET_SEQ(frame[4]);
    req->rq_lun = IPMB_GET_LUN(frame[4]);
    req->cmd = frame[5];
    req->data = &frame[6];
    req->data_len = (uint8_t)(len - 7u);
    return true;
}

/** @brief Build a response frame for the parsed request. */
uint8_t ipmb_build_response(const ipmi_request_t *req, const ipmi_response_t *rsp, uint8_t own_addr_8bit, uint8_t *out)
{
    uint8_t i;
    uint8_t idx;

    if ((req == 0) || (rsp == 0) || (out == 0)) return 0u;

    out[0] = req->rq_sa;
    out[1] = IPMB_MAKE_NETFN_LUN((uint8_t)(req->netfn + 1u), req->rq_lun);
    out[2] = ipmb_checksum(out, 2u);
    out[3] = own_addr_8bit;
    out[4] = IPMB_MAKE_SEQ_LUN(req->seq, req->rs_lun);
    out[5] = req->cmd;
    out[6] = rsp->completion_code;

    idx = 7u;
    for (i = 0u; (i < rsp->data_len) && (idx < (IPMB_MAX_FRAME_LEN - 1u)); i++) {
        out[idx++] = rsp->data[i];
    }
    out[idx] = ipmb_checksum(&out[3], (uint8_t)(idx - 3u));
    idx++;
    return idx;
}
