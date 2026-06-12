# HBDC130-05EG-I 电源模块 IPMB/IPMI + FRU/SDR V2 实现方案

**适用项目：** HBDC130-05EG-I / VITA90 电源模块  
**MCU：** GD32F103RCT6  
**版本：** V2.0  
**目标：** 在标准多主 IPMB 机制下，实现电源模块 IPMI 设备、20 个传感器查询、FRU 信息读取、Device SDR 自动发现。后续 MCU 代码可直接按本文进行模块划分和接口实现。

---

## 1. 实现目标与范围

### 1.1 总体目标

本方案将电源模块 MCU 实现为一个标准 IPMB 上的 **IPMI 管理控制器 / 电源 FRU 管理节点**，供机箱管理器、BMC 或 ChMC 查询模块状态。

模块需要实现：

1. 标准 IPMB Request/Response 帧解析。
2. 标准多主 IPMB 响应机制：
   - ChMC/BMC 作为 I2C Master 写 Request 到电源模块；
   - 电源模块作为 I2C Slave 接收 Request；
   - 电源模块处理完成后切换为 I2C Master，把 Response 主动写回 ChMC/BMC 地址。
3. 20 个 IPMI 传感器：
   - FRU 健康、FRU 电压、FRU 温度；
   - 输入/输出电压；
   - 输入/输出电流；
   - 温度；
   - 功耗。
4. FRU Inventory 信息读取。
5. Device SDR 信息读取，使上级管理器能发现传感器编号、类型、单位、阈值和 raw 换算规则。
6. 与现有电源控制、故障检测、ADC 采样、FAIL* 信号联动。

### 1.2 非目标

本方案不是完整的 VITA 46.11 全功能 Chassis Manager 实现，不实现：

1. 机箱级电源策略管理；
2. SEL 完整事件日志仓库；
3. LAN IPMI、RMCP/RMCP+、用户认证；
4. 风扇控制；
5. HPM.1 固件升级；
6. PICMG/VITA 46.11 全部扩展命令；
7. 热插拔策略状态机。

串口升级、同步信号、SYSRESET*、均流控制等仍按原软件需求执行，但不纳入 IPMI 最小命令集。

---

## 2. 硬件与资源约束

### 2.1 MCU 资源

| 项目 | 资源 |
|---|---|
| MCU | GD32F103RCT6 |
| 内核 | ARM Cortex-M3 |
| Flash | 256KB |
| SRAM | 48KB |
| ADC | 12bit ADC，使用 PA1~PA7、PC0~PC5 |
| IPMB | I2C1，PB6/PB7 |
| RS232 | USART1，PA9/PA10 |
| 地址识别 | GA0-MCU PC7，GA1-MCU PC8，GA2-MCU PA8 |

### 2.2 IPMB 硬件连接

| 信号 | 连接器定义 | MCU/电路 | 说明 |
|---|---|---|---|
| SM0 | 系统 0 管理总线 SCL | PB6 / I2C1_SCL | V2 主 IPMB 总线 |
| SM1 | 系统 0 管理总线 SDA | PB7 / I2C1_SDA | V2 主 IPMB 总线 |
| SM2 | 系统 1 管理总线 SCL | PB10 / I2C2_SCL | 预留，V2 不启用 |
| SM3 | 系统 1 管理总线 SDA | PB11 / I2C2_SDA | 预留，V2 不启用 |
| I2C1_EN | 网络名 PA0 / PD2 | 低电平使能 | 上电默认使能 |

### 2.3 IPMB 电气要求

1. 总线速率：400 kbit/s。
2. SDA/SCL 必须为开漏输出。
3. 电源模块内部预留 IPMB 上拉电阻位置但不上件；联调和整机中必须由背板/管理器侧提供上拉。
4. GD32 I2C1 必须支持：
   - Slave receive；
   - Master transmit；
   - 仲裁丢失 ARLO 处理；
   - NACK 处理；
   - BUSY 卡死恢复。

### 2.4 电源控制相关 GPIO

| 功能 | MCU 引脚 | 有效电平 | 软件动作 |
|---|---|---|---|
| EN 输入 | PB13 | MCU 侧低有效，外部经反相 | 读取 ENABLE* 状态 |
| INH 输入 | PB14 | MCU 侧低有效，外部经反相 | 读取 INHIBIT* 状态 |
| NVMRO 输入 | PB9 | MCU 侧低有效，外部经反相 | 控制代码/参数更新权限 |
| EN1 输出 | PC12 | 高有效 | 控制 +3.3V_AUX/+5V |
| INH2 输出 | PB4 | 低有效 | 控制 +12V |
| INH1 输出 | PB5 | 低有效 | 控制 +28V/-12V |
| FAIL 输出 | PC6 | 低有效 | 任意故障立即拉低 |
| SYSRESET | PB15 | 低有效，双向 | 输入低立即复位；输出暂定 500ms |

---

## 3. 软件总体架构

### 3.1 模块划分

```text
Application
├── app_main.c
├── power_ctrl.c          // EN/INH/NVMRO 真值表、软关断顺序
├── fault_manager.c       // 过压、欠压、过流、短路、过温、FAIL*
├── sensor_manager.c      // 20 个传感器工程量、raw 值、状态缓存
├── fru_manager.c         // FRU 二进制区生成/读取
└── sdr_manager.c         // SDR 记录生成/读取

Protocol
├── ipmb_driver.c         // I2C1 slave RX + master TX，多主总线恢复
├── ipmb_frame.c          // IPMB 帧解析、checksum、地址、Seq/LUN
├── ipmi_dispatch.c       // NetFn/Cmd 分发、Completion Code
├── ipmi_app.c            // Application NetFn 命令
├── ipmi_sensor.c         // Sensor/Event NetFn 命令
└── ipmi_storage.c        // Storage NetFn FRU 命令

Driver
├── bsp_gpio.c
├── bsp_i2c.c
├── bsp_adc_dma.c
├── bsp_timer.c
├── bsp_flash.c
├── bsp_uart.c
└── bsp_watchdog.c
```

### 3.2 主循环任务

| 周期 | 任务 |
|---|---|
| 1ms | 电源控制输入扫描、故障状态机、看门狗喂狗 |
| 10ms | ADC 滤波结果更新、阈值判断、FAIL* 状态更新 |
| ≤100ms | 模拟量传感器工程值与 IPMI raw 缓存更新 |
| 实时 | I2C/IPMB 中断接收、Response 队列发送 |
| 按需 | FRU/SDR 命令读取 |

### 3.3 关键实时要求

| 项目 | 要求 |
|---|---|
| IPMI 请求响应 | ≤10ms，若仲裁丢失可重试，但总响应建议 ≤50ms |
| 模拟量缓存更新 | ≤100ms |
| 离散状态更新 | 实时，建议在故障状态变化后 1ms 内更新 |
| 电源控制切换响应 | ≤10ms |
| 短路保护 | 软件辅助，硬件保护为主，软件响应 ≤100μs 仅用于快速关断标志 |

---

## 4. IPMB 地址设计

### 4.1 地址格式

IPMB 报文中使用 8-bit 地址表示：

