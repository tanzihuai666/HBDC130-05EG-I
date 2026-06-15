/**
 * @file    sdr_manager.c
 * @brief   IPMI Device SDR 记录生成与分段读取实现。
 *          本模块根据 sensor_manager 中维护的 20 个传感器描述，生成对应的 Full Sensor
 *          Record，并响应 Get Device SDR、Reserve Device SDR 等命令所需的数据访问。
 *          SDR 数据全部保存在 RAM 中，上电初始化后不再动态改变记录结构，仅传感器实时
 *          读数和事件状态由 sensor_manager 更新。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#include "sdr_manager.h"
#include "sensor_manager.h"
#include <string.h>

/* 保存全部 SDR 记录。第一维是记录编号减 1，第二维是记录字节内容。 */
static uint8_t g_sdr[SDR_RECORD_COUNT][SDR_MAX_RECORD_SIZE];

/* 保存每一条 SDR 记录的实际有效长度，避免读取未使用的尾部空间。 */
static uint8_t g_sdr_len[SDR_RECORD_COUNT];

/* 当前 Device SDR Reservation ID。0 保留不用，因此初始化为 1。 */
static uint16_t g_reservation = 1u;

/**
 * @brief  将 SDR 的 Rexp 和 Bexp 两个 4 位有符号指数打包成一个字节。
 * @param  r_exp 结果指数，取低 4 位放在 bit3:0。
 * @param  b_exp B 系数指数，取低 4 位放在 bit7:4。
 * @retval 打包后的指数域字节。
 */
static uint8_t sdr_encode_exp(int8_t r_exp, int8_t b_exp)
{
    return (uint8_t)(((b_exp & 0x0F) << 4) | (r_exp & 0x0F));
}

/**
 * @brief  按 SDR Type/Length 格式写入传感器名称。
 * @param  record SDR 记录缓冲区。
 * @param  pos    名称字段起始偏移。
 * @param  name   以 '\0' 结尾的传感器名称。
 * @retval 名称写入完成后的下一个可写偏移。
 * @note   当前名称限制为 15 字节，Type/Length 高两位使用 0xC0 表示 8-bit ASCII/Latin。
 */
static uint8_t sdr_put_name(uint8_t *record, uint8_t pos, const char *name)
{
    uint8_t length;

    length = (uint8_t)strlen(name);
    if (length > 15u) {
        length = 15u;
    }

    record[pos++] = (uint8_t)(0xC0u | length);
    memcpy(&record[pos], name, length);

    return (uint8_t)(pos + length);
}

/**
 * @brief  提取 M 或 B 的 10 位有符号编码中的高 2 位。
 * @param  value M 或 B 系数。
 * @retval 系数 bit9:8，放在返回值 bit1:0。
 */
static uint8_t sdr_get_msb10(int16_t value)
{
    return (uint8_t)(((uint16_t)value >> 8) & 0x03u);
}

/**
 * @brief  根据一个传感器描述生成一条 Full Sensor Record。
 * @param  index           SDR 数组下标，范围 0~SDR_RECORD_COUNT-1。
 * @param  owner_addr      Sensor Owner 的 8 位 IPMB 地址。
 * @param  entity_instance 实体实例号，通常由 GA_ID+1 得到。
 * @retval 无。
 */
static void sdr_make_record(uint8_t index, uint8_t owner_addr, uint8_t entity_instance)
{
    const ipmi_sensor_t *sensor;
    uint8_t *record;
    uint8_t pos;
    uint16_t record_id;
    uint8_t threshold_mask;

    /* 当前设计中 Sensor ID 从 1 连续编号，因此 index+1 即 Sensor ID。 */
    sensor = sensor_get((uint8_t)(index + 1u));
    if (sensor == 0) {
        APP_LOGE("SDR build failed: sensor index=%u not found", (unsigned int)index);
        return;
    }

    record = g_sdr[index];
    memset(record, 0, SDR_MAX_RECORD_SIZE);

    record_id = (uint16_t)(index + 1u);
    threshold_mask = sensor->threshold_mask;

    /* 字节 0~4：SDR 通用记录头。 */
    record[0] = (uint8_t)(record_id & 0xFFu);          /* Record ID 低字节。 */
    record[1] = (uint8_t)(record_id >> 8);             /* Record ID 高字节。 */
    record[2] = 0x51u;                                 /* SDR Version 1.5。 */
    record[3] = 0x01u;                                 /* Record Type 01h：Full Sensor Record。 */
    record[4] = 0u;                                    /* Record Length，最后计算。 */

    /* 字节 5~13：Owner、实体和传感器类型信息。 */
    record[5] = owner_addr;                            /* Sensor Owner ID。 */
    record[6] = 0x00u;                                 /* Owner LUN=0，Channel=0。 */
    record[7] = sensor->sensor_id;                     /* Sensor Number。 */
    record[8] = sensor->entity_id;                     /* Entity ID。 */
    record[9] = entity_instance;                       /* Entity Instance。 */
    record[10] = 0x67u;                                /* Sensor Initialization。 */
    record[11] = (sensor->kind == SENSOR_KIND_DISCRETE) ? 0x40u : 0x68u;
    record[12] = sensor->sensor_type;                  /* Sensor Type。 */
    record[13] = sensor->reading_type;                 /* Event/Reading Type。 */

    /*
     * 字节 14~19：事件断言、去断言和可读阈值掩码。
     * 离散传感器开放低 6 位状态；模拟量传感器使用实际 threshold_mask。
     */
    if (sensor->kind == SENSOR_KIND_DISCRETE) {
        record[14] = 0x3Fu;
        record[15] = 0x00u;
        record[16] = 0x3Fu;
        record[17] = 0x00u;
        record[18] = 0x00u;
        record[19] = 0x00u;
    } else {
        record[14] = threshold_mask;
        record[15] = 0x00u;
        record[16] = threshold_mask;
        record[17] = 0x00u;
        record[18] = 0x00u;
        record[19] = threshold_mask;
    }

    /* 字节 20~29：模拟数据格式、单位和 M/B/Rexp/Bexp 线性化参数。 */
    record[20] = (sensor->kind == SENSOR_KIND_ANALOG_S8) ? 0x80u : 0x00u;
    record[21] = sensor->base_unit;
    record[22] = 0x00u;
    record[23] = 0x00u;
    record[24] = (uint8_t)(sensor->m & 0xFF);
    record[25] = sdr_get_msb10(sensor->m);
    record[26] = (uint8_t)(sensor->b & 0xFF);
    record[27] = sdr_get_msb10(sensor->b);
    record[28] = 0x00u;
    record[29] = sdr_encode_exp(sensor->r_exp, sensor->b_exp);

    /* 字节 30~35：模拟特性、正常最大值/最小值及传感器最大值/最小值。 */
    record[30] = 0x00u;
    record[31] = sensor->raw_value;
    record[32] = 0xFFu;
    record[33] = 0x00u;
    record[34] = 0xFFu;
    record[35] = 0x00u;

    /* 字节 36~41：阈值顺序固定为 UNR、UC、UNC、LNR、LC、LNC。 */
    record[36] = sensor->unr;
    record[37] = sensor->uc;
    record[38] = sensor->unc;
    record[39] = sensor->lnr;
    record[40] = sensor->lc;
    record[41] = sensor->lnc;

    /* 字节 42~46：迟滞和 OEM 保留字段。 */
    record[42] = 0x01u;
    record[43] = 0x01u;
    record[44] = 0x00u;
    record[45] = 0x00u;
    record[46] = 0x00u;

    /* 从字节 47 开始写入 ID 字符串，并回填记录长度。 */
    pos = sdr_put_name(record, 47u, sensor->name);
    record[4] = (uint8_t)(pos - 5u);
    g_sdr_len[index] = pos;
}

