/**
 * @file    bsp_timer.c
 * @brief   SysTick 1ms 系统时基实现。
 *          本模块使用 Cortex-M3 内核 SysTick 定时器产生 1ms 中断，并维护一个 32 位毫秒
 *          计数器，供主循环中的 1ms、10ms、100ms 周期任务调度使用。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#include "bsp_timer.h"
#include "gd32f10x.h"

/*
 * 上电后累计的毫秒计数。
 * 该变量在 SysTick 中断中写、在主循环中读，因此必须声明为 volatile。
 */
static volatile uint32_t g_bsp_ms;

/**
 * @brief  SysTick 中断服务函数。
 * @param  无。
 * @retval 无。
 * @note   初始化完成后每 1ms 进入一次，仅执行自增操作，避免在高频中断中加入耗时逻辑。
 */
void SysTick_Handler(void)
{
    g_bsp_ms++;
}

/**
 * @brief  将 SysTick 初始化为 1ms 周期。
 * @param  无。
 * @retval 无。
 */
void bsp_timer_init_1ms(void)
{
    /* 上电初始化时清零系统时基。 */
    g_bsp_ms = 0u;

    /* SysTick_Config() 参数为每次中断所需的内核时钟周期数。 */
    SysTick_Config(SystemCoreClock / 1000u);
}

/**
 * @brief  获取当前系统毫秒计数。
 * @param  无。
 * @retval 从 bsp_timer_init_1ms() 调用后累计的毫秒数。
 * @note   32 位计数在约 49.7 天后回绕；调用方应采用无符号减法判断时间间隔。
 */
uint32_t bsp_millis(void)
{
    return g_bsp_ms;
}

/**
 * @brief  基于系统毫秒计数执行阻塞延时。
 * @param  ms 需要延时的毫秒数。
 * @retval 无。
 * @warning 该函数会占用 CPU，只适合初始化和调试，不应在主循环关键路径或中断中使用。
 */
void bsp_delay_ms(uint32_t ms)
{
    uint32_t start_ms;

    start_ms = bsp_millis();

    /* 使用无符号减法，可正确处理 32 位计数器回绕。 */
    while ((uint32_t)(bsp_millis() - start_ms) < ms) {
        /* 等待 SysTick 中断更新 g_bsp_ms。 */
    }
}
