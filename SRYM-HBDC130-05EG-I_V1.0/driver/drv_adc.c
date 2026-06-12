#include "drv_Init.h"

volatile uint16_t ADC_ConvertedValue[TIMES][AD_CHANNEL];

static void ADC_GpioConfig(void)
{
	GPIO_InitPara GPIO_InitStructure;
	
	RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOA | RCC_APB2PERIPH_GPIOB | RCC_APB2PERIPH_GPIOC , ENABLE);

	GPIO_InitStructure.GPIO_Pin = AD_TEMP_PIN | AD_VOL_48V_PIN | AD_VOL_28V_PIN | AD_CUR_28V_PIN | AD_VOL_24V_PIN | AD_CUR_24V_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_SPEED_50MHZ;
	GPIO_InitStructure.GPIO_Mode = GPIO_MODE_AIN;             //配置为模拟输入
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = AD_CUR_48V_PIN | AD_VOL_5V5_PIN | AD_CUR_5V5_PIN;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = AD_VOL_12V_PIN | AD_CUR_12V_PIN;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

static void ADC_DMAConfig()
{
	uint32_t  i;
	DMA_InitPara DMA_InitStructure;
	ADC_InitPara ADC_InitStructure;
	
	RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_ADC1, ENABLE);
	RCC_AHBPeriphClock_Enable(RCC_AHBPERIPH_DMA1, ENABLE);
	RCC_ADCCLKConfig(RCC_ADCCLK_APB2_DIV12);   //ADC时钟  配置12分频
	
	/* USART1 RX DMA1 Channel (triggered by USART1 Rx event) Config */
	DMA_DeInit(DMA1_CHANNEL1);
	DMA_InitStructure.DMA_PeripheralBaseAddr = (u32) &(ADC1->RDTR);
	DMA_InitStructure.DMA_MemoryBaseAddr = (u32)ADC_ConvertedValue;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PERIPHERALSRC;
	DMA_InitStructure.DMA_BufferSize = TIMES*AD_CHANNEL;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PERIPHERALINC_DISABLE;
	DMA_InitStructure.DMA_MemoryInc = DMA_MEMORYINC_ENABLE;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PERIPHERALDATASIZE_HALFWORD;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MEMORYDATASIZE_HALFWORD;
	DMA_InitStructure.DMA_Mode = DMA_MODE_CIRCULAR;
	DMA_InitStructure.DMA_Priority = DMA_PRIORITY_MEDIUM;
	DMA_InitStructure.DMA_MTOM = DMA_MEMTOMEM_DISABLE;
	DMA_Init(DMA1_CHANNEL1, &DMA_InitStructure);
	DMA_Enable(DMA1_CHANNEL1, ENABLE);
	
	ADC_InitStructure.ADC_Mode = ADC_MODE_INDEPENDENT;       //独立模式
	ADC_InitStructure.ADC_Mode_Scan = ENABLE;					//多通道
	ADC_InitStructure.ADC_Mode_Continuous = ENABLE;				//连续转换
	ADC_InitStructure.ADC_Trig_External = ADC_EXTERNAL_TRIGGER_MODE_NONE;          //软件触发而不是外部触发
	ADC_InitStructure.ADC_Data_Align = ADC_DATAALIGN_RIGHT;        //右对齐
	ADC_InitStructure.ADC_Channel_Number = AD_CHANNEL;				//通道数目
	ADC_Init(ADC1, &ADC_InitStructure);

	ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_1,  1, ADC_SAMPLETIME_55POINT5);	//temper
	ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_2,  2, ADC_SAMPLETIME_55POINT5);	//48v vol
	ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_10, 3, ADC_SAMPLETIME_55POINT5);	//48vcur
	ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_4,  4, ADC_SAMPLETIME_55POINT5);	//28v vol
	ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_5,  5, ADC_SAMPLETIME_55POINT5);	//28vcur
	ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_6,  6, ADC_SAMPLETIME_55POINT5);	//24v vol
	ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_7,  7, ADC_SAMPLETIME_55POINT5);	//24vcur
	ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_8,  8, ADC_SAMPLETIME_55POINT5);	//12v vol
	ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_9,  9, ADC_SAMPLETIME_55POINT5);	//12vcur
	ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_14, 10, ADC_SAMPLETIME_55POINT5);	//5.5v vol
	ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_15, 11, ADC_SAMPLETIME_55POINT5);	//5.5vcur
	
	ADC_DMA_Enable(ADC1, ENABLE);
	ADC_Enable(ADC1, ENABLE);                    //使能ADC
	ADC_Calibration(ADC1);
	for(i = 0; i < 60000; i++);                                                 //500us延时
	for(i = 0; i < 60000; i++);
	ADC_SoftwareStartConv_Enable(ADC1, ENABLE);
}

