/**
 * @file ech_state.c
 * @brief ECH 水加热器 ECU - 系统状态管理模块实现
 *
 * @copyright Copyright (c) 2026 ECH ECU Development Team
 * @license MIT License
 *
 * @version 0.1
 * @date 2026-03-04
 *
 * @details
 * 本文件实现 ECH 水加热器 ECU 的系统状态机管理功能。
 *
 * 过程域: SWE.3 (软件详细设计与单元实现)
 * 安全等级: ASIL B (根据 HARA 报告 SG-001/SG-003)
 *
 * 需求追溯:
 * - SWE-F-003: 系统状态管理
 * - SWE-F-010: 故障安全处理
 * - SWE-S-001: 状态转换逻辑
 * - SWE-S-005: 看门狗喂狗策略
 *
 * 单元测试: 见本文件末尾 UNIT_TEST 宏定义部分
 */

/* ============================================================================
 * 包含头文件
 * ============================================================================
 */

#include "ech_state.h"
#include <string.h>

/* ============================================================================
 * 局部宏定义
 * ============================================================================
 */

/** 默认配置参数 */
#define DEFAULT_OVER_TEMP_THRESHOLD (85.0f)    /**< 过温阈值 °C */
#define DEFAULT_OVER_CURRENT_THRESHOLD (50.0f) /**< 过流阈值 A */
#define DEFAULT_LOW_VOLTAGE_THRESHOLD (9.0f)   /**< 低电压阈值 V */
#define DEFAULT_HIGH_VOLTAGE_THRESHOLD (16.0f) /**< 高电压阈值 V */
#define DEFAULT_RECOVER_DELAY_MS (5000)        /**< 恢复延迟 5 秒 */

/* ============================================================================
 * 局部函数声明
 * ============================================================================
 */

/**
 * @brief 验证状态转换是否合法
 */
static bool IsValidTransition(EchSystemState_t fromState,
                              EchSystemState_t toState);

/**
 * @brief 执行状态进入动作
 */
static void OnStateEnter(EchStateController_t *controller,
                         EchSystemState_t newState, uint32_t timestamp_ms);

/**
 * @brief 执行状态退出动作
 */
static void OnStateExit(EchStateController_t *controller,
                        EchSystemState_t oldState);

/**
 * @brief 状态机逻辑处理
 */
static void ProcessStateLogic(EchStateController_t *controller,
                              uint32_t timestamp_ms);

/**
 * @brief 故障等级判断
 */
static EchFaultLevel_t GetFaultLevel(EchFaultType_t faultType);

/**
 * @brief 记录故障到历史
 */
static void RecordFault(EchStateController_t *controller,
                        EchFaultType_t faultType, EchSystemState_t state,
                        float value, uint32_t timestamp_ms);

/* ============================================================================
 * 函数实现
 * ============================================================================
 */

/**
 * @brief 初始化状态机控制器
 */
int32_t EchState_Init(EchStateController_t *controller,
                      const EchStateConfig_t *config) {
  /* 参数检查 */
  if (controller == NULL) {
    return -1;
  }

  /* 清零结构体 */
  memset(controller, 0, sizeof(EchStateController_t));

  /* 设置初始状态 */
  controller->currentState = ECH_STATE_INIT;
  controller->currentSubState = ECH_SUBSTATE_SELFTEST;
  controller->previousState = ECH_STATE_INVALID;

  /* 状态计时 */
  controller->stateEntryTime_ms = 0;
  controller->stateDuration_ms = 0;

  /* 故障管理 */
  controller->activeFault = ECH_FAULT_NONE;
  controller->activeFaultLevel = ECH_FAULT_LEVEL_INFO;
  controller->faultDebounceTimer_ms = 0;
  controller->faultHistoryIndex = 0;
  controller->faultCount = 0;

  /* 配置参数 */
  if (config != NULL) {
    controller->config = *config;
  } else {
    /* 使用默认配置 */
    controller->config.autoRecoverEnabled = true;
    controller->config.recoverDelay_ms = DEFAULT_RECOVER_DELAY_MS;
    controller->config.overTempThreshold = DEFAULT_OVER_TEMP_THRESHOLD;
    controller->config.overCurrentThreshold = DEFAULT_OVER_CURRENT_THRESHOLD;
    controller->config.lowVoltageThreshold = DEFAULT_LOW_VOLTAGE_THRESHOLD;
    controller->config.highVoltageThreshold = DEFAULT_HIGH_VOLTAGE_THRESHOLD;
  }

  /* 运行统计 */
  controller->stateTransitionCount = 0;
  controller->totalRunTime_ms = 0;
  controller->totalFaultCount = 0;

  controller->initialized = true;

  return 0;
}

