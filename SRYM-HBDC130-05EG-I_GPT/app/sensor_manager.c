/**
 * @file    sensor_manager.c
 * @brief   IPMI 传感器数据库、工程量到 8 位读数的换算以及阈值事件计算实现。
 *          本模块维护 20 个传感器，其中 3 个为离散状态传感器，13 个为电压、电流、
 *          温度和功率模拟量传感器，其余数据由已有工程量组合得到。ADC 板级驱动先把
 *          12 位采样码转换成实际工程量，本模块再根据 SDR 中的 M、B、Rexp 参数把工程量
 *          编码成 IPMI Get Sensor Reading 所使用的 8 位读数。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#include "sensor_manager.h"

/* IPMI SDR 基本单位编码：开尔文。 */
#define UNIT_DEG_K       0x07u

/* IPMI SDR 基本单位编码：伏特。 */
#define UNIT_VOLTS       0x04u

/* IPMI SDR 基本单位编码：安培。 */
#define UNIT_AMPS        0x05u

/* IPMI SDR 基本单位编码：瓦特。 */
#define UNIT_WATTS       0x06u

/* IPMI 实体编号 0Ah：电源模块。 */
#define ENTITY_POWER     0x0Au

/*
 * 最近一次由 ADC 板级驱动提供的工程量快照。
 * 主循环每 10ms 调用 sensor_set_adc_values() 更新，100ms 任务再统一生成 IPMI 读数。
 */
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

/* 固定长度的 IPMI 传感器数据库，索引 0~19 对应 Sensor ID 01h~14h。 */
static ipmi_sensor_t g_sensors[SENSOR_COUNT];

/**
 * @brief  将有符号整数限制到无符号 8 位范围。
 * @param  value 待限制整数。
 * @retval 小于 0 返回 0，大于 255 返回 255，其余直接转换。
 */
static uint8_t sensor_clamp_u8(int value)
{
    if (value < 0) {
        return 0u;
    }

    if (value > 255) {
        return 255u;
    }

    return (uint8_t)value;
}

/**
 * @brief  计算 10 的非负整数次幂。
 * @param  exponent 指数。
 * @retval exponent<=0 时返回 1，否则返回 10^exponent。
 */
static int sensor_pow10_int(int8_t exponent)
{
    int result;
    int8_t index;

    result = 1;

    if (exponent <= 0) {
        return result;
    }

    for (index = 0; index < exponent; index++) {
        result *= 10;
    }

    return result;
}

/**
 * @brief  按 IPMI SDR 线性公式将工程量转换为 8 位读数。
 * @param  sensor 传感器描述结构。
 * @param  value  待编码工程量。
 * @retval 编码后的 8 位读数。
 *
 * @note   本项目使用的简化线性关系为：
 *         工程量 = (M × 读数 + B) × 10^Rexp。
 *         因此反算读数 = (工程量 / 10^Rexp - B) / M。
 */
uint8_t sensor_eng_to_raw(const ipmi_sensor_t *sensor, float value)
{
    float result_scale;
    float raw_float;
    int raw_integer;

    if ((sensor == 0) || (sensor->m == 0)) {
        return 0u;
    }

    /* 将 Rexp 转换为实际十进制比例。 */
    if (sensor->r_exp < 0) {
        result_scale = 1.0f /
                       (float)sensor_pow10_int((int8_t)(-sensor->r_exp));
    } else {
        result_scale = (float)sensor_pow10_int(sensor->r_exp);
    }

    raw_float = (value / result_scale - (float)sensor->b) /
                (float)sensor->m;

    /* 正数加 0.5、负数减 0.5，实现接近四舍五入的整数转换。 */
    if (raw_float >= 0.0f) {
        raw_integer = (int)(raw_float + 0.5f);
    } else {
        raw_integer = (int)(raw_float - 0.5f);
    }

    if (sensor->kind == SENSOR_KIND_ANALOG_S8) {
        /* 有符号模拟量限制到 -128~127，再按二进制补码返回。 */
        if (raw_integer < -128) {
            raw_integer = -128;
        }
        if (raw_integer > 127) {
            raw_integer = 127;
        }

        return (uint8_t)((int8_t)raw_integer);
    }

    return sensor_clamp_u8(raw_integer);
}

