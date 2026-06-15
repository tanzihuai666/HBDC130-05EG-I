/**
 * @file    bsp_adc_dma.c
 * @brief   ADC1 与 DMA 循环采样驱动实现。
 *          ADC1 连续扫描 13 个规则通道，DMA1_CH1 循环保存 16 组完整扫描结果。
 *          10ms 任务对每一路执行均值滤波，然后按以下线性关系转换工程量：
 *          电压 V=(raw×Vref/4096)×k+b；电流 I=(raw×Vref/4096)×k+b。
 *          每一路电流均使用独立斜率和截距，因此实际采样曲线不需要经过原点。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#include "bsp_adc_dma.h"
#include "app_config.h"
#include <string.h>

/* DMA 异步更新的 16组×13路循环采样缓存。 */
volatile uint16_t g_adc_dma_buf[BSP_ADC_AVG_SAMPLES][BSP_ADC_CHANNEL_COUNT];

/* 各通道最近一次 16 点平均后的 12 位采样码。 */
static uint16_t g_adc_raw[BSP_ADC_CHANNEL_COUNT];

/* 最近一次计算完成的全部工程量。 */
static bsp_adc_values_t g_adc_values;

/* 六路电压标定表，顺序对应 VIN、+12V、+5V、+3.3V、-12V、+28V。 */
static const bsp_adc_linear_cal_t g_voltage_cal[6] = {
    { BSP_ADC_VIN_V_GAIN,  BSP_ADC_VIN_V_OFFSET_V  },
    { BSP_ADC_12V_V_GAIN,  BSP_ADC_12V_V_OFFSET_V  },
    { BSP_ADC_5V_V_GAIN,   BSP_ADC_5V_V_OFFSET_V   },
    { BSP_ADC_3V3_V_GAIN,  BSP_ADC_3V3_V_OFFSET_V  },
    { BSP_ADC_M12V_V_GAIN, BSP_ADC_M12V_V_OFFSET_V },
    { BSP_ADC_28V_V_GAIN,  BSP_ADC_28V_V_OFFSET_V  }
};

/* 六路电流运行时标定表，顺序对应 VIN、+12V、+5V、+3.3V、-12V、+28V。 */
static bsp_adc_linear_cal_t g_current_cal[6] = {
    { BSP_ADC_VIN_I_GAIN_A_PER_V,  BSP_ADC_VIN_I_OFFSET_A  },
    { BSP_ADC_12V_I_GAIN_A_PER_V,  BSP_ADC_12V_I_OFFSET_A  },
    { BSP_ADC_5V_I_GAIN_A_PER_V,   BSP_ADC_5V_I_OFFSET_A   },
    { BSP_ADC_3V3_I_GAIN_A_PER_V,  BSP_ADC_3V3_I_OFFSET_A  },
    { BSP_ADC_M12V_I_GAIN_A_PER_V, BSP_ADC_M12V_I_OFFSET_A },
    { BSP_ADC_28V_I_GAIN_A_PER_V,  BSP_ADC_28V_I_OFFSET_A  }
};

/**
 * @brief  将 12 位采样码转换为 ADC 引脚电压。
 * @param  raw ADC 采样码，范围 0~4095。
 * @retval ADC 引脚电压，单位 V。
 */
static float adc_raw_to_pin_voltage(uint16_t raw)
{
    return ((float)raw * BSP_ADC_REFERENCE_VOLTAGE_V) / BSP_ADC_CODE_COUNT;
}

/**
 * @brief  对采样码应用一次线性标定。
 * @param  raw ADC 采样码。
 * @param  cal 斜率和截距参数。
 * @retval 换算后的工程量。
 */
static float adc_apply_linear_calibration(uint16_t raw,
                                          const bsp_adc_linear_cal_t *cal)
{
    float pin_voltage;

    if (cal == 0) {
        return 0.0f;
    }

    pin_voltage = adc_raw_to_pin_voltage(raw);
    return pin_voltage * cal->gain + cal->offset;
}

/**
 * @brief  判断枚举值是否为电流通道。
 * @param  ch ADC 通道。
 * @retval true=电流通道；false=其他通道。
 */
