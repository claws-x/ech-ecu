/**
 * @file ech_pwm.c
 * @brief PTC 加热器 PWM 驱动实现
 * @process SWE.3 软件详细设计与单元实现
 * @version 0.1
 * @date 2026-03-02
 */

#include "ech_pwm.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/*============================================================================
 * 私有类型与宏定义
 *============================================================================*/

// PWM 硬件寄存器抽象（示例：基于通用 MCU，实际需适配具体平台如 RH850/AURIX）
typedef struct {
    volatile uint32_t CTRL;     // 控制寄存器
    volatile uint32_t FREQ;     // 频率配置
    volatile uint32_t DUTY;     // 占空比寄存器
    volatile uint32_t STATUS;   // 状态寄存器
} PWM_HW_Regs_t;

// 假设硬件基地址（实际由平台头文件定义）
#define PWM_BASE_ADDR         (0x40020000UL)
#define PWM_HW                ((PWM_HW_Regs_t*)PWM_BASE_ADDR)

// 控制寄存器位定义
#define PWM_CTRL_ENABLE       (1U << 0)
#define PWM_CTRL_AUTO_RELOAD  (1U << 1)

// 状态寄存器位定义
#define PWM_STATUS_READY      (1U << 0)
#define PWM_STATUS_FAULT      (1U << 1)

// 占空比分辨率（12-bit = 0-4095）
#define PWM_DUTY_RESOLUTION   (4096U)

/*============================================================================
 * 私有变量
 *============================================================================*/

static bool g_pwm_initialized = false;
static float g_current_duty = 0.0f;
static PWM_Config_t g_pwm_config;

/*============================================================================
 * 私有函数声明
 *============================================================================*/

/**
 * @brief 将占空比百分比转换为硬件寄存器值
 * @param duty_percent 占空比百分比 (0.0 - 100.0)
 * @return 硬件寄存器值 (0 - 4095)
 */
static uint32_t PWM_ConvertDutyToReg(float duty_percent);

/**
 * @brief 硬件寄存器初始化
 */
static void PWM_HW_Init(void);

/*============================================================================
 * 公有函数实现
 *============================================================================*/

bool PWM_Init(const PWM_Config_t* config)
{
    if (config == NULL) {
        return false;
    }

    // 验证配置参数
    if (config->frequency_hz == 0 || config->frequency_hz > 100000) {
        // 频率超出合理范围 (0-100kHz)
        return false;
    }

    if (config->channel > 3) {
        // 通道号超出范围 (假设支持 0-3 通道)
        return false;
    }

    // 保存配置
    g_pwm_config = *config;

    // 硬件初始化
    PWM_HW_Init();

    // 初始占空比为 0
    g_current_duty = 0.0f;
    PWM_HW->DUTY = 0;

    // 标记已初始化
    g_pwm_initialized = true;

    printf("[PWM] Initialized: freq=%lu Hz, channel=%u\n", 
           config->frequency_hz, config->channel);

    return true;
}

bool PWM_SetDuty(float duty_percent)
{
    // 检查初始化状态
    if (!g_pwm_initialized) {
        return false;
    }

    // 限制占空比范围 (0.0 - 100.0)
    if (duty_percent < 0.0f) {
        duty_percent = 0.0f;
    } else if (duty_percent > 100.0f) {
        duty_percent = 100.0f;
    }

    // 转换为硬件寄存器值
    uint32_t duty_reg = PWM_ConvertDutyToReg(duty_percent);

    // 写入硬件寄存器
    PWM_HW->DUTY = duty_reg;

    // 更新当前占空比
    g_current_duty = duty_percent;

    return true;
}

float PWM_GetDuty(void)
{
    return g_current_duty;
}

void PWM_Disable(void)
{
    if (!g_pwm_initialized) {
        return;
    }

    // 清除使能位
    PWM_HW->CTRL &= ~PWM_CTRL_ENABLE;

    printf("[PWM] Disabled\n");
}

void PWM_Enable(void)
{
    if (!g_pwm_initialized) {
        return;
    }

    // 检查硬件就绪
    if ((PWM_HW->STATUS & PWM_STATUS_READY) == 0) {
        printf("[PWM] Enable failed: hardware not ready\n");
        return;
    }

    // 设置使能位 + 自动重载
    PWM_HW->CTRL |= (PWM_CTRL_ENABLE | PWM_CTRL_AUTO_RELOAD);

    printf("[PWM] Enabled\n");
}