static void ADC_AdcConfig(void)
{
	uint32_t  i;
	ADC_InitPara ADC_InitStructure;
	
	RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_ADC1, ENABLE);
	RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_ADC2, ENABLE);
	RCC_ADCCLKConfig(RCC_ADCCLK_APB2_DIV12);   //ADC时钟
	
	ADC_InitStructure.ADC_Mode = ADC_MODE_INDEPENDENT;	                        //ADC工作模式:ADC1和ADC2工作在独立模式
	ADC_InitStructure.ADC_Mode_Scan = DISABLE;	                                //模数转换工作在单通道模式
	ADC_InitStructure.ADC_Mode_Continuous = DISABLE;	                          //模数转换工作在单次转换模式
	ADC_InitStructure.ADC_Trig_External = ADC_EXTERNAL_TRIGGER_MODE_NONE;       //转换由软件而不是外部触发启动
	ADC_InitStructure.ADC_Data_Align = ADC_DATAALIGN_RIGHT;		                  //ADC数据右对齐
	ADC_InitStructure.ADC_Channel_Number = AD_CHANNEL;					                //顺序进行规则转换的ADC通道的数目
	ADC_Init(ADC1, &ADC_InitStructure);
	
	ADC_Enable(ADC1, ENABLE);                                                   //ADC命令，使能
	for(i = 0; i < 60000; i++);                                                 //500us延时
	for(i = 0; i < 60000; i++);
	ADC_Calibration(ADC1);                                                      //开启ADC1
	ADC_SoftwareStartConv_Enable(ADC1, ENABLE);                                 //连续转换开始，ADC通过DMA方式不断的更新RAM区 

	//Config ADC2
	ADC_InitStructure.ADC_Mode = ADC_MODE_INDEPENDENT;	                        //ADC工作模式:ADC1和ADC2工作在独立模式
	ADC_InitStructure.ADC_Mode_Scan = DISABLE;	                                //模数转换工作在单通道模式
	ADC_InitStructure.ADC_Mode_Continuous = DISABLE;	                          //模数转换工作在单次转换模式
	ADC_InitStructure.ADC_Trig_External = ADC_EXTERNAL_TRIGGER_MODE_NONE;       //转换由软件而不是外部触发启动
	ADC_InitStructure.ADC_Data_Align = ADC_DATAALIGN_RIGHT;		                  //ADC数据右对齐
	ADC_InitStructure.ADC_Channel_Number = 1;					                          //顺序进行规则转换的ADC通道的数目
	ADC_Init(ADC2, &ADC_InitStructure);
	
	ADC_Enable(ADC2, ENABLE);                                                   //ADC命令，使能
	for(i=0;i<60000;i++);
	for(i=0;i<60000;i++);
	ADC_Calibration(ADC2);                                                      //开启ADC1
	ADC_SoftwareStartConv_Enable(ADC2, ENABLE);                                 //连续转换开始，ADC通过DMA方式不断的更新RAM?
}

void DRV_ADC_Init(void)
{
	ADC_GpioConfig();
	ADC_DMAConfig();

}




