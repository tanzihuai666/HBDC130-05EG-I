/**
 * @file    bsp_i2c.c
 * @brief   I2C1/IPMB 板级驱动。I2C1 用作 IPMB 7-bit 从机接收请求，并在主循环中切为 Master 写响应。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#include "bsp_i2c.h"
#include "bsp_gpio.h"
#include <string.h>

/* 等待 I2C 标志的超时计数，避免总线异常时死等。 */
#define I2C_TIMEOUT_SHORT      20000u
#define I2C_TIMEOUT_LONG       80000u

static bsp_i2c_rx_callback_t g_rx_cb;
static uint8_t g_rx_buf[BSP_I2C_RX_MAX];
static volatile uint8_t g_rx_len;
static volatile bool g_rx_overflow;
static uint8_t g_own_addr_7bit;
static uint32_t g_i2c_error_count;

/** @brief 读 STR1/STR2 清除地址匹配状态。 */
static void i2c_clear_addr(void)
{
    volatile uint16_t tmp;
    tmp = I2C_ReadRegister(I2C1, I2C_REGISTER_STR1);
    tmp = I2C_ReadRegister(I2C1, I2C_REGISTER_STR2);
    (void)tmp;
}

/** @brief 清除 STOP 检测状态并保持 I2C 使能。 */
static void i2c_clear_stop(void)
{
    volatile uint16_t tmp;
    tmp = I2C_ReadRegister(I2C1, I2C_REGISTER_STR1);
    (void)tmp;
    I2C_Enable(I2C1, ENABLE);
}

/** @brief 等待指定标志达到目标状态。 */
static bool i2c_wait_flag(uint32_t flag, TypeState state, uint32_t timeout)
{
    while (timeout-- > 0u) {
        if (I2C_GetBitState(I2C1, I2C_FLAG_BE) == SET) return false;
        if (I2C_GetBitState(I2C1, I2C_FLAG_LOSTARB) == SET) return false;
        if (I2C_GetBitState(I2C1, I2C_FLAG_AE) == SET) return false;
        if (I2C_GetBitState(I2C1, flag) == state) return true;
    }
    return false;
}

/** @brief 清除 I2C 错误标志。 */
static void i2c_clear_errors(void)
{
    if (I2C_GetBitState(I2C1, I2C_FLAG_BE) == SET) I2C_ClearBitState(I2C1, I2C_FLAG_BE);
    if (I2C_GetBitState(I2C1, I2C_FLAG_LOSTARB) == SET) I2C_ClearBitState(I2C1, I2C_FLAG_LOSTARB);
    if (I2C_GetBitState(I2C1, I2C_FLAG_AE) == SET) I2C_ClearBitState(I2C1, I2C_FLAG_AE);
    if (I2C_GetBitState(I2C1, I2C_FLAG_RXORE) == SET) I2C_ClearBitState(I2C1, I2C_FLAG_RXORE);
}

/** @brief 初始化 I2C1 GPIO、外设和中断。 */
static void i2c1_hw_init(uint8_t own_addr_7bit)
{
    GPIO_InitPara gpio;
    I2C_InitPara i2c;
    NVIC_InitPara nvic;

    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOB | RCC_APB2PERIPH_AF, ENABLE);
    RCC_APB1PeriphClock_Enable(RCC_APB1PERIPH_I2C1, ENABLE);

    gpio.GPIO_Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.GPIO_Speed = GPIO_SPEED_50MHZ;
    gpio.GPIO_Mode = GPIO_MODE_AF_OD;
    GPIO_Init(GPIOB, &gpio);

    I2C_DeInit(I2C1);
    I2C_ParaInit(&i2c);
    i2c.I2C_Protocol = I2C_PROTOCOL_I2C;
    i2c.I2C_DutyCycle = I2C_DUTYCYCLE_2;
    i2c.I2C_BitRate = 400000u;
    i2c.I2C_AddressingMode = I2C_ADDRESSING_MODE_7BIT;
    i2c.I2C_DeviceAddress = (uint16_t)(own_addr_7bit << 1);
    I2C_Init(I2C1, &i2c);
    I2C_Acknowledge_Enable(I2C1, ENABLE);
    I2C_StretchClock_Enable(I2C1, ENABLE);
    I2C_INTConfig(I2C1, (uint16_t)(I2C_INT_EIE | I2C_INT_EE | I2C_INT_BIE), ENABLE);
    I2C_Enable(I2C1, ENABLE);

    nvic.NVIC_IRQ = I2C1_EV_IRQn;
    nvic.NVIC_IRQPreemptPriority = 3u;
    nvic.NVIC_IRQSubPriority = 0u;
    nvic.NVIC_IRQEnable = ENABLE;
    NVIC_Init(&nvic);

    nvic.NVIC_IRQ = I2C1_ER_IRQn;
    nvic.NVIC_IRQPreemptPriority = 2u;
    nvic.NVIC_IRQSubPriority = 0u;
    nvic.NVIC_IRQEnable = ENABLE;
    NVIC_Init(&nvic);
}

