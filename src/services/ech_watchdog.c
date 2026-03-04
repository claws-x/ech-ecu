/**
 * @file ech_watchdog.c
 * @brief ECH 水加热器 ECU - 看门狗服务模块实现
 *
 * @copyright Copyright (c) 2026 ECH ECU Development Team
 * @license MIT License
 *
 * @version 0.1
 * @date 2026-03-04
 *
 * @details
 * 本文件实现看门狗 (Watchdog) 管理功能。
 *
 * 过程域: SWE.3 (软件详细设计与单元实现)
 * 安全等级: ASIL C (根据 HARA 报告 SG-001)
 *
 * 需求追溯:
 * - SWE-S-005: 看门狗喂狗策略
 * - SWE-S-006: 系统健康监控
 * - SWE-F-011: 故障安全恢复
 *
 * 单元测试: 见本文件末尾 UNIT_TEST 宏定义部分
 */

/* ============================================================================
 * 包含头文件
 * ============================================================================
 */

#include "ech_watchdog.h"
#include <string.h>

/* ============================================================================
 * 局部宏定义
 * ============================================================================
 */

/** 警告阈值 (超时前多少 ms 发出警告) */
#define WARNING_THRESHOLD_MS (200)

/** 最大任务数 */
#define MAX_TASKS (8)

/* ============================================================================
 * 局部函数声明
 * ============================================================================
 */

/**
 * @brief 查找任务
 */
static EchWdgTask_t *FindTask(EchWdgController_t *controller, uint8_t taskId);

/**
 * @brief 模拟硬件看门狗喂狗
 */
static void HardwareWdg_Feed(void);

/**
 * @brief 模拟硬件看门狗重置
 */
static void HardwareWdg_Reset(void);

/* ============================================================================
 * 函数实现
 * ============================================================================
 */

/**
 * @brief 初始化看门狗控制器
 */
int32_t EchWdg_Init(EchWdgController_t *controller,
                    const EchWdgConfig_t *config) {
  /* 参数检查 */
  if (controller == NULL) {
    return -1;
  }

  /* 清零结构体 */
  memset(controller, 0, sizeof(EchWdgController_t));

  /* 设置配置 */
  if (config != NULL) {
    controller->config = *config;
  } else {
    /* 使用默认配置 */
    controller->config.mode = ECH_WDG_MODE_INDEPENDENT;
    controller->config.timeout_ms = ECH_WDG_TIMEOUT_DEFAULT;
    controller->config.window_ms = ECH_WDG_WINDOW_EARLY;
    controller->config.autoResetEnabled = true;
    controller->config.maxResetCount = ECH_WDG_MAX_RESET_COUNT;
  }

  /* 验证配置 */
  if (controller->config.timeout_ms < ECH_WDG_TIMEOUT_MIN ||
      controller->config.timeout_ms > ECH_WDG_TIMEOUT_MAX) {
    return -2; /* 超时时间无效 */
  }

  /* 初始化状态 */
  controller->state = ECH_WDG_STATE_STOPPED;
  controller->initialized = true;
  controller->started = false;

  /* 任务管理 */
  controller->taskCount = 0;

  /* 统计 */
  controller->totalFeedCount = 0;
  controller->totalTimeoutCount = 0;
  controller->resetCount = 0;

  return 0;
}

/**
 * @brief 启动看门狗
 */
int32_t EchWdg_Start(EchWdgController_t *controller, uint32_t timestamp_ms) {
  if (controller == NULL || !controller->initialized) {
    return -1;
  }

  if (controller->config.mode == ECH_WDG_MODE_DISABLED) {
    return -2; /* 看门狗已禁用 */
  }

  controller->startTime_ms = timestamp_ms;
  controller->lastFeed_ms = timestamp_ms;
  controller->nextFeedDeadline_ms =
      timestamp_ms + controller->config.timeout_ms;
  controller->started = true;
  controller->state = ECH_WDG_STATE_RUNNING;

  /* 模拟硬件看门狗启动 */
  HardwareWdg_Feed();

  return 0;
}

/**
 * @brief 停止看门狗
 */
int32_t EchWdg_Stop(EchWdgController_t *controller) {
  if (controller == NULL || !controller->initialized) {
    return -1;
  }

  controller->started = false;
  controller->state = ECH_WDG_STATE_STOPPED;

  return 0;
}

/**
 * @brief 执行看门狗更新 (周期性调用)
 */