```c
uint8_t ipmb_addr_8bit;       // 例如 0x20、0x72
uint8_t i2c_addr_7bit = ipmb_addr_8bit >> 1;
```

注意：GD32 I2C 外设配置 Own Address 时使用 7-bit 地址，但 IPMB 帧 checksum 计算使用 8-bit IPMB 地址。

### 4.2 本模块地址

本方案建议先使用如下可配置公式：

```c
#define IPMB_PM_BASE_ADDR_8BIT   0x72u
#define IPMB_BMC_ADDR_8BIT       0x20u

uint8_t ga_id = (GA2 << 2) | (GA1 << 1) | GA0;
pm_addr_8bit = IPMB_PM_BASE_ADDR_8BIT + (ga_id << 1);
pm_addr_7bit = pm_addr_8bit >> 1;
```

示例：

| GA2 | GA1 | GA0 | ga_id | PM 8-bit 地址 | 7-bit I2C 地址 |
|---:|---:|---:|---:|---:|---:|
| 0 | 0 | 0 | 0 | 0x72 | 0x39 |
| 0 | 0 | 1 | 1 | 0x74 | 0x3A |
| 0 | 1 | 0 | 2 | 0x76 | 0x3B |
| 0 | 1 | 1 | 3 | 0x78 | 0x3C |
| 1 | 0 | 0 | 4 | 0x7A | 0x3D |
| 1 | 0 | 1 | 5 | 0x7C | 0x3E |
| 1 | 1 | 0 | 6 | 0x7E | 0x3F |
| 1 | 1 | 1 | 7 | 0x80 | 0x40 |

> 注意：`0x72` 基地址是软件方案默认值。若甲方提供正式地址分配表，只修改 `IPMB_PM_BASE_ADDR_8BIT` 和映射函数，不改协议栈。

---

## 5. IPMB 帧格式与解析

### 5.1 Request 格式

```text
[0] rsSA          目标地址，电源模块地址，例如 0x72
[1] NetFn/LUN     bit7~2 = NetFn，bit1~0 = rsLUN
[2] Checksum1     使 [0]+[1]+[2] 低 8 位为 0
[3] rqSA          请求者地址，通常 0x20
[4] Seq/LUN       bit7~2 = rqSeq，bit1~0 = rqLUN
[5] Cmd           IPMI 命令码
[6..n-2] Data     请求数据
[n-1] Checksum2   使 [3] 到 [n-1] 低 8 位为 0
```

### 5.2 Response 格式

```text
[0] rqSA              响应目标地址，原请求者地址，通常 0x20
[1] RespNetFn/LUN      bit7~2 = Request NetFn + 1，bit1~0 = rqLUN
[2] Checksum1
[3] rsSA              响应者地址，电源模块地址，例如 0x72
[4] Seq/LUN           原请求 Seq，bit1~0 = rsLUN
[5] Cmd               原 Cmd
[6] Completion Code   完成码
[7..n-2] Data         响应数据
[n-1] Checksum2
```

### 5.3 Checksum 算法

```c
static uint8_t ipmb_checksum(const uint8_t *buf, uint8_t len)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum = (uint8_t)(sum + buf[i]);
    }
    return (uint8_t)(0u - sum);
}

static bool ipmb_verify_checksum(const uint8_t *buf, uint8_t len)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum = (uint8_t)(sum + buf[i]);
    }
    return sum == 0;
}
```

### 5.4 字段解析宏

```c
#define IPMB_GET_NETFN(x)      ((uint8_t)((x) >> 2))
#define IPMB_GET_LUN(x)        ((uint8_t)((x) & 0x03u))
#define IPMB_MAKE_NETFN_LUN(netfn, lun)  ((uint8_t)(((netfn) << 2) | ((lun) & 0x03u)))

#define IPMB_GET_SEQ(x)        ((uint8_t)((x) >> 2))
#define IPMB_MAKE_SEQ_LUN(seq, lun)      ((uint8_t)(((seq) << 2) | ((lun) & 0x03u)))
```

---

## 6. 标准多主 IPMB 状态机

### 6.1 状态定义

```c
typedef enum {
    IPMB_STATE_IDLE = 0,
    IPMB_STATE_SLAVE_RX,
    IPMB_STATE_PARSE,
    IPMB_STATE_RESP_PENDING,
    IPMB_STATE_MASTER_TX,
    IPMB_STATE_WAIT_RETRY,
    IPMB_STATE_BUS_RECOVERY,
} ipmb_state_t;
```

### 6.2 接收流程

1. I2C1 配置为拥有本模块 7-bit 地址的 Slave。
2. ChMC/BMC 以 Master Write 写入 Request。
3. I2C RX 中断把接收到的字节放入 `rx_buf`。
4. STOP 条件到来后，置位 `rx_done`。
5. 主循环或协议任务调用 `ipmb_parse_request()`。
6. 若 checksum、地址、长度合法，调用 `ipmi_dispatch()`。
7. 生成 Response，压入 `tx_queue`。
8. 当总线空闲时，电源模块切为 Master Transmit，把响应写回 `rqSA`。
9. 发送完成后恢复 Slave Listen。

### 6.3 响应发送流程

```text
rx_done
  ↓
parse ok
  ↓
build response
  ↓
wait bus idle
  ↓
I2C master start
  ↓
send to rqSA >> 1
  ↓
if success: back to slave listen
if ARLO: random backoff then retry
if NACK: retry up to N times, then drop and log
if BUSY timeout: bus recovery
```

### 6.4 多主冲突处理

| 异常 | 处理 |
|---|---|
| ARLO 仲裁丢失 | 立即停止发送，清标志，随机退避 1~3ms，最多重试 3 次 |
| NACK | 记录 `ipmb_nack_count`，重试 1~3 次；仍失败则丢弃响应 |
| BUSY 超时 | 禁用 I2C，GPIO 模拟 SCL 脉冲 9 次释放 SDA，重新初始化 I2C |
| BERR | 清错误标志，重新初始化 I2C |
| OVR | 清 RX 缓冲，重新进入 Slave RX |

### 6.5 Response 队列

建议实现小队列，避免短时间多条请求导致响应丢失：

```c
#define IPMB_MAX_FRAME_LEN      64u
#define IPMB_TX_QUEUE_DEPTH     4u
#define IPMB_RX_BUF_LEN         64u

typedef struct {
    uint8_t data[IPMB_MAX_FRAME_LEN];
    uint8_t len;
    uint8_t dest_addr_8bit;
    uint8_t retry;
} ipmb_tx_item_t;
```

---

## 7. IPMI 命令集 V2

### 7.1 必须实现命令列表

| NetFn 请求 | NetFn 响应 | Cmd | 命令 | 模块 |
|---:|---:|---:|---|---|
| 0x06 | 0x07 | 0x01 | Get Device ID | Application |
| 0x06 | 0x07 | 0x04 | Get Self Test Results | Application |
| 0x04 | 0x05 | 0x20 | Get Device SDR Info | Sensor/Event |
| 0x04 | 0x05 | 0x21 | Get Device SDR | Sensor/Event |
| 0x04 | 0x05 | 0x22 | Reserve Device SDR Repository | Sensor/Event |
| 0x04 | 0x05 | 0x27 | Get Sensor Thresholds | Sensor/Event |
| 0x04 | 0x05 | 0x2D | Get Sensor Reading | Sensor/Event |
| 0x04 | 0x05 | 0x2F | Get Sensor Type | Sensor/Event |
| 0x0A | 0x0B | 0x10 | Get FRU Inventory Area Info | Storage |
| 0x0A | 0x0B | 0x11 | Read FRU Data | Storage |

