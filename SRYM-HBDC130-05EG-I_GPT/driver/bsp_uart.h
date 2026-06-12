/**
 * @file    bsp_uart.h
 * @brief   USART1 板级驱动接口，用于 RS232 调试日志输出。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#ifndef BSP_UART_H
#define BSP_UART_H

#include "gd32f10x.h"
#include <stdint.h>

/**
 * @brief  初始化 USART1。
 * @param  baud 串口波特率，调试默认 115200。
 * @retval 无。
 */
void bsp_uart1_init(uint32_t baud);

/**
 * @brief  发送单字节。
 * @param  ch 待发送字节。
 * @retval 无。
 */
void bsp_uart1_putc(uint8_t ch);

/**
 * @brief  发送字节数组。
 * @param  buf 数据指针。
 * @param  len 数据长度。
 * @retval 无。
 */
void bsp_uart1_write(const uint8_t *buf, uint16_t len);

#endif