/** @brief 注册从机接收完成回调。 */
void bsp_i2c1_register_rx_callback(bsp_i2c_rx_callback_t cb)
{
    g_rx_cb = cb;
}

/** @brief 初始化 I2C1 IPMB。 */
void bsp_i2c1_ipmb_init(uint8_t own_addr_7bit)
{
    g_own_addr_7bit = own_addr_7bit;
    g_rx_len = 0u;
    g_rx_overflow = false;
    bsp_gpio_set_i2c1_transceiver_enable(true);
    i2c1_hw_init(g_own_addr_7bit);
    APP_LOGI("i2c1 init own7=0x%02X", own_addr_7bit);
}

/** @brief 主机写响应帧。 */
bool bsp_i2c1_master_write(uint8_t dest_addr_7bit, const uint8_t *data, uint8_t len)
{
    uint8_t i;
    bool ok = false;

    if ((data == 0) || (len == 0u) || (len > BSP_I2C_TX_MAX)) return false;

    I2C_INTConfig(I2C1, (uint16_t)(I2C_INT_EIE | I2C_INT_EE | I2C_INT_BIE), DISABLE);
    i2c_clear_errors();

    if (i2c_wait_flag(I2C_FLAG_I2CBSY, RESET, I2C_TIMEOUT_LONG)) {
        I2C_StartOnBus_Enable(I2C1, ENABLE);
        if (i2c_wait_flag(I2C_FLAG_SBSEND, SET, I2C_TIMEOUT_SHORT)) {
            I2C_AddressingDevice_7bit(I2C1, (uint8_t)(dest_addr_7bit << 1), I2C_DIRECTION_TRANSMITTER);
            if (i2c_wait_flag(I2C_FLAG_ADDSEND, SET, I2C_TIMEOUT_SHORT)) {
                i2c_clear_addr();
                ok = true;
                for (i = 0u; i < len; i++) {
                    if (!i2c_wait_flag(I2C_FLAG_TBE, SET, I2C_TIMEOUT_SHORT)) {
                        ok = false;
                        break;
                    }
                    I2C_SendData(I2C1, data[i]);
                }
                if (ok) ok = i2c_wait_flag(I2C_FLAG_BTC, SET, I2C_TIMEOUT_SHORT);
            }
        }
    }

    I2C_StopOnBus_Enable(I2C1, ENABLE);
    if (!ok) {
        g_i2c_error_count++;
        i2c_clear_errors();
    }
    I2C_INTConfig(I2C1, (uint16_t)(I2C_INT_EIE | I2C_INT_EE | I2C_INT_BIE), ENABLE);
    return ok;
}

/** @brief 恢复总线并重新初始化 I2C1。 */
void bsp_i2c1_recover_bus(void)
{
    uint8_t i;
    GPIO_InitPara gpio;

    APP_LOGW("i2c1 recover bus");
    I2C_Enable(I2C1, DISABLE);
    bsp_gpio_set_i2c1_transceiver_enable(false);

    gpio.GPIO_Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.GPIO_Speed = GPIO_SPEED_50MHZ;
    gpio.GPIO_Mode = GPIO_MODE_OUT_OD;
    GPIO_Init(GPIOB, &gpio);

    GPIO_SetBits(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);
    for (i = 0u; i < 9u; i++) {
        GPIO_ResetBits(GPIOB, GPIO_PIN_6);
        for (volatile uint32_t d = 0u; d < 300u; d++) { }
        GPIO_SetBits(GPIOB, GPIO_PIN_6);
        for (volatile uint32_t d = 0u; d < 300u; d++) { }
    }

    bsp_gpio_set_i2c1_transceiver_enable(true);
    i2c1_hw_init(g_own_addr_7bit);
}

/** @brief 获取 I2C 错误计数。 */
uint32_t bsp_i2c1_get_error_count(void)
{
    return g_i2c_error_count;
}

/** @brief I2C1 事件中断，处理地址匹配、接收字节和 STOP。 */
void I2C1_EV_IRQHandler(void)
{
    if (I2C_GetIntBitState(I2C1, I2C_INT_ADDSEND) == SET) {
        i2c_clear_addr();
        g_rx_len = 0u;
        g_rx_overflow = false;
    }
    if (I2C_GetIntBitState(I2C1, I2C_INT_RBNE) == SET) {
        uint8_t b = I2C_ReceiveData(I2C1);
        if (g_rx_len < BSP_I2C_RX_MAX) g_rx_buf[g_rx_len++] = b;
        else g_rx_overflow = true;
    }
    if (I2C_GetIntBitState(I2C1, I2C_INT_STPSEND) == SET) {
        i2c_clear_stop();
        if ((!g_rx_overflow) && (g_rx_len > 0u) && (g_rx_cb != 0)) g_rx_cb(g_rx_buf, g_rx_len);
        g_rx_len = 0u;
        g_rx_overflow = false;
    }
}

/** @brief I2C1 错误中断，只清标志和复位接收状态。 */
void I2C1_ER_IRQHandler(void)
{
    g_i2c_error_count++;
    i2c_clear_errors();
    g_rx_len = 0u;
    g_rx_overflow = false;
}
