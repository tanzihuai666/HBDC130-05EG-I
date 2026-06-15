/**
 * @file    sdr_manager.h
 * @brief   IPMI Device SDR 记录生成与分段读取接口。
 *          本文件向 IPMI 命令分发层提供 SDR 初始化、记录计数、Reservation ID 生成以及
 *          指定记录分段读取能力。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#ifndef SDR_MANAGER_H
#define SDR_MANAGER_H

#include "app_config.h"
#include <stdint.h>

/**
 * @brief  根据 sensor_manager 中的传感器表生成全部 Full Sensor Record。
 * @param  owner_addr_8bit Sensor Owner 的 8 位 IPMB 地址。
 * @param  entity_instance 传感器实体实例号。
 * @retval 无。
 */
void sdr_manager_init(uint8_t owner_addr_8bit, uint8_t entity_instance);

/**
 * @brief  获取当前 Device SDR 记录总数。
 * @param  无。
 * @retval SDR 记录数量。
 */
uint8_t sdr_get_count(void);

/**
 * @brief  分配一个新的 Device SDR Reservation ID。
 * @param  无。
 * @retval 非零的 16 位 Reservation ID。
 */
uint16_t sdr_reserve(void);

/**
 * @brief  按 IPMI Get Device SDR 命令要求读取一段 SDR 数据。
 * @param  record_id 指定记录 ID；0 表示第一条记录。
 * @param  offset    记录内偏移。
 * @param  count     请求读取长度；0xFF 表示读取剩余全部字节。
 * @param  next_lsb  下一条记录 ID 低字节输出指针。
 * @param  next_msb  下一条记录 ID 高字节输出指针。
 * @param  out       SDR 数据输出缓冲区。
 * @retval 实际读取字节数；失败时返回 0。
 */
uint8_t sdr_read_record(uint16_t record_id,
                        uint8_t offset,
                        uint8_t count,
                        uint8_t *next_lsb,
                        uint8_t *next_msb,
                        uint8_t *out);

#endif
