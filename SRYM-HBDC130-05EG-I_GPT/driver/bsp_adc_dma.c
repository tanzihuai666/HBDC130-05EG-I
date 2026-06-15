/**
 * @file    bsp_adc_dma.c
 * @brief   ADC1 与 DMA 循环采样驱动实现。
 *          ADC1 连续扫描 13 个规则通道，DMA1_CH1 将每次完整扫描结果循环写入二维缓存。
 *          主循环每 10ms 调用 bsp_adc_dma_task_10ms()，对每一路执行 16 点平均，并将
 *          ADC 原始采样码转换成实际电压、电流和温度。
 *
 *          电压换算关系：V = (raw × Vref / 4096) × k + b。
 *          电流换算关系：I = (raw × Vref / 4096) × k + b。
 *          因此电流采样关系无需经过原点，每一路均可配置独立的斜率和截距。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#include "bsp_adc_dma.h"
#include "app_config.h"
#include <string.h>

/*
 * DMA 循环采样缓存。
 * 第一维表示连续采集的 16 组完整扫描，第二维表示每组扫描中的 13 个规则通道。
 * volatile 用于阻止编译器缓存该数组，因为数组内容由 DMA 外设异步更新。
 */
volatile uint16_t g_adc_dma_buf[BSP_ADC_AVG_SAMPLES][BSP_ADC_CHANNEL_COUNT];

/* 保存每一路最近一次计算得到的 16 点平均 ADC 采样码。 */
static uint16_t g_adc_raw[BSP_ADC_CHANNEL_COUNT];

/* 保存最近一次完成换算的全部工程量，供主循环和传感器管理层读取。 */
static bsp_adc_values_t g_adc_values;

/*
 * 六路电压通道标定参数。
 * 数组顺序严格对应 BSP_ADC_VIN_V~BSP_ADC_28V_V，-12V 通道先计算幅值，之后再取负号。
 */
static const bsp_adc_linear_cal_t g_voltage_cal[6] = {
    { BSP_ADC_VIN_V_GAIN,  BSP_ADC_VIN_V_OFFSET_V  },
    { BSP_ADC_12V_V_GAIN,  BSP_ADC_12V_V_OFFSET_V  },
    { BSP_ADC_5V_V_GAIN,   BSP_ADC_5V_V_OFFSET_V   },
    { BSP_ADC_3V3_V_GAIN,  BSP_ADC_3V3_V_OFFSET_V  },
    { BSP_ADC_M12V_V_GAIN, BSP_ADC_M12V_V_OFFSET_V },
    { BSP_ADC_28V_V_GAIN,  BSP_ADC_28V_V_OFFSET_V  }
};

/*
 * 六路电流通道运行时标定参数。
 * 与电压标定表不同，本表不是 const，允许调试阶段在 RAM 中修改 k 和 b。
 * 数组顺序严格对应 BSP_ADC_VIN_I~BSP_ADC_28V_I。
 */
static bsp_adc_linear_cal_t g_current_cal[6] = {
    { BSP_ADC_VIN_I_GAIN_A_PER_V,  BSP_ADC_VIN_I_OFFSET_A  },
    { BSP_ADC_12V_I_GAIN_A_PER_V,  BSP_ADC_12V_I_OFFSET_A  },
    { BSP_ADC_5V_I_GAIN_A_PER_V,   BSP_ADC_5V_I_OFFSET_A   },
    { BSP_ADC_3V3_I_GAIN_A_PER_V,  BSP_ADC_3V3_I_OFFSET_A  },
    { BSP_ADC_M12V_I_GAIN_A_PER_V, BSP_ADC_M12V_I_OFFSET_A },
    { BSP_ADC_28V_I_GAIN_A_PER_V,  BSP_ADC_28V_I_OFFSET_A  }
};

/**
 * @brief  将 12 位 ADC 原始采样码转换为 ADC 引脚电压。
 * @param  raw 12 位采样码，正常范围为 0~4095。
 * @retval ADC 引脚电压，单位 V。
 * @note   分母使用 4096，即 2^12；最大码 4095 对应略小于参考电压。
 */
static float adc_raw_to_pin_voltage(uint16_t raw)
{
    return ((float)raw * BSP_ADC_REFERENCE_VOLTAGE_V) / BSP_ADC_CODE_COUNT;
}

/**
 * @brief  对 ADC 采样码应用一次线性标定。
 * @param  raw ADC 原始采样码。
 * @param  cal 标定参数，包含斜率 gain 和截距 offset。
 * @retval 换算后的工程量。
 * @note   通用公式为：工程量 = (raw × Vref / 4096) × gain + offset。
 */
static float adc_apply_linear_calibration(uint16_t raw, const bsp_adc_linear_cal_t *cal)
{
    float pin_voltage;

    if (cal == 0) {
        return 0.0f;
    }

    pin_voltage = adc_raw_to_pin_voltage(raw);
    return pin_voltage * cal->gain + cal->offset;
}

