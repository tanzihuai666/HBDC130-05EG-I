/**
 * @file    sdr_manager.h
 * @brief   IPMI Device SDR 记录生成与读取接口。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#ifndef SDR_MANAGER_H
#define SDR_MANAGER_H

#include "app_config.h"
#include <stdint.h>

/** @brief 生成 SDR 记录。@param owner_addr_8bit Owner 地址。@param entity_instance 实体实例。@retval 无。 */
void sdr_manager_init(uint8_t owner_addr_8bit, uint8_t entity_instance);

/** @brief 获取 SDR 数量。@param 无。@retval SDR 数量。 */
uint8_t sdr_get_count(void);

/** @brief 生成 SDR 预留 ID。@param 无。@retval 预留 ID。 */
uint16_t sdr_reserve(void);

/** @brief 读取 SDR 记录片段。@param record_id 记录 ID。@param offset 偏移。@param count 长度。@param next_lsb 下一记录低字节。@param next_msb 下一记录高字节。@param out 输出缓存。@retval 实际字节数。 */
uint8_t sdr_read_record(uint16_t record_id, uint8_t offset, uint8_t count, uint8_t *next_lsb, uint8_t *next_msb, uint8_t *out);

#endif