/**
 * @brief  初始化一条传感器描述。
 * @param  index        g_sensors[] 数组下标。
 * @param  sensor_id    IPMI Sensor Number。
 * @param  name         SDR 中显示的传感器名称。
 * @param  sensor_type  IPMI Sensor Type。
 * @param  reading_type IPMI Event/Reading Type。
 * @param  kind         离散、无符号模拟量或有符号模拟量。
 * @param  unit         IPMI 基本单位编码。
 * @param  m            SDR 线性化 M 系数。
 * @param  b            SDR 线性化 B 系数。
 * @param  r_exp        SDR 结果指数。
 * @retval 无。
 */
static void sensor_init_one(uint8_t index,
                            uint8_t sensor_id,
                            const char *name,
                            uint8_t sensor_type,
                            uint8_t reading_type,
                            sensor_kind_t kind,
                            uint8_t unit,
                            int16_t m,
                            int16_t b,
                            int8_t r_exp)
{
    g_sensors[index].sensor_id = sensor_id;
    g_sensors[index].name = name;
    g_sensors[index].sensor_type = sensor_type;
    g_sensors[index].reading_type = reading_type;
    g_sensors[index].entity_id = ENTITY_POWER;
    g_sensors[index].entity_instance = 1u;
    g_sensors[index].base_unit = unit;
    g_sensors[index].kind = kind;
    g_sensors[index].m = m;
    g_sensors[index].b = b;
    g_sensors[index].r_exp = r_exp;
    g_sensors[index].b_exp = 0;
    g_sensors[index].sensor_status = IPMI_SENSOR_STATUS_NORMAL;
    g_sensors[index].event_status = 0u;
    g_sensors[index].threshold_mask = 0u;
    g_sensors[index].lnr = 0u;
    g_sensors[index].lc = 0u;
    g_sensors[index].lnc = 0u;
    g_sensors[index].unc = 0u;
    g_sensors[index].uc = 0u;
    g_sensors[index].unr = 0u;
}

/**
 * @brief  按 Sensor ID 查找可修改的传感器对象。
 * @param  sensor_id IPMI Sensor Number。
 * @retval 找到时返回对象指针，未找到返回 0。
 */
static ipmi_sensor_t *sensor_get_mutable(uint8_t sensor_id)
{
    uint8_t index;

    for (index = 0u; index < SENSOR_COUNT; index++) {
        if (g_sensors[index].sensor_id == sensor_id) {
            return &g_sensors[index];
        }
    }

    return 0;
}

/**
 * @brief  使用工程量设置一组传感器阈值，并立即转换成 8 位阈值读数。
 * @param  sensor_id 传感器编号。
 * @param  mask      哪些阈值有效。
 * @param  lnr       下不可恢复阈值。
 * @param  lc        下严重阈值。
 * @param  lnc       下非严重阈值。
 * @param  unc       上非严重阈值。
 * @param  uc        上严重阈值。
 * @param  unr       上不可恢复阈值。
 * @retval 无。
 */
static void sensor_set_thresholds_eng(uint8_t sensor_id,
                                      uint8_t mask,
                                      float lnr,
                                      float lc,
                                      float lnc,
                                      float unc,
                                      float uc,
                                      float unr)
{
    ipmi_sensor_t *sensor;

    sensor = sensor_get_mutable(sensor_id);
    if (sensor == 0) {
        APP_LOGE("Sensor threshold setup failed: id=0x%02X",
                 (unsigned int)sensor_id);
        return;
    }

    /* mask 用来区分“阈值数值为 0”和“该阈值未配置”两种情况。 */
    sensor->threshold_mask = mask;

    if ((mask & SENSOR_THRESH_LNR) != 0u) {
        sensor->lnr = sensor_eng_to_raw(sensor, lnr);
    }
    if ((mask & SENSOR_THRESH_LC) != 0u) {
        sensor->lc = sensor_eng_to_raw(sensor, lc);
    }
    if ((mask & SENSOR_THRESH_LNC) != 0u) {
        sensor->lnc = sensor_eng_to_raw(sensor, lnc);
    }
    if ((mask & SENSOR_THRESH_UNC) != 0u) {
        sensor->unc = sensor_eng_to_raw(sensor, unc);
    }
    if ((mask & SENSOR_THRESH_UC) != 0u) {
        sensor->uc = sensor_eng_to_raw(sensor, uc);
    }
    if ((mask & SENSOR_THRESH_UNR) != 0u) {
        sensor->unr = sensor_eng_to_raw(sensor, unr);
    }
}