### 7.2 可明确不支持的命令

| NetFn | Cmd | 命令 | 返回 |
|---:|---:|---|---|
| 0x04 | 0x26 | Set Sensor Thresholds | 0xC1 或 0xD5 |
| 0x0A | 0x12 | Write FRU Data | 默认 0xC1；若后续需要，可在 NVMRO 允许时开放 |
| 0x0A | 0x40+ | SEL 相关命令 | 0xC1 |
| 0x2C | 任意 | PICMG/VITA Extension | V2 不实现，返回 0xC1 |

### 7.3 Completion Code

| Code | 含义 | 使用场景 |
|---:|---|---|
| 0x00 | 成功 | 命令正常执行 |
| 0xC1 | Invalid Command | NetFn/Cmd 不支持 |
| 0xC7 | Request Data Length Invalid | 请求数据长度错误 |
| 0xC9 | Parameter Out of Range | offset/count 越界、请求长度过大 |
| 0xCB | Requested Sensor/Data/Record Not Present | Sensor ID 或 SDR Record ID 不存在 |
| 0xCC | Invalid Data Field in Request | 字段非法，例如保留值不为 0 |
| 0xD5 | Command Not Supported in Present State | 当前状态不允许写入或操作 |

---

## 8. Application NetFn 实现

### 8.1 Get Device ID

```text
NetFn Request  = 0x06
NetFn Response = 0x07
Cmd            = 0x01
Request Data   = 无
```

Response Data：

| Byte | 字段 | 建议值 |
|---:|---|---|
| 0 | Completion Code | 0x00 |
| 1 | Device ID | 0x01 |
| 2 | Device Revision | 0x81，bit7=1 表示提供 Device SDR |
| 3 | Firmware Revision 1 | 主版本 BCD，例如 0x02 |
| 4 | Firmware Revision 2 | 次版本 BCD，例如 0x00 |
| 5 | IPMI Version | 0x02，表示兼容 IPMI 2.0 格式 |
| 6 | Additional Device Support | 0x29，建议置 Sensor Device、SDR、FRU Inventory |
| 7 | Manufacturer ID LSB | 配置项 |
| 8 | Manufacturer ID MID | 配置项 |
| 9 | Manufacturer ID MSB | 配置项 |
| 10 | Product ID LSB | 配置项 |
| 11 | Product ID MSB | 配置项 |
| 12~15 | Aux Firmware Revision | 可返回 0 |

配置建议：

```c
#define IPMI_FW_REV_MAJOR_BCD      0x02
#define IPMI_FW_REV_MINOR_BCD      0x00
#define IPMI_MANUFACTURER_ID       0x000000u   // 待甲方/公司分配
#define IPMI_PRODUCT_ID            0x1305u     // HBDC130-05EG-I 项目内部值
```

### 8.2 Get Self Test Results

```text
NetFn Request  = 0x06
Cmd            = 0x04
Request Data   = 无
```

Response Data：

| Byte | 字段 | 正常值 |
|---:|---|---:|
| 0 | Completion Code | 0x00 |
| 1 | Self Test Result | 0x55，No error |
| 2 | Additional Error Info | 0x00 |

若 ADC 校准失败、FRU 校验失败、内部 Flash 参数 CRC 错误，可返回非 `0x55` 并在 Byte2 放内部错误位图。

---

## 9. Sensor/Event NetFn 实现

## 9.1 传感器清单

本项目固定 20 个传感器，Sensor ID 从 `0x01` 到 `0x14`。

| ID | 名称 | Sensor Type | Reading Type | 数据类型 | 单位 | 数据来源 |
|---:|---|---:|---:|---|---|---|
| 0x01 | FRU健康 | 0xF2 | 0x04 | 离散 | - | fault_manager 综合状态 |
| 0x02 | FRU电压 | 0x02 | 0x05 | 离散 | - | 各电压阈值状态 |
| 0x03 | FRU温度 | 0xF3 | 0x6F | 离散 | - | 过温/温度告警状态 |
| 0x04 | 输入电压 | 0x02 | 0x01 | 模拟 | V | PA2 VIN_V |
| 0x05 | +12V电压 | 0x02 | 0x01 | 模拟 | V | PA3 +12V-V |
| 0x06 | +5V电压 | 0x02 | 0x01 | 模拟 | V | PA4 +5V-V |
| 0x07 | +3.3V_AUX电压 | 0x02 | 0x01 | 模拟 | V | PA5 +3.3V-V |
| 0x08 | -12V电压 | 0x02 | 0x01 | 模拟 | V | PA6 -12V-V |
| 0x09 | +28V电压 | 0x02 | 0x01 | 模拟 | V | PA7 +28V-V |
| 0x0A | 输入电流 | 0x03 | 0x01 | 模拟 | A | PC0 VIN-I |
| 0x0B | +12V电流 | 0x03 | 0x01 | 模拟 | A | PC1 12V-I |
| 0x0C | +3.3V_AUX电流 | 0x03 | 0x01 | 模拟 | A | PC3 3.3V-I |
| 0x0D | +5V电流 | 0x03 | 0x01 | 模拟 | A | PC2 5V-I |
| 0x0E | 模块顶端边缘温度 | 0x01 | 0x01 | 模拟 | K | PA1 AD_TEMP，共用 |
| 0x0F | 模块正面散热器温度 | 0x01 | 0x01 | 模拟 | K | PA1 AD_TEMP，共用 |
| 0x10 | 模块中心温度 | 0x01 | 0x01 | 模拟 | K | PA1 AD_TEMP，共用 |
| 0x11 | 输入功耗 | 0x0B | 0x01 | 模拟 | W | 输入电压 × 输入电流 |
| 0x12 | +12V功耗 | 0x0B | 0x01 | 模拟 | W | +12V电压 × +12V电流 |
| 0x13 | +3.3V_AUX功耗 | 0x0B | 0x01 | 模拟 | W | +3.3V电压 × +3.3V电流 |
| 0x14 | +5V功耗 | 0x0B | 0x01 | 模拟 | W | +5V电压 × +5V电流 |

### 9.2 sensor_manager 数据结构

```c
typedef enum {
    SENSOR_KIND_DISCRETE = 0,
    SENSOR_KIND_ANALOG_U8,
    SENSOR_KIND_ANALOG_S8,
} sensor_kind_t;

typedef struct {
    uint8_t  sensor_id;
    const char *name;
    uint8_t  sensor_type;
    uint8_t  reading_type;
    uint8_t  entity_id;
    uint8_t  entity_instance;
    uint8_t  base_unit;
    sensor_kind_t kind;
    int16_t  m;
    int16_t  b;
    int8_t   r_exp;
    int8_t   b_exp;
    float    eng_value;
    uint8_t  raw_value;
    uint8_t  sensor_status;
    uint16_t event_status;
    uint8_t  lnr;
    uint8_t  lc;
    uint8_t  lnc;
    uint8_t  unc;
    uint8_t  uc;
    uint8_t  unr;
} ipmi_sensor_t;
```

