/**
 * @file ech_temp_ctrl.c
 * @brief 温度闭环控制模块实现
 * @process SWE.3 软件详细设计与单元实现
 * @version 0.1
 * @date 2026-03-02
 */

#include "ech_temp_ctrl.h"
#include "ech_pwm.h"
#include "ech_pid.h"
#include <stdio.h>
#include <string.h>

/*============================================================================
 * 私有类型与宏定义
 *============================================================================*/

// 安全阈值定义
#define TEMP_CUTOFF_HIGH        (85.0f)   // 过温切断阈值 °C
#define TEMP_CUTOFF_LOW         (-40.0f)  // 低温故障阈值 °C
#define TEMP_VALID_RANGE_MIN    (-40.0f)  // 有效温度范围最小值
#define TEMP_VALID_RANGE_MAX    (120.0f)  // 有效温度范围最大值

// 软启动参数
#define RAMP_RATE_DEFAULT       (10.0f)   // 默认爬升速率 %/s
#define CONTROL_CYCLE_MS        (10)      // 控制周期 ms

/*============================================================================
 * 私有变量
 *============================================================================*/

static TempCtrl_Config_t g_config;
static bool g_initialized = false;
static bool g_emergency_stop = false;
static float g_target_temp = 25.0f;     // 默认目标温度
static float g_actual_temp = 25.0f;     // 默认实际温度
static float g_current_duty = 0.0f;
static float g_prev_duty = 0.0f;        // 用于爬升限制

/*============================================================================
 * 私有函数声明
 *============================================================================*/

/**
 * @brief 检查温度传感器是否有效
 */
static bool TempCtrl_IsTempValid(float temp);

/**
 * @brief 应用爬升速率限制
 */
static float TempCtrl_ApplyRampLimit(float target_duty, float prev_duty, float dt_sec);

/**
 * @brief 安全切断逻辑
 */
static void TempCtrl_SafetyCutoff(void);

/*============================================================================
 * 公有函数实现
 *============================================================================*/

void TempCtrl_Init(const TempCtrl_Config_t* config)
{
    if (config == NULL) {
        // 使用默认配置
        g_config.kp = 2.5f;
        g_config.ki = 0.1f;
        g_config.kd = 0.05f;
        g_config.max_duty = 100.0f;
        g_config.min_duty = 0.0f;
        g_config.ramp_rate = RAMP_RATE_DEFAULT;
    } else {
        g_config = *config;
    }

    // 初始化 PID 控制器
    PID_Config_t pid_config = {
        .kp = g_config.kp,
        .ki = g_config.ki,
        .kd = g_config.kd,
        .output_min = g_config.min_duty,
        .output_max = g_config.max_duty
    };
    PID_Init(&pid_config);

    g_initialized = true;
    g_emergency_stop = false;
    g_current_duty = 0.0f;
    g_prev_duty = 0.0f;

    printf("[TempCtrl] Initialized: KP=%.2f, KI=%.2f, KD=%.2f, Ramp=%.1f%%/s\n",
           g_config.kp, g_config.ki, g_config.kd, g_config.ramp_rate);
}

float TempCtrl_CalcDuty(float target_temp, float actual_temp)
{
    // 检查初始化状态
    if (!g_initialized) {
        printf("[TempCtrl] Error: Not initialized\n");
        return 0.0f;
    }

    // 检查紧急停止状态
    if (g_emergency_stop) {
        return 0.0f;
    }

    // 保存当前温度
    g_target_temp = target_temp;
    g_actual_temp = actual_temp;

    // 1. 安全切断检查（优先级最高）
    if (actual_temp >= TEMP_CUTOFF_HIGH) {
        printf("[TempCtrl] SAFETY: Over-temperature cutoff at %.1f°C\n", actual_temp);
        TempCtrl_SafetyCutoff();
        return 0.0f;
    }

    // 2. 温度传感器有效性检查
    if (!TempCtrl_IsTempValid(actual_temp)) {
        printf("[TempCtrl] Error: Invalid temperature reading %.1f°C\n", actual_temp);
        TempCtrl_SafetyCutoff();
        return 0.0f;
    }

    // 3. 计算目标温度与实际温度的偏差
    float error = target_temp - actual_temp;

    // 4. 如果目标温度 <= 实际温度，关闭加热
    if (error <= 0.0f) {
        g_prev_duty = 0.0f;
        g_current_duty = 0.0f;
        PID_Reset();  // 重置 PID 积分项
        return 0.0f;
    }

    // 5. PID 计算
    float pid_output = PID_Calc(error, CONTROL_CYCLE_MS / 1000.0f);

    // 6. 应用爬升速率限制
    float dt_sec = CONTROL_CYCLE_MS / 1000.0f;
    float limited_duty = TempCtrl_ApplyRampLimit(pid_output, g_prev_duty, dt_sec);

    // 7. 更新占空比
    g_prev_duty = g_current_duty;
    g_current_duty = limited_duty;

    // 8. 输出到 PWM 驱动
    PWM_SetDuty(limited_duty);

    return limited_duty;
}

float TempCtrl_GetCurrentDuty(void)
{
    return g_current_duty;
}

void TempCtrl_EmergencyStop(void)
{
    g_emergency_stop = true;
    g_current_duty = 0.0f;
    PWM_SetDuty(0.0f);
    PID_Reset();
    printf("[TempCtrl] Emergency Stop Activated\n");
}

