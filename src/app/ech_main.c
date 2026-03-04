/**
 * @file ech_main.c
 * @brief ECH 水加热器 ECU - 主程序框架
 *
 * @copyright Copyright (c) 2026 ECH ECU Development Team
 * @license MIT License
 *
 * @version 0.1
 * @date 2026-03-04
 *
 * @details
 * 本文件是 ECH 水加热器 ECU
 * 的主程序入口，负责系统初始化、任务调度和主循环管理。
 *
 * 过程域: SWE.3 (软件详细设计与单元实现)
 * 安全等级: ASIL B (根据 HARA 报告 SG-001)
 *
 * 需求追溯:
 * - SWE-F-001: 系统初始化
 * - SWE-F-004: 任务调度
 * - SWE-S-002: 看门狗管理
 * - SWE-S-003: 故障安全处理
 */

/* ============================================================================
 * 包含头文件
 * ============================================================================
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* 驱动层 */
#include "../drivers/ech_adc.h"
#include "../drivers/ech_lin.h"
#include "../drivers/ech_pwm.h"

/* 服务层 */
#include "../services/ech_diag.h"
#include "../services/ech_pid.h"
#include "../services/ech_watchdog.h"

/* 应用层 */
#include "ech_state.h"

/* ============================================================================
 * 宏定义
 * ============================================================================
 */

/** 主程序版本号 */
#define ECH_MAIN_VERSION_MAJOR (0)
#define ECH_MAIN_VERSION_MINOR (1)
#define ECH_MAIN_VERSION_PATCH (0)

/** 主循环周期 (ms) */
#define MAIN_LOOP_PERIOD_MS (10)

/** 100ms 任务计数器 */
#define TASK_100MS_COUNT (10)

/** 500ms 任务计数器 */
#define TASK_500MS_COUNT (50)

/** 1000ms 任务计数器 */
#define TASK_1000MS_COUNT (100)

/** 系统启动延时 (ms) */
#define SYSTEM_STARTUP_DELAY_MS (100)

/* ============================================================================
 * 类型定义
 * ============================================================================
 */

/**
 * @brief 系统运行状态枚举
 */
typedef enum {
  SYS_STATE_INIT = 0,    /**< 初始化中 */
  SYS_STATE_RUNNING = 1, /**< 正常运行 */
  SYS_STATE_ERROR = 2,   /**< 错误状态 */
  SYS_STATE_SHUTDOWN = 3 /**< 关机状态 */
} SystemRunState_t;

/**
 * @brief 系统控制器结构体
 */
typedef struct {
  SystemRunState_t runState; /**< 运行状态 */
  uint32_t loopCount;        /**< 主循环计数 */
  uint32_t timestamp_ms;     /**< 系统时间戳 */
  bool systemReady;          /**< 系统就绪标志 */

  /* 各模块控制器 */
  EchAdcController_t adc;     /**< ADC 控制器 */
  EchLinController_t lin;     /**< LIN 控制器 */
  EchPidController_t pid;     /**< PID 控制器 */
  EchWdgController_t wdg;     /**< 看门狗控制器 */
  EchDiagController_t diag;   /**< 诊断控制器 */
  EchStateController_t state; /**< 状态机控制器 */
} SystemController_t;

/* ============================================================================
 * 全局变量
 * ============================================================================
 */

/** 系统控制器实例 */
static SystemController_t g_system;

/** 主循环周期计数器 */
static uint32_t g_loopCounter = 0;

/* ============================================================================
 * 局部函数声明
 * ============================================================================
 */

/**
 * @brief 系统初始化
 */
static int32_t System_Init(void);

/**
 * @brief 硬件初始化
 */
static void Hardware_Init(void);

/**
 * @brief 获取系统时间戳 (模拟)
 */
static uint32_t GetTimestamp_ms(void);

/**
 * @brief 10ms 周期任务
 */
static void Task_10ms(void);

/**
 * @brief 100ms 周期任务
 */
static void Task_100ms(void);

/**
 * @brief 500ms 周期任务
 */
static void Task_500ms(void);

/**
 * @brief 1000ms 周期任务
 */
static void Task_1000ms(void);

/**
 * @brief 系统关机处理
 */
static void System_Shutdown(void);

/* ============================================================================
 * 主函数
 * ============================================================================
 */

/**
 * @brief 主程序入口
 *
 * @details
 * ECH ECU 主程序入口，执行以下流程:
 * 1. 硬件初始化
 * 2. 各模块初始化
 * 3. 系统自检
 * 4. 进入主循环
 *
 * 主循环任务调度:
 * - 10ms: ADC 采样、状态机更新
 * - 100ms: PID 控制、LIN 通信
 * - 500ms: 看门狗喂狗、诊断更新
 * - 1000ms: 系统监控、故障检查
 */