static bool adc_is_current_channel(bsp_adc_channel_t ch)
{
    return ((ch >= BSP_ADC_VIN_I) && (ch <= BSP_ADC_28V_I)) ? true : false;
}

/**
 * @brief  将电流通道枚举转换为电流标定表下标。
 * @param  ch 电流通道。
 * @retval 0~5 的数组下标。
 */
static uint8_t adc_current_cal_index(bsp_adc_channel_t ch)
{
    return (uint8_t)((uint8_t)ch - (uint8_t)BSP_ADC_VIN_I);
}

/**
 * @brief  将温度通道采样码转换为摄氏温度。
 * @param  raw 温度通道采样码。
 * @retval 温度，单位 ℃。
 * @warning 当前仍为线性占位公式，NTC 硬件定型后必须替换为实测查表或热敏电阻公式。
 */
static float adc_to_temp_c(uint16_t raw)
{
    return -55.0f + (((float)raw * 155.0f) / BSP_ADC_CODE_COUNT);
}

/**
 * @brief  获取指定通道的平均采样码。
 * @param  ch ADC 通道。
 * @retval 12 位采样码；通道越界时返回 0。
 */
uint16_t bsp_adc_get_raw(bsp_adc_channel_t ch)
{
    if ((uint8_t)ch >= BSP_ADC_CHANNEL_COUNT) {
        return 0u;
    }
    return g_adc_raw[(uint8_t)ch];
}

/**
 * @brief  复制最近一次工程量结果。
 * @param  out 输出结构体指针。
 * @retval 无。
 */
void bsp_adc_get_values(bsp_adc_values_t *out)
{
    if (out != 0) {
        *out = g_adc_values;
    }
}

/**
 * @brief  设置指定电流通道的运行时斜率和截距。
 * @param  ch 电流通道。
 * @param  gain 斜率 k，单位 A/V。
 * @param  offset 截距 b，单位 A。
 * @retval true=成功；false=通道错误。
 * @note   日志按微单位整数输出，避免 ARMCC 精简 printf 引入浮点格式化问题。
 */
bool bsp_adc_set_current_calibration(bsp_adc_channel_t ch,
                                     float gain,
                                     float offset)
{
    uint8_t index;
    int32_t gain_micro;
    int32_t offset_micro;

    if (!adc_is_current_channel(ch)) {
        APP_LOGW("ADC calibration rejected: invalid current channel=%u",
                 (unsigned int)ch);
        return false;
    }

    index = adc_current_cal_index(ch);
    g_current_cal[index].gain = gain;
    g_current_cal[index].offset = offset;

    gain_micro = (int32_t)(gain * 1000000.0f);
    offset_micro = (int32_t)(offset * 1000000.0f);

    APP_LOGI("ADC current calibration: channel=%u gain_uA_per_V=%ld offset_uA=%ld",
             (unsigned int)ch,
             (long)gain_micro,
             (long)offset_micro);
    return true;
}

/**
 * @brief  获取指定电流通道当前标定参数。
 * @param  ch 电流通道。
 * @param  cal 参数输出指针。
 * @retval true=成功；false=参数无效。
 */
bool bsp_adc_get_current_calibration(bsp_adc_channel_t ch,
                                     bsp_adc_linear_cal_t *cal)
{
    if ((!adc_is_current_channel(ch)) || (cal == 0)) {
        return false;
    }

    *cal = g_current_cal[adc_current_cal_index(ch)];
    return true;
}

/**
 * @brief  配置 PA1~PA7、PC0~PC5 为模拟输入。
 * @param  无。
 * @retval 无。
 */
static void adc_gpio_init(void)
{
    GPIO_InitPara gpio;

    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOA |
                               RCC_APB2PERIPH_GPIOC,
                               ENABLE);

    gpio.GPIO_Speed = GPIO_SPEED_50MHZ;
    gpio.GPIO_Mode = GPIO_MODE_AIN;

    gpio.GPIO_Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                    GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
                    GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_Init(GPIOC, &gpio);
}

/**
 * @brief  初始化 ADC1、DMA1_CH1 和 13 路规则扫描序列。
 * @param  无。
 * @retval 无。
 */
