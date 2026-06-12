#include "sensor_manager.h"

#define UNIT_DEG_K       0x07u
#define UNIT_VOLTS       0x04u
#define UNIT_AMPS        0x05u
#define UNIT_WATTS       0x06u
#define ENTITY_POWER     0x0Au

static float g_vin_v = 28.0f;
static float g_v12_v = 12.0f;
static float g_v5_v = 5.0f;
static float g_v33_v = 3.3f;
static float g_vm12_v = -12.0f;
static float g_v28_v = 28.0f;
static float g_iin_a = 1.0f;
static float g_i12_a = 1.0f;
static float g_i33_a = 1.0f;
static float g_i5_a = 1.0f;
static float g_temp_c = 25.0f;

static ipmi_sensor_t g_sensors[SENSOR_COUNT];

static uint8_t clamp_u8(int v)
{
    if (v < 0) {
        return 0u;
    }
    if (v > 255) {
        return 255u;
    }
    return (uint8_t)v;
}

static int pow10_int(int8_t exp)
{
    int r = 1;
    int8_t i;
    if (exp <= 0) {
        return 1;
    }
    for (i = 0; i < exp; i++) {
        r *= 10;
    }
    return r;
}

uint8_t sensor_eng_to_raw(const ipmi_sensor_t *s, float value)
{
    float scale;
    float raw_f;
    int raw_i;

    if ((s == 0) || (s->m == 0)) {
        return 0u;
    }

    if (s->r_exp < 0) {
        scale = 1.0f / (float)pow10_int((int8_t)(-s->r_exp));
    } else {
        scale = (float)pow10_int(s->r_exp);
    }

    raw_f = (value / scale - (float)s->b) / (float)s->m;
    if (raw_f >= 0.0f) {
        raw_i = (int)(raw_f + 0.5f);
    } else {
        raw_i = (int)(raw_f - 0.5f);
    }

    if (s->kind == SENSOR_KIND_ANALOG_S8) {
        if (raw_i < -128) raw_i = -128;
        if (raw_i > 127) raw_i = 127;
        return (uint8_t)((int8_t)raw_i);
    }

    return clamp_u8(raw_i);
}

static void sensor_init_one(uint8_t idx, uint8_t id, const char *name, uint8_t type, uint8_t rtype,
                            sensor_kind_t kind, uint8_t unit, int16_t m, int16_t b, int8_t rexp)
{
    g_sensors[idx].sensor_id = id;
    g_sensors[idx].name = name;
    g_sensors[idx].sensor_type = type;
    g_sensors[idx].reading_type = rtype;
    g_sensors[idx].entity_id = ENTITY_POWER;
    g_sensors[idx].entity_instance = 1u;
    g_sensors[idx].base_unit = unit;
    g_sensors[idx].kind = kind;
    g_sensors[idx].m = m;
    g_sensors[idx].b = b;
    g_sensors[idx].r_exp = rexp;
    g_sensors[idx].b_exp = 0;
    g_sensors[idx].sensor_status = IPMI_SENSOR_STATUS_NORMAL;
    g_sensors[idx].threshold_mask = 0u;
    g_sensors[idx].lnr = 0u;
    g_sensors[idx].lc = 0u;
    g_sensors[idx].lnc = 0u;
    g_sensors[idx].unc = 0u;
    g_sensors[idx].uc = 0u;
    g_sensors[idx].unr = 0u;
}

static ipmi_sensor_t *sensor_get_mutable(uint8_t sensor_id)
{
    uint8_t i;
    for (i = 0u; i < SENSOR_COUNT; i++) {
        if (g_sensors[i].sensor_id == sensor_id) return &g_sensors[i];
    }
    return 0;
}

static void set_thresholds_eng(uint8_t sensor_id, uint8_t mask,
                               float lnr, float lc, float lnc, float unc, float uc, float unr)
{
    ipmi_sensor_t *s = sensor_get_mutable(sensor_id);
    if (s == 0) return;

    s->threshold_mask = mask;
    if (mask & SENSOR_THRESH_LNR) s->lnr = sensor_eng_to_raw(s, lnr);
    if (mask & SENSOR_THRESH_LC)  s->lc  = sensor_eng_to_raw(s, lc);
    if (mask & SENSOR_THRESH_LNC) s->lnc = sensor_eng_to_raw(s, lnc);
    if (mask & SENSOR_THRESH_UNC) s->unc = sensor_eng_to_raw(s, unc);
    if (mask & SENSOR_THRESH_UC)  s->uc  = sensor_eng_to_raw(s, uc);
    if (mask & SENSOR_THRESH_UNR) s->unr = sensor_eng_to_raw(s, unr);
}

