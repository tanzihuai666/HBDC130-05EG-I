# HBDC130-05EG-I_GPT 固件工程

本目录是在 `SRYM-HBDC130-05EG-I_V1.0` 最小 GD32F103RCT6 模板基础上建立的 GPT 实现版本，目标是按项目需求实现电源控制、ADC 采集、故障保护、IPMB/IPMI、FRU 和 SDR。

## 当前实现范围

- 已建立独立工程目录：`SRYM-HBDC130-05EG-I_GPT`。
- 新增应用层、协议层、驱动层目录，按 V2 方案拆分模块。
- 已实现核心 IPMB 帧解析/封装、IPMI 命令分发框架。
- 已实现必须命令的主体逻辑：
  - Get Device ID
  - Get Self Test Results
  - Get Sensor Reading
  - Get Sensor Type
  - Get Sensor Thresholds
  - Get Device SDR Info
  - Reserve Device SDR Repository
  - Get Device SDR
  - Get FRU Inventory Area Info
  - Read FRU Data
- 已建立 20 个传感器表、raw 换算、默认阈值与功耗计算框架。
- 已实现 GPIO 电源控制逻辑骨架，使用需求中的 GD32F103RCT6 引脚定义。

## 目录说明

```text
SRYM-HBDC130-05EG-I_GPT/
├── app/          电源控制、传感器、FRU、SDR、故障管理
├── protocol/     IPMB/IPMI 协议栈
├── driver/       GD32 外设适配层
├── project/      Keil uVision 工程文件占位
└── README_GPT.md
```

## 重要说明

1. 当前版本优先提交可审查的源代码框架和核心协议逻辑，I2C 多主硬件细节后续需要结合板级实测完善。
2. 新工程默认复用原模板的 GD32 SDK 路径，避免重复提交大量官方库文件。
3. IPMB 地址默认采用 `0x72 + (GA_ID << 1)`，BMC/ChMC 地址默认 `0x20`，后续可在 `app_config.h` 中修改。
4. ADC 采样转换系数暂采用默认比例，量产前必须根据硬件分压、运放增益、电流采样电阻和 NTC 参数校准。
