/**
 * @file ech_pid.h
 * @brief ECH 水加热器 ECU - PID 控制算法服务
 *
 * @copyright Copyright (c) 2026 ECH ECU Development Team
 * @license MIT License
 *
 * @version 0.1
 * @date 2026-03-04
 *
 * @details
 * 本模块提供 PID (比例 - 积分 - 微分) 控制算法，用于 ECH
 * 水加热器的温度闭环控制。
 *
 * 过程域: SWE.3 (软件详细设计与单元实现)
 * 安全等级: ASIL B (根据 HARA 报告 SG-001)
 *
 * 需求追溯:
 * - SWE-F-001: 温度闭环控制
 * - SWE-F-008: PID 参数可调
 * - SWE-P-003: 控制周期 100ms
 */

#ifndef ECH_PID_H
#define ECH_PID_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 包含头文件
 * ============================================================================
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * 宏定义
 * ============================================================================
 */

/** PID 模块版本号 */
#define ECH_PID_VERSION_MAJOR (0)
#define ECH_PID_VERSION_MINOR (1)
#define ECH_PID_VERSION_PATCH (0)

/** PID 参数默认值 */
#define ECH_PID_DEFAULT_KP (2.0f) /**< 默认比例增益 */
#define ECH_PID_DEFAULT_KI (0.1f) /**< 默认积分增益 */
#define ECH_PID_DEFAULT_KD (0.5f) /**< 默认微分增益 */

/** PID 输出限幅 (PWM 占空比 0-100%) */
#define ECH_PID_OUTPUT_MIN (0.0f)
#define ECH_PID_OUTPUT_MAX (100.0f)

/** 积分抗饱和限幅 */
#define ECH_PID_INTEGRAL_MIN (-500.0f)
#define ECH_PID_INTEGRAL_MAX (500.0f)

/** 误差死区 (°C) - 避免在目标值附近频繁震荡 */
#define ECH_PID_DEADBAND (0.5f)

/** 微分滤波系数 (0.0-1.0, 越小滤波越强) */
#define ECH_PID_DERIVATIVE_FILTER (0.1f)

/* ============================================================================
 * 类型定义
 * ============================================================================
 */

/**
 * @brief PID 控制器状态结构体
 *
 * @details
 * 包含 PID 控制所需的全部状态变量和参数。
 * 每个温度控制回路需要一个独立的实例。
 */
typedef struct {
  /* PID 参数 */
  float kp; /**< 比例增益 */
  float ki; /**< 积分增益 */
  float kd; /**< 微分增益 */

  /* 状态变量 */
  float integral;        /**< 积分累积值 */
  float prev_error;      /**< 上一次误差值 */
  float prev_derivative; /**< 上一次微分值 (用于滤波) */

  /* 设定值与测量值 */
  float setpoint; /**< 目标设定值 (°C) */
  float measured; /**< 实际测量值 (°C) */

  /* 输出与控制 */
  float output; /**< PID 计算输出 (PWM 占空比 %) */
  bool enabled; /**< 控制器使能标志 */

  /* 运行统计 */
  uint32_t cycle_count;    /**< 控制周期计数 */
  uint32_t last_update_ms; /**< 上次更新的时间戳 (ms) */

  /* 诊断信息 */
  bool integral_saturated; /**< 积分饱和标志 */
  bool output_saturated;   /**< 输出饱和标志 */
} EchPidController_t;

/**
 * @brief PID 配置参数结构体
 *
 * @details
 * 用于初始化或重新配置 PID 控制器参数。
 */
typedef struct {
  float kp;       /**< 比例增益 */
  float ki;       /**< 积分增益 */
  float kd;       /**< 微分增益 */
  float setpoint; /**< 初始设定值 (°C) */
  bool enabled;   /**< 初始使能状态 */
} EchPidConfig_t;

/**
 * @brief PID 诊断信息结构体
 *
 * @details
 * 用于外部读取 PID 控制器的运行状态和诊断信息。
 */
