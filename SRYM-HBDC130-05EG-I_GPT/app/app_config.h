/**
 * @file    app_config.h
 * @brief   HBDC130-05EG-I 应用层全局配置文件。
 *          本文件集中定义固件版本、IPMB/IPMI 命令码、Completion Code、
 *          FRU/SDR/传感器数量以及日志开关，避免各模块重复硬编码。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "gd32f10x.h"
#include <stdio.h>

/* 固件版本：以 BCD 形式上报给 IPMI Get Device ID。 */
#define APP_FW_MAJOR_BCD                 0x02u
#define APP_FW_MINOR_BCD                 0x00u

/* 日志开关：1 使能 printf 串口日志，0 编译期关闭日志调用。 */
#define APP_LOG_ENABLE                   1u
#define APP_LOG_LEVEL_ERROR              1u
#define APP_LOG_LEVEL_WARN               2u
#define APP_LOG_LEVEL_INFO               3u
#define APP_LOG_LEVEL_DEBUG              4u
#define APP_LOG_LEVEL                    APP_LOG_LEVEL_INFO

#if APP_LOG_ENABLE
#define APP_LOG_RAW(...)                 printf(__VA_ARGS__)
#define APP_LOGE(...)                    do { if (APP_LOG_LEVEL >= APP_LOG_LEVEL_ERROR) { printf("[E] "); printf(__VA_ARGS__); printf("\r\n"); } } while (0)
#define APP_LOGW(...)                    do { if (APP_LOG_LEVEL >= APP_LOG_LEVEL_WARN)  { printf("[W] "); printf(__VA_ARGS__); printf("\r\n"); } } while (0)
#define APP_LOGI(...)                    do { if (APP_LOG_LEVEL >= APP_LOG_LEVEL_INFO)  { printf("[I] "); printf(__VA_ARGS__); printf("\r\n"); } } while (0)
#define APP_LOGD(...)                    do { if (APP_LOG_LEVEL >= APP_LOG_LEVEL_DEBUG) { printf("[D] "); printf(__VA_ARGS__); printf("\r\n"); } } while (0)
#else
#define APP_LOG_RAW(...)                 do { } while (0)
#define APP_LOGE(...)                    do { } while (0)
#define APP_LOGW(...)                    do { } while (0)
#define APP_LOGI(...)                    do { } while (0)
#define APP_LOGD(...)                    do { } while (0)
#endif

/* IPMB 地址：8-bit 地址格式，即 7-bit 地址左移 1 位后最低位作为 R/W 位。 */
#define IPMB_PM_BASE_ADDR_8BIT           0x72u   /* 电源模块基础地址，GA[2:0] 再叠加偏移。 */
#define IPMB_BMC_ADDR_8BIT               0x20u   /* 管理控制器常见地址，调试保留。 */
#define IPMB_MAX_FRAME_LEN               64u     /* I2C 单帧最大缓存长度。 */
#define IPMB_MAX_RSP_DATA_LEN            48u     /* IPMI 响应数据区最大长度，预留帧头/校验。 */
#define IPMB_MAX_READ_CHUNK              32u     /* FRU/SDR 分段读取最大块。 */
#define IPMB_TX_QUEUE_DEPTH              4u      /* IPMB 响应发送队列深度。 */
#define IPMB_TX_RETRY_MAX                3u      /* 多主仲裁或 NACK 时最大重试次数。 */

/* IPMI NetFn：仅实现本项目 V2 最小命令集需要的三个功能组。 */
#define IPMI_NETFN_SENSOR_EVENT          0x04u
#define IPMI_NETFN_APP                   0x06u
#define IPMI_NETFN_STORAGE               0x0Au

/* IPMI Cmd：V2 方案要求的最小命令集。 */
#define IPMI_CMD_GET_DEVICE_ID           0x01u
#define IPMI_CMD_GET_SELF_TEST           0x04u
#define IPMI_CMD_GET_DEVICE_SDR_INFO     0x20u
#define IPMI_CMD_GET_DEVICE_SDR          0x21u
#define IPMI_CMD_RESERVE_DEVICE_SDR      0x22u
#define IPMI_CMD_GET_SENSOR_THRESH       0x27u
#define IPMI_CMD_GET_SENSOR_READING      0x2Du
#define IPMI_CMD_GET_SENSOR_TYPE         0x2Fu
#define IPMI_CMD_GET_FRU_INFO            0x10u
#define IPMI_CMD_READ_FRU_DATA           0x11u
#define IPMI_CMD_WRITE_FRU_DATA          0x12u

/* IPMI Completion Code：与 BMC/ChMC 调试时最常见的返回码。 */
#define IPMI_CC_OK                       0x00u
#define IPMI_CC_INVALID_CMD              0xC1u
#define IPMI_CC_REQ_LEN_INVALID          0xC7u
#define IPMI_CC_PARAM_OUT_OF_RANGE       0xC9u
#define IPMI_CC_NOT_PRESENT              0xCBu
#define IPMI_CC_INVALID_FIELD            0xCCu
#define IPMI_CC_NOT_SUPPORTED_STATE      0xD5u

/* FRU/SDR/传感器全局规模。 */
#define FRU_DEVICE_ID_POWER_MODULE       0x00u
#define SENSOR_COUNT                     20u
#define SDR_RECORD_COUNT                 SENSOR_COUNT
#define SDR_MAX_RECORD_SIZE              64u
#define FRU_AREA_MAX_SIZE                512u

/* IPMI 传感器状态字节。0xC0 表示扫描使能且读数可用。 */
#define IPMI_SENSOR_STATUS_NORMAL        0xC0u
#define IPMI_SENSOR_STATUS_UNAVAILABLE   0x20u

/* 阈值事件位：Get Sensor Reading 返回的 assertion bit。 */
#define IPMI_EVT_LNC_ASSERT              (1u << 0)
#define IPMI_EVT_LC_ASSERT               (1u << 1)
#define IPMI_EVT_LNR_ASSERT              (1u << 2)
#define IPMI_EVT_UNC_ASSERT              (1u << 3)
#define IPMI_EVT_UC_ASSERT               (1u << 4)
#define IPMI_EVT_UNR_ASSERT              (1u << 5)

#endif