### 9.3 raw 换算公式

IPMI SDR 中的工程值换算公式按如下方式使用：

```text
EngineeringValue = (M * Raw + B * 10^Bexp) * 10^Rexp
```

代码中同时需要反向换算：

```c
raw = round((EngineeringValue / pow10(Rexp) - B * pow10(Bexp)) / M);
```

### 9.4 建议 raw 比例表

| Sensor ID | 物理量 | raw 类型 | M | B | Rexp | Bexp | 换算 |
|---:|---|---|---:|---:|---:|---:|---|
| 0x04 | 输入电压 | unsigned | 2 | 0 | -1 | 0 | V = raw × 0.2 |
| 0x05 | +12V电压 | unsigned | 1 | 0 | -1 | 0 | V = raw × 0.1 |
| 0x06 | +5V电压 | unsigned | 2 | 0 | -2 | 0 | V = raw × 0.02 |
| 0x07 | +3.3V电压 | unsigned | 2 | 0 | -2 | 0 | V = raw × 0.02 |
| 0x08 | -12V电压 | signed 2's complement | 1 | 0 | -1 | 0 | V = signed(raw) × 0.1 |
| 0x09 | +28V电压 | unsigned | 2 | 0 | -1 | 0 | V = raw × 0.2 |
| 0x0A | 输入电流 | unsigned | 5 | 0 | -2 | 0 | A = raw × 0.05 |
| 0x0B | +12V电流 | unsigned | 5 | 0 | -2 | 0 | A = raw × 0.05 |
| 0x0C | +3.3V电流 | unsigned | 2 | 0 | -2 | 0 | A = raw × 0.02 |
| 0x0D | +5V电流 | unsigned | 2 | 0 | -2 | 0 | A = raw × 0.02 |
| 0x0E | 温度 K | unsigned | 1 | 200 | 0 | 0 | K = raw + 200 |
| 0x0F | 温度 K | unsigned | 1 | 200 | 0 | 0 | K = raw + 200 |
| 0x10 | 温度 K | unsigned | 1 | 200 | 0 | 0 | K = raw + 200 |
| 0x11 | 输入功耗 | unsigned | 1 | 0 | 0 | 0 | W = raw |
| 0x12 | +12V功耗 | unsigned | 1 | 0 | 0 | 0 | W = raw |
| 0x13 | +3.3V功耗 | unsigned | 1 | 0 | 0 | 0 | W = raw |
| 0x14 | +5V功耗 | unsigned | 1 | 0 | 0 | 0 | W = raw |

### 9.5 阈值配置表

#### 9.5.1 电压阈值

| 传感器 | LNC | LC | LNR | UNC | UC | UNR |
|---|---:|---:|---:|---:|---:|---:|
| 输入电压 | 12.0V | 10.0V | 9.0V | 36.0V | 38.0V | 40.0V |
| +12V | 10.8V | 10.2V | 9.6V | 13.2V | 13.8V | 14.4V |
| +5V | 4.5V | 4.25V | 4.0V | 5.5V | 5.75V | 6.0V |
| +3.3V | 2.97V | 2.80V | 2.64V | 3.63V | 3.80V | 3.96V |
| -12V | -10.8V | -10.2V | -9.6V | -13.2V | -13.8V | -14.4V |
| +28V | 25.2V | 23.8V | 22.4V | 30.8V | 32.0V | 33.6V |

> 对 -12V，由于数值越负幅值越大，阈值判断要单独处理：绝对值低于 10.8V 视为欠压，绝对值高于 13.2V 视为过压。SDR 中可使用 signed raw，事件状态由软件按实际语义生成。

#### 9.5.2 电流阈值

| 传感器 | 额定 | UNC | UC | UNR |
|---|---:|---:|---:|---:|
| 输入电流 | 按 90W/最低输入估算，建议 7.5A 以上需硬件确认 | 4.0A | 5.0A | 6.0A |
| +12V电流 | 8A | 8.8A | 12.0A | 16.0A |
| +3.3V电流 | 4A | 4.4A | 6.0A | 8.0A |
| +5V电流 | 2A | 2.2A | 3.0A | 4.0A |

> -12V 和 +28V 输出电流虽然 ADC 采集存在，但需求传感器表未列入 IPMI 上报功耗，V2 不作为独立 IPMI 传感器暴露，可用于内部保护。

#### 9.5.3 温度阈值

| 阈值 | 摄氏度 | 开尔文 |
|---|---:|---:|
| UNC | +85℃ | 358K |
| UC | +100℃ | 373K |
| UNR | +110℃ | 383K |
| LNC | -55℃ | 218K |

### 9.6 Get Sensor Reading

```text
NetFn Request  = 0x04
Cmd            = 0x2D
Request Data:
  Byte0 Sensor Number
```

Response Data：

| Byte | 字段 | 说明 |
|---:|---|---|
| 0 | Completion Code | 0x00 成功 |
| 1 | Sensor Reading | raw 值 |
| 2 | Sensor Status | 建议正常 0xC0 |
| 3 | Event/Threshold Status LSB | 阈值或离散事件 bit0~7 |
| 4 | Event/Threshold Status MSB | 阈值或离散事件 bit8~14 |

Sensor Status 建议：

```c
#define SENSOR_STATUS_EVENT_ENABLED     0x80u
#define SENSOR_STATUS_SCANNING_ENABLED  0x40u
#define SENSOR_STATUS_READING_UNAVAIL   0x20u
#define SENSOR_STATUS_NORMAL            0xC0u
```

### 9.7 模拟量阈值事件 bit 建议

```c
#define IPMI_EVT_LNC_ASSERT   (1u << 0)
#define IPMI_EVT_LC_ASSERT    (1u << 1)
#define IPMI_EVT_LNR_ASSERT   (1u << 2)
#define IPMI_EVT_UNC_ASSERT   (1u << 3)
#define IPMI_EVT_UC_ASSERT    (1u << 4)
#define IPMI_EVT_UNR_ASSERT   (1u << 5)
```

### 9.8 离散传感器状态 bit 建议

#### FRU健康，Sensor ID 0x01

| bit | 含义 |
|---:|---|
| 0 | 预测性故障，存在电压/电流/温度趋势告警 |
| 1 | 已发生保护动作 |
| 2 | 输入异常 |
| 3 | 输出异常 |
| 4 | 内部通信/ADC异常 |
| 5 | FRU/参数 CRC 异常 |

#### FRU电压，Sensor ID 0x02

| bit | 含义 |
|---:|---|
| 0 | 任一路电压未超限，正常 |
| 1 | 任一路电压超限 |
| 2 | 输入电压超限 |
| 3 | 输出电压超限 |

#### FRU温度，Sensor ID 0x03

| bit | 含义 |
|---:|---|
| 0 | 温度正常 |
| 1 | 温度超过 +85℃ 告警 |
| 2 | 温度超过 +100℃ 关断 |
| 3 | 温度传感器故障 |

