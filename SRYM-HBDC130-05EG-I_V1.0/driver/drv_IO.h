#ifndef __DRV_IO_H__
#define __DRV_IO_H__
#include "gd32f10x.h"

#define 	EN_5V5_PIN						GPIO_PIN_15
#define 	EN_5V5_GPIO_PORT			GPIOB
#define 	EN_5V5_GPIO_CLK				RCC_APB2PERIPH_GPIOB

#define		EN_5V5_OFF						GPIO_ResetBits(EN_5V5_GPIO_PORT, EN_5V5_PIN)
#define		EN_5V5_ON							GPIO_SetBits(EN_5V5_GPIO_PORT, EN_5V5_PIN)

#define 	EN_48V_PIN						GPIO_PIN_6
#define 	EN_48V_GPIO_PORT			GPIOC
#define 	EN_48V_GPIO_CLK				RCC_APB2PERIPH_GPIOC

#define		EN_48V_OFF						GPIO_ResetBits(EN_48V_GPIO_PORT, EN_48V_PIN)
#define		EN_48V_ON							GPIO_SetBits(EN_48V_GPIO_PORT, EN_48V_PIN)

#define 	EN_28V_PIN						GPIO_PIN_7
#define 	EN_28V_GPIO_PORT			GPIOC
#define 	EN_28V_GPIO_CLK				RCC_APB2PERIPH_GPIOC

#define		EN_28V_OFF						GPIO_ResetBits(EN_28V_GPIO_PORT, EN_28V_PIN)
#define		EN_28V_ON							GPIO_SetBits(EN_28V_GPIO_PORT, EN_28V_PIN)

#define 	EN_24V_PIN						GPIO_PIN_8
#define 	EN_24V_GPIO_PORT			GPIOC
#define 	EN_24V_GPIO_CLK				RCC_APB2PERIPH_GPIOC

#define		EN_24V_OFF						GPIO_ResetBits(EN_24V_GPIO_PORT, EN_24V_PIN)
#define		EN_24V_ON							GPIO_SetBits(EN_24V_GPIO_PORT, EN_24V_PIN)

#define 	EN_12V_PIN						GPIO_PIN_9
#define 	EN_12V_GPIO_PORT			GPIOC
#define 	EN_12V_GPIO_CLK				RCC_APB2PERIPH_GPIOC

#define		EN_12V_OFF						GPIO_ResetBits(EN_12V_GPIO_PORT, EN_12V_PIN)
#define		EN_12V_ON							GPIO_SetBits(EN_12V_GPIO_PORT, EN_12V_PIN)

#define 	V_DROP_PIN						GPIO_PIN_5
#define 	V_DROP_GPIO_PORT			GPIOB
#define 	V_DROP_GPIO_CLK				RCC_APB2PERIPH_GPIOB

#define		V_DROP_OFF						GPIO_SetBits(V_DROP_GPIO_PORT, V_DROP_PIN)
#define		V_DROP_ON							GPIO_ResetBits(V_DROP_GPIO_PORT, V_DROP_PIN)

#define 	AC_OK_PIN							GPIO_PIN_12
#define 	AC_OK_GPIO_PORT				GPIOB
#define 	AC_OK_GPIO_CLK				RCC_APB2PERIPH_GPIOB

#define 	READ_AC_OK						GPIO_ReadInputBit(AC_OK_GPIO_PORT, AC_OK_PIN);


void DRV_IO_Init(void);
void App_PowerCtl_Init(void);

#endif

