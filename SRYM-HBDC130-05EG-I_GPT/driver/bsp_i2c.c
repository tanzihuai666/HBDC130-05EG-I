/**
 * @file    bsp_i2c.c
 * @brief   I2C1/IPMB 板级驱动实现。
 *          I2C1 在空闲时作为 7 位从机接收 BMC 写入的 IPMB Request；收到完整请求后，
 *          协议层在主循环中完成解析和响应构造，再调用本模块将 I2C1 临时作为主机，
 *          以 I2C 写事务把 IPMB Response 发送回请求方。
 *
 *          中断服务函数只处理字节收发和状态清除，不在中断中打印日志，避免阻塞串口
 *          影响 I2C 时序。所有错误统计和恢复动作均在主循环上下文完成。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#include "bsp_i2c.h"
#include "bsp_gpio.h"
#include <string.h>

/*
 * I2C 轮询等待超时计数。
 * SHORT 用于 START、ADDR、TBE、BTC 等单阶段等待；LONG 用于等待总线空闲。
 * 该数值不是精确时间，仅用于防止异常总线导致主循环永久阻塞。
 */
#define I2C_TIMEOUT_SHORT      20000u
#define I2C_TIMEOUT_LONG       80000u

/* 上层注册的完整从机写帧接收回调。 */
static bsp_i2c_rx_callback_t g_rx_cb;

/* 从机接收缓存，STOP 到来后一次性提交给协议层。 */
static uint8_t g_rx_buf[BSP_I2C_RX_MAX];

/* 当前已接收字节数，由 I2C1_EV_IRQHandler() 更新。 */
static volatile uint8_t g_rx_len;

/* 接收缓存溢出标志，溢出后丢弃本次完整事务。 */
static volatile bool g_rx_overflow;

/* 本机 7 位从地址，用于总线恢复后重新初始化 I2C1。 */
static uint8_t g_own_addr_7bit;

/* 累计总线错误、仲裁丢失、NACK 或发送超时次数。 */
static uint32_t g_i2c_error_count;

/**
 * @brief  清除 I2C 地址匹配标志。
 * @param  无。
 * @retval 无。
 * @note   GD32F10x 与 STM32F1 类似，需要按顺序读取 STR1 和 STR2 才能清除 ADDSEND。
 */
static void i2c_clear_address_match(void)
{
    volatile uint16_t dummy;

    dummy = I2C_ReadRegister(I2C1, I2C_REGISTER_STR1);
    dummy = I2C_ReadRegister(I2C1, I2C_REGISTER_STR2);
    (void)dummy;
}

/**
 * @brief  清除从机 STOP 检测状态。
 * @param  无。
 * @retval 无。
 * @note   读取 STR1 后重新写使能位，使外设继续响应下一次从机寻址。
 */
static void i2c_clear_stop_detect(void)
{
    volatile uint16_t dummy;

    dummy = I2C_ReadRegister(I2C1, I2C_REGISTER_STR1);
    (void)dummy;
    I2C_Enable(I2C1, ENABLE);
}

/**
 * @brief  等待指定 I2C 标志达到目标状态。
 * @param  flag    I2C_FLAG_xxx 标志。
 * @param  state   期望状态，SET 或 RESET。
 * @param  timeout 最大轮询次数。
 * @retval true=目标状态到达；false=超时或检测到总线错误。
 */
static bool i2c_wait_flag(uint32_t flag, TypeState state, uint32_t timeout)
{
    while (timeout > 0u) {
        timeout--;

        /* 总线错误、仲裁丢失和应答错误任一出现，都立即终止当前发送流程。 */
        if (I2C_GetBitState(I2C1, I2C_FLAG_BE) == SET) {
            return false;
        }
        if (I2C_GetBitState(I2C1, I2C_FLAG_LOSTARB) == SET) {
            return false;
        }
        if (I2C_GetBitState(I2C1, I2C_FLAG_AE) == SET) {
            return false;
        }

        if (I2C_GetBitState(I2C1, flag) == state) {
            return true;
        }
    }

    return false;
}

/**
 * @brief  清除 I2C1 已置位的错误标志。
 * @param  无。
 * @retval 无。
 */
static void i2c_clear_error_flags(void)
{
    if (I2C_GetBitState(I2C1, I2C_FLAG_BE) == SET) {
        I2C_ClearBitState(I2C1, I2C_FLAG_BE);
    }
    if (I2C_GetBitState(I2C1, I2C_FLAG_LOSTARB) == SET) {
        I2C_ClearBitState(I2C1, I2C_FLAG_LOSTARB);
    }
    if (I2C_GetBitState(I2C1, I2C_FLAG_AE) == SET) {
        I2C_ClearBitState(I2C1, I2C_FLAG_AE);
    }
    if (I2C_GetBitState(I2C1, I2C_FLAG_RXORE) == SET) {
        I2C_ClearBitState(I2C1, I2C_FLAG_RXORE);
    }
}

/**
 * @brief  初始化 I2C1 GPIO、外设参数和 NVIC。
 * @param  own_addr_7bit 本机 7 位从地址。
 * @retval 无。
 */