### 9.9 Get Sensor Type

```text
NetFn Request  = 0x04
Cmd            = 0x2F
Request Data:
  Byte0 Sensor Number
```

Response Data：

| Byte | 字段 |
|---:|---|
| 0 | Completion Code |
| 1 | Sensor Type |
| 2 | Event/Reading Type Code |

若 Sensor Number 不存在，返回 `0xCB`。

### 9.10 Get Sensor Thresholds

```text
NetFn Request  = 0x04
Cmd            = 0x27
Request Data:
  Byte0 Sensor Number
```

Response Data：

| Byte | 字段 |
|---:|---|
| 0 | Completion Code |
| 1 | Readable Threshold Mask |
| 2 | LNR |
| 3 | LC |
| 4 | LNC |
| 5 | UNC |
| 6 | UC |
| 7 | UNR |

对模拟阈值传感器，返回转换后的 raw 阈值。对离散传感器，返回 `0xC1` 或 `0xCB`。建议对 0x01~0x03 返回 `0xC1`，表示该命令不适用于离散传感器。

---

## 10. Device SDR 实现

## 10.1 SDR 存储方式

V2 推荐不在 Flash 中存储固定二进制 SDR，而是由 `sdr_manager.c` 根据 `ipmi_sensor_t` 表在启动时生成：

```c
#define SDR_RECORD_COUNT        20u
#define SDR_MAX_RECORD_SIZE     64u

typedef struct {
    uint16_t record_id;
    uint8_t  data[SDR_MAX_RECORD_SIZE];
    uint8_t  len;
} sdr_record_t;

static sdr_record_t g_sdr_records[SDR_RECORD_COUNT];
```

优点：

1. 传感器表和 SDR 不会不一致；
2. 修改 M/B/Rexp、阈值、名称后自动生成对应 SDR；
3. 后续生成 C 代码时更容易维护。

### 10.2 SDR Record ID

| Sensor ID | SDR Record ID |
|---:|---:|
| 0x01 | 0x0001 |
| 0x02 | 0x0002 |
| ... | ... |
| 0x14 | 0x0014 |

`0xFFFF` 表示最后一条之后没有更多记录。

### 10.3 SDR Record 格式选择

为降低实现复杂度，V2 使用 **Full Sensor Record，Record Type = 0x01** 表示全部 20 个传感器。对于 0x01~0x03 离散传感器，阈值字段填 0，Event/Reading Type Code 使用需求表给定值。

若甲方使用非常严格的标准 SDR 解析器，后续可将 0x01~0x03 改为 Compact Sensor Record，但本 V2 先统一 Full Sensor Record，便于代码生成和调试。

### 10.4 Full Sensor Record 字段

| Offset | 字段 | 建议值/来源 |
|---:|---|---|
| 0~1 | Record ID | 0x0001~0x0014，LSB first |
| 2 | SDR Version | 0x51 |
| 3 | Record Type | 0x01 |
| 4 | Record Length | 从 offset 5 开始的数据长度 |
| 5 | Sensor Owner ID | 本模块 IPMB 地址，pm_addr_8bit |
| 6 | Sensor Owner LUN | 0x00 |
| 7 | Sensor Number | Sensor ID |
| 8 | Entity ID | 0x0A，Power Supply，若甲方另有规定则修改 |
| 9 | Entity Instance | ga_id 或 0x01 |
| 10 | Sensor Initialization | 0x67，扫描、事件、阈值初始化允许 |
| 11 | Sensor Capabilities | 0x68，自动重置、阈值访问能力按传感器类型设置 |
| 12 | Sensor Type | 传感器表 |
| 13 | Event/Reading Type | 传感器表 |
| 14~19 | Event Mask | 阈值传感器置 0x3F，离散按状态位定义 |
| 20 | Sensor Units 1 | unsigned/signed 数据格式 |
| 21 | Base Unit | V/A/K/W 等 |
| 22 | Modifier Unit | 0x00 |
| 23 | Linearization | 0x00，linear |
| 24~29 | M/B/Rexp/Bexp | 由比例表生成 |
| 30 | Analog Characteristic Flags | 0x00 |
| 31 | Nominal Reading | nominal raw |
| 32 | Normal Maximum | normal max raw |
| 33 | Normal Minimum | normal min raw |
| 34 | Sensor Maximum | max raw |
| 35 | Sensor Minimum | min raw |
| 36~41 | Thresholds | UNR/UC/UNC/LNR/LC/LNC，注意 IPMI 顺序 |
| 42 | Positive Hysteresis | 0x01，或 0 |
| 43 | Negative Hysteresis | 0x01，或 0 |
| 44~45 | Reserved | 0x00 |
| 46 | OEM | 0x00 |
| 47 | ID String Type/Length | 0xC0 | len，ASCII+Latin1 |
| 48.. | ID String | 传感器名称 |

### 10.5 M/B/Rexp/Bexp 编码

```c
typedef struct {
    int16_t m;       // signed 10-bit
    int16_t b;       // signed 10-bit
    int8_t  r_exp;   // signed 4-bit
    int8_t  b_exp;   // signed 4-bit
} sdr_linear_t;
```

编码函数：

```c
static uint8_t sdr_lo8(int16_t v)
{
    return (uint8_t)(v & 0xFF);
}

static uint8_t sdr_m_tolerance_byte(int16_t m, uint8_t tolerance)
{
    return (uint8_t)(((m >> 8) & 0x03u) | ((tolerance & 0x3Fu) << 2));
}

static uint8_t sdr_b_accuracy_lo_byte(int16_t b, uint8_t accuracy_lsb)
{
    return (uint8_t)(((b >> 8) & 0x03u) | ((accuracy_lsb & 0x3Fu) << 2));
}

static uint8_t sdr_exp_byte(int8_t r_exp, int8_t b_exp)
{
    return (uint8_t)(((b_exp & 0x0F) << 4) | (r_exp & 0x0F));
}
```

### 10.6 Get Device SDR Info

```text
NetFn Request  = 0x04
Cmd            = 0x20
Request Data   = 无
```

Response Data 建议：

| Byte | 字段 | 值 |
|---:|---|---:|
| 0 | Completion Code | 0x00 |
| 1 | Number of SDRs | 20 |
| 2 | Sensor Population Flags | 0x01，LUN0 有传感器，静态传感器集合 |

若后续传感器数量运行时可变，则 Byte2 置动态位；本项目固定 20 个，保持静态。

### 10.7 Reserve Device SDR Repository

```text
NetFn Request  = 0x04
Cmd            = 0x22
Request Data   = 无
```

Response Data：

| Byte | 字段 | 说明 |
|---:|---|---|
| 0 | Completion Code | 0x00 |
| 1 | Reservation ID LSB | 每次调用递增 |
| 2 | Reservation ID MSB | 每次调用递增 |

实现：

```c
static uint16_t g_sdr_reservation_id = 1;

uint16_t sdr_reserve(void)
{
    g_sdr_reservation_id++;
    if (g_sdr_reservation_id == 0) {
        g_sdr_reservation_id = 1;
    }
    return g_sdr_reservation_id;
}
```

### 10.8 Get Device SDR

