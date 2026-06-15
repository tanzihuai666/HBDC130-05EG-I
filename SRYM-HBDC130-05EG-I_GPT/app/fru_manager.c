/**
 * @file    fru_manager.c
 * @brief   IPMI FRU 数据镜像生成和分段读取实现。
 *          本模块在 RAM 中生成 FRU 公共头、板卡信息区和产品信息区，并按 IPMI
 *          Read FRU Data 命令要求提供分段读取。当前 FRU 内容为只读静态数据，后续可将
 *          制造商、序列号和硬件版本改为从 Flash 参数区加载。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#include "fru_manager.h"
#include <string.h>

/* FRU 完整字节镜像，最大容量由 FRU_AREA_MAX_SIZE 限制。 */
static uint8_t g_fru[FRU_AREA_MAX_SIZE];

/* 当前生成的 FRU 实际有效长度，单位字节。 */
static uint16_t g_fru_size;

/**
 * @brief  计算 IPMI FRU 使用的 8 位补码校验和。
 * @param  data 数据指针。
 * @param  length 数据长度。
 * @retval 使“数据字节之和 + 校验字节”低 8 位为 0 的校验值。
 */
static uint8_t fru_checksum8(const uint8_t *data, uint16_t length)
{
    uint8_t sum;
    uint16_t index;

    sum = 0u;

    for (index = 0u; index < length; index++) {
        sum = (uint8_t)(sum + data[index]);
    }

    return (uint8_t)(0u - sum);
}

/**
 * @brief  计算一段包含校验字节的数据累加和。
 * @param  data 数据指针。
 * @param  length 数据长度。
 * @retval 累加和低 8 位；返回 0 表示校验通过。
 */
static uint8_t fru_verify_sum(const uint8_t *data, uint16_t length)
{
    uint8_t sum;
    uint16_t index;

    sum = 0u;

    for (index = 0u; index < length; index++) {
        sum = (uint8_t)(sum + data[index]);
    }

    return sum;
}

/**
 * @brief  向 FRU 镜像追加一个 8 位文本字符串字段。
 * @param  pos 当前写入偏移。
 * @param  text 以 '\0' 结尾的文本字符串。
 * @retval 字段写入完成后的下一个可写偏移。
 * @note   字段长度最多 63 字节，类型/长度字节高两位 11b 表示 8 位文本。
 */
static uint16_t fru_append_text_field(uint16_t pos, const char *text)
{
    uint8_t length;

    length = (uint8_t)strlen(text);
    if (length > 63u) {
        length = 63u;
    }

    g_fru[pos++] = (uint8_t)(0xC0u | length);
    memcpy(&g_fru[pos], text, length);

    return (uint16_t)(pos + length);
}

/**
 * @brief  结束一个 FRU 信息区，补齐到 8 字节边界并写入长度和校验和。
 * @param  start 信息区起始偏移。
 * @param  pos   当前结束字段后的写入偏移。
 * @retval 下一个信息区可使用的起始偏移。
 */
static uint16_t fru_finish_area(uint16_t start, uint16_t pos)
{
    uint16_t length_with_checksum;
    uint16_t padding;

    /* C1h 为字段结束标记。 */
    g_fru[pos++] = 0xC1u;

    /* 预留最后 1 字节校验和后，计算需要补齐的字节数。 */
    length_with_checksum = (uint16_t)(pos - start + 1u);
    padding = (uint16_t)((8u - (length_with_checksum & 7u)) & 7u);

    while (padding > 0u) {
        g_fru[pos++] = 0u;
        padding--;
    }

    /* 信息区长度字段的单位为 8 字节。 */
    g_fru[start + 1u] = (uint8_t)((pos - start + 1u) / 8u);

    /* 校验和覆盖从信息区版本字节到校验和前一字节。 */
    g_fru[pos] = fru_checksum8(&g_fru[start], (uint16_t)(pos - start));

    return (uint16_t)(pos + 1u);
}

/**
 * @brief  构建板卡信息区。
 * @param  start 信息区起始偏移。
 * @retval 下一个信息区起始偏移。
 */
static uint16_t fru_build_board_area(uint16_t start)
{
    uint16_t pos;

    pos = start;

    g_fru[pos++] = 0x01u;  /* 格式版本 1。 */
    g_fru[pos++] = 0x00u;  /* 区域长度占位，结束时回填。 */
    g_fru[pos++] = 0x00u;  /* 语言代码 0：英语。 */
    g_fru[pos++] = 0x00u;  /* 制造时间分钟数低字节，当前暂填 0。 */
    g_fru[pos++] = 0x00u;  /* 制造时间分钟数中字节。 */
    g_fru[pos++] = 0x00u;  /* 制造时间分钟数高字节。 */

    pos = fru_append_text_field(pos, "TBD");
    pos = fru_append_text_field(pos, "HBDC130-05EG-I Power Module");
    pos = fru_append_text_field(pos, "0000000000");
    pos = fru_append_text_field(pos, "HBDC130-05EG-I");
    pos = fru_append_text_field(pos, "HBDC130-IPMI-V2");

    return fru_finish_area(start, pos);
}

