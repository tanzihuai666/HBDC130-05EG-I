/**
 * @file    ipmb_driver.h
 * @brief   IPMB 请求接收、IPMI 分发和响应发送集成层接口。
 *          本接口连接底层 I2C 驱动、IPMB 帧处理和 IPMI 命令分发模块，并提供通信统计值。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#ifndef IPMB_DRIVER_H
#define IPMB_DRIVER_H

#include "app_config.h"
#include <stdint.h>

/**
 * @brief  初始化 IPMB 集成层并向 I2C 驱动注册接收回调。
 * @param  own_addr_8bit 本机 8 位 IPMB 地址。
 * @retval 无。
 */
void ipmb_driver_init(uint8_t own_addr_8bit);

/**
 * @brief  主循环周期任务，处理待解析请求和待发送响应。
 * @param  无。
 * @retval 无。
 */
void ipmb_driver_task(void);

/**
 * @brief  获取累计成功接收并缓存的请求帧数量。
 * @param  无。
 * @retval 请求帧累计数量。
 */
uint32_t ipmb_driver_get_rx_count(void);

/**
 * @brief  获取累计成功发送的响应帧数量。
 * @param  无。
 * @retval 响应帧累计数量。
 */
uint32_t ipmb_driver_get_tx_count(void);

/**
 * @brief  获取因缓存忙、解析失败、队列满或重试耗尽造成的累计丢帧数量。
 * @param  无。
 * @retval 丢帧累计数量。
 */
uint32_t ipmb_driver_get_drop_count(void);

#endif
