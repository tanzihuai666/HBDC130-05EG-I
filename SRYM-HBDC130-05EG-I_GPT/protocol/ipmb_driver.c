/**
 * @file    ipmb_driver.c
 * @brief   IPMB 请求/响应集成层。I2C 中断只收包，本文件在主循环完成帧解析、
 *          IPMI 分发、响应构造和 I2C Master 发送。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#include "ipmb_driver.h"
#include "ipmb_frame.h"
#include "ipmi_dispatch.h"
#include "bsp_i2c.h"
#include <string.h>

/* 当前仅缓存 1 个待处理请求，简化中断和主循环之间的数据同步。 */
#define IPMB_RX_PENDING_MAX     1u

/** @brief IPMB 响应发送队列节点。 */
typedef struct {
    uint8_t data[IPMB_MAX_FRAME_LEN]; /* 完整 IPMB 响应帧。 */
    uint8_t len;                      /* 帧长度。 */
    uint8_t dest_addr_8bit;           /* 目的端 8-bit IPMB 地址。 */
    uint8_t retry;                    /* 当前发送重试次数。 */
    uint8_t used;                     /* 节点是否有效。 */
} ipmb_tx_item_t;

static uint8_t g_own_addr_8bit;
static volatile uint8_t g_rx_pending;
static uint8_t g_rx_buf[IPMB_MAX_FRAME_LEN];
static uint8_t g_rx_len;
static ipmb_tx_item_t g_txq[IPMB_TX_QUEUE_DEPTH];
static uint8_t g_tx_head;
static uint8_t g_tx_tail;
static uint32_t g_rx_count;
static uint32_t g_tx_count;
static uint32_t g_drop_count;

/**
 * @brief  I2C 从机接收完成回调。
 * @param  data I2C 收到的原始 IPMB 请求帧。
 * @param  len  帧长度。
 * @retval 无。
 */
static void ipmb_rx_callback(const uint8_t *data, uint8_t len)
{
    if ((data == 0) || (len == 0u) || (len > IPMB_MAX_FRAME_LEN)) {
        g_drop_count++;
        return;
    }
    if (g_rx_pending >= IPMB_RX_PENDING_MAX) {
        g_drop_count++;
        return;
    }
    memcpy(g_rx_buf, data, len);
    g_rx_len = len;
    g_rx_pending = 1u;
    g_rx_count++;
}

/** @brief 计算环形队列下一索引。@param idx 当前索引。@retval 下一索引。 */
static uint8_t tx_next(uint8_t idx)
{
    idx++;
    if (idx >= IPMB_TX_QUEUE_DEPTH) idx = 0u;
    return idx;
}

/** @brief 判断发送队列是否满。@param 无。@retval 1=满。 */
static int tx_is_full(void)
{
    return (tx_next(g_tx_tail) == g_tx_head) ? 1 : 0;
}

/** @brief 判断发送队列是否空。@param 无。@retval 1=空。 */
static int tx_is_empty(void)
{
    return (g_tx_head == g_tx_tail) ? 1 : 0;
}

/**
 * @brief  响应帧入队。
 * @param  dest_addr_8bit 目标 8-bit IPMB 地址。
 * @param  data 响应帧指针。
 * @param  len  响应帧长度。
 * @retval 无。
 */
static void tx_push(uint8_t dest_addr_8bit, const uint8_t *data, uint8_t len)
{
    ipmb_tx_item_t *item;

    if ((data == 0) || (len == 0u) || (len > IPMB_MAX_FRAME_LEN) || tx_is_full()) {
        g_drop_count++;
        APP_LOGW("ipmb tx queue full len=%u", len);
        return;
    }

    item = &g_txq[g_tx_tail];
    memcpy(item->data, data, len);
    item->len = len;
    item->dest_addr_8bit = dest_addr_8bit;
    item->retry = 0u;
    item->used = 1u;
    g_tx_tail = tx_next(g_tx_tail);
}

/** @brief 响应发送任务。@param 无。@retval 无。 */
static void tx_task(void)
{
    ipmb_tx_item_t *item;
    if (tx_is_empty()) return;

    item = &g_txq[g_tx_head];
    if (item->used == 0u) {
        g_tx_head = tx_next(g_tx_head);
        return;
    }

    if (bsp_i2c1_master_write((uint8_t)(item->dest_addr_8bit >> 1), item->data, item->len)) {
        item->used = 0u;
        g_tx_head = tx_next(g_tx_head);
        g_tx_count++;
        APP_LOGD("ipmb tx ok dest=0x%02X len=%u", item->dest_addr_8bit, item->len);
    } else {
        item->retry++;
        APP_LOGW("ipmb tx retry dest=0x%02X retry=%u", item->dest_addr_8bit, item->retry);
        if (item->retry >= IPMB_TX_RETRY_MAX) {
            item->used = 0u;
            g_tx_head = tx_next(g_tx_head);
            g_drop_count++;
            APP_LOGE("ipmb tx give up dest=0x%02X", item->dest_addr_8bit);
        }
    }
}

/** @brief 请求解析与 IPMI 分发任务。@param 无。@retval 无。 */
static void rx_task(void)
{
    uint8_t local[IPMB_MAX_FRAME_LEN];
    uint8_t len;
    ipmi_request_t req;
    ipmi_response_t rsp;
    uint8_t out[IPMB_MAX_FRAME_LEN];
    uint8_t out_len;

    if (g_rx_pending == 0u) return;

    len = g_rx_len;
    memcpy(local, g_rx_buf, len);
    g_rx_pending = 0u;

    if (!ipmb_parse_request(local, len, g_own_addr_8bit, &req)) {
        g_drop_count++;
        APP_LOGW("ipmb parse error len=%u", len);
        return;
    }

    APP_LOGI("ipmb req netfn=0x%02X cmd=0x%02X rq=0x%02X", req.netfn, req.cmd, req.rq_sa);
    ipmi_dispatch(&req, &rsp);
    out_len = ipmb_build_response(&req, &rsp, g_own_addr_8bit, out);
    if (out_len == 0u) {
        g_drop_count++;
        APP_LOGE("ipmb rsp build error cmd=0x%02X", req.cmd);
        return;
    }

    tx_push(req.rq_sa, out, out_len);
}

/**
 * @brief  初始化 IPMB 集成层。
 * @param  own_addr_8bit 本机 8-bit 地址。
 * @retval 无。
 */
void ipmb_driver_init(uint8_t own_addr_8bit)
{
    g_own_addr_8bit = own_addr_8bit;
    g_rx_pending = 0u;
    g_rx_len = 0u;
    g_tx_head = 0u;
    g_tx_tail = 0u;
    g_rx_count = 0u;
    g_tx_count = 0u;
    g_drop_count = 0u;
    memset(g_txq, 0, sizeof(g_txq));
    bsp_i2c1_register_rx_callback(ipmb_rx_callback);
    APP_LOGI("ipmb driver init own=0x%02X", own_addr_8bit);
}

/** @brief IPMB 主循环任务。@param 无。@retval 无。 */
void ipmb_driver_task(void)
{
    rx_task();
    tx_task();
}

/** @brief 获取接收计数。@param 无。@retval 接收帧数。 */
uint32_t ipmb_driver_get_rx_count(void)
{
    return g_rx_count;
}

/** @brief 获取发送计数。@param 无。@retval 发送帧数。 */
uint32_t ipmb_driver_get_tx_count(void)
{
    return g_tx_count;
}

/** @brief 获取丢弃计数。@param 无。@retval 丢弃帧数。 */
uint32_t ipmb_driver_get_drop_count(void)
{
    return g_drop_count;
}