int32_t EchWdg_Update(EchWdgController_t *controller, uint32_t timestamp_ms) {
  if (controller == NULL || !controller->initialized || !controller->started) {
    return -1;
  }

  /* 计算剩余时间 */
  uint32_t timeToTimeout = controller->nextFeedDeadline_ms - timestamp_ms;

  /* 检查窗口模式 (过早喂狗) */
  if (controller->config.mode == ECH_WDG_MODE_WINDOW) {
    uint32_t timeSinceLastFeed = timestamp_ms - controller->lastFeed_ms;
    if (timeSinceLastFeed < controller->config.window_ms) {
      /* 过早喂狗，记录错误 */
      controller->state = ECH_WDG_STATE_ERROR;
      return -3;
    }
  }

  /* 检查警告状态 */
  if (timeToTimeout <= WARNING_THRESHOLD_MS && timeToTimeout > 0) {
    controller->state = ECH_WDG_STATE_WARNING;
    controller->lastWarningTime_ms = timestamp_ms;
  }

  /* 检查超时 */
  if (timeToTimeout == 0 || timeToTimeout > controller->config.timeout_ms) {
    /* 已超时 */
    controller->state = ECH_WDG_STATE_EXPIRED;
    controller->totalTimeoutCount++;
    controller->consecutiveMisses++;

    /* 检查所有任务是否有超时 */
    for (uint8_t i = 0; i < controller->taskCount; i++) {
      EchWdgTask_t *task = &controller->tasks[i];
      if (task->enabled) {
        uint32_t taskTimeToTimeout =
            (task->lastFeed_ms + task->timeout_ms) - timestamp_ms;
        if (taskTimeToTimeout == 0 || taskTimeToTimeout > task->timeout_ms) {
          task->missCount++;
        }
      }
    }

    /* 自动重置 */
    if (controller->config.autoResetEnabled &&
        controller->resetCount < controller->config.maxResetCount) {
      HardwareWdg_Reset();
      controller->resetCount++;
      /* 重置后恢复运行 */
      controller->state = ECH_WDG_STATE_RUNNING;
      controller->lastFeed_ms = timestamp_ms;
      controller->nextFeedDeadline_ms =
          timestamp_ms + controller->config.timeout_ms;
    }

    return -4; /* 超时错误 */
  }

  /* 正常状态 */
  if (controller->state == ECH_WDG_STATE_WARNING ||
      controller->state == ECH_WDG_STATE_EXPIRED) {
    controller->state = ECH_WDG_STATE_RUNNING;
  }

  return 0;
}

/**
 * @brief 喂狗 (主任务)
 */
int32_t EchWdg_Feed(EchWdgController_t *controller, uint32_t timestamp_ms) {
  if (controller == NULL || !controller->initialized || !controller->started) {
    return -1;
  }

  /* 窗口模式检查 */
  if (controller->config.mode == ECH_WDG_MODE_WINDOW) {
    uint32_t timeSinceLastFeed = timestamp_ms - controller->lastFeed_ms;
    if (timeSinceLastFeed < controller->config.window_ms) {
      return -3; /* 过早喂狗 */
    }
  }

  /* 更新喂狗时间 */
  controller->lastFeed_ms = timestamp_ms;
  controller->nextFeedDeadline_ms =
      timestamp_ms + controller->config.timeout_ms;
  controller->totalFeedCount++;
  controller->consecutiveMisses = 0;

  /* 模拟硬件喂狗 */
  HardwareWdg_Feed();

  return 0;
}

/**
 * @brief 注册看门狗任务
 */
int32_t EchWdg_RegisterTask(EchWdgController_t *controller, uint8_t taskId,
                            const char *name, uint32_t timeout_ms) {
  if (controller == NULL || !controller->initialized) {
    return -1;
  }

  if (controller->taskCount >= MAX_TASKS) {
    return -2; /* 任务数已满 */
  }

  /* 检查任务是否已存在 */
  if (FindTask(controller, taskId) != NULL) {
    return -3; /* 任务已存在 */
  }

  /* 添加新任务 */
  EchWdgTask_t *task = &controller->tasks[controller->taskCount];
  task->taskId = taskId;
  task->taskName = name;
  task->enabled = true;
  task->lastFeed_ms = 0;
  task->timeout_ms = timeout_ms;
  task->feedCount = 0;
  task->missCount = 0;

  controller->taskCount++;

  return 0;
}

/**
 * @brief 任务喂狗
 */
int32_t EchWdg_FeedTask(EchWdgController_t *controller, uint8_t taskId,
                        uint32_t timestamp_ms) {
  if (controller == NULL || !controller->initialized) {
    return -1;
  }

  EchWdgTask_t *task = FindTask(controller, taskId);
  if (task == NULL || !task->enabled) {
    return -2; /* 任务未找到或已禁用 */
  }

  task->lastFeed_ms = timestamp_ms;
  task->feedCount++;

  /* 同时喂主看门狗 */
  return EchWdg_Feed(controller, timestamp_ms);
}

/**
 * @brief 获取看门狗状态
 */
EchWdgState_t EchWdg_GetState(const EchWdgController_t *controller) {
  if (controller == NULL || !controller->initialized) {
    return ECH_WDG_STATE_ERROR;
  }

  return controller->state;
}

