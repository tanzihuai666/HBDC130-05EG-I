#include "drv_Init.h"

static void DRV_IWDG_Configuration(void)   
{
	// Enable write access to IWDG_PSR and IWDG_RLDR registers 
	IWDG_Write_Enable(IWDG_WRITEACCESS_ENABLE);

	// IWDG counter clock: 40KHz(LSI) / 64 = 0.625 KHz 
	IWDG_SetPrescaler(IWDG_PRESCALER_64);

	// Set counter reload value to 625,615/625Hz=1s 
	IWDG_SetReloadValue(3125);   //(5*625)/625Hz=1s 

	// Reload IWDG counter 
	IWDG_ReloadCounter();

	// Enable IWDG (the LSI oscillator will be enabled by hardware) 
	IWDG_Enable();
}

void DRV_Init()
{	
	DRV_IO_Init();
	DRV_ADC_Init();
	DRV_Timer_Init();
	DRV_UART_Init(115200);
	DRV_IWDG_Configuration();
}