static void i2c1_hardware_init(uint8_t own_addr_7bit)
{
    GPIO_InitPara gpio;
    I2C_InitPara i2c;
    NVIC_InitPara nvic;

    /* PB6/PB7 属于 GPIOB，I2C1 属于 APB1，同时需要 AFIO 时钟。 */
    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOB | RCC_APB2PERIPH_AF, ENABLE);
    RCC_APB1PeriphClock_Enable(RCC_APB1PERIPH_I2C1, ENABLE);

    /* PB6=SCL、PB7=SDA，I2C 总线必须使用开漏复用输出。 */
    gpio.GPIO_Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.GPIO_Speed = GPIO_SPEED_50MHZ;
    gpio.GPIO_Mode = GPIO_MODE_AF_OD;
    GPIO_Init(GPIOB, &gpio);

    /* 复位外设并加载 I2C 标准模式参数。 */
    I2C_DeInit(I2C1);
    I2C_ParaInit(&i2c);
    i2c.I2C_Protocol = I2C_PROTOCOL_I2C;
    i2c.I2C_DutyCycle = I2C_DUTYCYCLE_2;
    i2c.I2C_BitRate = 400000u;
    i2c.I2C_AddressingMode = I2C_ADDRESSING_MODE_7BIT;

    /* 库函数内部寄存器使用 8 位地址形式，因此将 7 位地址左移 1 位。 */
    i2c.I2C_DeviceAddress = (uint16_t)(own_addr_7bit << 1);
    I2C_Init(I2C1, &i2c);

    /* 从机接收必须开启 ACK；允许 Clock Stretch 以容忍短时软件延迟。 */
    I2C_Acknowledge_Enable(I2C1, ENABLE);
    I2C_StretchClock_Enable(I2C1, ENABLE);

    /* 开启错误、事件和缓冲区中断。 */
    I2C_INTConfig(I2C1,
                  (uint16_t)(I2C_INT_EIE | I2C_INT_EE | I2C_INT_BIE),
                  ENABLE);
    I2C_Enable(I2C1, ENABLE);

    /* I2C 错误中断优先级高于事件中断，先处理总线异常。 */
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

/**
 * @brief  注册从机接收完成回调。
 * @param  cb 回调函数；传入 0 可取消回调。
 * @retval 无。
 */
void bsp_i2c1_register_rx_callback(bsp_i2c_rx_callback_t cb)
{
    g_rx_cb = cb;
}

/**
 * @brief  初始化 I2C1 为 IPMB 从机，并保留主机发送能力。
 * @param  own_addr_7bit 本机 7 位从地址。
 * @retval 无。
 */
void bsp_i2c1_ipmb_init(uint8_t own_addr_7bit)
{
    g_own_addr_7bit = own_addr_7bit;
    g_rx_len = 0u;
    g_rx_overflow = false;
    g_i2c_error_count = 0u;

    /* 先使能外部 I2C1 收发器，再初始化 MCU 内部 I2C1。 */
    bsp_gpio_set_i2c1_transceiver_enable(true);
    i2c1_hardware_init(g_own_addr_7bit);

    APP_LOGI("I2C1 initialized: own7=0x%02X speed=400kHz",
             (unsigned int)own_addr_7bit);
}

/**
 * @brief  以 I2C 主机写事务发送完整 IPMB Response。
 * @param  dest_addr_7bit 目标设备 7 位地址。
 * @param  data           待发送字节数组。
 * @param  len            待发送长度。
 * @retval true=发送成功；false=参数错误、超时、NACK、仲裁丢失或总线错误。
 */
bool bsp_i2c1_master_write(uint8_t dest_addr_7bit,
                           const uint8_t *data,
                           uint8_t len)
{
    uint8_t index;
    bool success;

    success = false;

    if ((data == 0) || (len == 0u) || (len > BSP_I2C_TX_MAX)) {
        return false;
    }

    /*
     * 主机发送期间暂时关闭 I2C 中断，避免主机状态和从机接收中断交叉修改同一外设状态。
     * 发送结束后无论成功失败都必须重新开启中断。
     */
    I2C_INTConfig(I2C1,
                  (uint16_t)(I2C_INT_EIE | I2C_INT_EE | I2C_INT_BIE),
                  DISABLE);
    i2c_clear_error_flags();

    /* 等待多主总线空闲，随后产生 START。 */
    if (i2c_wait_flag(I2C_FLAG_I2CBSY, RESET, I2C_TIMEOUT_LONG)) {
        I2C_StartOnBus_Enable(I2C1, ENABLE);

        /* START 已发送后写入目标地址和写方向。 */
        if (i2c_wait_flag(I2C_FLAG_SBSEND, SET, I2C_TIMEOUT_SHORT)) {
            I2C_AddressingDevice_7bit(I2C1,
                                      (uint8_t)(dest_addr_7bit << 1),
                                      I2C_DIRECTION_TRANSMITTER);

            /* 目标设备 ACK 地址后，按要求读取 STR1/STR2 清除 ADDSEND。 */
            if (i2c_wait_flag(I2C_FLAG_ADDSEND, SET, I2C_TIMEOUT_SHORT)) {
                i2c_clear_address_match();
                success = true;

                /* 等待 TBE 后逐字节写入数据寄存器。 */
                for (index = 0u; index < len; index++) {
                    if (!i2c_wait_flag(I2C_FLAG_TBE, SET, I2C_TIMEOUT_SHORT)) {
                        success = false;
                        break;
                    }

                    I2C_SendData(I2C1, data[index]);
                }

                /* 最后等待字节传输完成，确保最后一个字节已经移出移位寄存器。 */
                if (success) {
                    success = i2c_wait_flag(I2C_FLAG_BTC,
                                            SET,
                                            I2C_TIMEOUT_SHORT);
                }
            }
        }
    }

    /* 无论发送是否成功，都产生 STOP 释放总线。 */
    I2C_StopOnBus_Enable(I2C1, ENABLE);

    if (!success) {
        g_i2c_error_count++;
        i2c_clear_error_flags();
    }

    I2C_INTConfig(I2C1,
                  (uint16_t)(I2C_INT_EIE | I2C_INT_EE | I2C_INT_BIE),
                  ENABLE);

    return success;
}

