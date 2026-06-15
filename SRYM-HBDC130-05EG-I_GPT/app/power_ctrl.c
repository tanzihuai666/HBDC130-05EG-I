/**
 * @file    power_ctrl.c
 * @brief   电源控制状态机实现。
 *          本模块读取 EN、INH、NVMRO、GA 和 PWOUT_TEST 等硬件信号，按照软件需求控制
 *          EN1、INH1、INH2 三个输出，并根据 GA[2:0] 计算电源模块 IPMB 地址。
 *          GPIO 方向和上电安全电平统一由 bsp_gpio.c 配置，本模块只处理控制逻辑。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#include "power_ctrl.h"
#include "bsp_gpio.h"

/* 1ms 任务连续采样 5 次后确认输入变化，即去抖时间约为 5ms。 */
#define CTRL_DEBOUNCE_MS       5u

/*
 * 硬件引脚对应关系：
 * PB13：EN 输入，MCU 侧低电平有效；
 * PB14：INH 输入，MCU 侧低电平有效；
 * PB9 ：NVMRO 输入，MCU 侧低电平表示允许运行或更新；
 * PB8 ：PWOUT_TEST_MCU 输入，低电平表示输入电压正常；
 * PC12：EN1 输出，高电平使能 +3.3V_AUX 和 +5V；
 * PB4 ：INH2 输出，低电平使能 +12V；
 * PB5 ：INH1 输出，低电平使能 +28V 和 -12V；
 * PC6 ：FAIL 输出，低电平有效，由 fault_manager 统一控制；
 * PC7、PC8、PA8：分别对应 GA0、GA1、GA2。
 */

static power_state_t g_power_state = POWER_STATE_OFF;      /* 当前电源状态机状态。 */
static uint8_t g_ga_id;                                    /* GA[2:0] 组合得到的槽位号。 */
static uint8_t g_ipmb_addr_8bit = IPMB_PM_BASE_ADDR_8BIT;  /* 当前模块 8 位 IPMB 地址。 */
static uint8_t g_en_cnt;                                   /* EN 输入去抖计数器。 */
static uint8_t g_inh_cnt;                                  /* INH 输入去抖计数器。 */
static uint8_t g_nvmro_cnt;                                /* NVMRO 输入去抖计数器。 */
static bool g_en_active;                                   /* 去抖后的 EN 有效状态。 */
static bool g_inh_active;                                  /* 去抖后的 INH 有效状态。 */
static bool g_nvmro_allows_update;                         /* 去抖后的 NVMRO 允许状态。 */
static power_state_t g_last_logged_state = POWER_STATE_OFF; /* 最近一次已打印的状态。 */

/**
 * @brief  读取一个低电平有效的 GPIO 输入。
 * @param  port GPIO 端口。
 * @param  pin  GPIO 引脚。
 * @retval true=低电平有效；false=高电平无效。
 */
static bool input_low_active(GPIO_TypeDef *port, uint16_t pin)
{
    return (bsp_gpio_read(port, pin) == BSP_GPIO_LOW) ? true : false;
}

/**
 * @brief  对布尔输入执行连续采样去抖。
 * @param  sample 当前 1ms 采样值。
 * @param  cnt    去抖计数器指针。
 * @param  state  稳定状态指针。
 * @retval 无。
 */
static void debounce_bool(bool sample, uint8_t *cnt, bool *state)
{
    if (sample == *state) {
        /* 采样值与稳定值一致，不存在状态变化。 */
        *cnt = 0u;
    } else if (*cnt >= CTRL_DEBOUNCE_MS) {
        /* 新状态保持时间达到要求，正式更新稳定状态。 */
        *state = sample;
        *cnt = 0u;
    } else {
        /* 新状态尚未稳定，继续累计确认时间。 */
        (*cnt)++;
    }
}

/**
 * @brief  控制 +3.3V_AUX/+5V 前级使能。
 * @param  en true=开启；false=关闭。
 * @retval 无。
 */
static void rail_aux_5v_enable(bool en)
{
    bsp_gpio_write(GPIOC, GPIO_PIN_12, en ? BSP_GPIO_HIGH : BSP_GPIO_LOW);
}

/**
 * @brief  控制 +12V 电源轨，INH2 为低电平使能。
 * @param  en true=开启；false=关闭。
 * @retval 无。
 */