/**
 * @brief  初始化所有模拟量传感器阈值。
 * @param  无。
 * @retval 无。
 */
static void sensor_thresholds_init(void)
{
    /* 输入电压和各路输出电压配置完整六级阈值。 */
    sensor_set_thresholds_eng(0x04u, SENSOR_THRESH_ALL,
                              9.0f, 10.0f, 12.0f, 36.0f, 38.0f, 40.0f);
    sensor_set_thresholds_eng(0x05u, SENSOR_THRESH_ALL,
                              9.6f, 10.2f, 10.8f, 13.2f, 13.8f, 14.4f);
    sensor_set_thresholds_eng(0x06u, SENSOR_THRESH_ALL,
                              4.0f, 4.25f, 4.5f, 5.5f, 5.75f, 6.0f);
    sensor_set_thresholds_eng(0x07u, SENSOR_THRESH_ALL,
                              2.64f, 2.80f, 2.97f, 3.63f, 3.80f, 3.96f);
    sensor_set_thresholds_eng(0x08u, SENSOR_THRESH_ALL,
                              -9.6f, -10.2f, -10.8f, -13.2f, -13.8f, -14.4f);
    sensor_set_thresholds_eng(0x09u, SENSOR_THRESH_ALL,
                              22.4f, 23.8f, 25.2f, 30.8f, 32.0f, 33.6f);

    /* 电流只配置三个上限阈值，不配置负方向阈值。 */
    sensor_set_thresholds_eng(0x0Au,
                              SENSOR_THRESH_UNC | SENSOR_THRESH_UC | SENSOR_THRESH_UNR,
                              0.0f, 0.0f, 0.0f, 4.0f, 5.0f, 6.0f);
    sensor_set_thresholds_eng(0x0Bu,
                              SENSOR_THRESH_UNC | SENSOR_THRESH_UC | SENSOR_THRESH_UNR,
                              0.0f, 0.0f, 0.0f, 8.8f, 12.0f, 16.0f);
    sensor_set_thresholds_eng(0x0Cu,
                              SENSOR_THRESH_UNC | SENSOR_THRESH_UC | SENSOR_THRESH_UNR,
                              0.0f, 0.0f, 0.0f, 4.4f, 6.0f, 8.0f);
    sensor_set_thresholds_eng(0x0Du,
                              SENSOR_THRESH_UNC | SENSOR_THRESH_UC | SENSOR_THRESH_UNR,
                              0.0f, 0.0f, 0.0f, 2.2f, 3.0f, 4.0f);

    /* 温度使用开尔文工程量，218K 约等于 -55℃，383K 约等于 110℃。 */
    sensor_set_thresholds_eng(0x0Eu,
                              SENSOR_THRESH_LNC | SENSOR_THRESH_UNC |
                              SENSOR_THRESH_UC | SENSOR_THRESH_UNR,
                              0.0f, 0.0f, 218.0f, 358.0f, 373.0f, 383.0f);
    sensor_set_thresholds_eng(0x0Fu,
                              SENSOR_THRESH_LNC | SENSOR_THRESH_UNC |
                              SENSOR_THRESH_UC | SENSOR_THRESH_UNR,
                              0.0f, 0.0f, 218.0f, 358.0f, 373.0f, 383.0f);
    sensor_set_thresholds_eng(0x10u,
                              SENSOR_THRESH_LNC | SENSOR_THRESH_UNC |
                              SENSOR_THRESH_UC | SENSOR_THRESH_UNR,
                              0.0f, 0.0f, 218.0f, 358.0f, 373.0f, 383.0f);
}

/**
 * @brief  初始化全部 20 个传感器描述和阈值。
 * @param  无。
 * @retval 无。
 */
