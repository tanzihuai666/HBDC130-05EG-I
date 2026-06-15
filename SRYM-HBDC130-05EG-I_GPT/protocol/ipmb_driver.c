/**
 * @file    ipmb_driver.c
 * @brief   IPMB 请求接收、IPMI 分发和响应发送集成层实现。
 *          I2C 事件中断只负责接收完整写事务并复制原始字节，本模块在主循环中完成
 *          Request 校验与解析、IPMI 命令执行、Response 构造、响应队列管理和多次发送重试。
 *          这样可以避免在中断中执行复杂协议逻辑和阻塞式 I2C 主机发送。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#include "ipmb_driver.h"
#include "ipmb_frame.h"
#include "ipmi_dispatch.h"
#include "bsp_i2c.h"
#include <string.h>

/*
 * 当前仅允许保存 1 个尚未被主循环处理的请求。
 * 当 BMC 在上一请求尚未处理完成前再次发送请求，新请求会被丢弃并增加 drop_count。
 */
#define IPMB_RX_PENDING_MAX     1u

/**
 * @brief IPMB 响应发送队列节点。
 */
typedef struct {
    uint8_t data[IPMB_MAX_FRAME_LEN]; /* 已经完成两个校验和计算的完整 IPMB Response。 */
    uint8_t len;                      /* data[] 中的有效帧长度。 */
    uint8_t dest_addr_8bit;           /* 请求方 8 位 IPMB 地址。 */
    uint8_t retry;                    /* 当前帧已经发送失败的次数。 */
    uint8_t used;                     /* 1=节点有效，0=节点空闲。 */
} ipmb_tx_item_t;

/* 本机 8 位 IPMB 地址，用于 Request 地址检查和 Response 源地址填写。 */
static uint8_t g_own_addr_8bit;

/* I2C 回调置位的待处理请求标志。该变量在中断和主循环之间共享。 */
static volatile uint8_t g_rx_pending;

/* 从 I2C 中断缓存复制出来的待解析原始请求帧。 */
static uint8_t g_rx_buf[IPMB_MAX_FRAME_LEN];

/* g_rx_buf[] 当前有效字节数。 */
static uint8_t g_rx_len;

/* 响应发送环形队列。实际可用节点数为深度减 1，用一个空位置区分满和空。 */
static ipmb_tx_item_t g_tx_queue[IPMB_TX_QUEUE_DEPTH];

/* 环形队列头索引，指向下一条待发送响应。 */
static uint8_t g_tx_head;

/* 环形队列尾索引，指向下一处可写节点。 */
static uint8_t g_tx_tail;

/* 成功从 I2C 接收并缓存的请求帧累计数量。 */
static uint32_t g_rx_count;

/* 通过 I2C 主机写成功发送的响应帧累计数量。 */
static uint32_t g_tx_count;

/* 因参数错误、缓存忙、解析失败、队列满或重试耗尽而丢弃的帧累计数量。 */
static uint32_t g_drop_count;

/**
 * @brief  I2C 从机完整写事务接收回调。
 * @param  data I2C 驱动接收到的原始 IPMB Request 字节。
 * @param  len  原始帧长度。
 * @retval 无。
 * @note   该函数由 I2C 事件中断上下文调用，只执行参数检查、内存复制和计数，不打印日志。
 */
static void ipmb_receive_callback(const uint8_t *data, uint8_t len)
{
    /* 空指针、空帧或超过协议缓存容量的帧直接丢弃。 */
    if ((data == 0) ||
        (len == 0u) ||
        (len > IPMB_MAX_FRAME_LEN)) {
        g_drop_count++;
        return;
    }

    /* 上一帧尚未被主循环取走时不覆盖缓存，避免两个 Request 内容混合。 */
    if (g_rx_pending >= IPMB_RX_PENDING_MAX) {
        g_drop_count++;
        return;
    }

    memcpy(g_rx_buf, data, len);
    g_rx_len = len;
    g_rx_pending = 1u;
    g_rx_count++;
}