```text
NetFn Request  = 0x04
Cmd            = 0x21
Request Data:
  Byte0 Reservation ID LSB
  Byte1 Reservation ID MSB
  Byte2 Record ID LSB
  Byte3 Record ID MSB
  Byte4 Offset into record
  Byte5 Bytes to read
```

Response Data：

| Byte | 字段 |
|---:|---|
| 0 | Completion Code |
| 1 | Next Record ID LSB |
| 2 | Next Record ID MSB |
| 3.. | Requested SDR bytes |

实现规则：

1. Record ID `0x0000` 表示读取第一条记录。
2. Record ID `0x0001~0x0014` 表示读取对应记录。
3. 最后一条记录的 Next Record ID 返回 `0xFFFF`。
4. `Bytes to read = 0xFF` 时，返回从 offset 到记录末尾，但不得超过 IPMB 最大响应长度。
5. 建议单次最多返回 32 字节 SDR 数据，避免 IPMB 帧过长。
6. 若 offset 超过记录长度，返回 `0xC9`。
7. 若 Record ID 不存在，返回 `0xCB`。

---

## 11. FRU 实现

## 11.1 FRU 目标

FRU 用于让上级管理器读取电源模块身份信息。V2 实现只读 FRU。

FRU Device ID 固定为：

```c
#define FRU_DEVICE_ID_POWER_MODULE    0x00u
```

### 11.2 FRU 存储方式

建议采用“启动时从配置生成 FRU 二进制缓存”的方式：

```c
#define FRU_AREA_MAX_SIZE       512u

static uint8_t  g_fru_area[FRU_AREA_MAX_SIZE];
static uint16_t g_fru_area_size;
```

产品序列号、硬件版本、生产日期可从 Flash 参数区读取；若参数区无效，使用默认值。

### 11.3 FRU 配置项

```c
#define FRU_MANUFACTURER        "TBD"
#define FRU_PRODUCT_NAME        "HBDC130-05EG-I Power Module"
#define FRU_PART_NUMBER         "HBDC130-05EG-I"
#define FRU_SERIAL_NUMBER_DEF   "0000000000"
#define FRU_HW_REV              "A"
#define FRU_FW_REV              "2.0.0"
#define FRU_FILE_ID             "HBDC130-IPMI-V2"
```

### 11.4 FRU Common Header

FRU Common Header 固定 8 字节：

| Byte | 字段 | 说明 |
|---:|---|---|
| 0 | Format Version | 0x01 |
| 1 | Internal Use Area Offset | 0x00，不使用 |
| 2 | Chassis Info Area Offset | 0x00，不使用 |
| 3 | Board Info Area Offset | 以 8 字节块为单位 |
| 4 | Product Info Area Offset | 以 8 字节块为单位 |
| 5 | MultiRecord Area Offset | 0x00，不使用 |
| 6 | Pad | 0x00 |
| 7 | Header Checksum | 8 字节加和为 0 |

### 11.5 Board Info Area

建议包含：

```text
Board Manufacturer:  TBD
Board Product Name:  HBDC130-05EG-I Power Module
Board Serial Number: 参数区序列号，默认 0000000000
Board Part Number:   HBDC130-05EG-I
FRU File ID:         HBDC130-IPMI-V2
```

字段格式：

```c
// type = 3 表示 8-bit ASCII/Latin1
field_header = 0xC0 | len;
```

结束字段：

```c
#define FRU_FIELD_END 0xC1u
```

Area 长度必须按 8 字节对齐，最后 1 字节为 checksum。

### 11.6 Product Info Area

建议包含：

```text
Manufacturer Name: TBD
Product Name:      HBDC130-05EG-I Power Module
Part/Model Number: HBDC130-05EG-I
Product Version:   HW:A FW:2.0.0
Product Serial:    参数区序列号
Asset Tag:         空或项目编号
FRU File ID:       HBDC130-IPMI-V2
```

### 11.7 Get FRU Inventory Area Info

```text
NetFn Request = 0x0A
Cmd           = 0x10
Request Data:
  Byte0 FRU Device ID，固定 0x00
```

Response Data：

| Byte | 字段 | 说明 |
|---:|---|---|
| 0 | Completion Code | 0x00 |
| 1 | FRU Area Size LSB | `g_fru_area_size` LSB |
| 2 | FRU Area Size MSB | `g_fru_area_size` MSB |
| 3 | Access Type | 0x00，按字节访问 |

错误处理：

| 情况 | 返回 |
|---|---|
| 请求长度不是 1 | 0xC7 |
| FRU Device ID 不是 0 | 0xCB |

### 11.8 Read FRU Data

```text
NetFn Request = 0x0A
Cmd           = 0x11
Request Data:
  Byte0 FRU Device ID
  Byte1 Offset LSB
  Byte2 Offset MSB
  Byte3 Count to read
```

Response Data：

| Byte | 字段 |
|---:|---|
| 0 | Completion Code |
| 1 | Count returned |
| 2.. | FRU data bytes |

实现规则：

1. FRU Device ID 必须为 0。
2. offset 不能超过 `g_fru_area_size - 1`。
3. count 不能为 0。
4. 实际返回长度 = `min(count, g_fru_area_size - offset, IPMB_MAX_DATA_PER_RESPONSE)`。
5. 建议 `IPMB_MAX_DATA_PER_RESPONSE = 32`。
6. 若 offset 越界，返回 `0xC9`。

---

## 12. Storage NetFn 命令处理

```c
static ipmi_cc_t ipmi_storage_dispatch(const ipmi_request_t *req, ipmi_response_t *rsp)
{
    switch (req->cmd) {
    case IPMI_CMD_GET_FRU_INV_AREA_INFO:
        return ipmi_cmd_get_fru_inventory_area_info(req, rsp);
    case IPMI_CMD_READ_FRU_DATA:
        return ipmi_cmd_read_fru_data(req, rsp);
    case IPMI_CMD_WRITE_FRU_DATA:
        return IPMI_CC_INVALID_CMD;  // V2 默认只读
    default:
        return IPMI_CC_INVALID_CMD;
    }
}
```

---

## 13. ADC、工程量与传感器缓存

### 13.1 ADC 通道映射

| 物理量 | ADC 引脚 | 传感器 |
|---|---|---|
| 温度 | PA1 ADC0_CH1 | 0x0E/0x0F/0x10 |
| 输入电压 | PA2 ADC0_CH2 | 0x04 |
| +12V电压 | PA3 ADC0_CH3 | 0x05 |
| +5V电压 | PA4 ADC0_CH4 | 0x06 |
| +3.3V电压 | PA5 ADC0_CH5 | 0x07 |
| -12V电压 | PA6 ADC0_CH6 | 0x08 |
| +28V电压 | PA7 ADC0_CH7 | 0x09 |
| 输入电流 | PC0 ADC_CH10 | 0x0A |
| +12V电流 | PC1 ADC_CH11 | 0x0B |
| +5V电流 | PC2 ADC_CH12 | 0x0D |
| +3.3V电流 | PC3 ADC_CH13 | 0x0C |
| -12V电流 | PC4 ADC_CH14 | 内部保护 |
| +28V电流 | PC5 ADC_CH15 | 内部保护 |

