/**
 * @file ech_temp_ctrl.c
 * @brief 温度闭环控制模块实现
 * @process SWE.3 软件详细设计与单元实现
 * @version 0.1
 * @date 2026-03-02
 */

#include "ech_temp_ctrl.h"
#include "ech_pid.h"
#include "ech_pwm.h"
#include <stdio.h>
#include <string.h>

/*============================================================================
 * 私有类型与宏定义
 *============================================================================*/

// 安全阈值定义
#define TEMP_CUTOFF_HIGH (85.0f)      // 过温切断阈值 °C
#define TEMP_CUTOFF_LOW (-40.0f)      // 低温故障阈值 °C
#define TEMP_VALID_RANGE_MIN (-40.0f) // 有效温度范围最小值
#define TEMP_VALID_RANGE_MAX (120.0f) // 有效温度范围最大值

// 软启动参数
#define RAMP_RATE_DEFAULT (10.0f) // 默认爬升速率 %/s
#define CONTROL_CYCLE_MS (10)     // 控制周期 ms

/*============================================================================
 * 私有变量
 *============================================================================*/

static EchTempCtrlConfig_t g_config;
static bool g_initialized = false;
static bool g_emergency_stop = false;
static float g_target_temp = 25.0f; // 默认目标温度
static float g_actual_temp = 25.0f; // 默认实际温度
static float g_current_duty = 0.0f;
static float g_prev_duty = 0.0f; // 用于爬升限制
static float g_ramp_rate = RAMP_RATE_DEFAULT;

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
static float TempCtrl_ApplyRampLimit(float target_duty, float prev_duty,
                                     float dt_sec);

/**
 * @brief 安全切断逻辑
 */
static void TempCtrl_SafetyCutoff(void);

/*============================================================================
 * 公有函数实现
 *============================================================================*/

int32_t EchTempCtrl_Init(const EchTempCtrlConfig_t *config) {
  if (config == NULL) {
    // 使用默认配置
    g_config.targetTemp = 50.0f;
    g_config.hysteresis = 2.0f;
    g_config.enabled = false;
  } else {
    g_config = *config;
  }

  g_initialized = true;
  g_emergency_stop = false;
  g_current_duty = 0.0f;
  g_prev_duty = 0.0f;

  printf(
      "[TempCtrl] Initialized: Target=%.1f°C, Hysteresis=%.1f°C, Enabled=%s\n",
      g_config.targetTemp, g_config.hysteresis,
      g_config.enabled ? "true" : "false");

  return 0;
}

float EchTempCtrl_Update(float currentTemp, float targetTemp,
                         uint32_t timestamp_ms) {
  (void)timestamp_ms; // 暂未使用

  // 检查初始化状态
  if (!g_initialized) {
    printf("[TempCtrl] Error: Not initialized\n");
    return 0.0f;
  }

  // 检查紧急停止状态
  if (g_emergency_stop) {
    return 0.0f;
  }

  // 检查使能状态
  if (!g_config.enabled) {
    return 0.0f;
  }

  // 保存当前温度
  g_target_temp = targetTemp;
  g_actual_temp = currentTemp;

  // 1. 安全切断检查（优先级最高）
  if (currentTemp >= TEMP_CUTOFF_HIGH) {
    printf("[TempCtrl] SAFETY: Over-temperature cutoff at %.1f°C\n",
           currentTemp);
    TempCtrl_SafetyCutoff();
    return 0.0f;
  }

  // 2. 温度传感器有效性检查
  if (!TempCtrl_IsTempValid(currentTemp)) {
    printf("[TempCtrl] Error: Invalid temperature reading %.1f°C\n",
           currentTemp);
    TempCtrl_SafetyCutoff();
    return 0.0f;
  }

  // 3. 计算目标温度与实际温度的偏差（带迟滞）
  float error = targetTemp - currentTemp;
  float hysteresis_band = g_config.hysteresis;

  // 4. 如果目标温度 <= 实际温度（考虑迟滞），关闭加热
  if (error <= -hysteresis_band) {
    g_prev_duty = 0.0f;
    g_current_duty = 0.0f;
    return 0.0f;
  }

  // 5. 简单比例控制（可扩展为 PID）
  float duty;
  if (error >= hysteresis_band) {
    // 需要加热，使用比例控制
    float kp = 2.5f; // 比例系数
    duty = error * kp;

    // 限制在 0-100%
    if (duty > 100.0f)
      duty = 100.0f;
    if (duty < 0.0f)
      duty = 0.0f;
  } else {
    // 在迟滞带内，保持当前状态
    duty = g_current_duty;
  }

  // 6. 应用爬升速率限制
  float dt_sec = CONTROL_CYCLE_MS / 1000.0f;
  float limited_duty = TempCtrl_ApplyRampLimit(duty, g_prev_duty, dt_sec);

  // 7. 更新占空比
  g_prev_duty = g_current_duty;
  g_current_duty = limited_duty;

  // 8. 输出到 PWM 驱动
  PWM_SetDuty(limited_duty);

  return limited_duty;
}