/**
 * @brief  初始化全部 Device SDR 记录。
 * @param  owner_addr_8bit Sensor Owner 的 8 位 IPMB 地址。
 * @param  entity_instance 实体实例号。
 * @retval 无。
 */
void sdr_manager_init(uint8_t owner_addr_8bit, uint8_t entity_instance)
{
    uint8_t index;

    for (index = 0u; index < SDR_RECORD_COUNT; index++) {
        sdr_make_record(index, owner_addr_8bit, entity_instance);
    }

    APP_LOGI("SDR initialized: count=%u owner=0x%02X",
             (unsigned int)SDR_RECORD_COUNT,
             (unsigned int)owner_addr_8bit);
}

/**
 * @brief  获取 Device SDR 记录总数。
 * @param  无。
 * @retval SDR_RECORD_COUNT。
 */
uint8_t sdr_get_count(void)
{
    return SDR_RECORD_COUNT;
}

/**
 * @brief  生成新的 Device SDR Reservation ID。
 * @param  无。
 * @retval 新的非零 Reservation ID。
 */
uint16_t sdr_reserve(void)
{
    g_reservation++;

    /* 0 为无效 Reservation ID，发生 16 位回绕时跳过 0。 */
    if (g_reservation == 0u) {
        g_reservation = 1u;
    }

    APP_LOGD("SDR reserved: id=0x%04X", (unsigned int)g_reservation);
    return g_reservation;
}

/**
 * @brief  分段读取一条 SDR 记录。
 * @param  record_id 指定记录 ID；0 表示从第一条记录开始。
 * @param  offset    记录内字节偏移。
 * @param  count     请求读取字节数；0xFF 表示读取剩余全部内容。
 * @param  next_lsb  下一条记录 ID 低字节输出指针。
 * @param  next_msb  下一条记录 ID 高字节输出指针。
 * @param  out       数据输出缓冲区。
 * @retval 实际返回的数据字节数；参数错误或越界时返回 0。
 */
uint8_t sdr_read_record(uint16_t record_id,
                        uint8_t offset,
                        uint8_t count,
                        uint8_t *next_lsb,
                        uint8_t *next_msb,
                        uint8_t *out)
{
    uint8_t index;
    uint8_t read_length;

    if ((next_lsb == 0) || (next_msb == 0) || (out == 0)) {
        return 0u;
    }

    /* IPMI 约定 Record ID=0000h 表示读取第一条记录。 */
    if (record_id == 0u) {
        record_id = 1u;
    }

    if ((record_id < 1u) || (record_id > SDR_RECORD_COUNT)) {
        return 0u;
    }

    index = (uint8_t)(record_id - 1u);
    if (offset >= g_sdr_len[index]) {
        return 0u;
    }

    /* 最后一条记录之后返回 FFFFh，否则返回下一条连续记录 ID。 */
    if (record_id >= SDR_RECORD_COUNT) {
        *next_lsb = 0xFFu;
        *next_msb = 0xFFu;
    } else {
        *next_lsb = (uint8_t)((record_id + 1u) & 0xFFu);
        *next_msb = (uint8_t)((record_id + 1u) >> 8);
    }

    read_length = count;

    if (read_length == 0xFFu) {
        read_length = (uint8_t)(g_sdr_len[index] - offset);
    }

    if (read_length > IPMB_MAX_READ_CHUNK) {
        read_length = IPMB_MAX_READ_CHUNK;
    }

    if ((uint16_t)offset + read_length > g_sdr_len[index]) {
        read_length = (uint8_t)(g_sdr_len[index] - offset);
    }

    memcpy(out, &g_sdr[index][offset], read_length);
    return read_length;
}
