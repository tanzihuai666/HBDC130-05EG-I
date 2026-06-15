/**
 * @file    ipmb_frame.h
 * @brief   IPMB 帧解析、校验和计算及响应构造接口。
 *          本文件定义协议层统一使用的 IPMI Request/Response 中间结构，屏蔽原始 IPMB
 *          字节流中的 NetFn/LUN、Seq/LUN 等位域打包细节。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#ifndef IPMB_FRAME_H
#define IPMB_FRAME_H

#include "app_config.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 解析后的 IPMI Request 结构。
 */
typedef struct {
    uint8_t rs_sa;          /* Responder Slave Address，本机 8 位 IPMB 地址。 */
    uint8_t rq_sa;          /* Requester Slave Address，请求方 8 位 IPMB 地址。 */
    uint8_t netfn;          /* 请求 NetFn，已去除低 2 位 LUN。 */
    uint8_t rq_lun;         /* 请求方 LUN，取值 0~3。 */
    uint8_t rs_lun;         /* 响应方 LUN，取值 0~3。 */
    uint8_t seq;            /* 请求序号，范围 0~63，响应必须原样返回。 */
    uint8_t cmd;            /* IPMI 命令码。 */
    const uint8_t *data;    /* 请求数据区指针，指向原始接收缓冲区内部。 */
    uint8_t data_len;       /* 请求数据区字节数，不包含帧尾校验和。 */
} ipmi_request_t;

/**
 * @brief IPMI 命令处理完成后、尚未编码成 IPMB 字节流的 Response 结构。
 */
typedef struct {
    uint8_t netfn;                            /* 响应 NetFn，通常为请求 NetFn+1。 */
    uint8_t cmd;                              /* 响应命令码，应与请求 Cmd 相同。 */
    uint8_t completion_code;                  /* IPMI Completion Code。 */
    uint8_t data[IPMB_MAX_RSP_DATA_LEN];      /* Completion Code 之后的响应数据。 */
    uint8_t data_len;                         /* 响应数据区有效字节数。 */
} ipmi_response_t;

/**
 * @brief  计算 IPMB 8 位补码校验和。
 * @param  buf 数据指针。
 * @param  len 数据长度。
 * @retval 校验字节。
 */
uint8_t ipmb_checksum(const uint8_t *buf, uint8_t len);

/**
 * @brief  校验一段包含校验字节的 IPMB 数据。
 * @param  buf 数据指针。
 * @param  len 总长度。
 * @retval true=校验通过；false=校验失败。
 */
bool ipmb_verify_checksum(const uint8_t *buf, uint8_t len);

/**
 * @brief  解析完整 IPMB Request 帧。
 * @param  frame         原始帧指针。
 * @param  len           原始帧长度。
 * @param  own_addr_8bit 本机 8 位 IPMB 地址。
 * @param  req           解析结果输出指针。
 * @retval true=解析成功；false=格式、地址或校验错误。
 */
bool ipmb_parse_request(const uint8_t *frame,
                        uint8_t len,
                        uint8_t own_addr_8bit,
                        ipmi_request_t *req);

/**
 * @brief  构造完整 IPMB Response 帧。
 * @param  req           对应的请求结构。
 * @param  rsp           IPMI 响应结构。
 * @param  own_addr_8bit 本机 8 位 IPMB 地址。
 * @param  out           完整帧输出缓冲区。
 * @retval 完整帧长度；失败时返回 0。
 */
uint8_t ipmb_build_response(const ipmi_request_t *req,
                            const ipmi_response_t *rsp,
                            uint8_t own_addr_8bit,
                            uint8_t *out);

/* 从 NetFn/LUN 字节中提取 6 位 NetFn。 */
#define IPMB_GET_NETFN(value)                 ((uint8_t)((value) >> 2))

/* 从 NetFn/LUN 或 Seq/LUN 字节中提取低 2 位 LUN。 */
#define IPMB_GET_LUN(value)                   ((uint8_t)((value) & 0x03u))

/* 将 6 位 NetFn 和 2 位 LUN 打包成 1 字节。 */
#define IPMB_MAKE_NETFN_LUN(netfn, lun)       ((uint8_t)(((netfn) << 2) | ((lun) & 0x03u)))

/* 从 Seq/LUN 字节中提取高 6 位请求序号。 */
#define IPMB_GET_SEQ(value)                   ((uint8_t)((value) >> 2))

/* 将 6 位序号和 2 位 LUN 打包成 1 字节。 */
#define IPMB_MAKE_SEQ_LUN(seq, lun)           ((uint8_t)(((seq) << 2) | ((lun) & 0x03u)))

#endif
