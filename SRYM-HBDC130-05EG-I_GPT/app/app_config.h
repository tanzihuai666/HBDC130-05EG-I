#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "gd32f10x.h"

#define APP_FW_MAJOR_BCD                 0x02u
#define APP_FW_MINOR_BCD                 0x00u

#define IPMB_PM_BASE_ADDR_8BIT           0x72u
#define IPMB_BMC_ADDR_8BIT               0x20u
#define IPMB_MAX_FRAME_LEN               64u
#define IPMB_MAX_RSP_DATA_LEN            48u
#define IPMB_MAX_READ_CHUNK              32u
#define IPMB_TX_QUEUE_DEPTH              4u
#define IPMB_TX_RETRY_MAX                3u

#define IPMI_NETFN_SENSOR_EVENT          0x04u
#define IPMI_NETFN_APP                   0x06u
#define IPMI_NETFN_STORAGE               0x0Au

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

#define IPMI_CC_OK                       0x00u
#define IPMI_CC_INVALID_CMD              0xC1u
#define IPMI_CC_REQ_LEN_INVALID          0xC7u
#define IPMI_CC_PARAM_OUT_OF_RANGE       0xC9u
#define IPMI_CC_NOT_PRESENT              0xCBu
#define IPMI_CC_INVALID_FIELD            0xCCu
#define IPMI_CC_NOT_SUPPORTED_STATE      0xD5u

#define FRU_DEVICE_ID_POWER_MODULE       0x00u
#define SENSOR_COUNT                     20u
#define SDR_RECORD_COUNT                 SENSOR_COUNT
#define SDR_MAX_RECORD_SIZE              64u
#define FRU_AREA_MAX_SIZE                512u

#define IPMI_SENSOR_STATUS_NORMAL        0xC0u
#define IPMI_SENSOR_STATUS_UNAVAILABLE   0x20u

#define IPMI_EVT_LNC_ASSERT              (1u << 0)
#define IPMI_EVT_LC_ASSERT               (1u << 1)
#define IPMI_EVT_LNR_ASSERT              (1u << 2)
#define IPMI_EVT_UNC_ASSERT              (1u << 3)
#define IPMI_EVT_UC_ASSERT               (1u << 4)
#define IPMI_EVT_UNR_ASSERT              (1u << 5)

#endif
