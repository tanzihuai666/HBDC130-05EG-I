#include "drv_uart.h"

void UART1_GPIOInit()
{
	GPIO_InitPara GPIO_InitStructure;
	RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOA, ENABLE);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_PIN_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_SPEED_50MHZ;
	GPIO_InitStructure.GPIO_Mode = GPIO_MODE_AF_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_PIN_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_MODE_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
}

void UART1_Config(uint32_t baud)
{
	USART_InitPara USART_InitStructure;
	RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_USART1, ENABLE);
	
	USART_DeInit(USART1);
	USART_InitStructure.USART_BRR = baud;
	USART_InitStructure.USART_WL = USART_WL_8B;
	USART_InitStructure.USART_STBits = USART_STBITS_1;
	USART_InitStructure.USART_Parity = USART_PARITY_RESET;                            //ÎÞÐ£Ñé
	USART_InitStructure.USART_HardwareFlowControl = USART_HARDWAREFLOWCONTROL_NONE;
	USART_InitStructure.USART_RxorTx = USART_RXORTX_RX | USART_RXORTX_TX;
	USART_Init(USART1, &USART_InitStructure);
}

void UART1_NVIC_Config()
{
	NVIC_InitPara NVIC_InitStructure;
	
	NVIC_InitStructure.NVIC_IRQ                = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQPreemptPriority = 5;
	NVIC_InitStructure.NVIC_IRQSubPriority     = 1;
	NVIC_InitStructure.NVIC_IRQEnable          = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

void DRV_UART_Init(uint32_t UartBaud)
{
	UART1_GPIOInit();
	UART1_Config(UartBaud);
	UART1_NVIC_Config();
	
	USART_INT_Set(USART1, USART_INT_RBNE, ENABLE);
	USART_Enable(USART1, ENABLE);
}