void sensor_manager_init(void)
{
    /* 01h~03h：FRU 健康、电压和温度离散状态。 */
    sensor_init_one(0, 0x01u, "FRU Health", 0xF2u, 0x04u,
                    SENSOR_KIND_DISCRETE, 0u, 1, 0, 0);
    sensor_init_one(1, 0x02u, "FRU Voltage", 0x02u, 0x05u,
                    SENSOR_KIND_DISCRETE, 0u, 1, 0, 0);
    sensor_init_one(2, 0x03u, "FRU Temp", 0xF3u, 0x6Fu,
                    SENSOR_KIND_DISCRETE, 0u, 1, 0, 0);

    /* 04h~09h：输入和输出电压。 */
    sensor_init_one(3, 0x04u, "Input Voltage", 0x02u, 0x01u,
                    SENSOR_KIND_ANALOG_U8, UNIT_VOLTS, 2, 0, -1);
    sensor_init_one(4, 0x05u, "+12V Voltage", 0x02u, 0x01u,
                    SENSOR_KIND_ANALOG_U8, UNIT_VOLTS, 1, 0, -1);
    sensor_init_one(5, 0x06u, "+5V Voltage", 0x02u, 0x01u,
                    SENSOR_KIND_ANALOG_U8, UNIT_VOLTS, 2, 0, -2);
    sensor_init_one(6, 0x07u, "+3.3V Voltage", 0x02u, 0x01u,
                    SENSOR_KIND_ANALOG_U8, UNIT_VOLTS, 2, 0, -2);
    sensor_init_one(7, 0x08u, "-12V Voltage", 0x02u, 0x01u,
                    SENSOR_KIND_ANALOG_S8, UNIT_VOLTS, 1, 0, -1);
    sensor_init_one(8, 0x09u, "+28V Voltage", 0x02u, 0x01u,
                    SENSOR_KIND_ANALOG_U8, UNIT_VOLTS, 2, 0, -1);

    /* 0Ah~0Dh：输入、+12V、+3.3V 和 +5V 电流。 */
    sensor_init_one(9, 0x0Au, "Input Current", 0x03u, 0x01u,
                    SENSOR_KIND_ANALOG_U8, UNIT_AMPS, 5, 0, -2);
    sensor_init_one(10, 0x0Bu, "+12V Current", 0x03u, 0x01u,
                    SENSOR_KIND_ANALOG_U8, UNIT_AMPS, 5, 0, -2);
    sensor_init_one(11, 0x0Cu, "+3.3V Current", 0x03u, 0x01u,
                    SENSOR_KIND_ANALOG_U8, UNIT_AMPS, 2, 0, -2);
    sensor_init_one(12, 0x0Du, "+5V Current", 0x03u, 0x01u,
                    SENSOR_KIND_ANALOG_U8, UNIT_AMPS, 2, 0, -2);

    /* 0Eh~10h：三个温度位置，工程量使用开尔文。 */
    sensor_init_one(13, 0x0Eu, "Top Edge Temp", 0x01u, 0x01u,
                    SENSOR_KIND_ANALOG_U8, UNIT_DEG_K, 1, 200, 0);
    sensor_init_one(14, 0x0Fu, "Heatsink Temp", 0x01u, 0x01u,
                    SENSOR_KIND_ANALOG_U8, UNIT_DEG_K, 1, 200, 0);
    sensor_init_one(15, 0x10u, "Center Temp", 0x01u, 0x01u,
                    SENSOR_KIND_ANALOG_U8, UNIT_DEG_K, 1, 200, 0);

    /* 11h~14h：由电压乘以电流计算得到的功率传感器。 */
    sensor_init_one(16, 0x11u, "Input Power", 0x0Bu, 0x01u,
                    SENSOR_KIND_ANALOG_U8, UNIT_WATTS, 1, 0, 0);
    sensor_init_one(17, 0x12u, "+12V Power", 0x0Bu, 0x01u,
                    SENSOR_KIND_ANALOG_U8, UNIT_WATTS, 1, 0, 0);
    sensor_init_one(18, 0x13u, "+3.3V Power", 0x0Bu, 0x01u,
                    SENSOR_KIND_ANALOG_U8, UNIT_WATTS, 1, 0, 0);
    sensor_init_one(19, 0x14u, "+5V Power", 0x0Bu, 0x01u,
                    SENSOR_KIND_ANALOG_U8, UNIT_WATTS, 1, 0, 0);

    sensor_thresholds_init();
    sensor_manager_task_100ms();

    APP_LOGI("Sensor database initialized: count=%u",
             (unsigned int)SENSOR_COUNT);
}

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
                           float temp_c)
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

/**
 * @brief  根据当前读数和阈值掩码计算 IPMI 阈值事件位。
 * @param  sensor 传感器对象。
 * @retval 低 6 位分别对应 LNC、LC、LNR、UNC、UC、UNR 断言状态。
 */