static void sensor_thresholds_init(void)
{
    set_thresholds_eng(0x04u, SENSOR_THRESH_ALL, 9.0f, 10.0f, 12.0f, 36.0f, 38.0f, 40.0f);
    set_thresholds_eng(0x05u, SENSOR_THRESH_ALL, 9.6f, 10.2f, 10.8f, 13.2f, 13.8f, 14.4f);
    set_thresholds_eng(0x06u, SENSOR_THRESH_ALL, 4.0f, 4.25f, 4.5f, 5.5f, 5.75f, 6.0f);
    set_thresholds_eng(0x07u, SENSOR_THRESH_ALL, 2.64f, 2.80f, 2.97f, 3.63f, 3.80f, 3.96f);
    set_thresholds_eng(0x08u, SENSOR_THRESH_ALL, -9.6f, -10.2f, -10.8f, -13.2f, -13.8f, -14.4f);
    set_thresholds_eng(0x09u, SENSOR_THRESH_ALL, 22.4f, 23.8f, 25.2f, 30.8f, 32.0f, 33.6f);

    set_thresholds_eng(0x0Au, SENSOR_THRESH_UNC | SENSOR_THRESH_UC | SENSOR_THRESH_UNR,
                       0.0f, 0.0f, 0.0f, 4.0f, 5.0f, 6.0f);
    set_thresholds_eng(0x0Bu, SENSOR_THRESH_UNC | SENSOR_THRESH_UC | SENSOR_THRESH_UNR,
                       0.0f, 0.0f, 0.0f, 8.8f, 12.0f, 16.0f);
    set_thresholds_eng(0x0Cu, SENSOR_THRESH_UNC | SENSOR_THRESH_UC | SENSOR_THRESH_UNR,
                       0.0f, 0.0f, 0.0f, 4.4f, 6.0f, 8.0f);
    set_thresholds_eng(0x0Du, SENSOR_THRESH_UNC | SENSOR_THRESH_UC | SENSOR_THRESH_UNR,
                       0.0f, 0.0f, 0.0f, 2.2f, 3.0f, 4.0f);

    set_thresholds_eng(0x0Eu, SENSOR_THRESH_LNC | SENSOR_THRESH_UNC | SENSOR_THRESH_UC | SENSOR_THRESH_UNR,
                       0.0f, 0.0f, 218.0f, 358.0f, 373.0f, 383.0f);
    set_thresholds_eng(0x0Fu, SENSOR_THRESH_LNC | SENSOR_THRESH_UNC | SENSOR_THRESH_UC | SENSOR_THRESH_UNR,
                       0.0f, 0.0f, 218.0f, 358.0f, 373.0f, 383.0f);
    set_thresholds_eng(0x10u, SENSOR_THRESH_LNC | SENSOR_THRESH_UNC | SENSOR_THRESH_UC | SENSOR_THRESH_UNR,
                       0.0f, 0.0f, 218.0f, 358.0f, 373.0f, 383.0f);
}

