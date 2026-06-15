/**
 * @file    bsp_uart.h
 * @brief   USART1 调试串口板级接口。
 *          该接口为 printf 日志输出提供底层发送能力，同时保留原始字节流发送函数，供后续
 *          串口调试协议或升级协议复用。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#ifndef BSP_UART_H
#define BSP_UART_H

#include "gd32f10x.h"
#include <stdint.h>

/**
 * @brief  初始化 USART1、PA9/PA10 GPIO 和接收中断。
 * @param  baud 串口波特率，当前推荐 115200。
 * @retval 无。
 */
void bsp_uart1_init(uint32_t baud);

/**
 * @brief  阻塞发送一个字节。
 * @param  ch 待发送字节。
 * @retval 无。
 */
void bsp_uart1_putc(uint8_t ch);

/**
 * @brief  阻塞发送一段原始字节流。
 * @param  buf 数据指针。
 * @param  len 数据长度。
 * @retval 无。
 */
void bsp_uart1_write(const uint8_t *buf, uint16_t len);

#endif