/**
 * @brief 执行状态机更新
 */
EchSystemState_t EchState_Update(EchStateController_t *controller,
                                 uint32_t timestamp_ms) {
  if (controller == NULL || !controller->initialized) {
    return ECH_STATE_INVALID;
  }

  /* 初始化状态进入时间 (仅在第一次调用时，且 timestamp 不为 0) */
  if (controller->stateEntryTime_ms == 0 && timestamp_ms > 0) {
    controller->stateEntryTime_ms = timestamp_ms;
  }

  /* 更新状态持续时间 */
  controller->stateDuration_ms = timestamp_ms - controller->stateEntryTime_ms;

  /* 执行状态机逻辑 */
  ProcessStateLogic(controller, timestamp_ms);

  return controller->currentState;
}

/**
 * @brief 请求状态转换
 */
int32_t EchState_RequestTransition(EchStateController_t *controller,
                                   EchSystemState_t targetState,
                                   uint32_t timestamp_ms) {
  if (controller == NULL || !controller->initialized) {
    return -1;
  }

  /* 检查转换是否合法 */
  if (!IsValidTransition(controller->currentState, targetState)) {
    return -2; /* 非法转换 */
  }

  /* 保存旧状态 */
  EchSystemState_t oldState = controller->currentState;

  /* 执行退出动作 */
  OnStateExit(controller, oldState);

  /* 更新状态 */
  controller->previousState = oldState;
  controller->currentState = targetState;
  controller->stateTransitionCount++;

  /* 执行进入动作 (使用当前时间戳) */
  OnStateEnter(controller, targetState, timestamp_ms);

  return 0;
}

/**
 * @brief 报告故障
 */
int32_t EchState_ReportFault(EchStateController_t *controller,
                             EchFaultType_t faultType, float currentValue) {
  if (controller == NULL || !controller->initialized ||
      faultType == ECH_FAULT_NONE) {
    return -1;
  }

  /* 故障去抖 */
  uint32_t currentTime = controller->stateEntryTime_ms;
  bool firstFault = (controller->faultDebounceTimer_ms == 0);

  if (firstFault) {
    controller->faultDebounceTimer_ms = currentTime;
    /* 第一次故障，跳过去抖，立即报告 */
  } else if (currentTime - controller->faultDebounceTimer_ms <
             ECH_STATE_FAULT_DEBOUNCE_MS) {
    return 0; /* 去抖中，忽略 */
  }

  /* 确定故障等级 */
  EchFaultLevel_t faultLevel = GetFaultLevel(faultType);

  /* 记录故障 */
  RecordFault(controller, faultType, controller->currentState, currentValue,
              currentTime);
  controller->totalFaultCount++;

  /* 更新活动故障 */
  controller->activeFault = faultType;
  controller->activeFaultLevel = faultLevel;

  /* 根据故障等级采取行动 */
  if (faultLevel >= ECH_FAULT_LEVEL_ERROR) {
    /* 错误级及以上：立即跳转到故障状态 */
    EchState_RequestTransition(controller, ECH_STATE_FAULT, currentTime);
    controller->currentSubState = ECH_SUBSTATE_NONE;
  }

  return 0;
}

/**
 * @brief 清除故障
 */
int32_t EchState_ClearFault(EchStateController_t *controller,
                            EchFaultType_t faultType) {
  if (controller == NULL || !controller->initialized) {
    return -1;
  }

  /* 查找并清除故障 */
  for (int i = 0; i < controller->faultCount && i < ECH_STATE_MAX_FAULT_HISTORY;
       i++) {
    if (controller->faultHistory[i].faultType == faultType &&
        !controller->faultHistory[i].isCleared) {
      controller->faultHistory[i].isCleared = true;

      /* 如果是当前活动故障，清除它 */
      if (controller->activeFault == faultType) {
        controller->activeFault = ECH_FAULT_NONE;
        controller->activeFaultLevel = ECH_FAULT_LEVEL_INFO;
      }

      return 0;
    }
  }

  return -2; /* 故障未找到 */
}