/*============================================================================
 * 私有函数实现
 *============================================================================*/

static uint32_t PWM_ConvertDutyToReg(float duty_percent)
{
    // 百分比 -> 寄存器值 (0-4095)
    uint32_t duty_reg = (uint32_t)((duty_percent / 100.0f) * PWM_DUTY_RESOLUTION);

    // 限制最大值
    if (duty_reg >= PWM_DUTY_RESOLUTION) {
        duty_reg = PWM_DUTY_RESOLUTION - 1;
    }

    return duty_reg;
}

static void PWM_HW_Init(void)
{
    // 1. 使能 PWM 模块时钟（平台相关，此处为示例）
    // Platform_EnableClock(PWM_MODULE_ID);

    // 2. 配置 PWM 频率
    // 假设公式：FREQ = SystemClock / (Prescaler * Period)
    // 此处简化为直接写入
    PWM_HW->FREQ = g_pwm_config.frequency_hz;

    // 3. 配置 PWM 通道
    // PWM_HW->CHANNEL_SEL = g_pwm_config.channel;

    // 4. 配置为自动重载模式
    PWM_HW->CTRL |= PWM_CTRL_AUTO_RELOAD;

    // 5. 等待硬件就绪（实际应添加超时检测）
    // while ((PWM_HW->STATUS & PWM_STATUS_READY) == 0) { }

    // 6. 初始禁用输出
    PWM_HW->CTRL &= ~PWM_CTRL_ENABLE;
}

/*============================================================================
 * 单元测试桩（SWE.4 单元验证）
 *============================================================================*/

#ifdef UNIT_TEST

#include <assert.h>

void PWM_UT_Init_ValidConfig(void)
{
    PWM_Config_t config = {
        .frequency_hz = 20000,
        .channel = 0
    };

    bool result = PWM_Init(&config);
    assert(result == true);
    assert(g_pwm_initialized == true);
    assert(g_current_duty == 0.0f);

    printf("[UT] PWM_UT_Init_ValidConfig: PASSED\n");
}

void PWM_UT_SetDuty_NormalRange(void)
{
    // 先初始化
    PWM_Config_t config = { .frequency_hz = 20000, .channel = 0 };
    PWM_Init(&config);

    // 测试 50% 占空比
    bool result = PWM_SetDuty(50.0f);
    assert(result == true);
    assert(g_current_duty == 50.0f);

    // 测试 0% 占空比
    result = PWM_SetDuty(0.0f);
    assert(result == true);
    assert(g_current_duty == 0.0f);

    // 测试 100% 占空比
    result = PWM_SetDuty(100.0f);
    assert(result == true);
    assert(g_current_duty == 100.0f);

    printf("[UT] PWM_UT_SetDuty_NormalRange: PASSED\n");
}

void PWM_UT_SetDuty_Clamping(void)
{
    PWM_Config_t config = { .frequency_hz = 20000, .channel = 0 };
    PWM_Init(&config);

    // 测试负值钳位
    bool result = PWM_SetDuty(-10.0f);
    assert(result == true);
    assert(g_current_duty == 0.0f);

    // 测试超 100% 钳位
    result = PWM_SetDuty(150.0f);
    assert(result == true);
    assert(g_current_duty == 100.0f);

    printf("[UT] PWM_UT_SetDuty_Clamping: PASSED\n");
}

void PWM_UT_Init_NullConfig(void)
{
    bool result = PWM_Init(NULL);
    assert(result == false);
    assert(g_pwm_initialized == false);

    printf("[UT] PWM_UT_Init_NullConfig: PASSED\n");
}

void PWM_UT_RunAll(void)
{
    printf("\n=== PWM Driver Unit Tests ===\n");
    PWM_UT_Init_ValidConfig();
    PWM_UT_SetDuty_NormalRange();
    PWM_UT_SetDuty_Clamping();
    PWM_UT_Init_NullConfig();
    printf("=== All Tests Completed ===\n\n");
}

#endif // UNIT_TEST
