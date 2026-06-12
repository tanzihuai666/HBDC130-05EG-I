#include "drv_IO.h"
#include "App_Uart.h"

void DRV_IO_Init(void)
{
	GPIO_InitPara GPIO_InitStructure;
	RCC_APB2PeriphClock_Enable(EN_5V5_GPIO_CLK | EN_48V_GPIO_CLK, ENABLE);
	
	GPIO_InitStructure.GPIO_Pin = AC_OK_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_MODE_IN_FLOATING;
	GPIO_InitStructure.GPIO_Speed = GPIO_SPEED_50MHZ;
	GPIO_Init(AC_OK_GPIO_PORT,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = EN_5V5_PIN | V_DROP_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_MODE_OUT_PP;
	GPIO_Init(EN_5V5_GPIO_PORT,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = EN_48V_PIN | EN_28V_PIN | EN_24V_PIN | EN_12V_PIN;
	GPIO_Init(EN_5V5_GPIO_PORT,&GPIO_InitStructure);
}


void App_PowerCtl_Init()
{
//	EN_5V5_OFF;
//	EN_48V_OFF;
//	EN_28V_OFF;
//	EN_24V_OFF;
//	EN_12V_OFF;
	
	EN_5V5_ON;
	EN_48V_ON;
	EN_28V_ON;
	EN_24V_ON;
	EN_12V_ON;
	
	V_DROP_OFF;
	
}