/**
 * @brief 获取当前状态
 */
EchSystemState_t
EchState_GetCurrentState(const EchStateController_t *controller) {
  if (controller == NULL || !controller->initialized) {
    return ECH_STATE_INVALID;
  }

  return controller->currentState;
}

/**
 * @brief 获取当前子状态
 */
EchSystemSubState_t
EchState_GetCurrentSubState(const EchStateController_t *controller) {
  if (controller == NULL || !controller->initialized) {
    return ECH_SUBSTATE_NONE;
  }

  return controller->currentSubState;
}

/**
 * @brief 检查系统是否就绪
 */
bool EchState_IsSystemReady(const EchStateController_t *controller) {
  if (controller == NULL || !controller->initialized) {
    return false;
  }

  return (controller->currentState == ECH_STATE_STANDBY ||
          controller->currentState == ECH_STATE_RUNNING);
}

/**
 * @brief 检查系统是否运行中
 */
bool EchState_IsRunning(const EchStateController_t *controller) {
  if (controller == NULL || !controller->initialized) {
    return false;
  }

  return controller->currentState == ECH_STATE_RUNNING;
}

/**
 * @brief 获取活动故障信息
 */
EchFaultType_t EchState_GetActiveFault(const EchStateController_t *controller) {
  if (controller == NULL || !controller->initialized) {
    return ECH_FAULT_NONE;
  }

  return controller->activeFault;
}

/**
 * @brief 获取故障历史记录
 */
const EchFaultRecord_t *
EchState_GetFaultHistory(const EchStateController_t *controller,
                         uint8_t *count) {
  if (controller == NULL || !controller->initialized || count == NULL) {
    if (count != NULL)
      *count = 0;
    return NULL;
  }

  *count = (controller->faultCount < ECH_STATE_MAX_FAULT_HISTORY)
               ? controller->faultCount
               : ECH_STATE_MAX_FAULT_HISTORY;

  return controller->faultHistory;
}

/**
 * @brief 获取状态字符串描述
 */
const char *EchState_GetStateString(EchSystemState_t state) {
  switch (state) {
  case ECH_STATE_INIT:
    return "INIT";
  case ECH_STATE_STANDBY:
    return "STANDBY";
  case ECH_STATE_RUNNING:
    return "RUNNING";
  case ECH_STATE_FAULT:
    return "FAULT";
  case ECH_STATE_SHUTDOWN:
    return "SHUTDOWN";
  default:
    return "INVALID";
  }
}

/**
 * @brief 重置状态机
 */
void EchState_Reset(EchStateController_t *controller) {
  if (controller == NULL) {
    return;
  }

  /* 保留配置，清除状态 */
  EchStateConfig_t config = controller->config;
  memset(controller, 0, sizeof(EchStateController_t));
  controller->config = config;
  controller->currentState = ECH_STATE_INIT;
  controller->currentSubState = ECH_SUBSTATE_SELFTEST;
  controller->previousState = ECH_STATE_INVALID;
}

/**
 * @brief 获取模块版本号
 */
void EchState_GetVersion(uint8_t *major, uint8_t *minor, uint8_t *patch) {
  if (major != NULL)
    *major = ECH_STATE_VERSION_MAJOR;
  if (minor != NULL)
    *minor = ECH_STATE_VERSION_MINOR;
  if (patch != NULL)
    *patch = ECH_STATE_VERSION_PATCH;
}

/* ============================================================================
 * 局部函数实现
 * ============================================================================
 */

/**
 * @brief 验证状态转换是否合法
 *
 * @details
 * 合法转换规则:
 * - INIT → STANDBY, SHUTDOWN
 * - STANDBY → RUNNING, INIT, SHUTDOWN
 * - RUNNING → STANDBY, FAULT, SHUTDOWN
 * - FAULT → STANDBY (自动恢复), SHUTDOWN
 * - SHUTDOWN → INIT
 */
