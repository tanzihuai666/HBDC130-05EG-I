/**
 * @file    bsp_adc_dma.c
 * @brief   ADC1 + DMA 循环采样驱动。
 *          13 路模拟量由 ADC1 扫描转换，DMA1_CH1 写入 16 组循环缓存，10ms 任务做均值滤波
 *          和工程量换算。当前比例系数为默认估算值，需按实际硬件校准。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#include "bsp_adc_dma.h"
#include "app_config.h"
#include <string.h>

volatile uint16_t g_adc_dma_buf[BSP_ADC_AVG_SAMPLES][BSP_ADC_CHANNEL_COUNT]; /* DMA 循环采样缓存。 */
static uint16_t g_adc_raw[BSP_ADC_CHANNEL_COUNT];                            /* 16 点均值 raw。 */
static bsp_adc_values_t g_adc_values;                                        /* 工程量缓存。 */

/** @brief ADC raw 转电压。@param raw 12-bit raw。@param full_scale 满量程电压。@retval 电压值。 */
static float adc_to_voltage(uint16_t raw, float full_scale)
{
    return ((float)raw * full_scale) / 4095.0f;
}

/** @brief ADC raw 转电流。@param raw 12-bit raw。@param full_scale 满量程电流。@retval 电流值。 */
static float adc_to_current(uint16_t raw, float full_scale)
{
    return ((float)raw * full_scale) / 4095.0f;
}

/** @brief ADC raw 转温度。@note 当前为线性占位，后续需替换为 NTC 查表或 Steinhart-Hart。 */
static float adc_to_temp_c(uint16_t raw)
{
    return -55.0f + (((float)raw * 155.0f) / 4095.0f);
}

/** @brief 获取指定 ADC 通道 raw 均值。 */
uint16_t bsp_adc_get_raw(bsp_adc_channel_t ch)
{
    if ((uint8_t)ch >= BSP_ADC_CHANNEL_COUNT) return 0u;
    return g_adc_raw[(uint8_t)ch];
}

/** @brief 获取最新工程量快照。 */
void bsp_adc_get_values(bsp_adc_values_t *out)
{
    if (out != 0) *out = g_adc_values;
}

/** @brief 初始化 ADC 输入引脚。 */
static void adc_gpio_init(void)
{
    GPIO_InitPara gpio;

    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOA | RCC_APB2PERIPH_GPIOC, ENABLE);
    gpio.GPIO_Speed = GPIO_SPEED_50MHZ;
    gpio.GPIO_Mode = GPIO_MODE_AIN;

    gpio.GPIO_Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_Init(GPIOC, &gpio);
}

