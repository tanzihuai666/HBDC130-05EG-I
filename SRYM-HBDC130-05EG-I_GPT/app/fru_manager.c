/**
 * @file    fru_manager.c
 * @brief   IPMI FRU 数据生成与读取。
 *          生成 Common Header、Board Info Area、Product Info Area，并按 IPMI Read FRU Data
 *          命令要求提供分段读取接口。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#include "fru_manager.h"
#include <string.h>

static uint8_t g_fru[FRU_AREA_MAX_SIZE]; /* FRU 镜像缓存。 */
static uint16_t g_fru_size;              /* 当前 FRU 有效长度。 */

/** @brief 计算 IPMI 8-bit checksum。@param p 数据指针。@param n 长度。@retval checksum。 */
static uint8_t checksum8(const uint8_t *p, uint16_t n)
{
    uint8_t s = 0u;
    uint16_t i;
    for (i = 0u; i < n; i++) s = (uint8_t)(s + p[i]);
    return (uint8_t)(0u - s);
}

/** @brief 校验区域累加和是否为 0。@param p 数据指针。@param n 长度。@retval 累加和。 */
static uint8_t verify_sum0(const uint8_t *p, uint16_t n)
{
    uint8_t s = 0u;
    uint16_t i;
    for (i = 0u; i < n; i++) s = (uint8_t)(s + p[i]);
    return s;
}

/** @brief 追加 FRU 字符串字段，类型为 8-bit ASCII/Latin。 */
static uint16_t append_field(uint16_t pos, const char *s)
{
    uint8_t len = (uint8_t)strlen(s);
    if (len > 63u) len = 63u;
    g_fru[pos++] = (uint8_t)(0xC0u | len);
    memcpy(&g_fru[pos], s, len);
    return (uint16_t)(pos + len);
}

/** @brief 补齐区域到 8 字节对齐并写入 area length/checksum。 */
static uint16_t align_area(uint16_t start, uint16_t pos)
{
    uint16_t len_with_checksum;
    uint16_t pad;

    g_fru[pos++] = 0xC1u; /* End of fields marker. */
    len_with_checksum = (uint16_t)(pos - start + 1u);
    pad = (uint16_t)((8u - (len_with_checksum & 7u)) & 7u);
    while (pad-- > 0u) g_fru[pos++] = 0u;

    g_fru[start + 1u] = (uint8_t)((pos - start + 1u) / 8u);
    g_fru[pos] = checksum8(&g_fru[start], (uint16_t)(pos - start));
    return (uint16_t)(pos + 1u);
}

/** @brief 构建 Board Info Area。 */
static uint16_t build_board_area(uint16_t start)
{
    uint16_t pos = start;

    g_fru[pos++] = 0x01u;
    g_fru[pos++] = 0x00u;
    g_fru[pos++] = 0x00u;
    g_fru[pos++] = 0x00u;
    g_fru[pos++] = 0x00u;
    g_fru[pos++] = 0x00u;
    pos = append_field(pos, "TBD");
    pos = append_field(pos, "HBDC130-05EG-I Power Module");
    pos = append_field(pos, "0000000000");
    pos = append_field(pos, "HBDC130-05EG-I");
    pos = append_field(pos, "HBDC130-IPMI-V2");
    return align_area(start, pos);
}

/** @brief 构建 Product Info Area。 */
static uint16_t build_product_area(uint16_t start)
{
    uint16_t pos = start;

    g_fru[pos++] = 0x01u;
    g_fru[pos++] = 0x00u;
    g_fru[pos++] = 0x00u;
    pos = append_field(pos, "TBD");
    pos = append_field(pos, "HBDC130-05EG-I Power Module");
    pos = append_field(pos, "HBDC130-05EG-I");
    pos = append_field(pos, "HW:A FW:2.0.0");
    pos = append_field(pos, "0000000000");
    pos = append_field(pos, "");
    pos = append_field(pos, "HBDC130-IPMI-V2");
    return align_area(start, pos);
}

/** @brief 初始化 FRU 镜像。 */
void fru_manager_init(void)
{
    uint16_t board_start;
    uint16_t product_start;
    uint16_t pos;

    memset(g_fru, 0, sizeof(g_fru));
    board_start = 8u;
    pos = build_board_area(board_start);
    product_start = pos;
    pos = build_product_area(product_start);

    g_fru[0] = 0x01u;
    g_fru[1] = 0x00u;
    g_fru[2] = 0x00u;
    g_fru[3] = (uint8_t)(board_start / 8u);
    g_fru[4] = (uint8_t)(product_start / 8u);
    g_fru[5] = 0x00u;
    g_fru[6] = 0x00u;
    g_fru[7] = checksum8(g_fru, 7u);

    g_fru_size = pos;
    APP_LOGI("fru: init size=%u board=%u product=%u", g_fru_size, board_start, product_start);
}

/** @brief 获取 FRU 总长度。 */
uint16_t fru_get_area_size(void)
{
    return g_fru_size;
}

/** @brief 分段读取 FRU 数据。 */
uint8_t fru_read(uint16_t offset, uint8_t count, uint8_t *out)
{
    uint8_t n;

    if ((out == 0) || (count == 0u) || (offset >= g_fru_size)) return 0u;
    if (verify_sum0(g_fru, 8u) != 0u) {
        APP_LOGE("fru: header checksum error");
        return 0u;
    }

    n = count;
    if (n > IPMB_MAX_READ_CHUNK) n = IPMB_MAX_READ_CHUNK;
    if ((uint16_t)(offset + n) > g_fru_size) n = (uint8_t)(g_fru_size - offset);
    memcpy(out, &g_fru[offset], n);
    return n;
}