uint16_t sensor_calc_event(const ipmi_sensor_t *sensor)
{
    uint16_t event_bits;
    int raw_value;

    event_bits = 0u;

    if (sensor == 0) {
        return 0u;
    }

    if (sensor->kind == SENSOR_KIND_DISCRETE) {
        return sensor->event_status;
    }

    if (sensor->kind == SENSOR_KIND_ANALOG_S8) {
        raw_value = (int)((int8_t)sensor->raw_value);
    } else {
        raw_value = (int)sensor->raw_value;
    }

    if (sensor->sensor_id == 0x08u) {
        /*
         * -12V 使用有符号读数。数值越接近 0 表示绝对值越小，因此下限比较方向与正电压相反；
         * 数值越负表示绝对值越大，因此上限比较同样需要反向。
         */
        if (((sensor->threshold_mask & SENSOR_THRESH_LNC) != 0u) &&
            (raw_value >= (int)((int8_t)sensor->lnc))) {
            event_bits |= IPMI_EVT_LNC_ASSERT;
        }
        if (((sensor->threshold_mask & SENSOR_THRESH_LC) != 0u) &&
            (raw_value >= (int)((int8_t)sensor->lc))) {
            event_bits |= IPMI_EVT_LC_ASSERT;
        }
        if (((sensor->threshold_mask & SENSOR_THRESH_LNR) != 0u) &&
            (raw_value >= (int)((int8_t)sensor->lnr))) {
            event_bits |= IPMI_EVT_LNR_ASSERT;
        }
        if (((sensor->threshold_mask & SENSOR_THRESH_UNC) != 0u) &&
            (raw_value <= (int)((int8_t)sensor->unc))) {
            event_bits |= IPMI_EVT_UNC_ASSERT;
        }
        if (((sensor->threshold_mask & SENSOR_THRESH_UC) != 0u) &&
            (raw_value <= (int)((int8_t)sensor->uc))) {
            event_bits |= IPMI_EVT_UC_ASSERT;
        }
        if (((sensor->threshold_mask & SENSOR_THRESH_UNR) != 0u) &&
            (raw_value <= (int)((int8_t)sensor->unr))) {
            event_bits |= IPMI_EVT_UNR_ASSERT;
        }

        return event_bits;
    }

    /* 正向无符号传感器使用普通上下限比较。 */
    if (((sensor->threshold_mask & SENSOR_THRESH_LNC) != 0u) &&
        (raw_value <= (int)sensor->lnc)) {
        event_bits |= IPMI_EVT_LNC_ASSERT;
    }
    if (((sensor->threshold_mask & SENSOR_THRESH_LC) != 0u) &&
        (raw_value <= (int)sensor->lc)) {
        event_bits |= IPMI_EVT_LC_ASSERT;
    }
    if (((sensor->threshold_mask & SENSOR_THRESH_LNR) != 0u) &&
        (raw_value <= (int)sensor->lnr)) {
        event_bits |= IPMI_EVT_LNR_ASSERT;
    }
    if (((sensor->threshold_mask & SENSOR_THRESH_UNC) != 0u) &&
        (raw_value >= (int)sensor->unc)) {
        event_bits |= IPMI_EVT_UNC_ASSERT;
    }
    if (((sensor->threshold_mask & SENSOR_THRESH_UC) != 0u) &&
        (raw_value >= (int)sensor->uc)) {
        event_bits |= IPMI_EVT_UC_ASSERT;
    }
    if (((sensor->threshold_mask & SENSOR_THRESH_UNR) != 0u) &&
        (raw_value >= (int)sensor->unr)) {
        event_bits |= IPMI_EVT_UNR_ASSERT;
    }

    return event_bits;
}

/**
 * @brief  100ms 周期刷新所有传感器工程量、8 位读数和事件状态。
 * @param  无。
 * @retval 无。
 */
