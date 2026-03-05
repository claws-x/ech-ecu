/**
 * @file ech_pid.c
 * @brief ECH 水加热器 ECU - PID 控制算法服务实现
 *
 * @copyright Copyright (c) 2026 ECH ECU Development Team
 * @license MIT License
 *
 * @version 0.1
 * @date 2026-03-04
 *
 * @details
 * 本文件实现 PID (比例 - 积分 - 微分) 控制算法，用于 ECH
 * 水加热器的温度闭环控制。
 *
 * 过程域: SWE.3 (软件详细设计与单元实现)
 * 安全等级: ASIL B (根据 HARA 报告 SG-001)
 *
 * 需求追溯:
 * - SWE-F-001: 温度闭环控制
 * - SWE-F-008: PID 参数可调
 * - SWE-P-003: 控制周期 100ms
 *
 * 单元测试: 见 src/tests/test_ech_pid.c
 */

/* ============================================================================
 * 包含头文件
 * ============================================================================
 */

#include "ech_pid.h"
#include <string.h>

/* ============================================================================
 * 局部宏定义
 * ============================================================================
 */

/** 最小控制周期 (ms) - 防止过频调用 */
#define MIN_CONTROL_PERIOD_MS (50)

/** 误差死区阈值 */
#define DEADBAND_THRESHOLD (ECH_PID_DEADBAND)

/* ============================================================================
 * 局部函数声明
 * ============================================================================
 */

/**
 * @brief 计算误差值 (带死区处理)
 */
static float CalculateError(float setpoint, float measured);

/**
 * @brief 限幅函数
 */
static float Clamp(float value, float min, float max);

/* ============================================================================
 * 函数实现
 * ============================================================================
 */

/**
 * @brief 初始化 PID 控制器
 */
int32_t EchPid_Init(EchPidController_t *controller,
                    const EchPidConfig_t *config) {
  /* 参数检查 */
  if (controller == NULL || config == NULL) {
    return -1; /* 错误：空指针 */
  }

  /* 清零结构体 */
  memset(controller, 0, sizeof(EchPidController_t));

  /* 设置 PID 参数 */
  controller->kp = config->kp;
  controller->ki = config->ki;
  controller->kd = config->kd;

  /* 设置初始状态 */
  controller->setpoint = config->setpoint;
  controller->enabled = config->enabled;

  /* 初始化状态变量 */
  controller->integral = 0.0f;
  controller->prev_error = 0.0f;
  controller->prev_derivative = 0.0f;
  controller->output = 0.0f;
  controller->cycle_count = 0;
  controller->last_update_ms = 0;

  /* 诊断标志 */
  controller->integral_saturated = false;
  controller->output_saturated = false;

  return 0; /* 成功 */
}

/**
 * @brief 执行 PID 控制计算
 */
float EchPid_Calculate(EchPidController_t *controller, float measured_value,
                       uint32_t timestamp_ms) {
  /* 参数检查 */
  if (controller == NULL) {
    return 0.0f;
  }

  /* 检查控制器是否使能 */
  if (!controller->enabled) {
    controller->output = 0.0f;
    return controller->output;
  }

  /* 检查控制周期 (防止过频调用) */
  if (controller->last_update_ms > 0) {
    uint32_t delta_ms = timestamp_ms - controller->last_update_ms;
    if (delta_ms < MIN_CONTROL_PERIOD_MS) {
      /* 周期过短，返回上次输出 */
      return controller->output;
    }
  }

  /* 更新测量值 */
  controller->measured = measured_value;

  /* Step 1: 计算误差 (带死区) */
  float error = CalculateError(controller->setpoint, measured_value);

  /* Step 2: 计算时间增量 (秒) */
  float dt = 0.1f; /* 默认 100ms */
  if (controller->last_update_ms > 0) {
    dt = (float)(timestamp_ms - controller->last_update_ms) / 1000.0f;
  }

  /* Step 3: 比例项 */
  float proportional = controller->kp * error;

  /* Step 4: 积分项 (带抗饱和) */
  controller->integral += error * dt;
  controller->integral =
      Clamp(controller->integral, ECH_PID_INTEGRAL_MIN, ECH_PID_INTEGRAL_MAX);

  /* 检测积分饱和 */
  controller->integral_saturated =
      (controller->integral >= ECH_PID_INTEGRAL_MAX) ||
      (controller->integral <= ECH_PID_INTEGRAL_MIN);

  float integral_term = controller->ki * controller->integral;

  /* Step 5: 微分项 (带一阶低通滤波) */
  float derivative_raw = (error - controller->prev_error) / dt;
  controller->prev_derivative =
      ECH_PID_DERIVATIVE_FILTER * derivative_raw +
      (1.0f - ECH_PID_DERIVATIVE_FILTER) * controller->prev_derivative;

  float derivative_term = controller->kd * controller->prev_derivative;

  /* Step 6: 计算总输出 */
  controller->output = proportional + integral_term + derivative_term;

  /* Step 7: 输出限幅 */
  float prev_output = controller->output;
  controller->output =
      Clamp(controller->output, ECH_PID_OUTPUT_MIN, ECH_PID_OUTPUT_MAX);

  /* 检测输出饱和 */
  controller->output_saturated = (controller->output != prev_output);

  /* 更新状态 */
  controller->prev_error = error;
  controller->cycle_count++;
  controller->last_update_ms = timestamp_ms;

  return controller->output;
}