/**
 * @brief  构建产品信息区。
 * @param  start 信息区起始偏移。
 * @retval FRU 数据结束后的偏移。
 */
static uint16_t fru_build_product_area(uint16_t start)
{
    uint16_t pos;

    pos = start;

    g_fru[pos++] = 0x01u;  /* 格式版本 1。 */
    g_fru[pos++] = 0x00u;  /* 区域长度占位，结束时回填。 */
    g_fru[pos++] = 0x00u;  /* 语言代码 0：英语。 */

    pos = fru_append_text_field(pos, "TBD");
    pos = fru_append_text_field(pos, "HBDC130-05EG-I Power Module");
    pos = fru_append_text_field(pos, "HBDC130-05EG-I");
    pos = fru_append_text_field(pos, "HW:A FW:2.0.0");
    pos = fru_append_text_field(pos, "0000000000");
    pos = fru_append_text_field(pos, "");
    pos = fru_append_text_field(pos, "HBDC130-IPMI-V2");

    return fru_finish_area(start, pos);
}

/**
 * @brief  生成完整 FRU 镜像。
 * @param  无。
 * @retval 无。
 */
void fru_manager_init(void)
{
    uint16_t board_start;
    uint16_t product_start;
    uint16_t end_pos;

    /* 先清空整个缓存，保证未使用区域为 0。 */
    memset(g_fru, 0, sizeof(g_fru));

    /* 公共头固定占用前 8 字节，因此板卡信息区从偏移 8 开始。 */
    board_start = 8u;
    end_pos = fru_build_board_area(board_start);

    /* 产品信息区紧跟在已经 8 字节对齐的板卡信息区之后。 */
    product_start = end_pos;
    end_pos = fru_build_product_area(product_start);

    /* 构建 8 字节公共头。所有区域偏移的单位均为 8 字节。 */
    g_fru[0] = 0x01u;                               /* 公共头格式版本。 */
    g_fru[1] = 0x00u;                               /* Internal Use Area 未使用。 */
    g_fru[2] = 0x00u;                               /* Chassis Info Area 未使用。 */
    g_fru[3] = (uint8_t)(board_start / 8u);         /* Board Info Area 偏移。 */
    g_fru[4] = (uint8_t)(product_start / 8u);       /* Product Info Area 偏移。 */
    g_fru[5] = 0x00u;                               /* MultiRecord Area 未使用。 */
    g_fru[6] = 0x00u;                               /* 保留字节。 */
    g_fru[7] = fru_checksum8(g_fru, 7u);            /* 公共头校验和。 */

    g_fru_size = end_pos;

    APP_LOGI("FRU initialized: size=%u board_offset=%u product_offset=%u",
             (unsigned int)g_fru_size,
             (unsigned int)board_start,
             (unsigned int)product_start);
}

/**
 * @brief  获取 FRU 镜像总长度。
 * @param  无。
 * @retval 当前 FRU 有效字节数。
 */
uint16_t fru_get_area_size(void)
{
    return g_fru_size;
}

/**
 * @brief  从 FRU 镜像中分段读取数据。
 * @param  offset 读取起始偏移，单位字节。
 * @param  count  请求读取字节数。
 * @param  out    输出缓冲区。
 * @retval 实际读取字节数；参数错误、越界或公共头校验失败时返回 0。
 */
uint8_t fru_read(uint16_t offset, uint8_t count, uint8_t *out)
{
    uint8_t read_length;

    if ((out == 0) || (count == 0u) || (offset >= g_fru_size)) {
        return 0u;
    }

    /* 每次读取前检查公共头，防止内存被意外破坏后继续返回错误数据。 */
    if (fru_verify_sum(g_fru, 8u) != 0u) {
        APP_LOGE("FRU common header checksum error");
        return 0u;
    }

    read_length = count;

    /* 单次返回长度受 IPMB 响应缓存大小限制。 */
    if (read_length > IPMB_MAX_READ_CHUNK) {
        read_length = IPMB_MAX_READ_CHUNK;
    }

    /* 读取范围超过 FRU 末尾时，只返回剩余有效字节。 */
    if ((uint16_t)(offset + read_length) > g_fru_size) {
        read_length = (uint8_t)(g_fru_size - offset);
    }

    memcpy(out, &g_fru[offset], read_length);
    return read_length;
}
