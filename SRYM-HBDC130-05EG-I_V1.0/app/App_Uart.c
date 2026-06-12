#include "App_Uart.h"
#include "drv_Init.h"
#include "string.h"
#include "App_ADC.h"



void USART1_IRQHandler(void)
{
	uint8_t RX_Temp;
	
 	if(USART_GetIntBitState(USART1, USART_INT_RBNE) != RESET) 					//接收到数据
	{ 			 
		USART_ClearIntBitState(USART1, USART_INT_RBNE);
		RX_Temp = USART_DataReceive(USART1);
	}
}