/**
 * @brief 使能/禁用 PID 控制器
 */
void EchPid_SetEnabled(EchPidController_t *controller, bool enable) {
  if (controller == NULL) {
    return;
  }

  if (!enable && controller->enabled) {
    /* 禁用时清除积分累积 */
    controller->integral = 0.0f;
    controller->prev_error = 0.0f;
    controller->prev_derivative = 0.0f;
    controller->output = 0.0f;
  }

  controller->enabled = enable;
}

/**
 * @brief 设置 PID 目标设定值
 */
void EchPid_SetSetpoint(EchPidController_t *controller, float setpoint) {
  if (controller == NULL) {
    return;
  }

  controller->setpoint = setpoint;
  /* 设定值变化时不清除积分，避免输出跳变 */
}

/**
 * @brief 调整 PID 参数
 */
void EchPid_Tune(EchPidController_t *controller, float kp, float ki, float kd) {
  if (controller == NULL) {
    return;
  }

  controller->kp = kp;
  controller->ki = ki;
  controller->kd = kd;
  /* 参数变化时不清除积分，保持连续性 */
}

/**
 * @brief 重置 PID 控制器状态
 */
void EchPid_Reset(EchPidController_t *controller) {
  if (controller == NULL) {
    return;
  }

  controller->integral = 0.0f;
  controller->prev_error = 0.0f;
  controller->prev_derivative = 0.0f;
  controller->output = 0.0f;
  controller->cycle_count = 0;
  controller->last_update_ms = 0;
  controller->integral_saturated = false;
  controller->output_saturated = false;
}

/**
 * @brief 获取 PID 诊断信息
 */
void EchPid_GetDiagnostic(const EchPidController_t *controller,
                          EchPidDiagnostic_t *diagnostic) {
  if (controller == NULL || diagnostic == NULL) {
    return;
  }

  diagnostic->current_error = controller->setpoint - controller->measured;
  diagnostic->integral_value = controller->integral;
  diagnostic->derivative_value = controller->prev_derivative;
  diagnostic->output_value = controller->output;
  diagnostic->is_enabled = controller->enabled;
  diagnostic->is_saturated =
      controller->integral_saturated || controller->output_saturated;
  diagnostic->cycle_count = controller->cycle_count;
}

/**
 * @brief 获取 PID 模块版本号
 */
void EchPid_GetVersion(uint8_t *major, uint8_t *minor, uint8_t *patch) {
  if (major != NULL)
    *major = ECH_PID_VERSION_MAJOR;
  if (minor != NULL)
    *minor = ECH_PID_VERSION_MINOR;
  if (patch != NULL)
    *patch = ECH_PID_VERSION_PATCH;
}

/* ============================================================================
 * 局部函数实现
 * ============================================================================
 */

/**
 * @brief 计算误差值 (带死区处理)
 */
static float CalculateError(float setpoint, float measured) {
  float error = setpoint - measured;

  /* 死区处理：误差在±DEADBAND 范围内视为 0 */
  if (error > -DEADBAND_THRESHOLD && error < DEADBAND_THRESHOLD) {
    return 0.0f;
  }

  return error;
}

/**
 * @brief 限幅函数
 */
static float Clamp(float value, float min, float max) {
  if (value < min) {
    return min;
  }
  if (value > max) {
    return max;
  }
  return value;
}

/* ============================================================================
 * 单元测试桩代码 (UNIT_TEST 宏定义时编译)
 * ============================================================================
 */