/**
 * @brief  计算环形队列的下一个索引。
 * @param  index 当前索引。
 * @retval 加 1 并在达到队列深度后回绕到 0 的索引。
 */
static uint8_t ipmb_tx_next_index(uint8_t index)
{
    index++;

    if (index >= IPMB_TX_QUEUE_DEPTH) {
        index = 0u;
    }

    return index;
}

/**
 * @brief  判断响应发送队列是否已满。
 * @param  无。
 * @retval true=队列已满；false=仍有可用节点。
 */
static bool ipmb_tx_queue_is_full(void)
{
    return (ipmb_tx_next_index(g_tx_tail) == g_tx_head) ? true : false;
}

/**
 * @brief  判断响应发送队列是否为空。
 * @param  无。
 * @retval true=队列为空；false=存在待发送响应。
 */
static bool ipmb_tx_queue_is_empty(void)
{
    return (g_tx_head == g_tx_tail) ? true : false;
}

/**
 * @brief  将一个完整 IPMB Response 放入发送队列。
 * @param  dest_addr_8bit 请求方 8 位 IPMB 地址。
 * @param  data           完整响应帧。
 * @param  len            响应帧长度。
 * @retval true=入队成功；false=参数错误或队列已满。
 */
static bool ipmb_tx_queue_push(uint8_t dest_addr_8bit,
                               const uint8_t *data,
                               uint8_t len)
{
    ipmb_tx_item_t *item;

    if ((data == 0) ||
        (len == 0u) ||
        (len > IPMB_MAX_FRAME_LEN)) {
        g_drop_count++;
        APP_LOGW("IPMB response rejected: invalid length=%u",
                 (unsigned int)len);
        return false;
    }

    if (ipmb_tx_queue_is_full()) {
        g_drop_count++;
        APP_LOGW("IPMB response dropped: transmit queue is full");
        return false;
    }

    item = &g_tx_queue[g_tx_tail];
    memcpy(item->data, data, len);
    item->len = len;
    item->dest_addr_8bit = dest_addr_8bit;
    item->retry = 0u;
    item->used = 1u;

    /* 尾索引移动到下一空闲节点。 */
    g_tx_tail = ipmb_tx_next_index(g_tx_tail);
    return true;
}

/**
 * @brief  尝试发送队首的一条 IPMB Response。
 * @param  无。
 * @retval 无。
 * @note   每次主循环最多尝试一条响应。发送失败时保留队首并在下次任务继续重试。
 */
static void ipmb_transmit_task(void)
{
    ipmb_tx_item_t *item;

    if (ipmb_tx_queue_is_empty()) {
        return;
    }

    item = &g_tx_queue[g_tx_head];

    /* 理论上队首节点必须有效；若状态异常，跳过该节点以避免队列永久阻塞。 */
    if (item->used == 0u) {
        g_tx_head = ipmb_tx_next_index(g_tx_head);
        return;
    }

    /* I2C BSP 接口使用 7 位地址，因此把保存的 8 位 IPMB 地址右移 1 位。 */
    if (bsp_i2c1_master_write((uint8_t)(item->dest_addr_8bit >> 1),
                              item->data,
                              item->len)) {
        uint8_t sent_dest;
        uint8_t sent_len;

        /* 日志使用的字段在释放节点前保存，避免后续覆盖导致日志内容变化。 */
        sent_dest = item->dest_addr_8bit;
        sent_len = item->len;

        item->used = 0u;
        g_tx_head = ipmb_tx_next_index(g_tx_head);
        g_tx_count++;

        APP_LOGD("IPMB response sent: destination=0x%02X length=%u",
                 (unsigned int)sent_dest,
                 (unsigned int)sent_len);
    } else {
        item->retry++;

        APP_LOGW("IPMB response retry: destination=0x%02X attempt=%u",
                 (unsigned int)item->dest_addr_8bit,
                 (unsigned int)item->retry);

        if (item->retry >= IPMB_TX_RETRY_MAX) {
            uint8_t failed_dest;

            failed_dest = item->dest_addr_8bit;
            item->used = 0u;
            g_tx_head = ipmb_tx_next_index(g_tx_head);
            g_drop_count++;

            APP_LOGE("IPMB response abandoned: destination=0x%02X",
                     (unsigned int)failed_dest);
        }
    }
}

