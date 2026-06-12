/**
 * @file    power_ctrl.c
 * @brief   电源控制状态机。
 *          读取 EN/INH/NVMRO/GA/PWOUT_TEST 等硬件信号，按照需求控制 EN1、INH1、INH2，
 *          同时计算电源模块 IPMB 地址。GPIO 方向统一由 bsp_gpio.c 初始化，本模块只做业务逻辑。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#include "power_ctrl.h"
#include "bsp_gpio.h"

/* 输入去抖时间：1ms 任务调用一次，5 次连续采样后确认状态变化。 */
#define CTRL_DEBOUNCE_MS       5u

/*
 * GPIO mapping from software requirement:
 * EN input      PB13, MCU side low active after inverter
 * INH input     PB14, MCU side low active after inverter
 * NVMRO input   PB9,  MCU side low active after inverter
 * PWOUT_TEST    PB8,  low = input voltage normal, high = abnormal
 * EN1 output    PC12, high enables +3.3V_AUX/+5V
 * INH2 output   PB4,  low enables +12V
 * INH1 output   PB5,  low enables +28V/-12V
 * FAIL output   PC6,  low active, handled by fault_manager via BSP
 * GA0/GA1/GA2   PC7/PC8/PA8
 */

static power_state_t g_power_state = POWER_STATE_OFF;      /* 当前电源状态。 */
static uint8_t g_ga_id;                                    /* GA[2:0] 槽位 ID。 */
static uint8_t g_ipmb_addr_8bit = IPMB_PM_BASE_ADDR_8BIT;  /* 当前模块 IPMB 8-bit 地址。 */
static uint8_t g_en_cnt;                                   /* EN 去抖计数。 */
static uint8_t g_inh_cnt;                                  /* INH 去抖计数。 */
static uint8_t g_nvmro_cnt;                                /* NVMRO 去抖计数。 */
static bool g_en_active;                                   /* EN 有效标志，低有效。 */
static bool g_inh_active;                                  /* INH 有效标志，低有效。 */
static bool g_nvmro_allows_update;                         /* NVMRO 允许运行/更新标志，低有效。 */
static power_state_t g_last_logged_state = POWER_STATE_OFF; /* 日志去重用状态。 */

/**
 * @brief  读取低有效 GPIO 信号。
 * @param  port GPIO 端口。
 * @param  pin  GPIO 引脚。
 * @retval true=信号有效，false=信号无效。
 */
static bool input_low_active(GPIO_TypeDef *port, uint16_t pin)
{
    return (bsp_gpio_read(port, pin) == BSP_GPIO_LOW) ? true : false;
}

/**
 * @brief  对布尔输入信号做 1ms 粒度去抖。
 * @param  sample 当前采样值。
 * @param  cnt    去抖计数器。
 * @param  state  稳定状态输出。
 * @retval 无。
 */
static void debounce_bool(bool sample, uint8_t *cnt, bool *state)
{
    if (sample == *state) {
        *cnt = 0u;
    } else if (*cnt >= CTRL_DEBOUNCE_MS) {
        *state = sample;
        *cnt = 0u;
    } else {
        (*cnt)++;
    }
}

/** @brief 控制辅助电源/5V 使能。@param en true=开启。@retval 无。 */
static void rail_aux_5v_enable(bool en)
{
    bsp_gpio_write(GPIOC, GPIO_PIN_12, en ? BSP_GPIO_HIGH : BSP_GPIO_LOW);
}