static void rail_12v_enable(bool en)
{
    bsp_gpio_write(GPIOB, GPIO_PIN_4, en ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
}

/**
 * @brief  控制 +28V/-12V 电源轨，INH1 为低电平使能。
 * @param  en true=开启；false=关闭。
 * @retval 无。
 */
static void rail_28v_m12v_enable(bool en)
{
    bsp_gpio_write(GPIOB, GPIO_PIN_5, en ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
}

/**
 * @brief  按安全顺序更新三个电源控制输出。
 * @param  aux5    true=开启辅助电源和 +5V。
 * @param  v12     true=开启 +12V。
 * @param  v28_m12 true=开启 +28V/-12V。
 * @retval 无。
 */
static void apply_outputs(bool aux5, bool v12, bool v28_m12)
{
    /* 关闭时先禁止后级主电源，再改变前级辅助电源，降低异常瞬态。 */
    if ((!v12) || (!v28_m12)) {
        rail_12v_enable(false);
        rail_28v_m12v_enable(false);
    }

    rail_aux_5v_enable(aux5);

    /* 开启时在辅助电源状态更新后再开启后级电源。 */
    if (v12) {
        rail_12v_enable(true);
    }
    if (v28_m12) {
        rail_28v_m12v_enable(true);
    }
}

/**
 * @brief  初始化电源控制状态、输入初值和 IPMB 地址。
 * @param  无。
 * @retval 无。
 */
void power_ctrl_init(void)
{
    /* GPIO 方向和安全默认态已经由 bsp_gpio_init() 完成。 */
    g_ga_id = 0u;

    if (bsp_gpio_read(GPIOC, GPIO_PIN_7) == BSP_GPIO_HIGH) {
        g_ga_id |= 0x01u;
    }
    if (bsp_gpio_read(GPIOC, GPIO_PIN_8) == BSP_GPIO_HIGH) {
        g_ga_id |= 0x02u;
    }
    if (bsp_gpio_read(GPIOA, GPIO_PIN_8) == BSP_GPIO_HIGH) {
        g_ga_id |= 0x04u;
    }

    /* 8 位地址每增加一个 7 位地址需要增加 2。 */
    g_ipmb_addr_8bit = (uint8_t)(IPMB_PM_BASE_ADDR_8BIT + (g_ga_id << 1));

    /* 读取上电初值，避免去抖器从固定默认值产生虚假跳变。 */
    g_en_active = input_low_active(GPIOB, GPIO_PIN_13);
    g_inh_active = input_low_active(GPIOB, GPIO_PIN_14);
    g_nvmro_allows_update = input_low_active(GPIOB, GPIO_PIN_9);
    g_en_cnt = 0u;
    g_inh_cnt = 0u;
    g_nvmro_cnt = 0u;

    /* 初始化期间保持所有受控电源关闭。 */
    apply_outputs(false, false, false);
    g_power_state = POWER_STATE_OFF;
    g_last_logged_state = g_power_state;

    APP_LOGI("Power control initialized: EN=%u INH=%u NVMRO=%u GA=%u IPMB=0x%02X",
             (unsigned int)g_en_active,
             (unsigned int)g_inh_active,
             (unsigned int)g_nvmro_allows_update,
             (unsigned int)g_ga_id,
             (unsigned int)g_ipmb_addr_8bit);
}

/**
 * @brief  1ms 周期电源控制任务。
 * @param  无。
 * @retval 无。
 */
void power_ctrl_task_1ms(void)
{
    /* 分别更新三个低电平有效输入的稳定状态。 */
    debounce_bool(input_low_active(GPIOB, GPIO_PIN_13), &g_en_cnt, &g_en_active);
    debounce_bool(input_low_active(GPIOB, GPIO_PIN_14), &g_inh_cnt, &g_inh_active);
    debounce_bool(input_low_active(GPIOB, GPIO_PIN_9), &g_nvmro_cnt, &g_nvmro_allows_update);

    if ((!g_nvmro_allows_update) || (!g_en_active)) {
        /* NVMRO 不允许或 EN 无效时关闭全部电源轨。 */
        apply_outputs(false, false, false);
        g_power_state = POWER_STATE_OFF;
    } else if (g_inh_active) {
        /* 按当前需求，INH 有效时关闭 +12V，保留辅助和 +28V/-12V。 */
        apply_outputs(true, false, true);
        g_power_state = POWER_STATE_INHIBIT;
    } else {
        /* 所有输入条件满足时开启全部受控电源轨。 */
        apply_outputs(true, true, true);
        g_power_state = POWER_STATE_ON;
    }

    /* 仅在状态变化时打印，避免 1ms 任务持续刷屏。 */
    if (g_power_state != g_last_logged_state) {
        APP_LOGI("Power state changed: %u -> %u",
                 (unsigned int)g_last_logged_state,
                 (unsigned int)g_power_state);
        g_last_logged_state = g_power_state;
    }
}

/**
 * @brief  严重故障时强制关闭全部受控电源轨。
 * @param  无。
 * @retval 无。
 */
void power_ctrl_force_off(void)
{
    apply_outputs(false, false, false);
    g_power_state = POWER_STATE_FAULT;

    if (g_power_state != g_last_logged_state) {
        APP_LOGW("Power forced off by fault protection");
        g_last_logged_state = g_power_state;
    }
}

/**
 * @brief  获取当前电源状态。
 * @param  无。
 * @retval 当前 power_state_t 状态。
 */
power_state_t power_ctrl_get_state(void)
{
    return g_power_state;
}

/**
 * @brief  获取去抖后的 EN 有效状态。
 * @param  无。
 * @retval true=有效；false=无效。
 */
bool power_ctrl_enable_active(void)
{
    return g_en_active;
}

/**
 * @brief  获取去抖后的 INH 有效状态。
 * @param  无。
 * @retval true=有效；false=无效。
 */
bool power_ctrl_inhibit_active(void)
{
    return g_inh_active;
}

/**
 * @brief  获取去抖后的 NVMRO 允许状态。
 * @param  无。
 * @retval true=允许；false=不允许。
 */
bool power_ctrl_nvmro_allows_update(void)
{
    return g_nvmro_allows_update;
}

/**
 * @brief  检查 PWOUT_TEST_MCU 输入电压状态。
 * @param  无。
 * @retval true=PB8 为低、输入电压正常；false=PB8 为高、输入电压异常。
 */
bool power_ctrl_input_voltage_ok(void)
{
    return (bsp_gpio_read(GPIOB, GPIO_PIN_8) == BSP_GPIO_LOW) ? true : false;
}

/**
 * @brief  获取 GA[2:0] 组合得到的槽位号。
 * @param  无。
 * @retval 0~7 的槽位号。
 */
uint8_t power_ctrl_get_ga_id(void)
{
    return g_ga_id;
}

/**
 * @brief  获取根据 GA 地址计算得到的 8 位 IPMB 地址。
 * @param  无。
 * @retval 本机 8 位 IPMB 地址。
 */
uint8_t power_ctrl_get_ipmb_addr_8bit(void)
{
    return g_ipmb_addr_8bit;
}
