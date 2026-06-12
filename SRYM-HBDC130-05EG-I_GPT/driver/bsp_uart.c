/**
 * @file    bsp_uart.c
 * @brief   USART1 板级驱动与 printf 重定向。
 *          USART1 使用 PA9/PA10，默认用于 RS232 调试日志输出；本轮仅实现日志打印，
 *          不实现命令行协议和固件升级协议。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#include "bsp_uart.h"
#include <stdio.h>

/* ARMCC5 下关闭半主机，确保 printf 不依赖调试器而是走串口。 */
#if defined(__CC_ARM)
#pragma import(__use_no_semihosting)
struct __FILE { int handle; };
FILE __stdout;
FILE __stdin;

/**
 * @brief  标准库退出钩子，避免半主机链接错误。
 * @param  x 退出码，裸机环境不使用。
 * @retval 无。
 */
void _sys_exit(int x)
{
    (void)x;
    while (1) {
    }
}

/**
 * @brief  ARMCC 字符输出钩子。
 * @param  ch 待输出字符。
 * @retval 无。
 */
void _ttywrch(int ch)
{
    bsp_uart1_putc((uint8_t)ch);
}
#endif

/**
 * @brief  初始化 USART1 复用 GPIO。
 * @param  无。
 * @retval 无。
 */
static void uart1_gpio_init(void)
{
    GPIO_InitPara gpio;
    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOA, ENABLE);

    /* PA9 = USART1_TX，推挽复用输出。 */
    gpio.GPIO_Pin = GPIO_PIN_9;
    gpio.GPIO_Speed = GPIO_SPEED_50MHZ;
    gpio.GPIO_Mode = GPIO_MODE_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    /* PA10 = USART1_RX，浮空输入。后续升级/命令行可复用此脚。 */
    gpio.GPIO_Pin = GPIO_PIN_10;
    gpio.GPIO_Mode = GPIO_MODE_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);
}

/**
 * @brief  初始化 USART1。
 * @param  baud 波特率，例如 115200。
 * @retval 无。
 */
void bsp_uart1_init(uint32_t baud)
{
    USART_InitPara usart;
    NVIC_InitPara nvic;

    uart1_gpio_init();
    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_USART1, ENABLE);

    USART_DeInit(USART1);
    usart.USART_BRR = baud;
    usart.USART_WL = USART_WL_8B;
    usart.USART_STBits = USART_STBITS_1;
    usart.USART_Parity = USART_PARITY_RESET;
    usart.USART_HardwareFlowControl = USART_HARDWAREFLOWCONTROL_NONE;
    usart.USART_RxorTx = USART_RXORTX_RX | USART_RXORTX_TX;
    USART_Init(USART1, &usart);

    /* 预留接收中断，当前只用于后续 RS232 调试/升级扩展。 */
    nvic.NVIC_IRQ = USART1_IRQn;
    nvic.NVIC_IRQPreemptPriority = 5u;
    nvic.NVIC_IRQSubPriority = 1u;
    nvic.NVIC_IRQEnable = ENABLE;
    NVIC_Init(&nvic);

    USART_INT_Set(USART1, USART_INT_RBNE, ENABLE);
    USART_Enable(USART1, ENABLE);
}

/**
 * @brief  阻塞发送 1 个字节。
 * @param  ch 待发送字节。
 * @retval 无。
 */
void bsp_uart1_putc(uint8_t ch)
{
    while (USART_GetBitState(USART1, USART_FLAG_TBE) == RESET) {
    }
    USART_DataSend(USART1, ch);
}

/**
 * @brief  阻塞发送一段字节流。
 * @param  buf 数据指针。
 * @param  len 数据长度。
 * @retval 无。
 */
void bsp_uart1_write(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    if (buf == 0) return;
    for (i = 0u; i < len; i++) {
        bsp_uart1_putc(buf[i]);
    }
}

/**
 * @brief  printf/fputc 重定向入口。
 * @param  ch 待输出字符。
 * @param  f  标准库文件句柄，裸机环境不使用。
 * @retval 输出字符。
 */
int fputc(int ch, FILE *f)
{
    (void)f;
    if (ch == '\n') {
        bsp_uart1_putc('\r');
    }
    bsp_uart1_putc((uint8_t)ch);
    return ch;
}
