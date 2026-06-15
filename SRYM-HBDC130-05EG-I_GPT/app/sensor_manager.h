/**
 * @file    sensor_manager.h
 * @brief   IPMI 传感器数据库、工程量编码和阈值事件接口。
 *          本文件定义传感器描述结构、阈值掩码、读数类型以及供 IPMI、SDR、FRU 和故障
 *          管理模块调用的统一查询接口。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "app_config.h"
#include <stdint.h>
#include <stdbool.h>

/* 下不可恢复阈值有效。 */
#define SENSOR_THRESH_LNR   0x01u

/* 下严重阈值有效。 */
#define SENSOR_THRESH_LC    0x02u

/* 下非严重阈值有效。 */
#define SENSOR_THRESH_LNC   0x04u

/* 上非严重阈值有效。 */
#define SENSOR_THRESH_UNC   0x08u

/* 上严重阈值有效。 */
#define SENSOR_THRESH_UC    0x10u

/* 上不可恢复阈值有效。 */
#define SENSOR_THRESH_UNR   0x20u

/* 六级阈值全部有效。 */
#define SENSOR_THRESH_ALL   0x3Fu

/**
 * @brief 传感器读数表示方式。
 */
typedef enum {
    SENSOR_KIND_DISCRETE = 0, /* 离散状态传感器，读数和事件位表示状态集合。 */
    SENSOR_KIND_ANALOG_U8,    /* 无符号 8 位模拟量，范围 0~255。 */
    SENSOR_KIND_ANALOG_S8     /* 有符号 8 位模拟量，范围 -128~127，当前用于 -12V。 */
} sensor_kind_t;

/**
 * @brief 单个 IPMI 传感器的静态描述和运行时状态。
 */
typedef struct {
    uint8_t sensor_id;          /* IPMI 传感器编号。 */
    const char *name;           /* SDR 中显示的传感器名称字符串。 */
    uint8_t sensor_type;        /* IPMI 传感器类型编码。 */
    uint8_t reading_type;       /* IPMI 事件/读数类型编码。 */
    uint8_t entity_id;          /* 所属实体编号。 */
    uint8_t entity_instance;    /* 所属实体实例号。 */
    uint8_t base_unit;          /* IPMI 基本单位编码。 */
    sensor_kind_t kind;         /* 离散、无符号模拟量或有符号模拟量。 */
    int16_t m;                  /* SDR 线性化 M 系数。 */
    int16_t b;                  /* SDR 线性化 B 系数。 */
    int8_t r_exp;               /* SDR 结果十进制指数。 */
    int8_t b_exp;               /* SDR B 系数十进制指数。 */
    float eng_value;            /* 最近一次工程量，单位由 base_unit 决定。 */
    uint8_t raw_value;          /* Get Sensor Reading 返回的 8 位读数。 */
    uint8_t sensor_status;      /* 传感器扫描和事件状态字节。 */
    uint16_t event_status;      /* 当前已经断言的事件位集合。 */
    uint8_t threshold_mask;     /* 六级阈值中哪些字段有效。 */
    uint8_t lnr;                /* 下不可恢复阈值的 8 位编码。 */
    uint8_t lc;                 /* 下严重阈值的 8 位编码。 */
    uint8_t lnc;                /* 下非严重阈值的 8 位编码。 */
    uint8_t unc;                /* 上非严重阈值的 8 位编码。 */
    uint8_t uc;                 /* 上严重阈值的 8 位编码。 */
    uint8_t unr;                /* 上不可恢复阈值的 8 位编码。 */
} ipmi_sensor_t;

/**
 * @brief  初始化 20 个传感器描述、默认工程量和阈值表。
 * @param  无。
 * @retval 无。
 */
void sensor_manager_init(void);

/**
 * @brief  每 100ms 将工程量转换成 8 位读数并重新计算事件状态。
 * @param  无。
 * @retval 无。
 */
void sensor_manager_task_100ms(void);

/**
 * @brief  按 IPMI Sensor Number 查询传感器。
 * @param  sensor_id 传感器编号。
 * @retval 找到时返回只读指针，未找到返回 0。
 */
const ipmi_sensor_t *sensor_get(uint8_t sensor_id);

/**
 * @brief  获取传感器总数。
 * @param  无。
 * @retval 传感器数量。
 */
uint8_t sensor_get_count(void);

/**
 * @brief  按 SDR 线性参数将工程量转换为 IPMI 8 位读数。
 * @param  sensor 传感器描述指针。
 * @param  value  待编码工程量。
 * @retval 8 位读数。
 */
uint8_t sensor_eng_to_raw(const ipmi_sensor_t *sensor, float value);

/**
 * @brief  根据当前读数、阈值和阈值掩码计算事件位。
 * @param  sensor 传感器描述指针。
 * @retval 事件断言位集合。
 */
uint16_t sensor_calc_event(const ipmi_sensor_t *sensor);

/**
 * @brief  更新 ADC 板级驱动提供的工程量快照。
 * @param  vin_v  输入电压，单位 V。
 * @param  v12    +12V 电压，单位 V。
 * @param  v5     +5V 电压，单位 V。
 * @param  v33    +3.3V 电压，单位 V。
 * @param  vm12   -12V 电压，单位 V。
 * @param  v28    +28V 电压，单位 V。
 * @param  iin    输入电流，单位 A。
 * @param  i12    +12V 电流，单位 A。
 * @param  i33    +3.3V 电流，单位 A。
 * @param  i5     +5V 电流，单位 A。
 * @param  temp_c 温度，单位 ℃。
 * @retval 无。
 */
void sensor_set_adc_values(float vin_v,
                           float v12,
                           float v5,
                           float v33,
                           float vm12,
                           float v28,
                           float iin,
                           float i12,
                           float i33,
                           float i5,
                           float temp_c);

/**
 * @brief  获取 FRU Health 综合离散状态位。
 * @param  无。
 * @retval 健康状态位集合。
 */
uint16_t sensor_get_fru_health_bits(void);

/**
 * @brief  获取输入和输出电压离散状态位。
 * @param  无。
 * @retval 电压状态位集合。
 */
uint16_t sensor_get_fru_voltage_bits(void);

/**
 * @brief  获取温度离散状态位。
 * @param  无。
 * @retval 温度状态位集合。
 */
uint16_t sensor_get_fru_temperature_bits(void);

#endif
