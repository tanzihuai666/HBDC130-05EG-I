#include "drv_Timer.h"

T2_DELAY_STRUCT		T2_COUNT;

//static void NVIC_Configuration(void)
//{
//	NVIC_VectTableSet(NVIC_VECTTAB_FLASH, 0x0);  
//}

static void DRV_TIM2_NVIC_Configuration(void)
{
	NVIC_InitPara NVIC_InitStructure;

	NVIC_PRIGroup_Enable(NVIC_PRIGROUP_1);

	NVIC_InitStructure.NVIC_IRQ                = TIMER2_IRQn;
	NVIC_InitStructure.NVIC_IRQPreemptPriority = 0;
	NVIC_InitStructure.NVIC_IRQSubPriority     = 6;
	NVIC_InitStructure.NVIC_IRQEnable          = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

//Tout=((arr+1)*(psc+1))/Ft =1000us
static void DRV_TIM2_Configuration(void)       //0.5ms                     //中断周期为1ms
{
	TIMER_BaseInitPara  TIM_TimeBaseStructure;

	RCC_APB1PeriphClock_Enable(RCC_APB1PERIPH_TIMER2, ENABLE);

	TIMER_DeInit(TIMER2);
	TIM_TimeBaseStructure.TIMER_Prescaler     = 5399;                 //108MHz/(5399+1)= 20KHz   时钟预分频数
	TIM_TimeBaseStructure.TIMER_Period        = 9;                   //TIMER2=20KHz/(39+1)=1KHz=0.5ms  自动重装载寄存器周期的值(计数值) 
	TIM_TimeBaseStructure.TIMER_ClockDivision = TIMER_CDIV_DIV1;
	TIM_TimeBaseStructure.TIMER_CounterMode   = TIMER_COUNTER_UP;
	//TIM_TimeBaseStructure.TIMER_RepetitionCounter = 0;
	TIMER_BaseInit(TIMER2, &TIM_TimeBaseStructure);
//	TIMER_CARLPreloadConfig(TIMER2, ENABLE);
	TIMER_INTConfig(TIMER2, TIMER_INT_UPDATE, ENABLE);
	TIMER_SinglePulseMode(TIMER2, TIMER_SP_MODE_REPETITIVE);
  TIMER_Enable(TIMER2, ENABLE);
}

void DRV_Timer_Init()
{
	DRV_TIM2_Configuration();
	DRV_TIM2_NVIC_Configuration();
}

void TIMER2_IRQHandler(void)  //0.5ms
{
	IWDG_ReloadCounter();			      //独立看门狗喂狗

	if(TIMER_GetIntBitState(TIMER2,TIMER_INT_UPDATE)!=RESET)
	{
		T2_COUNT.AD_Count++;
		T2_COUNT.RS422_Count++;
		T2_COUNT.AD_Cur_Count++;
		
		TIMER_ClearIntBitState(TIMER2,TIMER_INT_UPDATE);
	}
	TIMER_ClearIntBitState(TIMER2,TIMER_INT_UPDATE);
}