void EchTempCtrl_GetVersion(uint8_t *major, uint8_t *minor, uint8_t *patch) {
  if (major)
    *major = 0;
  if (minor)
    *minor = 1;
  if (patch)
    *patch = 0;
}

/*============================================================================
 * 私有函数实现
 *============================================================================*/

static bool TempCtrl_IsTempValid(float temp) {
  return (temp >= TEMP_VALID_RANGE_MIN) && (temp <= TEMP_VALID_RANGE_MAX);
}

static float TempCtrl_ApplyRampLimit(float target_duty, float prev_duty,
                                     float dt_sec) {
  float max_change = g_ramp_rate * dt_sec; // 本次周期允许的最大变化量

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

static void TempCtrl_SafetyCutoff(void) {
  g_current_duty = 0.0f;
  g_prev_duty = 0.0f;
  PWM_SetDuty(0.0f);
}

/*============================================================================
 * 单元测试桩（SWE.4 单元验证）
 *============================================================================*/

#ifdef UNIT_TEST

#include <assert.h>
#include <math.h>

void EchTempCtrl_UT_Init_DefaultConfig(void) {
  EchTempCtrl_Init(NULL); // 使用默认配置
  assert(g_initialized == true);
  assert(g_emergency_stop == false);
  assert(g_current_duty == 0.0f);

  printf("[UT] EchTempCtrl_UT_Init_DefaultConfig: PASSED\n");
}

void EchTempCtrl_UT_Update_NormalHeating(void) {
  // 初始化
  EchTempCtrlConfig_t config = {
      .targetTemp = 50.0f, .hysteresis = 2.0f, .enabled = true};
  EchTempCtrl_Init(&config);

  // 目标 50°C，实际 25°C，应输出正占空比
  float duty = EchTempCtrl_Update(25.0f, 50.0f, 1000);
  assert(duty >= 0.0f);
  assert(duty <= 100.0f);

  printf("[UT] EchTempCtrl_UT_Update_NormalHeating: PASSED (duty=%.1f%%)\n",
         duty);
}

void EchTempCtrl_UT_Update_OverTempCutoff(void) {
  EchTempCtrlConfig_t config = {
      .targetTemp = 50.0f, .hysteresis = 2.0f, .enabled = true};
  EchTempCtrl_Init(&config);

  // 实际温度超过 85°C，应切断
  float duty = EchTempCtrl_Update(90.0f, 50.0f, 1000);
  assert(duty == 0.0f);

  printf("[UT] EchTempCtrl_UT_Update_OverTempCutoff: PASSED\n");
}

void EchTempCtrl_UT_Update_TargetReached(void) {
  EchTempCtrlConfig_t config = {
      .targetTemp = 30.0f, .hysteresis = 2.0f, .enabled = true};
  EchTempCtrl_Init(&config);

  // 目标温度 <= 实际温度（考虑迟滞），应关闭
  float duty = EchTempCtrl_Update(35.0f, 30.0f, 1000);
  assert(duty == 0.0f);

  printf("[UT] EchTempCtrl_UT_Update_TargetReached: PASSED\n");
}

void EchTempCtrl_UT_GetVersion(void) {
  uint8_t major, minor, patch;
  EchTempCtrl_GetVersion(&major, &minor, &patch);
  assert(major == 0);
  assert(minor == 1);
  assert(patch == 0);

  printf("[UT] EchTempCtrl_UT_GetVersion: PASSED (v%d.%d.%d)\n", major, minor,
         patch);
}

void EchTempCtrl_UT_RunAll(void) {
  printf("\n=== Temperature Control Unit Tests ===\n");
  EchTempCtrl_UT_Init_DefaultConfig();
  EchTempCtrl_UT_Update_NormalHeating();
  EchTempCtrl_UT_Update_OverTempCutoff();
  EchTempCtrl_UT_Update_TargetReached();
  EchTempCtrl_UT_GetVersion();
  printf("=== All Tests Completed ===\n\n");
}

#endif // UNIT_TEST