/**
 * @brief 获取剩余时间
 */
uint32_t EchWdg_GetTimeToTimeout(const EchWdgController_t *controller,
                                 uint32_t timestamp_ms) {
  if (controller == NULL || !controller->initialized || !controller->started) {
    return 0;
  }

  if (controller->nextFeedDeadline_ms > timestamp_ms) {
    return controller->nextFeedDeadline_ms - timestamp_ms;
  }

  return 0; /* 已超时 */
}

/**
 * @brief 获取诊断信息
 */
void EchWdg_GetDiagnostic(const EchWdgController_t *controller,
                          uint32_t timestamp_ms,
                          EchWdgDiagnostic_t *diagnostic) {
  if (controller == NULL || diagnostic == NULL || !controller->initialized) {
    return;
  }

  diagnostic->state = controller->state;
  diagnostic->mode = controller->config.mode;
  diagnostic->timeout_ms = controller->config.timeout_ms;
  diagnostic->timeToTimeout_ms =
      EchWdg_GetTimeToTimeout(controller, timestamp_ms);
  diagnostic->totalFeedCount = controller->totalFeedCount;
  diagnostic->totalTimeoutCount = controller->totalTimeoutCount;
  diagnostic->resetCount = controller->resetCount;
  diagnostic->isHealthy = EchWdg_IsHealthy(controller, timestamp_ms);
}

/**
 * @brief 重置看门狗
 */
int32_t EchWdg_Reset(EchWdgController_t *controller) {
  if (controller == NULL || !controller->initialized) {
    return -1;
  }

  controller->state = ECH_WDG_STATE_STOPPED;
  controller->started = false;
  controller->totalFeedCount = 0;
  controller->totalTimeoutCount = 0;
  controller->resetCount = 0;
  controller->consecutiveMisses = 0;

  /* 重置所有任务 */
  for (uint8_t i = 0; i < controller->taskCount; i++) {
    controller->tasks[i].lastFeed_ms = 0;
    controller->tasks[i].feedCount = 0;
    controller->tasks[i].missCount = 0;
  }

  return 0;
}

/**
 * @brief 检查系统健康状态
 */
bool EchWdg_IsHealthy(const EchWdgController_t *controller,
                      uint32_t timestamp_ms) {
  (void)timestamp_ms; /* 暂未使用，保留用于未来扩展 */

  if (controller == NULL || !controller->initialized) {
    return false;
  }

  /* 未启动时认为健康 */
  if (!controller->started) {
    return true;
  }

  /* 检查主看门狗状态 */
  if (controller->state == ECH_WDG_STATE_EXPIRED ||
      controller->state == ECH_WDG_STATE_ERROR) {
    return false;
  }

  /* 检查连续错过次数 */
  if (controller->consecutiveMisses >= 3) {
    return false;
  }

  /* 检查重置次数 */
  if (controller->resetCount >= controller->config.maxResetCount) {
    return false;
  }

  /* 检查所有任务 */
  for (uint8_t i = 0; i < controller->taskCount; i++) {
    const EchWdgTask_t *task = &controller->tasks[i];
    if (task->enabled && task->missCount >= 3) {
      return false;
    }
  }

  return true;
}

/**
 * @brief 获取模块版本号
 */
void EchWdg_GetVersion(uint8_t *major, uint8_t *minor, uint8_t *patch) {
  if (major != NULL)
    *major = ECH_WDG_VERSION_MAJOR;
  if (minor != NULL)
    *minor = ECH_WDG_VERSION_MINOR;
  if (patch != NULL)
    *patch = ECH_WDG_VERSION_PATCH;
}

/* ============================================================================
 * 局部函数实现
 * ============================================================================
 */

/**
 * @brief 查找任务
 */
static EchWdgTask_t *FindTask(EchWdgController_t *controller, uint8_t taskId) {
  for (uint8_t i = 0; i < controller->taskCount; i++) {
    if (controller->tasks[i].taskId == taskId) {
      return &controller->tasks[i];
    }
  }
  return NULL;
}

/**
 * @brief 模拟硬件看门狗喂狗
 */
static void HardwareWdg_Feed(void) {
  /* 实际项目中替换为真实硬件操作 */
  /* 例如：IWDG_Reload() */
}

/**
 * @brief 模拟硬件看门狗重置
 */
static void HardwareWdg_Reset(void) {
  /* 实际项目中替换为真实硬件操作 */
  /* 例如：NVIC_SystemReset() */
}

/* ============================================================================
 * 单元测试桩代码 (UNIT_TEST 宏定义时编译)
 * ============================================================================
 */

#ifdef UNIT_TEST

#include <assert.h>
#include <stdio.h>