void bsp_adc_dma_init(void)
{
    uint32_t delay_count;
    DMA_InitPara dma;
    ADC_InitPara adc;

    memset((void *)g_adc_dma_buf, 0, sizeof(g_adc_dma_buf));
    memset(g_adc_raw, 0, sizeof(g_adc_raw));
    memset(&g_adc_values, 0, sizeof(g_adc_values));

    adc_gpio_init();

    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_ADC1, ENABLE);
    RCC_AHBPeriphClock_Enable(RCC_AHBPERIPH_DMA1, ENABLE);
    RCC_ADCCLKConfig(RCC_ADCCLK_APB2_DIV12);

    /* DMA 从 ADC1 数据寄存器循环搬运 16×13 个半字到内存。 */
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

    /* ADC1 使用独立、扫描、连续、软件触发和右对齐模式。 */
    adc.ADC_Mode = ADC_MODE_INDEPENDENT;
    adc.ADC_Mode_Scan = ENABLE;
    adc.ADC_Mode_Continuous = ENABLE;
    adc.ADC_Trig_External = ADC_EXTERNAL_TRIGGER_MODE_NONE;
    adc.ADC_Data_Align = ADC_DATAALIGN_RIGHT;
    adc.ADC_Channel_Number = BSP_ADC_CHANNEL_COUNT;
    ADC_Init(ADC1, &adc);

    /* 规则序列必须与 bsp_adc_channel_t 枚举顺序完全一致。 */
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

    /* 校准后短暂等待模拟电路和首组采样稳定。 */
    for (delay_count = 0u; delay_count < 120000u; delay_count++) {
        __NOP();
    }

    ADC_SoftwareStartConv_Enable(ADC1, ENABLE);

    APP_LOGI("ADC initialized: channels=%u average_samples=%u vref_mV=3300",
             (unsigned int)BSP_ADC_CHANNEL_COUNT,
             (unsigned int)BSP_ADC_AVG_SAMPLES);
}

/**
 * @brief  对 DMA 缓冲执行 16 点平均并更新全部工程量。
 * @param  无。
 * @retval 无。
 */
void bsp_adc_dma_task_10ms(void)
{
    uint8_t channel;
    uint8_t sample_index;

    for (channel = 0u; channel < BSP_ADC_CHANNEL_COUNT; channel++) {
        uint32_t sum;

        sum = 0u;
        for (sample_index = 0u;
             sample_index < BSP_ADC_AVG_SAMPLES;
             sample_index++) {
            sum += g_adc_dma_buf[sample_index][channel];
        }
        g_adc_raw[channel] = (uint16_t)(sum / BSP_ADC_AVG_SAMPLES);
    }

    g_adc_values.temp_c = adc_to_temp_c(g_adc_raw[BSP_ADC_TEMP]);

    /* 电压通道使用各自 k、b；-12V 通道换算幅值后恢复负号。 */
    g_adc_values.vin_v = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_VIN_V], &g_voltage_cal[0]);
    g_adc_values.v12_v = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_12V_V], &g_voltage_cal[1]);
    g_adc_values.v5_v = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_5V_V], &g_voltage_cal[2]);
    g_adc_values.v33_v = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_3V3_V], &g_voltage_cal[3]);
    g_adc_values.vm12_v = -adc_apply_linear_calibration(g_adc_raw[BSP_ADC_M12V_V], &g_voltage_cal[4]);
    g_adc_values.v28_v = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_28V_V], &g_voltage_cal[5]);

    /* 电流通道使用 I=(raw×Vref/4096)×k+b，不强制截断负值。 */
    g_adc_values.vin_i = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_VIN_I], &g_current_cal[0]);
    g_adc_values.i12_a = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_12V_I], &g_current_cal[1]);
    g_adc_values.i5_a = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_5V_I], &g_current_cal[2]);
    g_adc_values.i33_a = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_3V3_I], &g_current_cal[3]);
    g_adc_values.im12_a = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_M12V_I], &g_current_cal[4]);
    g_adc_values.i28_a = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_28V_I], &g_current_cal[5]);
}
