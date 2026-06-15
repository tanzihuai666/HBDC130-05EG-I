/**
 * @file    ipmi_dispatch.h
 * @brief   IPMI 命令分发接口。
 *          本接口位于 IPMB 帧处理层与传感器、FRU、SDR、故障管理模块之间，负责将解析后的
 *          Request 转换为统一 Response。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#ifndef IPMI_DISPATCH_H
#define IPMI_DISPATCH_H

#include "ipmb_frame.h"

/**
 * @brief  初始化 IPMI 命令处理所依赖的传感器、FRU 和 SDR 数据库。
 * @param  own_addr_8bit 本机 8 位 IPMB 地址。
 * @param  ga_id         GA[2:0] 槽位号，范围 0~7。
 * @retval 无。
 */
void ipmi_init(uint8_t own_addr_8bit, uint8_t ga_id);

/**
 * @brief  根据 Request 的 NetFn 和 Cmd 执行命令，并填写 Response。
 * @param  req 已完成 IPMB 地址和校验和检查的请求结构。
 * @param  rsp 响应结构输出指针。
 * @retval 无，命令执行结果写入 rsp->completion_code。
 */
void ipmi_dispatch(const ipmi_request_t *req, ipmi_response_t *rsp);

#endif
