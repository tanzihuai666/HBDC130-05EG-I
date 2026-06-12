# GPT 代码审查落实记录

## 依据

- 用户上传的《SRYM-HBDC130-05EG-I_GPT 代码审查报告》
- 软件需求规格说明书_GD32F103RCT6 V1.1
- HBDC130-05EG-I IPMB/IPMI + FRU/SDR V2 实现方案

## 本轮已落实

1. `driver/bsp_i2c.c/.h`
   - 实现 I2C1 初始化为 7-bit slave，400 kbit/s。
   - 实现 Slave RX 中断接收：ADDR/RBNE/STOP 处理。
   - 实现 Master TX 轮询发送：START、地址、数据、STOP。
   - 增加 BE/ARLO/NACK/RXORE 错误清除和错误计数。
   - 增加总线恢复：禁用 I2C、GPIO 模拟 9 个 SCL 脉冲、重新初始化。

2. `protocol/ipmb_driver.c/.h`
   - 新增 IPMB 集成层。
   - 将 I2C RX 回调接入 `ipmb_parse_request()`。
   - 接入 `ipmi_dispatch()`。
   - 接入 `ipmb_build_response()`。
   - 增加 Response 队列和重试计数。

3. `app/main.c`
   - 初始化 `ipmb_driver`。
   - 主循环调用 `ipmb_driver_task()`，形成 Request/Response 链路。

4. `app/sensor_manager.c/.h`
   - 增加 `threshold_mask`。
   - 按 V2 方案初始化电压、电流、温度阈值 raw 值。
   - 修复阈值为 0 时无法表达的问题。
   - 增加 -12V signed raw 专用阈值判断。

5. `protocol/ipmi_dispatch.c`
   - `Get Sensor Thresholds` 返回真实 `threshold_mask` 和阈值 raw。
   - `Get Self Test Results` 可反映当前故障位。

6. `app/sdr_manager.c`
   - SDR 记录写入真实阈值 mask。
   - M/B 高位编码改成显式 10-bit 处理。

7. `app/fru_manager.c`
   - Product Info Area 补入 Language Code。
   - Board/Product Area 生成函数拆分，字段更清晰。
   - FRU 读取前校验 Common Header checksum。

8. `driver/bsp_flash.c`
   - 实现参数区页擦除。
   - 实现 32-bit word 写入和回读校验。
   - 保留 CRC32。

9. `app/power_ctrl.c/.h`、`driver/bsp_gpio.c`、`app/fault_manager.c`
   - 消除 `power_ctrl` 内部 GPIO 二次初始化。
   - 统一通过 BSP GPIO 读写硬件。
   - 增加 EN/INH/NVMRO 5ms 去抖。
   - 增加 PB8 `PWOUT_TEST_MCU` 输入检测并接入故障管理。

10. `project/HBDC130-05EG-I_GPT.uvprojx`
    - 新增 `ipmb_driver.c` 到 Protocol 分组。

## 本轮明确暂不实现

按用户要求，以下审查报告条目本轮不实现：

- 3.4 RS232 调试与固件升级
- 3.5 同步信号处理
- 3.6 SYSRESET* 处理

因此 USART1 仍仅保持基础初始化和发送函数；PA11/PA12、PB15 仅做安全 GPIO 配置，不做功能状态机。

## 与审查报告不完全一致处

1. 审查报告指出 FRU Board Info Area 缺少 Language Code/Mfg Time。实际旧代码中 Board Area 已包含 Format、Length、Language、3 字节 Mfg Time；真正缺失的是 Product Info Area 的 Language Code。本轮已修复 Product Area。
2. 审查报告指出 SDR 阈值字段顺序可能错误，但同时说明代码顺序符合 IPMI Full Sensor Record；本轮保留顺序，并补入真实阈值 mask。

## 仍需本地 Keil 验证

我无法在当前环境运行 Keil ARMCC。请在 Keil 中重新 Clean/Rebuild。若出现 GD32 SPL API 名称、结构体字段、I2C 标志位清除细节相关编译错误或运行问题，继续依据编译日志修正。