void sensor_manager_init(void)
{
    sensor_init_one(0, 0x01u, "FRU Health", 0xF2u, 0x04u, SENSOR_KIND_DISCRETE, 0, 1, 0, 0);
    sensor_init_one(1, 0x02u, "FRU Voltage", 0x02u, 0x05u, SENSOR_KIND_DISCRETE, 0, 1, 0, 0);
    sensor_init_one(2, 0x03u, "FRU Temp", 0xF3u, 0x6Fu, SENSOR_KIND_DISCRETE, 0, 1, 0, 0);
    sensor_init_one(3, 0x04u, "Input Voltage", 0x02u, 0x01u, SENSOR_KIND_ANALOG_U8, UNIT_VOLTS, 2, 0, -1);
    sensor_init_one(4, 0x05u, "+12V Voltage", 0x02u, 0x01u, SENSOR_KIND_ANALOG_U8, UNIT_VOLTS, 1, 0, -1);
    sensor_init_one(5, 0x06u, "+5V Voltage", 0x02u, 0x01u, SENSOR_KIND_ANALOG_U8, UNIT_VOLTS, 2, 0, -2);
    sensor_init_one(6, 0x07u, "+3.3V Voltage", 0x02u, 0x01u, SENSOR_KIND_ANALOG_U8, UNIT_VOLTS, 2, 0, -2);
    sensor_init_one(7, 0x08u, "-12V Voltage", 0x02u, 0x01u, SENSOR_KIND_ANALOG_S8, UNIT_VOLTS, 1, 0, -1);
    sensor_init_one(8, 0x09u, "+28V Voltage", 0x02u, 0x01u, SENSOR_KIND_ANALOG_U8, UNIT_VOLTS, 2, 0, -1);
    sensor_init_one(9, 0x0Au, "Input Current", 0x03u, 0x01u, SENSOR_KIND_ANALOG_U8, UNIT_AMPS, 5, 0, -2);
    sensor_init_one(10, 0x0Bu, "+12V Current", 0x03u, 0x01u, SENSOR_KIND_ANALOG_U8, UNIT_AMPS, 5, 0, -2);
    sensor_init_one(11, 0x0Cu, "+3.3V Current", 0x03u, 0x01u, SENSOR_KIND_ANALOG_U8, UNIT_AMPS, 2, 0, -2);
    sensor_init_one(12, 0x0Du, "+5V Current", 0x03u, 0x01u, SENSOR_KIND_ANALOG_U8, UNIT_AMPS, 2, 0, -2);
    sensor_init_one(13, 0x0Eu, "Top Edge Temp", 0x01u, 0x01u, SENSOR_KIND_ANALOG_U8, UNIT_DEG_K, 1, 200, 0);
    sensor_init_one(14, 0x0Fu, "Heatsink Temp", 0x01u, 0x01u, SENSOR_KIND_ANALOG_U8, UNIT_DEG_K, 1, 200, 0);
    sensor_init_one(15, 0x10u, "Center Temp", 0x01u, 0x01u, SENSOR_KIND_ANALOG_U8, UNIT_DEG_K, 1, 200, 0);
    sensor_init_one(16, 0x11u, "Input Power", 0x0Bu, 0x01u, SENSOR_KIND_ANALOG_U8, UNIT_WATTS, 1, 0, 0);
    sensor_init_one(17, 0x12u, "+12V Power", 0x0Bu, 0x01u, SENSOR_KIND_ANALOG_U8, UNIT_WATTS, 1, 0, 0);
    sensor_init_one(18, 0x13u, "+3.3V Power", 0x0Bu, 0x01u, SENSOR_KIND_ANALOG_U8, UNIT_WATTS, 1, 0, 0);
    sensor_init_one(19, 0x14u, "+5V Power", 0x0Bu, 0x01u, SENSOR_KIND_ANALOG_U8, UNIT_WATTS, 1, 0, 0);

    sensor_thresholds_init();
    sensor_manager_task_100ms();
}

void sensor_set_adc_values(float vin_v, float v12, float v5, float v33, float vm12, float v28,
                           float iin, float i12, float i33, float i5, float temp_c)
{
    g_vin_v = vin_v;
    g_v12_v = v12;
    g_v5_v = v5;
    g_v33_v = v33;
    g_vm12_v = vm12;
    g_v28_v = v28;
    g_iin_a = iin;
    g_i12_a = i12;
    g_i33_a = i33;
    g_i5_a = i5;
    g_temp_c = temp_c;
}

uint16_t sensor_calc_event(const ipmi_sensor_t *s)
{
    uint16_t evt = 0u;
    int raw;

    if (s == 0) return 0u;
    if (s->kind == SENSOR_KIND_DISCRETE) return s->event_status;

    raw = (s->kind == SENSOR_KIND_ANALOG_S8) ? (int)((int8_t)s->raw_value) : (int)s->raw_value;

    if (s->sensor_id == 0x08u) {
        /* -12V signed raw: less negative means undervoltage, more negative means overvoltage. */
        if ((s->threshold_mask & SENSOR_THRESH_LNC) && (raw >= (int)((int8_t)s->lnc))) evt |= IPMI_EVT_LNC_ASSERT;
        if ((s->threshold_mask & SENSOR_THRESH_LC)  && (raw >= (int)((int8_t)s->lc)))  evt |= IPMI_EVT_LC_ASSERT;
        if ((s->threshold_mask & SENSOR_THRESH_LNR) && (raw >= (int)((int8_t)s->lnr))) evt |= IPMI_EVT_LNR_ASSERT;
        if ((s->threshold_mask & SENSOR_THRESH_UNC) && (raw <= (int)((int8_t)s->unc))) evt |= IPMI_EVT_UNC_ASSERT;
        if ((s->threshold_mask & SENSOR_THRESH_UC)  && (raw <= (int)((int8_t)s->uc)))  evt |= IPMI_EVT_UC_ASSERT;
        if ((s->threshold_mask & SENSOR_THRESH_UNR) && (raw <= (int)((int8_t)s->unr))) evt |= IPMI_EVT_UNR_ASSERT;
        return evt;
    }

    if ((s->threshold_mask & SENSOR_THRESH_LNC) && (raw <= (int)s->lnc)) evt |= IPMI_EVT_LNC_ASSERT;
    if ((s->threshold_mask & SENSOR_THRESH_LC)  && (raw <= (int)s->lc))  evt |= IPMI_EVT_LC_ASSERT;
    if ((s->threshold_mask & SENSOR_THRESH_LNR) && (raw <= (int)s->lnr)) evt |= IPMI_EVT_LNR_ASSERT;
    if ((s->threshold_mask & SENSOR_THRESH_UNC) && (raw >= (int)s->unc)) evt |= IPMI_EVT_UNC_ASSERT;
    if ((s->threshold_mask & SENSOR_THRESH_UC)  && (raw >= (int)s->uc))  evt |= IPMI_EVT_UC_ASSERT;
    if ((s->threshold_mask & SENSOR_THRESH_UNR) && (raw >= (int)s->unr)) evt |= IPMI_EVT_UNR_ASSERT;
    return evt;
}