/** @brief 控制 +12V 使能，硬件 INH2 为低使能。@param en true=开启。@retval 无。 */
static void rail_12v_enable(bool en)
{
    bsp_gpio_write(GPIOB, GPIO_PIN_4, en ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
}

/** @brief 控制 +28V/-12V 使能，硬件 INH1 为低使能。@param en true=开启。@retval 无。 */
static void rail_28v_m12v_enable(bool en)
{
    bsp_gpio_write(GPIOB, GPIO_PIN_5, en ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
}

/**
 * @brief  按需求顺序更新电源轨输出。
 * @param  aux5    true=开启辅助/5V 相关轨。
 * @param  v12     true=开启 +12V。
 * @param  v28_m12 true=开启 +28V/-12V。
 * @retval 无。
 */
static void apply_outputs(bool aux5, bool v12, bool v28_m12)
{
    /* 关断时先关后级大功率输出，再关前级/辅助电源，降低异常瞬态。 */
    if ((!v12) || (!v28_m12)) {
        rail_12v_enable(false);
        rail_28v_m12v_enable(false);
    }

    rail_aux_5v_enable(aux5);

    if (v12) {
        rail_12v_enable(true);
    }
    if (v28_m12) {
        rail_28v_m12v_enable(true);
    }
}

/**
 * @brief  电源控制初始化。
 * @param  无。
 * @retval 无。
 */
void power_ctrl_init(void)
{
    /* GPIO 方向和安全默认态已经在 bsp_gpio_init() 中完成。 */
    g_ga_id = 0u;
    if (bsp_gpio_read(GPIOC, GPIO_PIN_7) == BSP_GPIO_HIGH) g_ga_id |= 0x01u;
    if (bsp_gpio_read(GPIOC, GPIO_PIN_8) == BSP_GPIO_HIGH) g_ga_id |= 0x02u;
    if (bsp_gpio_read(GPIOA, GPIO_PIN_8) == BSP_GPIO_HIGH) g_ga_id |= 0x04u;
    g_ipmb_addr_8bit = (uint8_t)(IPMB_PM_BASE_ADDR_8BIT + (g_ga_id << 1));

    g_en_active = input_low_active(GPIOB, GPIO_PIN_13);
    g_inh_active = input_low_active(GPIOB, GPIO_PIN_14);
    g_nvmro_allows_update = input_low_active(GPIOB, GPIO_PIN_9);
    g_en_cnt = 0u;
    g_inh_cnt = 0u;
    g_nvmro_cnt = 0u;

    apply_outputs(false, false, false);
    g_power_state = POWER_STATE_OFF;
    g_last_logged_state = g_power_state;
    APP_LOGI("power_ctrl: init EN=%u INH=%u NVMRO=%u", g_en_active, g_inh_active, g_nvmro_allows_update);
}

/**
 * @brief  1ms 电源控制任务。
 * @param  无。
 * @retval 无。
 */
void power_ctrl_task_1ms(void)
{
    debounce_bool(input_low_active(GPIOB, GPIO_PIN_13), &g_en_cnt, &g_en_active);
    debounce_bool(input_low_active(GPIOB, GPIO_PIN_14), &g_inh_cnt, &g_inh_active);
    debounce_bool(input_low_active(GPIOB, GPIO_PIN_9), &g_nvmro_cnt, &g_nvmro_allows_update);

    if ((!g_nvmro_allows_update) || (!g_en_active)) {
        apply_outputs(false, false, false);
        g_power_state = POWER_STATE_OFF;
    } else if (g_inh_active) {
        apply_outputs(true, false, true);
        g_power_state = POWER_STATE_INHIBIT;
    } else {
        apply_outputs(true, true, true);
        g_power_state = POWER_STATE_ON;
    }

    if (g_power_state != g_last_logged_state) {
        APP_LOGI("power_ctrl: state %u -> %u", g_last_logged_state, g_power_state);
        g_last_logged_state = g_power_state;
    }
}

/**
 * @brief  故障联动强制关断。
 * @param  无。
 * @retval 无。
 */
void power_ctrl_force_off(void)
{
    apply_outputs(false, false, false);
    g_power_state = POWER_STATE_FAULT;
    if (g_power_state != g_last_logged_state) {
        APP_LOGW("power_ctrl: force off by fault");
        g_last_logged_state = g_power_state;
    }
}

/** @brief 获取当前电源状态。@param 无。@retval power_state_t 当前状态。 */
power_state_t power_ctrl_get_state(void)
{
    return g_power_state;
}

/** @brief 获取 EN 是否有效。@param 无。@retval true=EN 有效。 */
bool power_ctrl_enable_active(void)
{
    return g_en_active;
}

/** @brief 获取 INH 是否有效。@param 无。@retval true=INH 有效。 */
bool power_ctrl_inhibit_active(void)
{
    return g_inh_active;
}

/** @brief 获取 NVMRO 是否允许运行/更新。@param 无。@retval true=允许。 */
bool power_ctrl_nvmro_allows_update(void)
{
    return g_nvmro_allows_update;
}

/**
 * @brief  检查 PWOUT_TEST_MCU 输入。
 * @param  无。
 * @retval true=输入电压检测正常，false=异常。
 */
bool power_ctrl_input_voltage_ok(void)
{
    return (bsp_gpio_read(GPIOB, GPIO_PIN_8) == BSP_GPIO_LOW) ? true : false;
}

/** @brief 获取 GA 槽位 ID。@param 无。@retval 0~7 槽位 ID。 */
uint8_t power_ctrl_get_ga_id(void)
{
    return g_ga_id;
}

/** @brief 获取 IPMB 8-bit 地址。@param 无。@retval IPMB 8-bit 地址。 */
uint8_t power_ctrl_get_ipmb_addr_8bit(void)
{
    return g_ipmb_addr_8bit;
}