/**
 * @brief  处理一个待解析的 IPMB Request。
 * @param  无。
 * @retval 无。
 */
static void ipmb_receive_task(void)
{
    uint8_t local_frame[IPMB_MAX_FRAME_LEN];
    uint8_t local_length;
    ipmi_request_t request;
    ipmi_response_t response;
    uint8_t response_frame[IPMB_MAX_FRAME_LEN];
    uint8_t response_length;

    if (g_rx_pending == 0u) {
        return;
    }

    /*
     * 先把中断共享缓存复制到局部数组，再清除 pending 标志。
     * 此后 I2C 中断可以接收下一帧，不会覆盖当前正在处理的局部数据。
     */
    local_length = g_rx_len;
    memcpy(local_frame, g_rx_buf, local_length);
    g_rx_pending = 0u;

    /* 校验地址、长度和两个校验和，并拆分 NetFn、Seq、Cmd 等字段。 */
    if (!ipmb_parse_request(local_frame,
                            local_length,
                            g_own_addr_8bit,
                            &request)) {
        g_drop_count++;
        APP_LOGW("IPMB request parse failed: length=%u",
                 (unsigned int)local_length);
        return;
    }

    APP_LOGI("IPMB request received: source=0x%02X netfn=0x%02X cmd=0x%02X seq=%u",
             (unsigned int)request.rq_sa,
             (unsigned int)request.netfn,
             (unsigned int)request.cmd,
             (unsigned int)request.seq);

    /* 执行 IPMI 命令并获得 Completion Code 与响应数据。 */
    ipmi_dispatch(&request, &response);

    /* 将响应结构编码为带两个校验和的完整 IPMB Response。 */
    response_length = ipmb_build_response(&request,
                                          &response,
                                          g_own_addr_8bit,
                                          response_frame);

    if (response_length == 0u) {
        g_drop_count++;
        APP_LOGE("IPMB response build failed: command=0x%02X",
                 (unsigned int)request.cmd);
        return;
    }

    /* 目的地址使用 Request 中的请求方地址 RqSA。 */
    (void)ipmb_tx_queue_push(request.rq_sa,
                             response_frame,
                             response_length);
}

/**
 * @brief  初始化 IPMB 请求/响应集成层。
 * @param  own_addr_8bit 本机 8 位 IPMB 地址。
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

    memset(g_rx_buf, 0, sizeof(g_rx_buf));
    memset(g_tx_queue, 0, sizeof(g_tx_queue));

    /* 注册底层 I2C 从机完整事务回调。 */
    bsp_i2c1_register_rx_callback(ipmb_receive_callback);

    APP_LOGI("IPMB driver initialized: own_address=0x%02X queue_depth=%u",
             (unsigned int)own_addr_8bit,
             (unsigned int)IPMB_TX_QUEUE_DEPTH);
}

/**
 * @brief  IPMB 主循环任务。
 * @param  无。
 * @retval 无。
 * @note   先处理 Request，再尝试发送 Response，使新生成的响应可以在同一轮主循环发送。
 */
void ipmb_driver_task(void)
{
    ipmb_receive_task();
    ipmb_transmit_task();
}

/**
 * @brief  获取累计成功接收的请求帧数量。
 * @param  无。
 * @retval 请求帧累计数量。
 */
uint32_t ipmb_driver_get_rx_count(void)
{
    return g_rx_count;
}

/**
 * @brief  获取累计成功发送的响应帧数量。
 * @param  无。
 * @retval 响应帧累计数量。
 */
uint32_t ipmb_driver_get_tx_count(void)
{
    return g_tx_count;
}

/**
 * @brief  获取累计丢弃帧数量。
 * @param  无。
 * @retval 丢弃帧累计数量。
 */
uint32_t ipmb_driver_get_drop_count(void)
{
    return g_drop_count;
}