### 13.2 采样建议

1. 定时器触发 ADC + DMA。
2. 每通道 ≥1KSPS。
3. 每通道至少 16 点均值。
4. 对突变点做中值滤波或异常剔除。
5. 每 10ms 更新工程值，每 100ms 内更新 IPMI raw。
6. ADC 校准系数存放在 Flash 参数区。

### 13.3 功耗计算

```c
power_in_w  = vin_v * vin_i;
power_12_w  = v12_v * i12_a;
power_33_w  = v33_v * i33_a;
power_5_w   = v5_v  * i5_a;
```

结果超出 255W 时 raw 饱和到 255，并置 Sensor Status 或 FRU Health 告警位。

---

## 14. 故障状态与 IPMI 状态联动

### 14.1 故障来源

| 故障 | 来源 | 动作 |
|---|---|---|
| 输入欠压/过压 | VIN_V、PWOUT_TEST_MCU | 关闭输出，FAIL* 拉低 |
| 输出欠压/过压 | 各输出电压 ADC | 记录故障，按保护策略关断 |
| 输出过流/短路 | 各输出电流 ADC/硬件保护 | 快速关断对应输出 |
| 过温告警 | AD_TEMP > 85℃ | 设置 FRU温度告警 |
| 过温关断 | AD_TEMP > 100℃ | 关断输出，FAIL* 拉低 |
| ADC异常 | ADC 超时/越界 | 设置 FRU健康 bit4 |
| FRU/参数 CRC 异常 | Flash 校验 | 设置 FRU健康 bit5 |

### 14.2 FAIL* 联动

```c
if (fault_manager_has_active_fault()) {
    gpio_write(FAIL_MCU, LOW);
} else if (power_ctrl_enable_active()) {
    gpio_write(FAIL_MCU, HIGH);
}
```

### 14.3 Get Sensor Reading 中的事件状态

模拟传感器根据当前工程值生成阈值 bit：

```c
uint16_t sensor_calc_threshold_event(const ipmi_sensor_t *s)
{
    uint16_t evt = 0;
    int raw = s->raw_value;

    if (s->has_lower_thresholds) {
        if (raw <= s->lnc) evt |= IPMI_EVT_LNC_ASSERT;
        if (raw <= s->lc)  evt |= IPMI_EVT_LC_ASSERT;
        if (raw <= s->lnr) evt |= IPMI_EVT_LNR_ASSERT;
    }
    if (s->has_upper_thresholds) {
        if (raw >= s->unc) evt |= IPMI_EVT_UNC_ASSERT;
        if (raw >= s->uc)  evt |= IPMI_EVT_UC_ASSERT;
        if (raw >= s->unr) evt |= IPMI_EVT_UNR_ASSERT;
    }
    return evt;
}
```

-12V 传感器需要先按 signed raw 或绝对值专门判断，避免方向错误。

---

## 15. IPMI Dispatch 设计

### 15.1 Request 结构

```c
typedef struct {
    uint8_t rs_sa;
    uint8_t rq_sa;
    uint8_t netfn;
    uint8_t rq_lun;
    uint8_t rs_lun;
    uint8_t seq;
    uint8_t cmd;
    const uint8_t *data;
    uint8_t data_len;
} ipmi_request_t;
```

### 15.2 Response 结构

```c
typedef struct {
    uint8_t netfn;
    uint8_t cmd;
    uint8_t completion_code;
    uint8_t data[48];
    uint8_t data_len;
} ipmi_response_t;
```

### 15.3 分发函数

```c
static void ipmi_dispatch(const ipmi_request_t *req, ipmi_response_t *rsp)
{
    rsp->netfn = req->netfn + 1u;
    rsp->cmd = req->cmd;
    rsp->data_len = 0u;

    switch (req->netfn) {
    case IPMI_NETFN_APP:
        rsp->completion_code = ipmi_app_dispatch(req, rsp);
        break;
    case IPMI_NETFN_SENSOR_EVENT:
        rsp->completion_code = ipmi_sensor_dispatch(req, rsp);
        break;
    case IPMI_NETFN_STORAGE:
        rsp->completion_code = ipmi_storage_dispatch(req, rsp);
        break;
    default:
        rsp->completion_code = IPMI_CC_INVALID_CMD;
        break;
    }
}
```

### 15.4 不响应的情况

以下情况直接丢弃，不返回 Completion Code：

1. 目标地址不是本模块地址；
2. Checksum1 错误；
3. Checksum2 错误；
4. 帧长度小于 7 字节；
5. I2C 接收过程中发生总线错误导致帧不完整。

---

## 16. 典型通信示例

### 16.1 读取 +12V 电压

假设：

```text
PM_SA  = 0x72
BMC_SA = 0x20
NetFn  = 0x04
Cmd    = 0x2D
Sensor = 0x05
Seq    = 0x15
```

Request：

```text
72 10 7E 20 54 2D 05 5A
```

解析：

| Byte | 值 | 含义 |
|---:|---:|---|
| 0 | 72 | 电源模块地址 |
| 1 | 10 | NetFn=0x04, LUN=0 |
| 2 | 7E | Checksum1 |
| 3 | 20 | BMC 地址 |
| 4 | 54 | Seq=0x15, LUN=0 |
| 5 | 2D | Get Sensor Reading |
| 6 | 05 | +12V 电压 |
| 7 | 5A | Checksum2 |

若当前 +12V = 12.1V，raw = 121 = 0x79。

Response：

```text
20 14 CC 72 54 2D 00 79 C0 00 00 D4
```

### 16.2 读取 FRU 信息区大小

Request Data：

```text
FRU Device ID = 00
```

完整 Request 示例：

```text
72 28 66 20 58 10 00 78
```

说明：

```text
NetFn = 0x0A，NetFn/LUN = 0x28
Cmd = 0x10，Get FRU Inventory Area Info
Seq = 0x16，Seq/LUN = 0x58
```

若 FRU 区大小 256 字节，Response Data 为：

```text
00 00 01 00
```

其中：

```text
Completion = 00
Size LSB   = 00
Size MSB   = 01
Access     = 00，byte access
```

### 16.3 读取第一条 SDR

步骤：

1. BMC 发送 `Reserve Device SDR Repository`，获取 Reservation ID。
2. BMC 发送 `Get Device SDR`，Record ID = 0x0000，Offset = 0，Count = 0xFF 或 32。
3. 电源模块返回第一条 SDR 和 Next Record ID。
4. BMC 用 Next Record ID 继续读取，直到 Next Record ID = 0xFFFF。

---

## 17. Flash 参数区设计

### 17.1 参数区内容

建议在 16KB 参数/日志区中划分：

```text
param_header
calibration_table
fru_identity
fault_log_ring
backup_copy
```

### 17.2 FRU 身份参数

```c
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    char serial_number[32];
    char manufacturer[32];
    char product_name[64];
    char part_number[32];
    char hw_rev[16];
    char fw_rev[16];
    uint32_t crc32;
} fru_identity_t;
```

若 CRC 无效：

1. 使用默认 FRU 字符串；
2. Get Self Test Results 返回参数 CRC 告警；
3. FRU健康 bit5 置位。