static bool IsValidTransition(EchSystemState_t fromState,
                              EchSystemState_t toState) {
  /* 相同状态不需要转换 */
  if (fromState == toState) {
    return false;
  }

  switch (fromState) {
  case ECH_STATE_INIT:
    return (toState == ECH_STATE_STANDBY || toState == ECH_STATE_SHUTDOWN);

  case ECH_STATE_STANDBY:
    return (toState == ECH_STATE_RUNNING || toState == ECH_STATE_INIT ||
            toState == ECH_STATE_SHUTDOWN);

  case ECH_STATE_RUNNING:
    return (toState == ECH_STATE_STANDBY || toState == ECH_STATE_FAULT ||
            toState == ECH_STATE_SHUTDOWN);

  case ECH_STATE_FAULT:
    return (toState == ECH_STATE_STANDBY || toState == ECH_STATE_SHUTDOWN);

  case ECH_STATE_SHUTDOWN:
    return (toState == ECH_STATE_INIT);

  default:
    return false;
  }
}

/**
 * @brief 执行状态进入动作
 */
static void OnStateEnter(EchStateController_t *controller,
                         EchSystemState_t newState, uint32_t timestamp_ms) {
  /* 使用传入的时间戳作为状态进入时间 */
  controller->stateEntryTime_ms =
      (timestamp_ms > 0) ? timestamp_ms : controller->stateEntryTime_ms;
  controller->stateDuration_ms = 0;

  /* 设置子状态 */
  switch (newState) {
  case ECH_STATE_INIT:
    controller->currentSubState = ECH_SUBSTATE_SELFTEST;
    break;

  case ECH_STATE_STANDBY:
    controller->currentSubState = ECH_SUBSTATE_NONE;
    break;

  case ECH_STATE_RUNNING:
    controller->currentSubState = ECH_SUBSTATE_PREHEAT;
    break;

  case ECH_STATE_FAULT:
    controller->currentSubState = ECH_SUBSTATE_NONE;
    break;

  case ECH_STATE_SHUTDOWN:
    controller->currentSubState = ECH_SUBSTATE_COOLDOWN;
    break;

  default:
    controller->currentSubState = ECH_SUBSTATE_NONE;
    break;
  }
}

/**
 * @brief 执行状态退出动作
 */
static void OnStateExit(EchStateController_t *controller,
                        EchSystemState_t oldState) {
  /* 统计运行时间 */
  if (oldState == ECH_STATE_RUNNING) {
    controller->totalRunTime_ms += controller->stateDuration_ms;
  }
}

/**
 * @brief 状态机逻辑处理
 */
static void ProcessStateLogic(EchStateController_t *controller,
                              uint32_t timestamp_ms) {
  /* 根据当前状态执行相应逻辑 */
  switch (controller->currentState) {
  case ECH_STATE_INIT:
    /* 初始化状态：执行自检，完成后转到 STANDBY */
    /* 实际应用中这里会检查硬件初始化状态 */
    if (controller->stateDuration_ms >= 100) { /* 模拟 100ms 自检 */
      EchState_RequestTransition(controller, ECH_STATE_STANDBY, timestamp_ms);
    }
    break;

  case ECH_STATE_STANDBY:
    /* 待机状态：等待启动命令 */
    /* 实际应用中这里会检查启动条件 */
    break;

  case ECH_STATE_RUNNING:
    /* 运行状态：监控故障，执行加热控制 */
    /* 子状态转换：PREHEAT → NORMAL */
    if (controller->currentSubState == ECH_SUBSTATE_PREHEAT) {
      if (controller->stateDuration_ms >= 500) { /* 预热 500ms */
        controller->currentSubState = ECH_SUBSTATE_NORMAL;
      }
    }
    break;

  case ECH_STATE_FAULT:
    /* 故障状态：检查是否可自动恢复 */
    if (controller->config.autoRecoverEnabled &&
        controller->activeFault == ECH_FAULT_NONE &&
        controller->stateDuration_ms >= controller->config.recoverDelay_ms) {
      EchState_RequestTransition(controller, ECH_STATE_STANDBY, timestamp_ms);
      controller->currentSubState = ECH_SUBSTATE_RECOVER;
    }
    break;

  case ECH_STATE_SHUTDOWN:
    /* 关机状态：等待冷却完成 */
    /* 实际应用中这里会监控温度下降到安全值 */
    break;

  default:
    break;
  }
}

/**
 * @brief 故障等级判断
 */