/**
 * @brief  判断通道是否属于六路电流采样通道。
 * @param  ch ADC 通道枚举。
 * @retval true=电流通道，false=其他通道。
 */
static bool adc_is_current_channel(bsp_adc_channel_t ch)
{
    return ((ch >= BSP_ADC_VIN_I) && (ch <= BSP_ADC_28V_I)) ? true : false;
}

/**
 * @brief  将电流通道枚举转换为 g_current_cal[] 数组下标。
 * @param  ch 电流通道枚举。
 * @retval 0~5 的标定表下标。
 * @note   调用前必须先通过 adc_is_current_channel() 检查通道有效性。
 */
static uint8_t adc_current_cal_index(bsp_adc_channel_t ch)
{
    return (uint8_t)((uint8_t)ch - (uint8_t)BSP_ADC_VIN_I);
}

/**
 * @brief  将温度通道 ADC 采样码转换为摄氏温度。
 * @param  raw 温度通道 12 位采样码。
 * @retval 温度值，单位 ℃。
 * @warning 当前公式仍是 -55℃~100℃ 的线性占位模型。若硬件使用 NTC，应根据阻值网络
 *          替换为查表法或 Steinhart-Hart 方程，否则温度读数仅能用于软件流程调试。
 */
static float adc_to_temp_c(uint16_t raw)
{
    return -55.0f + (((float)raw * 155.0f) / BSP_ADC_CODE_COUNT);
}

/**
 * @brief  获取指定通道最近一次平均后的 ADC 原始采样码。
 * @param  ch ADC 通道枚举。
 * @retval 0~4095 的采样码；通道越界时返回 0。
 */
uint16_t bsp_adc_get_raw(bsp_adc_channel_t ch)
{
    if ((uint8_t)ch >= BSP_ADC_CHANNEL_COUNT) {
        return 0u;
    }

    return g_adc_raw[(uint8_t)ch];
}

/**
 * @brief  获取最近一次换算完成的全部工程量。
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
 * @param  ch     电流通道枚举。
 * @param  gain   斜率 k，单位 A/V。
 * @param  offset 截距 b，单位 A。
 * @retval true=设置成功，false=通道不是电流通道。
 */
bool bsp_adc_set_current_calibration(bsp_adc_channel_t ch, float gain, float offset)
{
    uint8_t index;

    if (!adc_is_current_channel(ch)) {
        APP_LOGW("ADC标定失败：通道%u不是电流通道", (unsigned int)ch);
        return false;
    }

    index = adc_current_cal_index(ch);
    g_current_cal[index].gain = gain;
    g_current_cal[index].offset = offset;

    APP_LOGI("ADC电流标定更新：通道=%u k=%.6f b=%.6f",
             (unsigned int)ch,
             (double)gain,
             (double)offset);
    return true;
}

/**
 * @brief  读取指定电流通道当前使用的标定参数。
 * @param  ch  电流通道枚举。
 * @param  cal 标定参数输出指针。
 * @retval true=读取成功，false=参数无效。
 */
bool bsp_adc_get_current_calibration(bsp_adc_channel_t ch, bsp_adc_linear_cal_t *cal)
{
    uint8_t index;

    if ((!adc_is_current_channel(ch)) || (cal == 0)) {
        return false;
    }

    index = adc_current_cal_index(ch);
    *cal = g_current_cal[index];
    return true;
}

/**
 * @brief  初始化所有 ADC 模拟输入 GPIO。
 * @param  无。
 * @retval 无。
 */
static void adc_gpio_init(void)
{
    GPIO_InitPara gpio;

    /* 打开 GPIOA 和 GPIOC 外设时钟。 */
    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOA | RCC_APB2PERIPH_GPIOC, ENABLE);

    /* 模拟输入模式关闭数字输入缓冲，可降低功耗并避免数字噪声影响 ADC。 */
    gpio.GPIO_Speed = GPIO_SPEED_50MHZ;
    gpio.GPIO_Mode = GPIO_MODE_AIN;

    /* PA1~PA7 对应 ADC_CH1~ADC_CH7。 */
    gpio.GPIO_Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                    GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_Init(GPIOA, &gpio);

    /* PC0~PC5 对应 ADC_CH10~ADC_CH15。 */
    gpio.GPIO_Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
                    GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_Init(GPIOC, &gpio);
}

/**
 * @brief  初始化 ADC1、DMA1_CH1 和规则通道扫描序列。
 * @param  无。
 * @retval 无。
 */