void TempCtrl_ClearEmergencyStop(void)
{
    g_emergency_stop = false;
    printf("[TempCtrl] Emergency Stop Cleared\n");
}

/*============================================================================
 * 私有函数实现
 *============================================================================*/

static bool TempCtrl_IsTempValid(float temp)
{
    return (temp >= TEMP_VALID_RANGE_MIN) && (temp <= TEMP_VALID_RANGE_MAX);
}

static float TempCtrl_ApplyRampLimit(float target_duty, float prev_duty, float dt_sec)
{
    float max_change = g_config.ramp_rate * dt_sec;  // 本次周期允许的最大变化量

    float delta = target_duty - prev_duty;

    if (delta > max_change) {
        // 正向爬升限制
        return prev_duty + max_change;
    } else if (delta < -max_change) {
        // 负向下降限制（可更快，此处对称处理）
        return prev_duty - max_change;
    } else {
        // 在限制范围内，直接返回
        return target_duty;
    }
}

static void TempCtrl_SafetyCutoff(void)
{
    g_current_duty = 0.0f;
    g_prev_duty = 0.0f;
    PWM_SetDuty(0.0f);
    PID_Reset();
}

/*============================================================================
 * 单元测试桩（SWE.4 单元验证）
 *============================================================================*/

#ifdef UNIT_TEST

#include <assert.h>
#include <math.h>

void TempCtrl_UT_Init_DefaultConfig(void)
{
    TempCtrl_Init(NULL);  // 使用默认配置
    assert(g_initialized == true);
    assert(g_emergency_stop == false);
    assert(g_current_duty == 0.0f);

    printf("[UT] TempCtrl_UT_Init_DefaultConfig: PASSED\n");
}

void TempCtrl_UT_CalcDuty_NormalHeating(void)
{
    // 初始化
    TempCtrl_Config_t config = {
        .kp = 2.5f, .ki = 0.1f, .kd = 0.05f,
        .max_duty = 100.0f, .min_duty = 0.0f,
        .ramp_rate = 10.0f
    };
    TempCtrl_Init(&config);

    // 目标 50°C，实际 25°C，应输出正占空比
    float duty = TempCtrl_CalcDuty(50.0f, 25.0f);
    assert(duty > 0.0f);
    assert(duty <= 100.0f);

    printf("[UT] TempCtrl_UT_CalcDuty_NormalHeating: PASSED (duty=%.1f%%)\n", duty);
}

void TempCtrl_UT_CalcDuty_OverTempCutoff(void)
{
    TempCtrl_Init(NULL);

    // 实际温度超过 85°C，应切断
    float duty = TempCtrl_CalcDuty(50.0f, 90.0f);
    assert(duty == 0.0f);

    printf("[UT] TempCtrl_UT_CalcDuty_OverTempCutoff: PASSED\n");
}

void TempCtrl_UT_CalcDuty_TargetReached(void)
{
    TempCtrl_Init(NULL);

    // 目标温度 <= 实际温度，应关闭
    float duty = TempCtrl_CalcDuty(30.0f, 35.0f);
    assert(duty == 0.0f);

    printf("[UT] TempCtrl_UT_CalcDuty_TargetReached: PASSED\n");
}

void TempCtrl_UT_EmergencyStop(void)
{
    TempCtrl_Init(NULL);

    // 先正常计算
    TempCtrl_CalcDuty(50.0f, 25.0f);
    assert(g_current_duty > 0.0f);

    // 触发紧急停止
    TempCtrl_EmergencyStop();
    assert(g_emergency_stop == true);
    assert(g_current_duty == 0.0f);

    // 紧急停止后计算应返回 0
    float duty = TempCtrl_CalcDuty(50.0f, 25.0f);
    assert(duty == 0.0f);

    printf("[UT] TempCtrl_UT_EmergencyStop: PASSED\n");
}

void TempCtrl_UT_RampLimit(void)
{
    TempCtrl_Config_t config = {
        .kp = 5.0f, .ki = 0.5f, .kd = 0.1f,  // 高增益以产生大输出
        .max_duty = 100.0f, .min_duty = 0.0f,
        .ramp_rate = 10.0f  // 10%/s
    };
    TempCtrl_Init(&config);

    // 第一次调用
    float duty1 = TempCtrl_CalcDuty(80.0f, 20.0f);

    // 10ms 后再次调用，爬升不应超过 0.1% (10%/s * 0.01s)
    float duty2 = TempCtrl_CalcDuty(80.0f, 20.0f);
    float delta = duty2 - duty1;

    // 允许一定浮点误差
    assert(delta <= 0.15f);  // 10%/s * 0.01s = 0.1%，留余量

    printf("[UT] TempCtrl_UT_RampLimit: PASSED (delta=%.2f%%)\n", delta);
}

void TempCtrl_UT_RunAll(void)
{
    printf("\n=== Temperature Control Unit Tests ===\n");
    TempCtrl_UT_Init_DefaultConfig();
    TempCtrl_UT_CalcDuty_NormalHeating();
    TempCtrl_UT_CalcDuty_OverTempCutoff();
    TempCtrl_UT_CalcDuty_TargetReached();
    TempCtrl_UT_EmergencyStop();
    TempCtrl_UT_RampLimit();
    printf("=== All Tests Completed ===\n\n");
}

#endif // UNIT_TEST
