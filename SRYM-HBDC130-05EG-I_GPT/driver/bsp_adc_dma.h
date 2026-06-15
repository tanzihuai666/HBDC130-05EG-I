/**
 * @file    bsp_adc_dma.h
 * @brief   ADC1 与 DMA 循环采样驱动接口。
 *          本模块负责 13 路模拟量采样、16 点均值滤波以及 ADC 采样码到实际电压、
 *          电流和温度工程量的换算。电压和电流均采用可配置线性关系，电流换算支持
 *          非零截距：I = (raw × Vref / 4096) × k + b。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#ifndef BSP_ADC_DMA_H
#define BSP_ADC_DMA_H

#include "gd32f10x.h"
#include <stdint.h>
#include <stdbool.h>

/* ADC 扫描通道总数：PA1~PA7 共 7 路，PC0~PC5 共 6 路，总计 13 路。 */
#define BSP_ADC_CHANNEL_COUNT              13u

/* DMA 循环缓存中保存 16 组完整扫描结果，10ms 任务对每一路做 16 点算术平均。 */
#define BSP_ADC_AVG_SAMPLES                16u

/* GD32F103 ADC 为 12 位，采样码范围为 0~4095，换算分母按 2^12=4096 处理。 */
#define BSP_ADC_CODE_COUNT                 4096.0f

/* ADC 参考电压。若实测 VDDA 与 3.3V 偏差较大，应修改为实测值或后续加入在线校准。 */
#define BSP_ADC_REFERENCE_VOLTAGE_V        3.3f

/*
 * 各电压通道的线性增益，单位为“实际电压/ADC 引脚电压”。
 * 当前取值由旧程序的满量程估算值反推，仅用于保持原有量程：
 * 实际电压 = ADC引脚电压 × 增益 + 偏置。
 * 硬件联调时应根据分压电阻实测值重新标定。
 */
#define BSP_ADC_VIN_V_GAIN                 (40.0f / BSP_ADC_REFERENCE_VOLTAGE_V)
#define BSP_ADC_12V_V_GAIN                 (15.0f / BSP_ADC_REFERENCE_VOLTAGE_V)
#define BSP_ADC_5V_V_GAIN                  (6.0f  / BSP_ADC_REFERENCE_VOLTAGE_V)
#define BSP_ADC_3V3_V_GAIN                 (4.0f  / BSP_ADC_REFERENCE_VOLTAGE_V)
#define BSP_ADC_M12V_V_GAIN                (15.0f / BSP_ADC_REFERENCE_VOLTAGE_V)
#define BSP_ADC_28V_V_GAIN                 (32.0f / BSP_ADC_REFERENCE_VOLTAGE_V)

/* 电压通道偏置，单位 V。当前硬件关系按经过原点处理，因此全部为 0。 */
#define BSP_ADC_VIN_V_OFFSET_V             0.0f
#define BSP_ADC_12V_V_OFFSET_V             0.0f
#define BSP_ADC_5V_V_OFFSET_V              0.0f
#define BSP_ADC_3V3_V_OFFSET_V             0.0f
#define BSP_ADC_M12V_V_OFFSET_V            0.0f
#define BSP_ADC_28V_V_OFFSET_V             0.0f

/*
 * 各电流通道的线性斜率 k，单位 A/V。
 * 换算公式：I = (raw × 3.3 / 4096) × k + b。
 * 当前 k 值由旧程序的满量程值反推，用于保持兼容；取得实测标定数据后，应逐路替换。
 */
#define BSP_ADC_VIN_I_GAIN_A_PER_V         (5.0f  / BSP_ADC_REFERENCE_VOLTAGE_V)
#define BSP_ADC_12V_I_GAIN_A_PER_V         (10.0f / BSP_ADC_REFERENCE_VOLTAGE_V)
#define BSP_ADC_5V_I_GAIN_A_PER_V          (3.0f  / BSP_ADC_REFERENCE_VOLTAGE_V)
#define BSP_ADC_3V3_I_GAIN_A_PER_V         (5.0f  / BSP_ADC_REFERENCE_VOLTAGE_V)
#define BSP_ADC_M12V_I_GAIN_A_PER_V        (0.5f  / BSP_ADC_REFERENCE_VOLTAGE_V)
#define BSP_ADC_28V_I_GAIN_A_PER_V         (0.5f  / BSP_ADC_REFERENCE_VOLTAGE_V)

/*
 * 各电流通道的截距 b，单位 A。
 * 这里暂时设置为 0，只是为了在没有实测标定数据时维持旧程序行为；
 * 实际硬件若零电流对应非零 ADC 电压，必须将对应通道修改为实测截距。
 */
#define BSP_ADC_VIN_I_OFFSET_A             0.0f
#define BSP_ADC_12V_I_OFFSET_A             0.0f
#define BSP_ADC_5V_I_OFFSET_A              0.0f
#define BSP_ADC_3V3_I_OFFSET_A             0.0f
#define BSP_ADC_M12V_I_OFFSET_A            0.0f
#define BSP_ADC_28V_I_OFFSET_A             0.0f