/**
 * @brief  对被从设备拉低的 I2C 总线执行恢复。
 * @param  无。
 * @retval 无。
 * @note   关闭 I2C 外设后，将 PB6/PB7 临时改为开漏 GPIO，通过 9 个 SCL 脉冲尝试
 *         推动从设备释放 SDA，随后重新初始化 I2C1。
 */
void bsp_i2c1_recover_bus(void)
{
    uint8_t pulse_index;
    uint32_t delay_count;
    GPIO_InitPara gpio;

    APP_LOGW("I2C1 bus recovery started");

    I2C_Enable(I2C1, DISABLE);
    bsp_gpio_set_i2c1_transceiver_enable(false);

    /* 临时将 SCL/SDA 配置为开漏 GPIO。 */
    gpio.GPIO_Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.GPIO_Speed = GPIO_SPEED_50MHZ;
    gpio.GPIO_Mode = GPIO_MODE_OUT_OD;
    GPIO_Init(GPIOB, &gpio);

    /* 先释放 SCL/SDA，再产生 9 个时钟脉冲。 */
    GPIO_SetBits(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);

    for (pulse_index = 0u; pulse_index < 9u; pulse_index++) {
        GPIO_ResetBits(GPIOB, GPIO_PIN_6);
        for (delay_count = 0u; delay_count < 300u; delay_count++) {
            __NOP();
        }

        GPIO_SetBits(GPIOB, GPIO_PIN_6);
        for (delay_count = 0u; delay_count < 300u; delay_count++) {
            __NOP();
        }
    }

    /* 恢复外部收发器和 I2C1 复用功能。 */
    bsp_gpio_set_i2c1_transceiver_enable(true);
    i2c1_hardware_init(g_own_addr_7bit);

    APP_LOGI("I2C1 bus recovery finished");
}

/**
 * @brief  获取累计 I2C 错误次数。
 * @param  无。
 * @retval 错误计数。
 */
uint32_t bsp_i2c1_get_error_count(void)
{
    return g_i2c_error_count;
}

/**
 * @brief  I2C1 事件中断服务函数。
 * @param  无。
 * @retval 无。
 * @note   处理从机地址匹配、接收数据寄存器非空以及 STOP 检测三个事件。
 */
void I2C1_EV_IRQHandler(void)
{
    /* 地址匹配意味着一个新的从机写事务开始，清空上一帧接收状态。 */
    if (I2C_GetIntBitState(I2C1, I2C_INT_ADDSEND) == SET) {
        i2c_clear_address_match();
        g_rx_len = 0u;
        g_rx_overflow = false;
    }

    /* 每次 RBNE 置位读取一个字节，防止接收寄存器溢出。 */
    if (I2C_GetIntBitState(I2C1, I2C_INT_RBNE) == SET) {
        uint8_t byte_value;

        byte_value = I2C_ReceiveData(I2C1);

        if (g_rx_len < BSP_I2C_RX_MAX) {
            g_rx_buf[g_rx_len] = byte_value;
            g_rx_len++;
        } else {
            g_rx_overflow = true;
        }
    }

    /* STOP 表示本次 I2C 写事务结束，此时才向上层提交完整帧。 */
    if (I2C_GetIntBitState(I2C1, I2C_INT_STPSEND) == SET) {
        i2c_clear_stop_detect();

        if ((!g_rx_overflow) &&
            (g_rx_len > 0u) &&
            (g_rx_cb != 0)) {
            g_rx_cb(g_rx_buf, g_rx_len);
        }

        g_rx_len = 0u;
        g_rx_overflow = false;
    }
}

/**
 * @brief  I2C1 错误中断服务函数。
 * @param  无。
 * @retval 无。
 * @note   中断内只计数、清标志和复位接收状态，不调用 printf。
 */
void I2C1_ER_IRQHandler(void)
{
    g_i2c_error_count++;
    i2c_clear_error_flags();
    g_rx_len = 0u;
    g_rx_overflow = false;
}
