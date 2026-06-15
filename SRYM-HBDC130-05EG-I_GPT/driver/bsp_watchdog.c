/**
 * @file    bsp_watchdog.c
 * @brief   独立看门狗 IWDG 板级驱动实现。
 *          主循环只有在电源控制、IPMB、ADC、故障检测等关键任务均正常返回后才喂狗，
 *          用于检测主循环卡死、外设死等或不可恢复的软件异常。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#include "bsp_watchdog.h"
#include "app_config.h"
#include "gd32f10x.h"

/* IWDG 预分频系数。LSI 约 40kHz，64 分频后计数频率约 625Hz。 */
#define BSP_IWDG_PRESCALER      IWDG_PRESCALER_64

/* 重装值 625，对应理论超时时间约 1 秒。实际时间会随 LSI 误差变化。 */
#define BSP_IWDG_RELOAD_VALUE   625u

/**
 * @brief  初始化并启动独立看门狗。
 * @param  无。
 * @retval 无。
 * @note   IWDG 启动后通常只能通过芯片复位停止，因此必须确保主循环能够持续喂狗。
 */
void bsp_watchdog_init(void)
{
    /* 允许写入预分频和重装寄存器。 */
    IWDG_Write_Enable(IWDG_WRITEACCESS_ENABLE);

    /* 配置约 1 秒超时时间。 */
    IWDG_SetPrescaler(BSP_IWDG_PRESCALER);
    IWDG_SetReloadValue(BSP_IWDG_RELOAD_VALUE);

    /* 启动前先装载一次计数器，避免使用旧值。 */
    IWDG_ReloadCounter();
    IWDG_Enable();

    APP_LOGI("Watchdog initialized: timeout approximately 1s");
}

/**
 * @brief  重装独立看门狗计数器。
 * @param  无。
 * @retval 无。
 * @note   应在主循环所有关键任务正常完成后调用，不应在中断中无条件调用。
 */
void bsp_watchdog_feed(void)
{
    IWDG_ReloadCounter();
}
