/**
 * @file    ipmb_driver.h
 * @brief   IPMB 请求/响应集成层接口。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#ifndef IPMB_DRIVER_H
#define IPMB_DRIVER_H

#include "app_config.h"
#include <stdint.h>

/** @brief 初始化 IPMB 集成层。@param own_addr_8bit 本机 8-bit 地址。@retval 无。 */
void ipmb_driver_init(uint8_t own_addr_8bit);

/** @brief 主循环周期任务，处理请求解析和响应发送。@param 无。@retval 无。 */
void ipmb_driver_task(void);

/** @brief 获取已接收请求计数。@param 无。@retval 接收帧数。 */
uint32_t ipmb_driver_get_rx_count(void);

/** @brief 获取已发送响应计数。@param 无。@retval 发送帧数。 */
uint32_t ipmb_driver_get_tx_count(void);

/** @brief 获取丢弃帧计数。@param 无。@retval 丢弃帧数。 */
uint32_t ipmb_driver_get_drop_count(void);

#endif
