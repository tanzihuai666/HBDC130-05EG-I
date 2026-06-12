#include "bsp_uart.h"

static void uart1_gpio_init(void)
{
    GPIO_InitPara gpio;
    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOA, ENABLE);

    gpio.GPIO_Pin = GPIO_PIN_9;
    gpio.GPIO_Speed = GPIO_SPEED_50MHZ;
    gpio.GPIO_Mode = GPIO_MODE_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_PIN_10;
    gpio.GPIO_Mode = GPIO_MODE_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);
}

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

    nvic.NVIC_IRQ = USART1_IRQn;
    nvic.NVIC_IRQPreemptPriority = 5u;
    nvic.NVIC_IRQSubPriority = 1u;
    nvic.NVIC_IRQEnable = ENABLE;
    NVIC_Init(&nvic);

    USART_INT_Set(USART1, USART_INT_RBNE, ENABLE);
    USART_Enable(USART1, ENABLE);
}

void bsp_uart1_putc(uint8_t ch)
{
    while (USART_GetBitState(USART1, USART_FLAG_TBE) == RESET) {
    }
    USART_DataSend(USART1, ch);
}

void bsp_uart1_write(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    if (buf == 0) return;
    for (i = 0u; i < len; i++) {
        bsp_uart1_putc(buf[i]);
    }
}
