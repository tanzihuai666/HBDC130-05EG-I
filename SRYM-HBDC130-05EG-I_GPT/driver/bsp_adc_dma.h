#ifndef BSP_ADC_DMA_H
#define BSP_ADC_DMA_H

#include "gd32f10x.h"
#include <stdint.h>

#define BSP_ADC_CHANNEL_COUNT      13u
#define BSP_ADC_AVG_SAMPLES        16u

typedef enum {
    BSP_ADC_TEMP = 0,
    BSP_ADC_VIN_V,
    BSP_ADC_12V_V,
    BSP_ADC_5V_V,
    BSP_ADC_3V3_V,
    BSP_ADC_M12V_V,
    BSP_ADC_28V_V,
    BSP_ADC_VIN_I,
    BSP_ADC_12V_I,
    BSP_ADC_5V_I,
    BSP_ADC_3V3_I,
    BSP_ADC_M12V_I,
    BSP_ADC_28V_I
} bsp_adc_channel_t;

typedef struct {
    float temp_c;
    float vin_v;
    float v12_v;
    float v5_v;
    float v33_v;
    float vm12_v;
    float v28_v;
    float vin_i;
    float i12_a;
    float i5_a;
    float i33_a;
    float im12_a;
    float i28_a;
} bsp_adc_values_t;

void bsp_adc_dma_init(void);
void bsp_adc_dma_task_10ms(void);
void bsp_adc_get_values(bsp_adc_values_t *out);
uint16_t bsp_adc_get_raw(bsp_adc_channel_t ch);

#endif