void sensor_manager_task_100ms(void)
{
    float temp_k;
    uint8_t index;

    /* IPMI 温度传感器使用开尔文，因此在摄氏温度基础上加 273.15。 */
    temp_k = g_temp_c + 273.15f;

    /* 将 ADC 工程量映射到各模拟量传感器。 */
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

    /* 功率传感器由对应电压和电流实时相乘得到。 */
    g_sensors[16].eng_value = g_vin_v * g_iin_a;
    g_sensors[17].eng_value = g_v12_v * g_i12_a;
    g_sensors[18].eng_value = g_v33_v * g_i33_a;
    g_sensors[19].eng_value = g_v5_v * g_i5_a;

    /* 先刷新三个离散状态传感器。 */
    g_sensors[0].event_status = sensor_get_fru_health_bits();
    g_sensors[1].event_status = sensor_get_fru_voltage_bits();
    g_sensors[2].event_status = sensor_get_fru_temperature_bits();
    g_sensors[0].raw_value = (uint8_t)g_sensors[0].event_status;
    g_sensors[1].raw_value = (uint8_t)g_sensors[1].event_status;
    g_sensors[2].raw_value = (uint8_t)g_sensors[2].event_status;

    /* 再对全部模拟量执行工程量编码和阈值事件计算。 */
    for (index = 3u; index < SENSOR_COUNT; index++) {
        g_sensors[index].raw_value =
            sensor_eng_to_raw(&g_sensors[index], g_sensors[index].eng_value);
        g_sensors[index].event_status = sensor_calc_event(&g_sensors[index]);
    }
}

/**
 * @brief  按 Sensor ID 查询只读传感器对象。
 * @param  sensor_id IPMI Sensor Number。
 * @retval 找到时返回对象指针，未找到返回 0。
 */
const ipmi_sensor_t *sensor_get(uint8_t sensor_id)
{
    uint8_t index;

    for (index = 0u; index < SENSOR_COUNT; index++) {
        if (g_sensors[index].sensor_id == sensor_id) {
            return &g_sensors[index];
        }
    }

    return 0;
}

/**
 * @brief  获取传感器总数。
 * @param  无。
 * @retval SENSOR_COUNT。
 */
uint8_t sensor_get_count(void)
{
    return SENSOR_COUNT;
}

/**
 * @brief  生成 FRU Voltage 离散状态位。
 * @param  无。
 * @retval 电压离散状态位。
 */
uint16_t sensor_get_fru_voltage_bits(void)
{
    uint16_t bits;

    /* bit0 初始表示所有电压正常。 */
    bits = 0x0001u;

    /* 输入电压异常时设置输入异常相关位。 */
    if ((g_vin_v < 12.0f) || (g_vin_v > 36.0f)) {
        bits |= 0x0006u;
    }

    /* 任一输出电压超出非严重范围时设置输出异常位，并清除正常位。 */
    if ((g_v12_v < 10.8f) || (g_v12_v > 13.2f) ||
        (g_v5_v < 4.5f) || (g_v5_v > 5.5f) ||
        (g_v33_v < 2.97f) || (g_v33_v > 3.63f) ||
        (g_v28_v < 25.2f) || (g_v28_v > 30.8f) ||
        (g_vm12_v > -10.8f) || (g_vm12_v < -13.2f)) {
        bits |= 0x000Au;
        bits &= (uint16_t)~0x0001u;
    }

    return bits;
}

/**
 * @brief  生成 FRU Temperature 离散状态位。
 * @param  无。
 * @retval 温度离散状态位。
 */
uint16_t sensor_get_fru_temperature_bits(void)
{
    uint16_t bits;

    /* bit0 初始表示温度正常。 */
    bits = 0x0001u;

    if (g_temp_c > 85.0f) {
        bits |= 0x0002u;
    }

    if (g_temp_c > 100.0f) {
        bits |= 0x0004u;
    }

    if (bits != 0x0001u) {
        bits &= (uint16_t)~0x0001u;
    }

    return bits;
}

/**
 * @brief  生成 FRU Health 综合离散状态位。
 * @param  无。
 * @retval 健康状态位集合。
 */
uint16_t sensor_get_fru_health_bits(void)
{
    uint16_t bits;

    bits = 0u;

    if (sensor_get_fru_voltage_bits() != 0x0001u) {
        bits |= 0x0008u;
    }

    if ((sensor_get_fru_temperature_bits() & 0x0006u) != 0u) {
        bits |= 0x0001u;
    }

    if (g_temp_c > 100.0f) {
        bits |= 0x0002u;
    }

    if ((g_vin_v < 12.0f) || (g_vin_v > 36.0f)) {
        bits |= 0x0004u;
    }

    return bits;
}
