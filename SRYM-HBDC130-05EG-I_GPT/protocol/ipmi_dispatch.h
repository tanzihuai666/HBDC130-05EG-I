/**
 * @file    ipmi_dispatch.h
 * @brief   IPMI 命令分发接口。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#ifndef IPMI_DISPATCH_H
#define IPMI_DISPATCH_H

#include "ipmb_frame.h"

/** @brief 初始化 IPMI 数据库。@param own_addr_8bit 本机 IPMB 8-bit 地址。@param ga_id 槽位 ID。@retval 无。 */
void ipmi_init(uint8_t own_addr_8bit, uint8_t ga_id);

/** @brief 分发 IPMI 请求并填写响应。@param req 请求。@param rsp 响应。@retval 无。 */
void ipmi_dispatch(const ipmi_request_t *req, ipmi_response_t *rsp);

#endif
