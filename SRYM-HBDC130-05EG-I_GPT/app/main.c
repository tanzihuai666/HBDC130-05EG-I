/**
 * @file    main.c
 * @brief   系统入口、初始化顺序和裸机主循环调度实现。
 *          程序采用无操作系统的前后台结构：中断层只完成 SysTick、I2C、DMA 等必要操作，
 *          主循环按 1ms、10ms、100ms 周期依次调度电源控制、IPMB、ADC、故障检测和
 *          传感器刷新任务，所有关键任务正常返回后才喂独立看门狗。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#include "gd32f10x.h"
#include "power_ctrl.h"
#include "sensor_manager.h"
#include "fault_manager.h"
#include "ipmi_dispatch.h"
#include "ipmb_driver.h"
#include "bsp_gpio.h"
#include "bsp_timer.h"
#include "bsp_adc_dma.h"
#include "bsp_uart.h"
#include "bsp_watchdog.h"
#include "bsp_i2c.h"

/**
 * @brief  完成板级外设、应用模块和 IPMI/IPMB 协议栈初始化。
 * @param  无。
 * @retval 无。
 *
 * @note   初始化顺序必须保持：
 *         1. 系统时钟和 SysTick 先初始化，后续模块才可使用毫秒时基；
 *         2. GPIO 先进入安全默认态，避免电源输出在初始化过程中误动作；
 *         3. USART1 先初始化，再允许任何日志宏输出；
 *         4. 电源控制先读取 GA 地址，再用最终地址初始化 IPMI、IPMB 和 I2C1；
 *         5. I2C1 最后开启，避免协议数据库尚未准备好时收到 BMC 请求；
 *         6. 看门狗最后启动，防止初始化过程尚未完成就发生误复位。
 */
static void board_init(void)
{
    uint8_t own_addr_8bit;

    /* 初始化系统时钟。启动文件通常已经调用一次，再次调用用于确保时钟配置一致。 */
    SystemInit();

    /* 建立 1ms 系统时基。 */
    bsp_timer_init_1ms();

    /* 将电源控制、FAIL 和收发器使能引脚设置到安全默认电平。 */
    bsp_gpio_init();

    /* 初始化 115200、8N1 调试串口；此行之后才允许调用 APP_LOGx。 */
    bsp_uart1_init(115200u);
    APP_LOGI("Boot started: HBDC130-05EG-I_GPT firmware=%02X.%02X",
             (unsigned int)APP_FW_MAJOR_BCD,
             (unsigned int)APP_FW_MINOR_BCD);

    /* 启动 ADC1 连续扫描和 DMA 循环采样。 */
    bsp_adc_dma_init();

    /* 读取 GA[2:0]、EN、INH、NVMRO 初始状态，并保持电源输出关闭。 */
    power_ctrl_init();
    own_addr_8bit = power_ctrl_get_ipmb_addr_8bit();

    /*
     * IPMI 初始化会生成传感器、FRU 和 SDR 数据库；
     * IPMB 驱动随后注册 I2C 接收回调，但此时 I2C1 尚未对外响应。
     */
    ipmi_init(own_addr_8bit, power_ctrl_get_ga_id());
    ipmb_driver_init(own_addr_8bit);

    /* I2C 硬件使用 7 位地址，因此将内部 8 位 IPMB 地址右移 1 位。 */
    bsp_i2c1_ipmb_init((uint8_t)(own_addr_8bit >> 1));

    /* 初始化故障位并释放低有效 FAIL 输出。 */
    fault_manager_init();

    APP_LOGI("Protocol ready: GA=%u IPMB8=0x%02X I2C7=0x%02X",
             (unsigned int)power_ctrl_get_ga_id(),
             (unsigned int)own_addr_8bit,
             (unsigned int)(own_addr_8bit >> 1));

    /* 所有关键模块准备完成后再启动独立看门狗。 */
    bsp_watchdog_init();
    APP_LOGI("System initialization completed");
}

/**
 * @brief  固件主函数。
 * @param  无。
 * @retval 裸机程序不会正常返回，返回类型保留为 C 标准 main() 形式。
 */
int main(void)
{
    uint32_t last_10ms;
    uint32_t last_100ms;

    last_10ms = 0u;
    last_100ms = 0u;

    board_init();

    while (1) {
        uint32_t now_ms;

        /* 保存本轮调度使用的统一时间快照，避免同一轮多次读取产生边界差异。 */
        now_ms = bsp_millis();

        /*
         * 1ms 高频任务：对 EN、INH、NVMRO 做去抖并更新电源状态机。
         * 该函数必须频繁调用，才能保证输入确认时间与 CTRL_DEBOUNCE_MS 一致。
         */
        power_ctrl_task_1ms();

        /*
         * IPMB 非固定周期任务：处理 I2C 中断提交的 Request，并尝试发送排队的 Response。
         * 每轮主循环都调用可降低协议响应延迟。
         */
        ipmb_driver_task();

        /* 使用无符号减法判断周期，可正确处理 32 位毫秒计数回绕。 */
        if ((uint32_t)(now_ms - last_10ms) >= 10u) {
            bsp_adc_values_t adc_values;

            last_10ms = now_ms;

            /* 对 DMA 循环缓存做 16 点平均，并完成电压、电流和温度线性换算。 */
            bsp_adc_dma_task_10ms();
            bsp_adc_get_values(&adc_values);

            /*
             * 将最新 ADC 工程量写入传感器管理层。
             * 当前 IPMI 传感器只使用输入、+12V、+5V、+3.3V 四路电流；
             * -12V 和 +28V 电流仍保存在 BSP 工程量结构中，供后续扩展使用。
             */
            sensor_set_adc_values(adc_values.vin_v,
                                  adc_values.v12_v,
                                  adc_values.v5_v,
                                  adc_values.v33_v,
                                  adc_values.vm12_v,
                                  adc_values.v28_v,
                                  adc_values.vin_i,
                                  adc_values.i12_a,
                                  adc_values.i33_a,
                                  adc_values.i5_a,
                                  adc_values.temp_c);

            /* 基于最新工程量和 PWOUT_TEST 输入更新故障位及 FAIL 输出。 */
            fault_manager_task_10ms();
        }

        if ((uint32_t)(now_ms - last_100ms) >= 100u) {
            last_100ms = now_ms;

            /* 将工程量转换成 IPMI 8 位 raw，并重新计算阈值事件状态。 */
            sensor_manager_task_100ms();
        }

        /* 只有主循环全部关键任务正常返回后才喂狗，避免掩盖软件卡死。 */
        bsp_watchdog_feed();
    }
}
