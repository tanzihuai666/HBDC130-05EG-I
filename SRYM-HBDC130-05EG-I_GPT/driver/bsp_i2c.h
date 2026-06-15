/**
 * @file    bsp_i2c.h
 * @brief   I2C1/IPMB 板级驱动接口。
 *          本模块将 I2C1 配置为支持标准多主 IPMB 的双角色接口：空闲时作为从机接收请求，
 *          需要返回响应时作为主机向请求方执行一次 I2C 写事务。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#ifndef BSP_I2C_H
#define BSP_I2C_H

#include "app_config.h"
#include <stdint.h>
#include <stdbool.h>

/* 从机接收缓存最大字节数，必须不小于协议层允许的最大 IPMB 帧长度。 */
#define BSP_I2C_RX_MAX        64u

/* 主机发送缓存最大字节数，必须不小于协议层允许的最大 IPMB 响应长度。 */
#define BSP_I2C_TX_MAX        64u

/**
 * @brief I2C 从机接收完整写事务后的回调类型。
 * @param data 接收缓冲区指针，仅在回调执行期间有效。
 * @param len  接收到的有效字节数。
 * @note  回调可能从 I2C 事件中断上下文调用，因此不得执行阻塞日志打印或耗时操作。
 */
typedef void (*bsp_i2c_rx_callback_t)(const uint8_t *data, uint8_t len);

/**
 * @brief  初始化 I2C1 为 400kHz、7 位地址 IPMB 接口。
 * @param  own_addr_7bit 本机 7 位 I2C 从地址。
 * @retval 无。
 */
void bsp_i2c1_ipmb_init(uint8_t own_addr_7bit);

/**
 * @brief  注册完整从机写帧接收回调。
 * @param  cb 回调函数；传入 0 表示取消回调。
 * @retval 无。
 */
void bsp_i2c1_register_rx_callback(bsp_i2c_rx_callback_t cb);

/**
 * @brief  以 I2C 主机写方式发送一个完整 IPMB 响应帧。
 * @param  dest_addr_7bit 目标设备 7 位地址。
 * @param  data           待发送数据指针。
 * @param  len            待发送字节数。
 * @retval true=发送成功；false=参数错误、超时、NACK、仲裁丢失或总线错误。
 */
bool bsp_i2c1_master_write(uint8_t dest_addr_7bit,
                           const uint8_t *data,
                           uint8_t len);

/**
 * @brief  执行 I2C1 总线恢复。
 * @param  无。
 * @retval 无。
 * @note   函数会临时关闭 I2C1，通过 GPIO 模拟 9 个 SCL 脉冲，然后重新初始化外设。
 */
void bsp_i2c1_recover_bus(void);

/**
 * @brief  获取累计 I2C 错误计数。
 * @param  无。
 * @retval 从初始化开始累计的错误次数。
 */
uint32_t bsp_i2c1_get_error_count(void);

/**
 * @brief  I2C1 事件中断服务函数。
 * @param  无。
 * @retval 无。
 */
void I2C1_EV_IRQHandler(void);

/**
 * @brief  I2C1 错误中断服务函数。
 * @param  无。
 * @retval 无。
 */
void I2C1_ER_IRQHandler(void);

#endif