/** @brief 初始化 ADC1 扫描转换和 DMA 循环模式。 */
void bsp_adc_dma_init(void)
{
    uint32_t i;
    DMA_InitPara dma;
    ADC_InitPara adc;

    memset((void *)g_adc_dma_buf, 0, sizeof(g_adc_dma_buf));
    memset(g_adc_raw, 0, sizeof(g_adc_raw));
    memset(&g_adc_values, 0, sizeof(g_adc_values));

    adc_gpio_init();

    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_ADC1, ENABLE);
    RCC_AHBPeriphClock_Enable(RCC_AHBPERIPH_DMA1, ENABLE);
    RCC_ADCCLKConfig(RCC_ADCCLK_APB2_DIV12);

    DMA_DeInit(DMA1_CHANNEL1);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&(ADC1->RDTR);
    dma.DMA_MemoryBaseAddr = (uint32_t)g_adc_dma_buf;
    dma.DMA_DIR = DMA_DIR_PERIPHERALSRC;
    dma.DMA_BufferSize = BSP_ADC_AVG_SAMPLES * BSP_ADC_CHANNEL_COUNT;
    dma.DMA_PeripheralInc = DMA_PERIPHERALINC_DISABLE;
    dma.DMA_MemoryInc = DMA_MEMORYINC_ENABLE;
    dma.DMA_PeripheralDataSize = DMA_PERIPHERALDATASIZE_HALFWORD;
    dma.DMA_MemoryDataSize = DMA_MEMORYDATASIZE_HALFWORD;
    dma.DMA_Mode = DMA_MODE_CIRCULAR;
    dma.DMA_Priority = DMA_PRIORITY_MEDIUM;
    dma.DMA_MTOM = DMA_MEMTOMEM_DISABLE;
    DMA_Init(DMA1_CHANNEL1, &dma);
    DMA_Enable(DMA1_CHANNEL1, ENABLE);

    adc.ADC_Mode = ADC_MODE_INDEPENDENT;
    adc.ADC_Mode_Scan = ENABLE;
    adc.ADC_Mode_Continuous = ENABLE;
    adc.ADC_Trig_External = ADC_EXTERNAL_TRIGGER_MODE_NONE;
    adc.ADC_Data_Align = ADC_DATAALIGN_RIGHT;
    adc.ADC_Channel_Number = BSP_ADC_CHANNEL_COUNT;
    ADC_Init(ADC1, &adc);

    ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_1,  1,  ADC_SAMPLETIME_55POINT5);
    ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_2,  2,  ADC_SAMPLETIME_55POINT5);
    ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_3,  3,  ADC_SAMPLETIME_55POINT5);
    ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_4,  4,  ADC_SAMPLETIME_55POINT5);
    ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_5,  5,  ADC_SAMPLETIME_55POINT5);
    ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_6,  6,  ADC_SAMPLETIME_55POINT5);
    ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_7,  7,  ADC_SAMPLETIME_55POINT5);
    ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_10, 8,  ADC_SAMPLETIME_55POINT5);
    ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_11, 9,  ADC_SAMPLETIME_55POINT5);
    ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_12, 10, ADC_SAMPLETIME_55POINT5);
    ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_13, 11, ADC_SAMPLETIME_55POINT5);
    ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_14, 12, ADC_SAMPLETIME_55POINT5);
    ADC_RegularChannel_Config(ADC1, ADC_CHANNEL_15, 13, ADC_SAMPLETIME_55POINT5);

    ADC_DMA_Enable(ADC1, ENABLE);
    ADC_Enable(ADC1, ENABLE);
    ADC_Calibration(ADC1);
    for (i = 0; i < 120000u; i++) { __NOP(); }
    ADC_SoftwareStartConv_Enable(ADC1, ENABLE);
    APP_LOGI("adc: init channels=%u avg=%u", BSP_ADC_CHANNEL_COUNT, BSP_ADC_AVG_SAMPLES);
}

/** @brief 10ms 均值滤波和工程量计算任务。 */
void bsp_adc_dma_task_10ms(void)
{
    uint8_t ch;
    uint8_t sample;

    for (ch = 0u; ch < BSP_ADC_CHANNEL_COUNT; ch++) {
        uint32_t sum = 0u;
        for (sample = 0u; sample < BSP_ADC_AVG_SAMPLES; sample++) sum += g_adc_dma_buf[sample][ch];
        g_adc_raw[ch] = (uint16_t)(sum / BSP_ADC_AVG_SAMPLES);
    }

    g_adc_values.temp_c = adc_to_temp_c(g_adc_raw[BSP_ADC_TEMP]);
    g_adc_values.vin_v  = adc_to_voltage(g_adc_raw[BSP_ADC_VIN_V], 40.0f);
    g_adc_values.v12_v  = adc_to_voltage(g_adc_raw[BSP_ADC_12V_V], 15.0f);
    g_adc_values.v5_v   = adc_to_voltage(g_adc_raw[BSP_ADC_5V_V], 6.0f);
    g_adc_values.v33_v  = adc_to_voltage(g_adc_raw[BSP_ADC_3V3_V], 4.0f);
    g_adc_values.vm12_v = -adc_to_voltage(g_adc_raw[BSP_ADC_M12V_V], 15.0f);
    g_adc_values.v28_v  = adc_to_voltage(g_adc_raw[BSP_ADC_28V_V], 32.0f);
    g_adc_values.vin_i  = adc_to_current(g_adc_raw[BSP_ADC_VIN_I], 5.0f);
    g_adc_values.i12_a  = adc_to_current(g_adc_raw[BSP_ADC_12V_I], 10.0f);
    g_adc_values.i5_a   = adc_to_current(g_adc_raw[BSP_ADC_5V_I], 3.0f);
    g_adc_values.i33_a  = adc_to_current(g_adc_raw[BSP_ADC_3V3_I], 5.0f);
    g_adc_values.im12_a = adc_to_current(g_adc_raw[BSP_ADC_M12V_I], 0.5f);
    g_adc_values.i28_a  = adc_to_current(g_adc_raw[BSP_ADC_28V_I], 0.5f);
}