---

## 18. 代码生成优先级

后续生成 MCU 代码时，建议按以下顺序：

1. `bsp_gpio`：GA/EN/INH/NVMRO/FAIL/SYSRESET/电源控制 GPIO。
2. `bsp_i2c`：I2C1 Slave RX + Master TX 基础驱动。
3. `ipmb_frame`：IPMB 帧解析与封装。
4. `ipmi_dispatch`：NetFn/Cmd 分发框架。
5. `sensor_manager`：20 个传感器表和假数据模式。
6. `ipmi_sensor`：Get Sensor Reading、Get Sensor Type。
7. `sdr_manager`：生成 20 条 SDR，支持 Get Device SDR Info/Reserve/Get Device SDR。
8. `fru_manager`：生成 FRU 二进制区，支持 Get FRU Info/Read FRU Data。
9. `bsp_adc_dma`：接入真实 ADC。
10. `fault_manager`：接入保护状态、FAIL*、离散传感器。
11. `param_storage`：接入校准系数、FRU 序列号、故障日志。
12. 联调脚本：用上位机发送 IPMB raw 帧验证每个命令。

---

## 19. 编译期配置头文件建议

建议新增 `ipmi_project_config.h`：

```c
#ifndef IPMI_PROJECT_CONFIG_H
#define IPMI_PROJECT_CONFIG_H

#define IPMB_PM_BASE_ADDR_8BIT       0x72u
#define IPMB_BMC_ADDR_8BIT           0x20u
#define IPMB_MAX_FRAME_LEN           64u
#define IPMB_MAX_RSP_DATA_LEN        48u
#define IPMB_MAX_READ_CHUNK          32u
#define IPMB_TX_QUEUE_DEPTH          4u
#define IPMB_TX_RETRY_MAX            3u

#define IPMI_NETFN_SENSOR_EVENT      0x04u
#define IPMI_NETFN_APP               0x06u
#define IPMI_NETFN_STORAGE           0x0Au

#define IPMI_CMD_GET_DEVICE_ID       0x01u
#define IPMI_CMD_GET_SELF_TEST       0x04u
#define IPMI_CMD_GET_DEVICE_SDR_INFO 0x20u
#define IPMI_CMD_GET_DEVICE_SDR      0x21u
#define IPMI_CMD_RESERVE_DEVICE_SDR  0x22u
#define IPMI_CMD_GET_SENSOR_THRESH   0x27u
#define IPMI_CMD_GET_SENSOR_READING  0x2Du
#define IPMI_CMD_GET_SENSOR_TYPE     0x2Fu
#define IPMI_CMD_GET_FRU_INFO        0x10u
#define IPMI_CMD_READ_FRU_DATA       0x11u
#define IPMI_CMD_WRITE_FRU_DATA      0x12u

#define FRU_DEVICE_ID_POWER_MODULE   0x00u
#define SENSOR_COUNT                 20u
#define SDR_RECORD_COUNT             SENSOR_COUNT

#endif
```

---

## 20. 联调测试清单

### 20.1 IPMB 基础测试

| 测试项 | 预期 |
|---|---|
| 地址匹配 | 只有写入本模块地址时响应 |
| Checksum1 错误 | 丢弃，不响应 |
| Checksum2 错误 | 丢弃，不响应 |
| 不支持 NetFn | 返回 0xC1 |
| 不支持 Cmd | 返回 0xC1 |
| Seq 回显 | Response Seq 与 Request 完全一致 |
| 多主仲裁 | ARLO 后退避重发，最终恢复监听 |

### 20.2 IPMI 命令测试

| 命令 | 测试内容 |
|---|---|
| Get Device ID | 返回 Device SDR/FRU/Sensor 能力 |
| Get Self Test Results | 正常返回 0x55/0x00 |
| Get Sensor Type | 0x01~0x14 返回正确类型；0x15 返回 0xCB |
| Get Sensor Reading | 20 个传感器均可读；模拟量 raw 与工程量一致 |
| Get Sensor Thresholds | 模拟量返回阈值 raw；离散量返回不支持 |
| Get Device SDR Info | 返回 20 条 SDR |
| Reserve Device SDR Repository | reservation ID 递增且非 0 |
| Get Device SDR | 可分段读完全部 20 条 SDR |
| Get FRU Inventory Area Info | 返回 FRU 大小和 byte access |
| Read FRU Data | 可分段读完整 FRU 区，checksum 正确 |

### 20.3 传感器联动测试

| 条件 | 预期 |
|---|---|
| +12V 正常 12.1V | Sensor 0x05 raw = 0x79 |
| 输入电压 <12V | 输入电压阈值 bit 置位，FRU电压异常，FAIL* 拉低 |
| 温度 >85℃ | 温度 UNC bit 置位，FRU温度告警 |
| 温度 >100℃ | UC/UNR bit 置位，关断输出，FAIL* 拉低 |
| 参数 CRC 错误 | FRU健康 bit5 置位，Self Test 反映异常 |

---

## 21. 仍需甲方确认的配置项

这些内容不会阻塞代码框架实现，但量产前必须确认：

1. IPMB 基地址和 GA[2:0] 映射公式。
2. BMC/ChMC 地址是否固定为 0x20。
3. Manufacturer ID、Product ID。
4. FRU Manufacturer、Product Name、Part Number、Serial Number 编码规则。
5. 各路电流保护阈值、输入电流最大值。
6. -12V 在上位机中的显示方式：signed raw 还是绝对值 + 负轨名称。
7. 离散传感器 0x01/0x02/0x03 的 VITA 46.11 OEM bit 定义是否有甲方专用要求。
8. 是否要求 I2C2/SM2/SM3 作为冗余 IPMB 总线。
9. 是否需要开放 Write FRU Data 或通过 RS232 参数命令写 FRU 身份信息。

---

## 22. V2 交付判据

V2 版本完成后应满足：

1. 标准多主 IPMB 通信可运行，电源模块能主动写回 Response。
2. ChMC/BMC 能通过 `Get Device ID` 识别模块能力。
3. ChMC/BMC 能通过 `Get Device SDR Info` 和 `Get Device SDR` 读取 20 条传感器 SDR。
4. ChMC/BMC 能通过 `Get Sensor Reading` 读取 20 个传感器实时数据。
5. ChMC/BMC 能通过 `Get FRU Inventory Area Info` 和 `Read FRU Data` 读取 FRU 身份信息。
6. ADC 工程量、raw 值、SDR 换算参数一致。
7. 故障状态能同步影响：
   - FAIL*；
   - 离散传感器；
   - 模拟传感器事件位；
   - Self Test / FRU健康状态。
8. 错误帧、非法命令、非法传感器 ID、非法 SDR/FRU offset 均返回正确 Completion Code 或按 IPMB 规则丢弃。

---

## 23. 参考依据

1. HBDC130-05EG-I 电源模块软件需求规格说明书 V1.1。
2. VITA90 电源模块技术需求。
3. IPMI Specification Second Generation v2.0。
4. Platform Management FRU Information Storage Definition。
5. IPMI 设备 SDR、传感器和 FRU 命令的行业通用实现惯例。