#ifdef UNIT_TEST

#include <assert.h>
#include <stdio.h>

void test_pid_init(void) {
  EchPidController_t pid;
  EchPidConfig_t config = {
      .kp = 2.0f, .ki = 0.1f, .kd = 0.5f, .setpoint = 50.0f, .enabled = true};

  int32_t result = EchPid_Init(&pid, &config);
  assert(result == 0);
  assert(pid.kp == 2.0f);
  assert(pid.ki == 0.1f);
  assert(pid.kd == 0.5f);
  assert(pid.setpoint == 50.0f);
  assert(pid.enabled == true);

  printf("✓ test_pid_init passed\n");
}

void test_pid_calculate(void) {
  EchPidController_t pid;
  EchPidConfig_t config = {
      .kp = 2.0f, .ki = 0.1f, .kd = 0.5f, .setpoint = 50.0f, .enabled = true};

  EchPid_Init(&pid, &config);

  /* 模拟温度从 20°C 上升到 50°C 的过程 */
  float output = EchPid_Calculate(&pid, 20.0f, 100);
  printf("  t=100ms, T=20°C, PWM=%.2f%%\n", output);
  assert(output > 0.0f); /* 应该有正输出加热 */

  output = EchPid_Calculate(&pid, 35.0f, 200);
  printf("  t=200ms, T=35°C, PWM=%.2f%%\n", output);

  output = EchPid_Calculate(&pid, 48.0f, 300);
  printf("  t=300ms, T=48°C, PWM=%.2f%%\n", output);

  output = EchPid_Calculate(&pid, 50.0f, 400);
  printf("  t=400ms, T=50°C, PWM=%.2f%%\n", output);

  assert(pid.cycle_count == 4);
  printf("✓ test_pid_calculate passed\n");
}

void test_pid_deadband(void) {
  EchPidController_t pid;
  EchPidConfig_t config = {
      .kp = 2.0f, .ki = 0.1f, .kd = 0.5f, .setpoint = 50.0f, .enabled = true};

  EchPid_Init(&pid, &config);

  /* 误差在死区内，输出应为 0 */
  float output = EchPid_Calculate(&pid, 49.8f, 100); /* 误差 -0.2°C */
  assert(output == 0.0f);

  output = EchPid_Calculate(&pid, 50.3f, 200); /* 误差 +0.3°C */
  assert(output == 0.0f);

  /* 误差超出死区，应有输出 */
  output = EchPid_Calculate(&pid, 49.0f, 300); /* 误差 -1.0°C */
  assert(output > 0.0f);

  printf("✓ test_pid_deadband passed\n");
}

void test_pid_enabled_disable(void) {
  EchPidController_t pid;
  EchPidConfig_t config = {
      .kp = 2.0f, .ki = 0.1f, .kd = 0.5f, .setpoint = 50.0f, .enabled = true};

  EchPid_Init(&pid, &config);

  /* 使能状态下计算 */
  float output = EchPid_Calculate(&pid, 20.0f, 100);
  assert(output > 0.0f);

  /* 禁用后输出应为 0 */
  EchPid_SetEnabled(&pid, false);
  output = EchPid_Calculate(&pid, 20.0f, 200);
  assert(output == 0.0f);

  /* 重新使能 */
  EchPid_SetEnabled(&pid, true);
  output = EchPid_Calculate(&pid, 20.0f, 300);
  assert(output > 0.0f);

  printf("✓ test_pid_enabled_disable passed\n");
}

void test_pid_diagnostic(void) {
  EchPidController_t pid;
  EchPidConfig_t config = {
      .kp = 2.0f, .ki = 0.1f, .kd = 0.5f, .setpoint = 50.0f, .enabled = true};

  EchPid_Init(&pid, &config);
  EchPid_Calculate(&pid, 45.0f, 100);

  EchPidDiagnostic_t diag;
  EchPid_GetDiagnostic(&pid, &diag);

  assert(diag.current_error == 5.0f);
  assert(diag.is_enabled == true);
  assert(diag.cycle_count == 1);

  printf("✓ test_pid_diagnostic passed\n");
}

int main(void) {
  printf("=== ECH PID Module Unit Tests ===\n\n");

  test_pid_init();
  test_pid_calculate();
  test_pid_deadband();
  test_pid_enabled_disable();
  test_pid_diagnostic();

  printf("\n=== All tests passed! ===\n");
  return 0;
}

#endif /* UNIT_TEST */
