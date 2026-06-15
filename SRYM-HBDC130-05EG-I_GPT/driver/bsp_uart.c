/**
 * @file    bsp_uart.c
 * @brief   USART1 板级驱动和 printf 重定向实现。
 *          USART1 使用 PA9/PA10，通过外部 RS232 收发器输出调试日志。当前版本只实现
 *          日志发送和接收中断基础配置，不实现串口命令行和固件升级协议。
 *
 *          所有日志宏统一输出 '\n'，本文件的 fputc() 自动将单独的 '\n' 转换成终端
 *          常用的 "\r\n"。若上层已经显式输出 "\r\n"，则不会重复插入 '\r'。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#include "bsp_uart.h"
#include <stdio.h>

/* 记录上一个通过 fputc() 输出的字符，用于避免 "\r\r\n" 重复回车。 */
static uint8_t g_previous_putchar;

/*
 * ARMCC5 默认 printf 可能调用半主机接口并等待调试器。
 * 导入 __use_no_semihosting 后，所有标准输出均通过本文件提供的 fputc() 发送到 USART1。
 */
#if defined(__CC_ARM)
#pragma import(__use_no_semihosting)

/* ARMCC 标准库要求应用提供最小 FILE 结构定义。 */
struct __FILE {
    int handle;
};

/* 标准输出和标准输入对象，裸机程序不使用文件系统句柄。 */
FILE __stdout;
FILE __stdin;

/**
 * @brief  标准库退出钩子。
 * @param  exit_code 标准库请求的退出码，裸机环境中不使用。
 * @retval 无。
 * @note   裸机程序不允许真正退出，因此进入永久循环等待看门狗复位。
 */
void _sys_exit(int exit_code)
{
    (void)exit_code;

    while (1) {
        /* 不喂看门狗，使异常退出最终触发系统复位。 */
    }
}

/**
 * @brief  ARMCC 低层字符输出钩子。
 * @param  ch 待输出字符。
 * @retval 无。
 */
void _ttywrch(int ch)
{
    bsp_uart1_putc((uint8_t)ch);
}
#endif

/**
 * @brief  初始化 USART1 使用的 GPIO。
 * @param  无。
 * @retval 无。
 */
static void uart1_gpio_init(void)
{
    GPIO_InitPara gpio;

    /* USART1 位于 GPIOA，先打开 GPIOA 时钟。 */
    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOA, ENABLE);

    /* PA9 配置为 50MHz 推挽复用输出，连接 USART1_TX。 */
    gpio.GPIO_Pin = GPIO_PIN_9;
    gpio.GPIO_Speed = GPIO_SPEED_50MHZ;
    gpio.GPIO_Mode = GPIO_MODE_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    /* PA10 配置为浮空输入，连接 USART1_RX。 */
    gpio.GPIO_Pin = GPIO_PIN_10;
    gpio.GPIO_Mode = GPIO_MODE_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);
}

/**
 * @brief  初始化 USART1 调试串口。
 * @param  baud 波特率，当前主程序传入 115200。
 * @retval 无。
 */
void bsp_uart1_init(uint32_t baud)
{
    USART_InitPara usart;
    NVIC_InitPara nvic;

    g_previous_putchar = 0u;

    uart1_gpio_init();

    /* USART1 位于 APB2 总线。 */
    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_USART1, ENABLE);

    /* 先复位 USART1，再配置 8N1、无流控、收发同时开启。 */
    USART_DeInit(USART1);
    usart.USART_BRR = baud;
    usart.USART_WL = USART_WL_8B;
    usart.USART_STBits = USART_STBITS_1;
    usart.USART_Parity = USART_PARITY_RESET;
    usart.USART_HardwareFlowControl = USART_HARDWAREFLOWCONTROL_NONE;
    usart.USART_RxorTx = USART_RXORTX_RX | USART_RXORTX_TX;
    USART_Init(USART1, &usart);

    /*
     * 配置接收非空中断，为后续串口命令和升级协议预留入口。
     * 当前版本尚未实现 USART1_IRQHandler() 中的协议解析逻辑。
     */
    nvic.NVIC_IRQ = USART1_IRQn;
    nvic.NVIC_IRQPreemptPriority = 5u;
    nvic.NVIC_IRQSubPriority = 1u;
    nvic.NVIC_IRQEnable = ENABLE;
    NVIC_Init(&nvic);

    USART_INT_Set(USART1, USART_INT_RBNE, ENABLE);
    USART_Enable(USART1, ENABLE);
}

/**
 * @brief  采用阻塞方式发送 1 个字节。
 * @param  ch 待发送字节。
 * @retval 无。
 * @note   函数会等待发送数据寄存器为空，因此不得在高优先级实时中断中调用。
 */
void bsp_uart1_putc(uint8_t ch)
{
    while (USART_GetBitState(USART1, USART_FLAG_TBE) == RESET) {
        /* 等待发送数据寄存器可写。 */
    }

    USART_DataSend(USART1, ch);
}

/**
 * @brief  采用阻塞方式发送一段原始字节流。
 * @param  buf 数据缓冲区指针。
 * @param  len 数据长度。
 * @retval 无。
 * @note   本函数不自动处理换行，适合发送协议数据或已经格式化完成的文本。
 */
void bsp_uart1_write(const uint8_t *buf, uint16_t len)
{
    uint16_t index;

    if (buf == 0) {
        return;
    }

    for (index = 0u; index < len; index++) {
        bsp_uart1_putc(buf[index]);
    }
}

/**
 * @brief  将标准库 printf/fputc 输出重定向到 USART1。
 * @param  ch 待输出字符。
 * @param  stream 标准库文件对象，裸机环境不使用。
 * @retval 返回已输出字符。
 *
 * @note   换行处理规则：
 *         1. 上层只输出 '\n' 时，自动先补 '\r'，串口得到 "\r\n"；
 *         2. 上层已经输出 "\r\n" 时，不再额外补 '\r'；
 *         3. 其他字符保持原样输出。
 */
int fputc(int ch, FILE *stream)
{
    (void)stream;

    if ((ch == '\n') && (g_previous_putchar != '\r')) {
        bsp_uart1_putc('\r');
    }

    bsp_uart1_putc((uint8_t)ch);
    g_previous_putchar = (uint8_t)ch;

    return ch;
}