/**
 * @brief ADC 通道索引。
 * @note  枚举顺序必须与 ADC_RegularChannel_Config() 的规则序列以及 DMA 缓冲区排列完全一致。
 */
typedef enum {
    BSP_ADC_TEMP = 0,  /* PA1/ADC_CH1：温度检测。 */
    BSP_ADC_VIN_V,     /* PA2/ADC_CH2：输入电压。 */
    BSP_ADC_12V_V,     /* PA3/ADC_CH3：+12V 电压。 */
    BSP_ADC_5V_V,      /* PA4/ADC_CH4：+5V 电压。 */
    BSP_ADC_3V3_V,     /* PA5/ADC_CH5：+3.3V 电压。 */
    BSP_ADC_M12V_V,    /* PA6/ADC_CH6：-12V 电压。 */
    BSP_ADC_28V_V,     /* PA7/ADC_CH7：+28V 电压。 */
    BSP_ADC_VIN_I,     /* PC0/ADC_CH10：输入电流。 */
    BSP_ADC_12V_I,     /* PC1/ADC_CH11：+12V 电流。 */
    BSP_ADC_5V_I,      /* PC2/ADC_CH12：+5V 电流。 */
    BSP_ADC_3V3_I,     /* PC3/ADC_CH13：+3.3V 电流。 */
    BSP_ADC_M12V_I,    /* PC4/ADC_CH14：-12V 电流。 */
    BSP_ADC_28V_I      /* PC5/ADC_CH15：+28V 电流。 */
} bsp_adc_channel_t;

/**
 * @brief 一次线性标定参数。
 * @note  工程量 = ADC引脚电压 × gain + offset。
 */
typedef struct {
    float gain;        /* 斜率：电压通道单位 V/V，电流通道单位 A/V。 */
    float offset;      /* 截距：电压通道单位 V，电流通道单位 A。 */
} bsp_adc_linear_cal_t;

/**
 * @brief ADC 转换后的工程量集合。
 */
typedef struct {
    float temp_c;      /* 温度，单位 ℃。 */
    float vin_v;       /* 输入电压，单位 V。 */
    float v12_v;       /* +12V 电压，单位 V。 */
    float v5_v;        /* +5V 电压，单位 V。 */
    float v33_v;       /* +3.3V 电压，单位 V。 */
    float vm12_v;      /* -12V 电压，单位 V，正常值为负数。 */
    float v28_v;       /* +28V 电压，单位 V。 */
    float vin_i;       /* 输入电流，单位 A。 */
    float i12_a;       /* +12V 电流，单位 A。 */
    float i5_a;        /* +5V 电流，单位 A。 */
    float i33_a;       /* +3.3V 电流，单位 A。 */
    float im12_a;      /* -12V 电流，单位 A。 */
    float i28_a;       /* +28V 电流，单位 A。 */
} bsp_adc_values_t;

/**
 * @brief  初始化 ADC1、DMA1_CH1 和全部模拟输入 GPIO。
 * @param  无。
 * @retval 无。
 */
void bsp_adc_dma_init(void);

/**
 * @brief  10ms 周期采样处理任务。
 * @param  无。
 * @retval 无。
 * @note   函数对 DMA 缓冲区做 16 点平均，再根据各通道线性标定参数计算工程量。
 */
void bsp_adc_dma_task_10ms(void);

/**
 * @brief  获取最近一次计算完成的全部工程量。
 * @param  out 输出结构体指针，不允许为空。
 * @retval 无。
 */
void bsp_adc_get_values(bsp_adc_values_t *out);

/**
 * @brief  获取指定通道最近一次 16 点平均后的 ADC 原始采样码。
 * @param  ch ADC 通道枚举。
 * @retval 0~4095 的 12 位采样码；参数越界时返回 0。
 */
uint16_t bsp_adc_get_raw(bsp_adc_channel_t ch);

/**
 * @brief  设置指定电流通道的运行时标定参数。
 * @param  ch     只能是 BSP_ADC_VIN_I~BSP_ADC_28V_I 的电流通道。
 * @param  gain   斜率 k，单位 A/V。
 * @param  offset 截距 b，单位 A。
 * @retval true=设置成功，false=通道不是电流通道。
 * @note   当前设置只保存在 RAM 中，掉电后恢复为宏定义默认值。
 */
bool bsp_adc_set_current_calibration(bsp_adc_channel_t ch, float gain, float offset);

/**
 * @brief  读取指定电流通道当前使用的运行时标定参数。
 * @param  ch  只能是电流通道。
 * @param  cal 标定参数输出指针。
 * @retval true=读取成功，false=参数无效。
 */
bool bsp_adc_get_current_calibration(bsp_adc_channel_t ch, bsp_adc_linear_cal_t *cal);

#endif
