#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "app_config.h"
#include <stdint.h>
#include <stdbool.h>

#define SENSOR_THRESH_LNR   0x01u
#define SENSOR_THRESH_LC    0x02u
#define SENSOR_THRESH_LNC   0x04u
#define SENSOR_THRESH_UNC   0x08u
#define SENSOR_THRESH_UC    0x10u
#define SENSOR_THRESH_UNR   0x20u
#define SENSOR_THRESH_ALL   0x3Fu

typedef enum {
    SENSOR_KIND_DISCRETE = 0,
    SENSOR_KIND_ANALOG_U8,
    SENSOR_KIND_ANALOG_S8
} sensor_kind_t;

typedef struct {
    uint8_t sensor_id;
    const char *name;
    uint8_t sensor_type;
    uint8_t reading_type;
    uint8_t entity_id;
    uint8_t entity_instance;
    uint8_t base_unit;
    sensor_kind_t kind;
    int16_t m;
    int16_t b;
    int8_t r_exp;
    int8_t b_exp;
    float eng_value;
    uint8_t raw_value;
    uint8_t sensor_status;
    uint16_t event_status;
    uint8_t threshold_mask;
    uint8_t lnr;
    uint8_t lc;
    uint8_t lnc;
    uint8_t unc;
    uint8_t uc;
    uint8_t unr;
} ipmi_sensor_t;

void sensor_manager_init(void);
void sensor_manager_task_100ms(void);
const ipmi_sensor_t *sensor_get(uint8_t sensor_id);
uint8_t sensor_get_count(void);
uint8_t sensor_eng_to_raw(const ipmi_sensor_t *s, float value);
uint16_t sensor_calc_event(const ipmi_sensor_t *s);
void sensor_set_adc_values(float vin_v, float v12, float v5, float v33, float vm12, float v28,
                           float iin, float i12, float i33, float i5, float temp_c);
uint16_t sensor_get_fru_health_bits(void);
uint16_t sensor_get_fru_voltage_bits(void);
uint16_t sensor_get_fru_temperature_bits(void);

#endif