typedef struct {
  float current_error;    /**< 当前误差 */
  float integral_value;   /**< 当前积分值 */
  float derivative_value; /**< 当前微分值 */
  float output_value;     /**< 当前输出值 */
  bool is_enabled;        /**< 使能状态 */
  bool is_saturated;      /**< 饱和状态 (积分或输出) */
  uint32_t cycle_count;   /**< 运行周期数 */
} EchPidDiagnostic_t;

/* ============================================================================
 * 函数声明
 * ============================================================================
 */

/**
 * @brief 初始化 PID 控制器
 *
 * @param controller 指向 PID 控制器结构体的指针
 * @param config 指向配置参数结构体的指针
 * @return int32_t 0=成功，负值=错误码
 *
 * @details
 * 初始化 PID 控制器的所有参数和状态变量。
 * 必须在调用其他函数前调用此函数。
 *
 * @pre controller != NULL
 * @pre config != NULL
 * @post controller->enabled == config->enabled
 *
 * @note 时间复杂度: O(1)
 * @note 可重入性: 是 (针对不同实例)
 */
int32_t EchPid_Init(EchPidController_t *controller,
                    const EchPidConfig_t *config);

/**
 * @brief 执行 PID 控制计算
 *
 * @param controller 指向 PID 控制器结构体的指针
 * @param measured_value 当前测量值 (°C)
 * @param timestamp_ms 当前时间戳 (ms)
 * @return float PID 计算输出值 (PWM 占空比 %)
 *
 * @details
 * 执行完整的 PID 控制算法，包括：
 * 1. 误差计算
 * 2. 比例项计算
 * 3. 积分项计算 (带抗饱和)
 * 4. 微分项计算 (带滤波)
 * 5. 输出限幅
 *
 * @pre controller != NULL
 * @pre controller->enabled == true
 *
 * @note 建议调用周期: 100ms (SWE-P-003)
 * @note 时间复杂度: O(1)
 */
float EchPid_Calculate(EchPidController_t *controller, float measured_value,
                       uint32_t timestamp_ms);

/**
 * @brief 使能/禁用 PID 控制器
 *
 * @param controller 指向 PID 控制器结构体的指针
 * @param enable true=使能，false=禁用
 *
 * @details
 * 禁用时清除积分累积，避免积分风up。
 * 使能时平滑过渡，避免输出跳变。
 */
void EchPid_SetEnabled(EchPidController_t *controller, bool enable);

/**
 * @brief 设置 PID 目标设定值
 *
 * @param controller 指向 PID 控制器结构体的指针
 * @param setpoint 目标设定值 (°C)
 *
 * @details
 * 设定值变化时，控制器会自动调整输出。
 * 建议设定值变化率不超过 5°C/s，避免热冲击。
 */
void EchPid_SetSetpoint(EchPidController_t *controller, float setpoint);

/**
 * @brief 调整 PID 参数
 *
 * @param controller 指向 PID 控制器结构体的指针
 * @param kp 新的比例增益
 * @param ki 新的积分增益
 * @param kd 新的微分增益
 *
 * @details
 * 支持运行时参数调整，用于标定和优化。
 * 参数变化时，积分值保持不变，避免输出跳变。
 */
void EchPid_Tune(EchPidController_t *controller, float kp, float ki, float kd);

/**
 * @brief 重置 PID 控制器状态
 *
 * @param controller 指向 PID 控制器结构体的指针
 *
 * @details
 * 清除所有状态变量 (积分、误差、输出等)。
 * 用于系统复位或故障恢复场景。
 */
void EchPid_Reset(EchPidController_t *controller);

/**
 * @brief 获取 PID 诊断信息
 *
 * @param controller 指向 PID 控制器结构体的指针
 * @param diagnostic 指向诊断信息结构体的指针
 *
 * @details
 * 用于外部监控和诊断，支持 UDS 服务读取。
 */
void EchPid_GetDiagnostic(EchPidController_t *controller,
                          EchPidDiagnostic_t *diagnostic);

/**
 * @brief 获取 PID 模块版本号
 *
 * @param major 主版本号输出指针
 * @param minor 次版本号输出指针
 * @param patch 修订版本号输出指针
 */
void EchPid_GetVersion(uint8_t *major, uint8_t *minor, uint8_t *patch);

#ifdef __cplusplus
}
#endif

#endif /* ECH_PID_H */