int main(void) {
  int32_t result;

  printf("=== ECH ECU System Starting ===\n");

  /* Step 1: 硬件初始化 */
  Hardware_Init();
  printf("Hardware initialized\n");

  /* Step 2: 系统初始化 */
  result = System_Init();
  if (result != 0) {
    printf("System init failed: %d\n", result);
    g_system.runState = SYS_STATE_ERROR;
    while (1) {
      /* 错误状态，无限循环 */
    }
  }
  printf("System initialized\n");

  /* Step 3: 启动看门狗 */
  EchWdg_Start(&g_system.wdg, GetTimestamp_ms());
  printf("Watchdog started\n");

  /* Step 4: 系统自检延时 */
  printf("System self-test...\n");

  /* Step 5: 切换到待机状态 */
  EchState_RequestTransition(&g_system.state, ECH_STATE_STANDBY,
                             GetTimestamp_ms());
  g_system.systemReady = true;
  g_system.runState = SYS_STATE_RUNNING;

  printf("=== System Ready ===\n\n");

  /* Step 6: 主循环 */
  while (g_system.runState == SYS_STATE_RUNNING) {
    /* 主循环周期控制 (模拟 10ms) */

    /* 更新系统时间戳 */
    g_system.timestamp_ms = GetTimestamp_ms();
    g_loopCounter++;

    /* 10ms 周期任务 (每次循环执行) */
    Task_10ms();

    /* 100ms 周期任务 */
    if (g_loopCounter % TASK_100MS_COUNT == 0) {
      Task_100ms();
    }

    /* 500ms 周期任务 */
    if (g_loopCounter % TASK_500MS_COUNT == 0) {
      Task_500ms();
    }

    /* 1000ms 周期任务 */
    if (g_loopCounter % TASK_1000MS_COUNT == 0) {
      Task_1000ms();
    }

    /* 喂狗 */
    EchWdg_Feed(&g_system.wdg, g_system.timestamp_ms);
  }

  /* Step 7: 系统关机 */
  System_Shutdown();

  return 0;
}

/* ============================================================================
 * 局部函数实现
 * ============================================================================
 */

/**
 * @brief 系统初始化
 */
static int32_t System_Init(void) {
  int32_t result;

  /* 清零全局变量 */
  g_system.runState = SYS_STATE_INIT;
  g_system.loopCount = 0;
  g_system.timestamp_ms = 0;
  g_system.systemReady = false;
  g_loopCounter = 0;

  /* 初始化 ADC */
  result = EchAdc_Init(&g_system.adc);
  if (result != 0)
    return -1;

  /* 初始化 LIN */
  EchLinConfig_t linConfig = {
      .baudrate = 19200, .nodeId = 1, .isMaster = true, .scheduleTableId = 0};
  result = EchLin_Init(&g_system.lin, &linConfig);
  if (result != 0)
    return -2;

  /* 初始化 PID */
  EchPidConfig_t pidConfig = {
      .kp = 2.0f, .ki = 0.1f, .kd = 0.5f, .setpoint = 50.0f, .enabled = false};
  result = EchPid_Init(&g_system.pid, &pidConfig);
  if (result != 0)
    return -3;

  /* 初始化看门狗 */
  EchWdgConfig_t wdgConfig = {.mode = ECH_WDG_MODE_INDEPENDENT,
                              .timeout_ms = 1000,
                              .window_ms = 200,
                              .autoResetEnabled = true,
                              .maxResetCount = 3};
  result = EchWdg_Init(&g_system.wdg, &wdgConfig);
  if (result != 0)
    return -4;

  /* 初始化诊断 */
  EchDiagConfig_t diagConfig = {.sessionTimeout_ms = 5000,
                                .p2ServerTime_ms = 50,
                                .securityEnabled = false};
  result = EchDiag_Init(&g_system.diag, &diagConfig);
  if (result != 0)
    return -5;

  /* 初始化状态机 */
  EchStateConfig_t stateConfig = {.autoRecoverEnabled = true,
                                  .recoverDelay_ms = 5000,
                                  .overTempThreshold = 85.0f,
                                  .overCurrentThreshold = 50.0f,
                                  .lowVoltageThreshold = 9.0f,
                                  .highVoltageThreshold = 16.0f};
  result = EchState_Init(&g_system.state, &stateConfig);
  if (result != 0)
    return -6;

  /* 注册看门狗任务 */
  EchWdg_RegisterTask(&g_system.wdg, 1, "MainLoop", 1000);
  EchWdg_RegisterTask(&g_system.wdg, 2, "CommTask", 500);

  return 0;
}

/**
 * @brief 硬件初始化 (模拟)
 */
static void Hardware_Init(void) {
  /* 实际项目中替换为真实硬件初始化 */
  /* 例如：时钟配置、GPIO 初始化、ADC 配置、PWM 配置等 */
}

/**
 * @brief 获取系统时间戳 (模拟)
 */
