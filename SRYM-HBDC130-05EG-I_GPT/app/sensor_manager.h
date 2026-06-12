/**
 * @file    sensor_manager.h
 * @brief   IPMI 传感器数据库与工程量转换接口。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "app_config.h"
#include <stdint.h>
#include <stdbool.h>

/* 阈值可读/可比较掩码，顺序与 Get Sensor Thresholds 响应保持一致。 */
#define SENSOR_THRESH_LNR   0x01u  /* Lower Non-Recoverable 阈值有效。 */
#define SENSOR_THRESH_LC    0x02u  /* Lower Critical 阈值有效。 */
#define SENSOR_THRESH_LNC   0x04u  /* Lower Non-Critical 阈值有效。 */
#define SENSOR_THRESH_UNC   0x08u  /* Upper Non-Critical 阈值有效。 */
#define SENSOR_THRESH_UC    0x10u  /* Upper Critical 阈值有效。 */
#define SENSOR_THRESH_UNR   0x20u  /* Upper Non-Recoverable 阈值有效。 */
#define SENSOR_THRESH_ALL   0x3Fu  /* 六个阈值全部有效。 */

/** @brief 传感器读数类型。 */
typedef enum {
    SENSOR_KIND_DISCRETE = 0, /* 离散传感器，raw/event 表示状态位。 */
    SENSOR_KIND_ANALOG_U8,    /* 无符号 8-bit 模拟量。 */
    SENSOR_KIND_ANALOG_S8     /* 有符号 8-bit 模拟量，当前用于 -12V。 */
} sensor_kind_t;

/** @brief IPMI 传感器描述与运行时状态。 */
typedef struct {
    uint8_t sensor_id;          /* IPMI Sensor Number。 */
    const char *name;           /* SDR 中显示的传感器名称。 */
    uint8_t sensor_type;        /* IPMI Sensor Type。 */
    uint8_t reading_type;       /* IPMI Event/Reading Type。 */
    uint8_t entity_id;          /* 实体 ID。 */
    uint8_t entity_instance;    /* 实体实例。 */
    uint8_t base_unit;          /* IPMI 基本单位。 */
    sensor_kind_t kind;         /* 读数类型。 */
    int16_t m;                  /* SDR 线性化 M 系数。 */
    int16_t b;                  /* SDR 线性化 B 系数。 */
    int8_t r_exp;               /* SDR Rexp。 */
    int8_t b_exp;               /* SDR Bexp。 */
    float eng_value;            /* 工程量缓存。 */
    uint8_t raw_value;          /* IPMI raw 读数。 */
    uint8_t sensor_status;      /* IPMI 传感器状态字节。 */
    uint16_t event_status;      /* 当前断言事件位。 */
    uint8_t threshold_mask;     /* 哪些阈值字段有效。 */
    uint8_t lnr;                /* LNR raw 阈值。 */
    uint8_t lc;                 /* LC raw 阈值。 */
    uint8_t lnc;                /* LNC raw 阈值。 */
    uint8_t unc;                /* UNC raw 阈值。 */
    uint8_t uc;                 /* UC raw 阈值。 */
    uint8_t unr;                /* UNR raw 阈值。 */
} ipmi_sensor_t;

/** @brief 初始化 20 个传感器和阈值表。@param 无。@retval 无。 */
void sensor_manager_init(void);

/** @brief 100ms 刷新传感器 raw/event 状态。@param 无。@retval 无。 */
void sensor_manager_task_100ms(void);

/** @brief 按 Sensor ID 查询传感器。@param sensor_id IPMI Sensor Number。@retval 传感器指针或空。 */
const ipmi_sensor_t *sensor_get(uint8_t sensor_id);

/** @brief 获取传感器数量。@param 无。@retval 传感器数量。 */
uint8_t sensor_get_count(void);

/** @brief 工程量转换为 IPMI raw。@param s 传感器。@param value 工程量。@retval raw 值。 */
uint8_t sensor_eng_to_raw(const ipmi_sensor_t *s, float value);

/** @brief 根据 raw 和阈值计算事件位。@param s 传感器。@retval 事件位。 */
uint16_t sensor_calc_event(const ipmi_sensor_t *s);

/** @brief 写入 ADC 工程量快照。@param vin_v 输入电压。@param v12 +12V。@param v5 +5V。@param v33 +3.3V。@param vm12 -12V。@param v28 +28V。@param iin 输入电流。@param i12 +12V 电流。@param i33 +3.3V 电流。@param i5 +5V 电流。@param temp_c 温度。@retval 无。 */
void sensor_set_adc_values(float vin_v, float v12, float v5, float v33, float vm12, float v28,
                           float iin, float i12, float i33, float i5, float temp_c);

/** @brief 获取 FRU Health 离散状态位。@param 无。@retval 状态位。 */
uint16_t sensor_get_fru_health_bits(void);

/** @brief 获取电压类离散状态位。@param 无。@retval 状态位。 */
uint16_t sensor_get_fru_voltage_bits(void);

/** @brief 获取温度类离散状态位。@param 无。@retval 状态位。 */
uint16_t sensor_get_fru_temperature_bits(void);

#endif