void test_wdg_init(void) {
  EchWdgController_t wdg;
  EchWdgConfig_t config = {.mode = ECH_WDG_MODE_INDEPENDENT,
                           .timeout_ms = 1000,
                           .window_ms = 200,
                           .autoResetEnabled = true,
                           .maxResetCount = 3};

  int32_t result = EchWdg_Init(&wdg, &config);
  assert(result == 0);
  assert(wdg.initialized == true);
  assert(wdg.state == ECH_WDG_STATE_STOPPED);
  assert(wdg.config.timeout_ms == 1000);

  printf("✓ test_wdg_init passed\n");
}

void test_wdg_start_stop(void) {
  EchWdgController_t wdg;
  EchWdg_Init(&wdg, NULL);

  /* 启动 */
  int32_t result = EchWdg_Start(&wdg, 100);
  assert(result == 0);
  assert(wdg.started == true);
  assert(wdg.state == ECH_WDG_STATE_RUNNING);
  printf("  Watchdog started\n");

  /* 停止 */
  result = EchWdg_Stop(&wdg);
  assert(result == 0);
  assert(wdg.started == false);
  assert(wdg.state == ECH_WDG_STATE_STOPPED);
  printf("  Watchdog stopped\n");

  printf("✓ test_wdg_start_stop passed\n");
}

void test_wdg_feed(void) {
  EchWdgController_t wdg;
  EchWdg_Init(&wdg, NULL);
  EchWdg_Start(&wdg, 100);

  /* 喂狗 */
  int32_t result = EchWdg_Feed(&wdg, 500);
  assert(result == 0);
  assert(wdg.lastFeed_ms == 500);
  assert(wdg.totalFeedCount == 1);
  printf("  Fed at 500ms, total=%u\n", wdg.totalFeedCount);

  /* 再次喂狗 */
  EchWdg_Feed(&wdg, 1000);
  assert(wdg.totalFeedCount == 2);
  printf("  Fed at 1000ms, total=%u\n", wdg.totalFeedCount);

  printf("✓ test_wdg_feed passed\n");
}

void test_wdg_timeout(void) {
  EchWdgController_t wdg;
  EchWdgConfig_t config = {
      .mode = ECH_WDG_MODE_INDEPENDENT,
      .timeout_ms = 500,        /* 短超时用于测试 */
      .autoResetEnabled = false /* 禁用自动重置 */
  };

  EchWdg_Init(&wdg, &config);
  EchWdg_Start(&wdg, 100);

  /* 不喂狗，等待超时 */
  EchWdg_Update(&wdg, 700); /* 600ms 后，应超时 */
  assert(wdg.state == ECH_WDG_STATE_EXPIRED);
  assert(wdg.totalTimeoutCount == 1);
  printf("  Timeout detected at 700ms\n");

  printf("✓ test_wdg_timeout passed\n");
}

void test_wdg_task_management(void) {
  EchWdgController_t wdg;
  EchWdg_Init(&wdg, NULL);

  /* 注册任务 */
  int32_t result = EchWdg_RegisterTask(&wdg, 1, "MainLoop", 500);
  assert(result == 0);
  assert(wdg.taskCount == 1);
  printf("  Registered task: MainLoop\n");

  result = EchWdg_RegisterTask(&wdg, 2, "CommTask", 300);
  assert(result == 0);
  assert(wdg.taskCount == 2);
  printf("  Registered task: CommTask\n");

  /* 任务喂狗 */
  EchWdg_Start(&wdg, 100);
  result = EchWdg_FeedTask(&wdg, 1, 500);
  assert(result == 0);
  printf("  Fed task 1\n");

  printf("✓ test_wdg_task_management passed\n");
}

void test_wdg_diagnostic(void) {
  EchWdgController_t wdg;
  EchWdg_Init(&wdg, NULL);
  EchWdg_Start(&wdg, 100);

  /* 喂狗几次 */
  EchWdg_Feed(&wdg, 500);
  EchWdg_Feed(&wdg, 1000);

  /* 获取诊断信息 */
  EchWdgDiagnostic_t diag;
  EchWdg_GetDiagnostic(&wdg, 1000, &diag);

  assert(diag.state == ECH_WDG_STATE_RUNNING);
  assert(diag.totalFeedCount == 2);
  assert(diag.isHealthy == true);
  assert(diag.timeout_ms == 1000);

  printf("  Diagnostic: state=%d, feeds=%u, healthy=%d\n", diag.state,
         diag.totalFeedCount, diag.isHealthy);

  printf("✓ test_wdg_diagnostic passed\n");
}

int main(void) {
  printf("=== ECH Watchdog Module Unit Tests ===\n\n");

  test_wdg_init();
  test_wdg_start_stop();
  test_wdg_feed();
  test_wdg_timeout();
  test_wdg_task_management();
  test_wdg_diagnostic();

  printf("\n=== All tests passed! ===\n");
  return 0;
}

#endif /* UNIT_TEST */
