/**
 * @file    bsp_adc_dma.h
 * @brief   ADC1 + DMA 采样驱动接口。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#ifndef BSP_ADC_DMA_H
#define BSP_ADC_DMA_H

#include "gd32f10x.h"
#include <stdint.h>

/* ADC 通道数量：PA1~PA7 + PC0~PC5，共 13 路。 */
#define BSP_ADC_CHANNEL_COUNT      13u
/* 均值滤波窗口：DMA 循环缓存 16 组采样。 */
#define BSP_ADC_AVG_SAMPLES        16u

/** @brief ADC 通道索引，与 DMA 扫描顺序一一对应。 */
typedef enum {
    BSP_ADC_TEMP = 0,  /* 温度检测。 */
    BSP_ADC_VIN_V,    /* 输入电压。 */
    BSP_ADC_12V_V,    /* +12V 电压。 */
    BSP_ADC_5V_V,     /* +5V 电压。 */
    BSP_ADC_3V3_V,    /* +3.3V 电压。 */
    BSP_ADC_M12V_V,   /* -12V 电压。 */
    BSP_ADC_28V_V,    /* +28V 电压。 */
    BSP_ADC_VIN_I,    /* 输入电流。 */
    BSP_ADC_12V_I,    /* +12V 电流。 */
    BSP_ADC_5V_I,     /* +5V 电流。 */
    BSP_ADC_3V3_I,    /* +3.3V 电流。 */
    BSP_ADC_M12V_I,   /* -12V 电流。 */
    BSP_ADC_28V_I     /* +28V 电流。 */
} bsp_adc_channel_t;

/** @brief ADC 转换后的工程量集合。 */
typedef struct {
    float temp_c;  /* 温度，摄氏度。 */
    float vin_v;   /* 输入电压，V。 */
    float v12_v;   /* +12V 电压，V。 */
    float v5_v;    /* +5V 电压，V。 */
    float v33_v;   /* +3.3V 电压，V。 */
    float vm12_v;  /* -12V 电压，V。 */
    float v28_v;   /* +28V 电压，V。 */
    float vin_i;   /* 输入电流，A。 */
    float i12_a;   /* +12V 电流，A。 */
    float i5_a;    /* +5V 电流，A。 */
    float i33_a;   /* +3.3V 电流，A。 */
    float im12_a;  /* -12V 电流，A。 */
    float i28_a;   /* +28V 电流，A。 */
} bsp_adc_values_t;

/** @brief 初始化 ADC1、DMA1_CH1 和模拟输入 GPIO。@param 无。@retval 无。 */
void bsp_adc_dma_init(void);

/** @brief 10ms 周期任务，计算 16 点均值并转换工程量。@param 无。@retval 无。 */
void bsp_adc_dma_task_10ms(void);

/** @brief 获取最新工程量。@param out 输出结构体指针。@retval 无。 */
void bsp_adc_get_values(bsp_adc_values_t *out);

/** @brief 获取指定通道 raw 均值。@param ch ADC 通道枚举。@retval 12-bit raw 值。 */
uint16_t bsp_adc_get_raw(bsp_adc_channel_t ch);

#endif