static EchFaultLevel_t GetFaultLevel(EchFaultType_t faultType) {
  switch (faultType) {
  case ECH_FAULT_NONE:
    return ECH_FAULT_LEVEL_INFO;

  case ECH_FAULT_LOW_VOLTAGE:
  case ECH_FAULT_HIGH_VOLTAGE:
    return ECH_FAULT_LEVEL_WARNING;

  case ECH_FAULT_OVER_TEMP:
  case ECH_FAULT_OVER_CURRENT:
    return ECH_FAULT_LEVEL_ERROR;

  case ECH_FAULT_SENSOR_OPEN:
  case ECH_FAULT_SENSOR_SHORT:
  case ECH_FAULT_COMM_LOST:
    return ECH_FAULT_LEVEL_ERROR;

  case ECH_FAULT_WATCHDOG:
    return ECH_FAULT_LEVEL_CRITICAL;

  default:
    return ECH_FAULT_LEVEL_INFO;
  }
}

/**
 * @brief 记录故障到历史
 */
static void RecordFault(EchStateController_t *controller,
                        EchFaultType_t faultType, EchSystemState_t state,
                        float value, uint32_t timestamp_ms) {
  uint8_t index = controller->faultHistoryIndex % ECH_STATE_MAX_FAULT_HISTORY;

  controller->faultHistory[index].faultType = faultType;
  controller->faultHistory[index].faultLevel = GetFaultLevel(faultType);
  controller->faultHistory[index].timestamp_ms = timestamp_ms;
  controller->faultHistory[index].stateAtFault = state;
  controller->faultHistory[index].temperature = value;
  controller->faultHistory[index].isCleared = false;

  controller->faultHistoryIndex++;
  if (controller->faultCount < ECH_STATE_MAX_FAULT_HISTORY) {
    controller->faultCount++;
  }
}

/* ============================================================================
 * 单元测试桩代码 (UNIT_TEST 宏定义时编译)
 * ============================================================================
 */

#ifdef UNIT_TEST

#include <assert.h>
#include <stdio.h>

void test_state_init(void) {
  EchStateController_t state;
  EchStateConfig_t config = {.autoRecoverEnabled = true,
                             .recoverDelay_ms = 5000,
                             .overTempThreshold = 85.0f,
                             .overCurrentThreshold = 50.0f,
                             .lowVoltageThreshold = 9.0f,
                             .highVoltageThreshold = 16.0f};

  int32_t result = EchState_Init(&state, &config);
  assert(result == 0);
  assert(state.initialized == true);
  assert(state.currentState == ECH_STATE_INIT);
  assert(state.currentSubState == ECH_SUBSTATE_SELFTEST);

  printf("✓ test_state_init passed\n");
}

void test_state_transition(void) {
  EchStateController_t state;
  EchState_Init(&state, NULL);

  printf("  After init: state=%d, entryTime=%u\n", state.currentState,
         state.stateEntryTime_ms);

  /* 模拟时间推进，让 INIT 自检完成 */
  /* 第一次调用，timestamp=100，设置 entryTime=100，duration=0，不转换 */
  EchState_Update(&state, 100);
  printf("  After Update(100): state=%d, entryTime=%u, duration=%u\n",
         state.currentState, state.stateEntryTime_ms, state.stateDuration_ms);
  assert(state.currentState == ECH_STATE_INIT);

  /* 第二次调用，timestamp=200，duration=100，触发转换 */
  EchState_Update(&state, 200);
  printf("  After Update(200): state=%d, entryTime=%u, duration=%u\n",
         state.currentState, state.stateEntryTime_ms, state.stateDuration_ms);
  assert(state.currentState == ECH_STATE_STANDBY);
  printf("  INIT → STANDBY: OK\n");

  /* 请求转到 RUNNING (timestamp=200) */
  int32_t result = EchState_RequestTransition(&state, ECH_STATE_RUNNING, 200);
  assert(result == 0);
  assert(state.currentState == ECH_STATE_RUNNING);
  printf("  STANDBY → RUNNING: OK\n");

  /* 请求转到 STANDBY (timestamp=300) */
  result = EchState_RequestTransition(&state, ECH_STATE_STANDBY, 300);
  assert(result == 0);
  assert(state.currentState == ECH_STATE_STANDBY);
  printf("  RUNNING → STANDBY: OK\n");

  /* 非法转换：STANDBY → FAULT (应该失败) */
  result = EchState_RequestTransition(&state, ECH_STATE_FAULT, 400);
  assert(result == -2); /* 非法转换 */
  printf("  STANDBY → FAULT (illegal): OK\n");

  printf("✓ test_state_transition passed\n");
}

