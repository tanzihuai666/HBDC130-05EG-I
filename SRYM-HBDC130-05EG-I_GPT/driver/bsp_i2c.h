/**
 * @file    bsp_i2c.h
 * @brief   I2C1/IPMB 板级驱动接口。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#ifndef BSP_I2C_H
#define BSP_I2C_H

#include "gd32f10x.h"
#include <stdint.h>
#include <stdbool.h>

/* IPMB 单帧收发缓存长度，需小于全局 IPMB_MAX_FRAME_LEN。 */
#define BSP_I2C_RX_MAX        64u
#define BSP_I2C_TX_MAX        64u

/** @brief I2C 从机收到完整写帧后的回调。data 在回调返回后失效，调用者需复制。 */
typedef void (*bsp_i2c_rx_callback_t)(const uint8_t *data, uint8_t len);

/** @brief 初始化 I2C1 为 IPMB 7-bit 从机，同时保留 Master TX 能力。@param own_addr_7bit 本机 7-bit 地址。@retval 无。 */
void bsp_i2c1_ipmb_init(uint8_t own_addr_7bit);

/** @brief 注册 I2C 接收完成回调。@param cb 回调函数。@retval 无。 */
void bsp_i2c1_register_rx_callback(bsp_i2c_rx_callback_t cb);

/** @brief 以 Master 写方式发送 IPMB 响应。@param dest_addr_7bit 目标 7-bit 地址。@param data 数据。@param len 长度。@retval true=发送成功。 */
bool bsp_i2c1_master_write(uint8_t dest_addr_7bit, const uint8_t *data, uint8_t len);

/** @brief I2C 总线恢复，模拟 9 个 SCL 脉冲并重新初始化外设。@param 无。@retval 无。 */
void bsp_i2c1_recover_bus(void);

/** @brief 获取 I2C 错误计数。@param 无。@retval 错误次数。 */
uint32_t bsp_i2c1_get_error_count(void);

/** @brief I2C1 事件中断入口。@param 无。@retval 无。 */
void I2C1_EV_IRQHandler(void);

/** @brief I2C1 错误中断入口。@param 无。@retval 无。 */
void I2C1_ER_IRQHandler(void);

#endif