static uint32_t GetTimestamp_ms(void) {
  /* 实际项目中替换为真实系统计时器 */
  /* 例如：SysTick->VAL 或 HAL_GetTick() */
  static uint32_t simulatedTime = 0;
  simulatedTime += MAIN_LOOP_PERIOD_MS;
  return simulatedTime;
}

/**
 * @brief 10ms 周期任务
 */
static void Task_10ms(void) {
  /* ADC 采样 */
  EchAdc_Sample(&g_system.adc, g_system.timestamp_ms);

  /* 获取温度值（用于监控，暂未用于控制） */
  (void)EchAdc_GetTemperature(&g_system.adc, ECH_ADC_CH_TEMP_INLET);
  (void)EchAdc_GetTemperature(&g_system.adc, ECH_ADC_CH_TEMP_OUTLET);

  /* 更新状态机 */
  EchState_Update(&g_system.state, g_system.timestamp_ms);

  /* 更新诊断状态 */
  EchDiag_Update(&g_system.diag, g_system.timestamp_ms);
}

/**
 * @brief 100ms 周期任务
 */
static void Task_100ms(void) {
  /* LIN 通信调度 */
  EchLin_Schedule(&g_system.lin, g_system.timestamp_ms);

  /* 获取目标温度 (从 LIN 或 CAN 接收) */
  const float targetTemp = 50.0f; /* 默认目标温度 */

  /* 获取实际温度 */
  const float currentTemp =
      EchAdc_GetTemperature(&g_system.adc, ECH_ADC_CH_TEMP_OUTLET);

  /* PID 控制计算 */
  if (EchState_IsRunning(&g_system.state)) {
    const float pwmDuty =
        EchPid_Calculate(&g_system.pid, currentTemp, g_system.timestamp_ms);

    /* 设置 PWM 输出（暂未启用） */
    (void)pwmDuty;
  }
  (void)targetTemp;
  (void)currentTemp;
}

/**
 * @brief 500ms 周期任务
 */
static void Task_500ms(void) {
  /* 看门狗喂狗 (子任务) */
  EchWdg_FeedTask(&g_system.wdg, 2, g_system.timestamp_ms);

  /* LIN 心跳发送 */
  EchLin_SendHeartbeat(&g_system.lin, g_system.timestamp_ms);

  /* 诊断信息更新 */
  /* 可通过 UDS 服务读取 */
}

/**
 * @brief 1000ms 周期任务
 */
static void Task_1000ms(void) {
  /* 系统健康检查 */
  bool isHealthy = EchWdg_IsHealthy(&g_system.wdg, g_system.timestamp_ms);

  if (!isHealthy) {
    /* 系统不健康，报告故障 */
    EchState_ReportFault(&g_system.state, ECH_FAULT_WATCHDOG, 0.0f);
  }

  /* 检查传感器故障 */
  EchAdcStatus_t adcStatus =
      EchAdc_GetChannelStatus(&g_system.adc, ECH_ADC_CH_TEMP_OUTLET);
  if (adcStatus == ECH_ADC_STATUS_OPEN || adcStatus == ECH_ADC_STATUS_SHORT) {
    EchState_ReportFault(&g_system.state, ECH_FAULT_SENSOR_OPEN, 0.0f);
  }

  /* 检查过温 */
  float outletTemp =
      EchAdc_GetTemperature(&g_system.adc, ECH_ADC_CH_TEMP_OUTLET);
  if (outletTemp > 85.0f) {
    EchState_ReportFault(&g_system.state, ECH_FAULT_OVER_TEMP, outletTemp);
  }

  /* 打印系统状态 (调试用) */
  printf("[%" PRIu32 "ms] State=%s, Temp=%.1f°C, Healthy=%d\n",
         g_system.timestamp_ms,
         EchState_GetStateString(EchState_GetCurrentState(&g_system.state)),
         outletTemp, isHealthy);
}

/**
 * @brief 系统关机处理
 */
static void System_Shutdown(void) {
  printf("System shutting down...\n");

  /* 停止 PWM 输出 */
  /* EchPwm_SetDuty(0); */

  /* 停止看门狗 */
  EchWdg_Stop(&g_system.wdg);

  /* 进入关机状态 */
  EchState_RequestTransition(&g_system.state, ECH_STATE_SHUTDOWN,
                             g_system.timestamp_ms);

  printf("System shutdown complete\n");
}

/* ============================================================================
 * 版本信息
 * ============================================================================
 */

/**
 * @brief 获取主程序版本号
 */
void EchMain_GetVersion(uint8_t *major, uint8_t *minor, uint8_t *patch) {
  if (major != NULL)
    *major = ECH_MAIN_VERSION_MAJOR;
  if (minor != NULL)
    *minor = ECH_MAIN_VERSION_MINOR;
  if (patch != NULL)
    *patch = ECH_MAIN_VERSION_PATCH;
}