void test_state_fault_reporting(void) {
  EchStateController_t state;
  EchState_Init(&state, NULL);

  /* 先转到 RUNNING */
  EchState_Update(&state, 100); /* entryTime=100 */
  EchState_Update(&state, 200); /* duration=100, 转到 STANDBY */
  EchState_RequestTransition(&state, ECH_STATE_RUNNING, 300);
  assert(state.currentState == ECH_STATE_RUNNING);

  /* 报告过温故障 */
  int32_t result = EchState_ReportFault(&state, ECH_FAULT_OVER_TEMP, 90.0f);
  assert(result == 0);
  assert(state.currentState == ECH_STATE_FAULT);
  assert(state.activeFault == ECH_FAULT_OVER_TEMP);
  printf("  Over-temp fault reported: OK\n");

  /* 清除故障 */
  result = EchState_ClearFault(&state, ECH_FAULT_OVER_TEMP);
  assert(result == 0);
  assert(state.activeFault == ECH_FAULT_NONE);
  printf("  Fault cleared: OK\n");

  printf("✓ test_state_fault_reporting passed\n");
}

void test_state_fault_history(void) {
  EchStateController_t state;
  EchState_Init(&state, NULL);

  /* 报告多个故障 */
  EchState_ReportFault(&state, ECH_FAULT_OVER_TEMP, 90.0f);
  EchState_ClearFault(&state, ECH_FAULT_OVER_TEMP);
  EchState_ReportFault(&state, ECH_FAULT_OVER_CURRENT, 55.0f);

  uint8_t count = 0;
  const EchFaultRecord_t *history = EchState_GetFaultHistory(&state, &count);

  assert(count == 2);
  assert(history[0].faultType == ECH_FAULT_OVER_TEMP);
  assert(history[1].faultType == ECH_FAULT_OVER_CURRENT);
  assert(history[0].isCleared == true);
  assert(history[1].isCleared == false);

  printf("  Fault history count=%d: OK\n", count);
  printf("✓ test_state_fault_history passed\n");
}

void test_state_helper_functions(void) {
  EchStateController_t state;
  EchState_Init(&state, NULL);

  /* 测试 IsSystemReady */
  assert(EchState_IsSystemReady(&state) == false); /* INIT 状态 */

  EchState_Update(&state, 100); /* entryTime=100 */
  EchState_Update(&state, 200); /* duration=100, 转到 STANDBY */
  assert(EchState_IsSystemReady(&state) == true);

  /* 测试 IsRunning */
  assert(EchState_IsRunning(&state) == false);
  EchState_RequestTransition(&state, ECH_STATE_RUNNING, 200);
  assert(EchState_IsRunning(&state) == true);

  /* 测试 GetStateString */
  assert(strcmp(EchState_GetStateString(ECH_STATE_INIT), "INIT") == 0);
  assert(strcmp(EchState_GetStateString(ECH_STATE_RUNNING), "RUNNING") == 0);
  assert(strcmp(EchState_GetStateString(ECH_STATE_FAULT), "FAULT") == 0);

  printf("✓ test_state_helper_functions passed\n");
}

void test_state_substates(void) {
  EchStateController_t state;
  EchState_Init(&state, NULL);

  /* 检查 INIT 状态的子状态 */
  assert(state.currentSubState == ECH_SUBSTATE_SELFTEST);

  /* 转到 STANDBY */
  EchState_Update(&state, 100); /* entryTime=100, duration=0 */
  EchState_Update(&state, 200); /* duration=100, 自检完成 */
  assert(state.currentSubState == ECH_SUBSTATE_NONE);

  /* 转到 RUNNING */
  EchState_RequestTransition(&state, ECH_STATE_RUNNING, 200);
  assert(state.currentSubState == ECH_SUBSTATE_PREHEAT);

  /* 模拟预热完成 */
  EchState_Update(&state, 700); /* 500ms 后 */
  assert(state.currentSubState == ECH_SUBSTATE_NORMAL);

  printf("✓ test_state_substates passed\n");
}

int main(void) {
  printf("=== ECH State Machine Module Unit Tests ===\n\n");

  test_state_init();
  test_state_transition();
  test_state_fault_reporting();
  test_state_fault_history();
  test_state_helper_functions();
  test_state_substates();

  printf("\n=== All tests passed! ===\n");
  return 0;
}

#endif /* UNIT_TEST */