void sensor_manager_task_100ms(void)
{
    float temp_k = g_temp_c + 273.15f;

    g_sensors[3].eng_value = g_vin_v;
    g_sensors[4].eng_value = g_v12_v;
    g_sensors[5].eng_value = g_v5_v;
    g_sensors[6].eng_value = g_v33_v;
    g_sensors[7].eng_value = g_vm12_v;
    g_sensors[8].eng_value = g_v28_v;
    g_sensors[9].eng_value = g_iin_a;
    g_sensors[10].eng_value = g_i12_a;
    g_sensors[11].eng_value = g_i33_a;
    g_sensors[12].eng_value = g_i5_a;
    g_sensors[13].eng_value = temp_k;
    g_sensors[14].eng_value = temp_k;
    g_sensors[15].eng_value = temp_k;
    g_sensors[16].eng_value = g_vin_v * g_iin_a;
    g_sensors[17].eng_value = g_v12_v * g_i12_a;
    g_sensors[18].eng_value = g_v33_v * g_i33_a;
    g_sensors[19].eng_value = g_v5_v * g_i5_a;

    g_sensors[0].event_status = sensor_get_fru_health_bits();
    g_sensors[1].event_status = sensor_get_fru_voltage_bits();
    g_sensors[2].event_status = sensor_get_fru_temperature_bits();
    g_sensors[0].raw_value = (uint8_t)g_sensors[0].event_status;
    g_sensors[1].raw_value = (uint8_t)g_sensors[1].event_status;
    g_sensors[2].raw_value = (uint8_t)g_sensors[2].event_status;

    for (uint8_t i = 3u; i < SENSOR_COUNT; i++) {
        g_sensors[i].raw_value = sensor_eng_to_raw(&g_sensors[i], g_sensors[i].eng_value);
        g_sensors[i].event_status = sensor_calc_event(&g_sensors[i]);
    }
}

const ipmi_sensor_t *sensor_get(uint8_t sensor_id)
{
    uint8_t i;
    for (i = 0u; i < SENSOR_COUNT; i++) {
        if (g_sensors[i].sensor_id == sensor_id) return &g_sensors[i];
    }
    return 0;
}

uint8_t sensor_get_count(void)
{
    return SENSOR_COUNT;
}

uint16_t sensor_get_fru_voltage_bits(void)
{
    uint16_t bits = 0x0001u;
    if ((g_vin_v < 12.0f) || (g_vin_v > 36.0f)) bits |= 0x0006u;
    if ((g_v12_v < 10.8f) || (g_v12_v > 13.2f) || (g_v5_v < 4.5f) || (g_v5_v > 5.5f) ||
        (g_v33_v < 2.97f) || (g_v33_v > 3.63f) || (g_v28_v < 25.2f) || (g_v28_v > 30.8f) ||
        (g_vm12_v > -10.8f) || (g_vm12_v < -13.2f)) {
        bits |= 0x000Au;
        bits &= (uint16_t)~0x0001u;
    }
    return bits;
}

uint16_t sensor_get_fru_temperature_bits(void)
{
    uint16_t bits = 0x0001u;
    if (g_temp_c > 85.0f) bits |= 0x0002u;
    if (g_temp_c > 100.0f) bits |= 0x0004u;
    if (bits != 0x0001u) bits &= (uint16_t)~0x0001u;
    return bits;
}

uint16_t sensor_get_fru_health_bits(void)
{
    uint16_t bits = 0u;
    if (sensor_get_fru_voltage_bits() != 0x0001u) bits |= 0x0008u;
    if (sensor_get_fru_temperature_bits() & 0x0006u) bits |= 0x0001u;
    if (g_temp_c > 100.0f) bits |= 0x0002u;
    if ((g_vin_v < 12.0f) || (g_vin_v > 36.0f)) bits |= 0x0004u;
    return bits;
}