void bsp_adc_dma_init(void)
{
    uint32_t delay_count;
    DMA_InitPara dma;
    ADC_InitPara adc;

    /* 上电时清零 DMA 缓冲、平均采样码和工程量，避免使用未初始化数据。 */
    memset((void *)g_adc_dma_buf, 0, sizeof(g_adc_dma_buf));
    memset(g_adc_raw, 0, sizeof(g_adc_raw));
    memset(&g_adc_values, 0, sizeof(g_adc_values));

    adc_gpio_init();

    /* 打开 ADC1、DMA1 时钟，并将 ADC 时钟分频到器件允许范围。 */
    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_ADC1, ENABLE);
    RCC_AHBPeriphClock_Enable(RCC_AHBPERIPH_DMA1, ENABLE);
    RCC_ADCCLKConfig(RCC_ADCCLK_APB2_DIV12);

    /*
     * DMA1_CH1 配置：外设地址固定为 ADC1 数据寄存器，内存地址递增，半字传输，
     * 循环模式持续覆盖 16×13 个采样单元。
     */
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

    /* ADC1 配置为独立、扫描、连续转换、软件触发和右对齐。 */
    adc.ADC_Mode = ADC_MODE_INDEPENDENT;
    adc.ADC_Mode_Scan = ENABLE;
    adc.ADC_Mode_Continuous = ENABLE;
    adc.ADC_Trig_External = ADC_EXTERNAL_TRIGGER_MODE_NONE;
    adc.ADC_Data_Align = ADC_DATAALIGN_RIGHT;
    adc.ADC_Channel_Number = BSP_ADC_CHANNEL_COUNT;
    ADC_Init(ADC1, &adc);

    /*
     * 规则序列编号必须与 bsp_adc_channel_t 枚举完全一致。
     * 较长采样时间用于降低高阻分压网络和运放输出阻抗造成的建立时间误差。
     */
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

    /* 打开 ADC DMA 请求，执行 ADC 自校准，然后启动连续软件转换。 */
    ADC_DMA_Enable(ADC1, ENABLE);
    ADC_Enable(ADC1, ENABLE);
    ADC_Calibration(ADC1);

    /* 自校准完成后保留短暂稳定时间，避免立即启动产生首组异常采样。 */
    for (delay_count = 0u; delay_count < 120000u; delay_count++) {
        __NOP();
    }

    ADC_SoftwareStartConv_Enable(ADC1, ENABLE);

    APP_LOGI("ADC初始化完成：通道=%u，平均点数=%u，参考电压=%.3fV",
             (unsigned int)BSP_ADC_CHANNEL_COUNT,
             (unsigned int)BSP_ADC_AVG_SAMPLES,
             (double)BSP_ADC_REFERENCE_VOLTAGE_V);
}

/**
 * @brief  执行 16 点平均并更新全部工程量。
 * @param  无。
 * @retval 无。
 */
void bsp_adc_dma_task_10ms(void)
{
    uint8_t channel;
    uint8_t sample_index;

    /*
     * 对每个通道分别累加 16 组 DMA 数据并取平均。
     * 16×4095=65520，小于 uint32_t 上限，因此累加不会溢出。
     */
    for (channel = 0u; channel < BSP_ADC_CHANNEL_COUNT; channel++) {
        uint32_t sum = 0u;

        for (sample_index = 0u; sample_index < BSP_ADC_AVG_SAMPLES; sample_index++) {
            sum += g_adc_dma_buf[sample_index][channel];
        }

        g_adc_raw[channel] = (uint16_t)(sum / BSP_ADC_AVG_SAMPLES);
    }

    /* 温度通道使用独立换算模型。 */
    g_adc_values.temp_c = adc_to_temp_c(g_adc_raw[BSP_ADC_TEMP]);

    /*
     * 六路电压均使用 V=(raw×Vref/4096)×k+b。
     * -12V 硬件采样得到的是绝对值，因此换算完成后添加负号恢复实际极性。
     */
    g_adc_values.vin_v = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_VIN_V], &g_voltage_cal[0]);
    g_adc_values.v12_v = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_12V_V], &g_voltage_cal[1]);
    g_adc_values.v5_v = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_5V_V], &g_voltage_cal[2]);
    g_adc_values.v33_v = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_3V3_V], &g_voltage_cal[3]);
    g_adc_values.vm12_v = -adc_apply_linear_calibration(g_adc_raw[BSP_ADC_M12V_V], &g_voltage_cal[4]);
    g_adc_values.v28_v = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_28V_V], &g_voltage_cal[5]);

    /*
     * 六路电流均使用 I=(raw×Vref/4096)×k+b。
     * 这里不强制把负数钳位为 0，避免掩盖错误的零点标定；调试时可通过日志观察并修正 b。
     */
    g_adc_values.vin_i = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_VIN_I], &g_current_cal[0]);
    g_adc_values.i12_a = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_12V_I], &g_current_cal[1]);
    g_adc_values.i5_a = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_5V_I], &g_current_cal[2]);
    g_adc_values.i33_a = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_3V3_I], &g_current_cal[3]);
    g_adc_values.im12_a = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_M12V_I], &g_current_cal[4]);
    g_adc_values.i28_a = adc_apply_linear_calibration(g_adc_raw[BSP_ADC_28V_I], &g_current_cal[5]);
}
