/**
 * @file ech_lin.c
 * @brief ECH 水加热器 ECU - LIN 驱动模块实现
 *
 * @copyright Copyright (c) 2026 ECH ECU Development Team
 * @license MIT License
 *
 * @version 0.1
 * @date 2026-03-04
 *
 * @details
 * 本文件实现 LIN (Local Interconnect Network) 总线驱动功能。
 * LIN 2.1 协议，主站模式，波特率 19.2kbps。
 *
 * 过程域: SWE.3 (软件详细设计与单元实现)
 * 安全等级: ASIL B (根据 HARA 报告 SG-003)
 *
 * 需求追溯:
 * - SWE-I-010: LIN 通信接口
 * - SWE-I-011: LIN 2.1 协议支持
 * - SWE-I-012: 19.2kbps 波特率
 * - SWE-I-019: LIN 主站功能
 * - SWE-I-020: LIN 帧调度
 * - SWE-I-021: LIN 休眠/唤醒
 *
 * 单元测试: 见本文件末尾 UNIT_TEST 宏定义部分
 */

/* ============================================================================
 * 包含头文件
 * ============================================================================
 */

#include "ech_lin.h"
#include <string.h>

/* ============================================================================
 * 局部宏定义
 * ============================================================================
 */

/** 默认配置 */
#define DEFAULT_LIN_BAUDRATE (19200)
#define DEFAULT_NODE_ID (1)

/** LIN 帧头同步场 */
#define LIN_SYNC_FIELD (0x55)

/** 增强型校验和掩码 (LIN 2.1) */
#define LIN_CHECKSUM_MASK (0xFF)

/* ============================================================================
 * 局部函数声明
 * ============================================================================
 */

/**
 * @brief 模拟硬件发送 (实际项目中替换为真实硬件操作)
 */
static int32_t HardwareSend(const uint8_t *data, uint8_t length);

/**
 * @brief 模拟硬件接收 (实际项目中替换为真实硬件操作)
 */
static int32_t HardwareReceive(uint8_t *data, uint8_t *length);

/**
 * @brief 构建 LIN 帧 (ID + 数据 + 校验和)
 */
static void BuildLinFrame(EchLinFrame_t *frame, uint8_t id, const uint8_t *data,
                          uint8_t length);

/**
 * @brief 验证 LIN 帧校验和
 */
static bool VerifyChecksum(const EchLinFrame_t *frame);

/**
 * @brief 处理心跳超时
 */
static void ProcessHeartbeatTimeout(EchLinController_t *controller,
                                    uint32_t timestamp_ms);

/* ============================================================================
 * 函数实现
 * ============================================================================
 */

/**
 * @brief 初始化 LIN 控制器
 */
int32_t EchLin_Init(EchLinController_t *controller,
                    const EchLinConfig_t *config) {
  /* 参数检查 */
  if (controller == NULL) {
    return -1;
  }

  /* 清零结构体 */
  memset(controller, 0, sizeof(EchLinController_t));

  /* 设置配置 */
  if (config != NULL) {
    controller->config = *config;
  } else {
    /* 使用默认配置 */
    controller->config.baudrate = DEFAULT_LIN_BAUDRATE;
    controller->config.nodeId = DEFAULT_NODE_ID;
    controller->config.isMaster = true;
    controller->config.scheduleTableId = 0;
  }

  /* 初始化状态 */
  controller->state = ECH_LIN_STATE_INIT;
  controller->lastActivity_ms = 0;
  controller->errorCount = 0;

  /* 发送/接收缓冲 */
  controller->txPending = false;
  controller->rxAvailable = false;

  /* 心跳管理 */
  controller->heartbeatCounter = 0;
  controller->lastHeartbeat_ms = 0;
  controller->heartbeatTimeout = false;

  /* 诊断 */
  controller->lastError = 0;
  controller->frameCount = 0;
  controller->errorFrameCount = 0;

  controller->initialized = true;

  /* 切换到活动状态（状态机转换） */
  controller->state = ECH_LIN_STATE_ACTIVE; /* 从 INIT 转换到 ACTIVE */

  return 0;
}

/**
 * @brief 执行 LIN 调度 (主站调用)
 */
int32_t EchLin_Schedule(EchLinController_t *controller, uint32_t timestamp_ms) {
  if (controller == NULL || !controller->initialized) {
    return -1;
  }

  if (controller->state != ECH_LIN_STATE_ACTIVE) {
    return -2; /* 状态不正确 */
  }

  /* 更新最后活动时间 */
  controller->lastActivity_ms = timestamp_ms;

  /* 检查心跳超时 */
  ProcessHeartbeatTimeout(controller, timestamp_ms);

  /* 主站调度逻辑 */
  if (controller->config.isMaster) {
    /* 定时发送心跳 (每 500ms) */
    if (timestamp_ms - controller->lastHeartbeat_ms >= 500) {
      EchLin_SendHeartbeat(controller, timestamp_ms);
    }
  }

  return 0;
}

/**
 * @brief 发送 LIN 帧
 */
int32_t EchLin_SendFrame(EchLinController_t *controller,
                         const EchLinFrame_t *frame) {
  if (controller == NULL || !controller->initialized || frame == NULL) {
    return -1;
  }

  if (controller->state != ECH_LIN_STATE_ACTIVE) {
    return -2;
  }

  /* 验证帧 ID 范围 (0-63) */
  if (frame->id > 63) {
    controller->lastError = ECH_LIN_STATUS_ERROR;
    return -3;
  }

  /* 构建完整的 LIN 帧 */
  EchLinFrame_t linFrame;
  BuildLinFrame(&linFrame, frame->id, frame->data, frame->length);

  /* 模拟硬件发送 */
  uint8_t txBuffer[12]; /* 同步场 (1) + ID 场 (2) + 数据场 (8) + 校验和 (1) */
  txBuffer[0] = LIN_SYNC_FIELD;
  txBuffer[1] = 0x80 | (frame->id & 0x3F); /* P 比特 = 1 */
  memcpy(&txBuffer[2], frame->data, frame->length);
  txBuffer[2 + frame->length] = linFrame.checksum;

  int32_t result = HardwareSend(txBuffer, 2 + frame->length + 1);

  if (result == 0) {
    controller->txFrame = *frame;
    controller->txPending = false;
    controller->frameCount++;
    /* lastActivity_ms 在 Schedule 中更新 */
  } else {
    controller->errorCount++;
    controller->errorFrameCount++;
    controller->lastError = ECH_LIN_STATUS_ERROR;
  }

  return result;
}

/**
 * @brief 接收 LIN 帧
 */
int32_t EchLin_ReceiveFrame(EchLinController_t *controller,
                            EchLinFrame_t *frame, uint32_t timestamp_ms) {
  if (controller == NULL || !controller->initialized || frame == NULL) {
    return -1;
  }

  if (!controller->rxAvailable) {
    return -2; /* 无可用数据 */
  }

  /* 复制接收帧 */
  *frame = controller->rxFrame;

  /* 验证校验和 */
  if (!VerifyChecksum(frame)) {
    controller->lastError = ECH_LIN_STATUS_CHECKSUM_ERROR;
    controller->errorCount++;
    controller->errorFrameCount++;
    controller->rxAvailable = false;
    return -3;
  }

  /* 清除接收标志 */
  controller->rxAvailable = false;
  controller->frameCount++;
  controller->lastActivity_ms = timestamp_ms;

  return 0;
}

/**
 * @brief 发送心跳帧
 */
int32_t EchLin_SendHeartbeat(EchLinController_t *controller,
                             uint32_t timestamp_ms) {
  if (controller == NULL || !controller->initialized) {
    return -1;
  }

  EchLinFrame_t heartbeat;
  heartbeat.id = ECH_LIN_FRAME_ID_HEARTBEAT;
  heartbeat.length = ECH_LIN_DATA_LENGTH_HEARTBEAT;
  heartbeat.frameType = ECH_LIN_FRAME_UNCONDITIONAL;
  heartbeat.timestamp_ms = timestamp_ms;

  /* 心跳数据：[计数器 LSB, 计数器 MSB] */
  heartbeat.data[0] = (uint8_t)(controller->heartbeatCounter & 0xFF);
  heartbeat.data[1] = (uint8_t)((controller->heartbeatCounter >> 8) & 0xFF);

  int32_t result = EchLin_SendFrame(controller, &heartbeat);

  if (result == 0) {
    controller->heartbeatCounter++;
    controller->lastHeartbeat_ms = timestamp_ms;
    controller->heartbeatTimeout = false;
  }

  return result;
}

/**
 * @brief 发送状态帧
 */
int32_t EchLin_SendStatus(EchLinController_t *controller,
                          const uint8_t *statusData, uint32_t timestamp_ms) {
  if (controller == NULL || !controller->initialized || statusData == NULL) {
    return -1;
  }

  EchLinFrame_t status;
  status.id = ECH_LIN_FRAME_ID_STATUS;
  status.length = ECH_LIN_DATA_LENGTH_STATUS;
  status.frameType = ECH_LIN_FRAME_UNCONDITIONAL;
  status.timestamp_ms = timestamp_ms;

  memcpy(status.data, statusData, ECH_LIN_DATA_LENGTH_STATUS);

  return EchLin_SendFrame(controller, &status);
}

/**
 * @brief 接收命令帧
 */
int32_t EchLin_ReceiveCommand(EchLinController_t *controller,
                              uint8_t *commandData, uint32_t timestamp_ms) {
  if (controller == NULL || !controller->initialized || commandData == NULL) {
    return -1;
  }

  EchLinFrame_t frame;
  int32_t result = EchLin_ReceiveFrame(controller, &frame, timestamp_ms);

  if (result == 0 && frame.id == ECH_LIN_FRAME_ID_COMMAND) {
    memcpy(commandData, frame.data, ECH_LIN_DATA_LENGTH_COMMAND);
    return 0;
  }

  return -2; /* 非命令帧 */
}

/**
 * @brief 进入休眠模式
 */
int32_t EchLin_EnterSleep(EchLinController_t *controller) {
  if (controller == NULL || !controller->initialized) {
    return -1;
  }

  /* 发送休眠命令 (总线休眠帧) */
  /* 实际实现中发送特定的休眠模式帧 */

  controller->state = ECH_LIN_STATE_SLEEP;
  controller->txPending = false;
  controller->rxAvailable = false;

  return 0;
}

/**
 * @brief 唤醒 LIN 总线
 */
int32_t EchLin_WakeUp(EchLinController_t *controller, uint32_t timestamp_ms) {
  if (controller == NULL || !controller->initialized) {
    return -1;
  }

  if (controller->state != ECH_LIN_STATE_SLEEP) {
    return 0; /* 已经在活动状态 */
  }

  /* 发送唤醒脉冲 (至少 150μs 显性电平) */
  /* 实际实现中控制硬件发送唤醒脉冲 */

  /* 模拟唤醒成功 */
  controller->state = ECH_LIN_STATE_ACTIVE;
  controller->lastActivity_ms = timestamp_ms;
  controller->heartbeatTimeout = false;

  return 0;
}

/**
 * @brief 获取 LIN 状态
 */
EchLinState_t EchLin_GetState(const EchLinController_t *controller) {
  if (controller == NULL || !controller->initialized) {
    return ECH_LIN_STATE_ERROR;
  }

  return controller->state;
}

/**
 * @brief 获取诊断信息
 */
void EchLin_GetDiagnostic(const EchLinController_t *controller,
                          EchLinDiagnostic_t *diagnostic) {
  if (controller == NULL || diagnostic == NULL || !controller->initialized) {
    return;
  }

  diagnostic->state = controller->state;
  diagnostic->errorCount = controller->errorCount;
  diagnostic->lastError = controller->lastError;
  diagnostic->frameCount = controller->frameCount;
  diagnostic->errorFrameCount = controller->errorFrameCount;
  diagnostic->isMaster = controller->config.isMaster;
  diagnostic->baudrate = controller->config.baudrate;
}

/**
 * @brief 清除错误状态
 */
void EchLin_ClearError(EchLinController_t *controller) {
  if (controller == NULL || !controller->initialized) {
    return;
  }

  controller->errorCount = 0;
  controller->lastError = 0;

  if (controller->state == ECH_LIN_STATE_ERROR) {
    controller->state = ECH_LIN_STATE_ACTIVE;
  }
}

/**
 * @brief 计算 LIN 校验和 (增强型)
 *
 * @details
 * LIN 2.1 增强型校验和:
 * - 对所有数据字节和受保护 ID 进行求和
 * - 取反码
 */
uint8_t EchLin_CalculateChecksum(const uint8_t *data, uint8_t length,
                                 uint8_t id) {
  uint16_t sum = 0;

  /* LIN 2.1: 校验和包括 ID */
  sum = id & 0x3F;

  /* 累加所有数据字节 */
  for (uint8_t i = 0; i < length; i++) {
    sum += data[i];
    if (sum > 0xFF) {
      sum -= 0xFF;
    }
  }

  /* 取反码 */
  return (uint8_t)(~sum & LIN_CHECKSUM_MASK);
}

/**
 * @brief 获取模块版本号
 */
void EchLin_GetVersion(uint8_t *major, uint8_t *minor, uint8_t *patch) {
  if (major != NULL)
    *major = ECH_LIN_VERSION_MAJOR;
  if (minor != NULL)
    *minor = ECH_LIN_VERSION_MINOR;
  if (patch != NULL)
    *patch = ECH_LIN_VERSION_PATCH;
}

/* ============================================================================
 * 局部函数实现
 * ============================================================================
 */

/**
 * @brief 模拟硬件发送
 * @return 0 表示成功，非 0 表示失败
 */
static int32_t HardwareSend(const uint8_t *data, uint8_t length) {
  /* 实际项目中替换为真实硬件发送 */
  /* 这里模拟成功发送（始终返回 0） */
  (void)data;
  (void)length;
  return 0; /* 模拟成功 */
}

/**
 * @brief 模拟硬件发送（带错误注入，用于测试）
 * @note 仅用于单元测试，生产环境禁用
 */
#ifdef LIN_TEST_MODE
static int32_t HardwareSendWithErrorInjection(const uint8_t *data,
                                              uint8_t length, float errorRate) {
  (void)data;
  (void)length;
  /* 根据错误率随机返回错误 */
  return (rand() % 100 < (int)(errorRate * 100)) ? -1 : 0;
}
#endif

/**
 * @brief 模拟硬件接收（预留接口）
 * @note 当前未使用，保留用于未来 LIN 从机模式实现
 */
__attribute__((unused)) static int32_t HardwareReceive(uint8_t *data,
                                                       uint8_t *length) {
  /* 实际项目中替换为真实硬件接收 */
  /* 这里模拟无数据 */
  (void)data;
  (void)length;
  return -1;
}

/**
 * @brief 构建 LIN 帧
 */
static void BuildLinFrame(EchLinFrame_t *frame, uint8_t id, const uint8_t *data,
                          uint8_t length) {
  frame->id = id & 0x3F;
  frame->length = length;
  memcpy(frame->data, data, length);
  frame->checksum = EchLin_CalculateChecksum(data, length, id);
}

/**
 * @brief 验证 LIN 帧校验和
 */
static bool VerifyChecksum(const EchLinFrame_t *frame) {
  if (frame == NULL) {
    return false;
  }

  uint8_t calculatedChecksum =
      EchLin_CalculateChecksum(frame->data, frame->length, frame->id);

  return (calculatedChecksum == frame->checksum);
}

/**
 * @brief 处理心跳超时
 */
static void ProcessHeartbeatTimeout(EchLinController_t *controller,
                                    uint32_t timestamp_ms) {
  /* 仅从站需要检测心跳超时 */
  if (!controller->config.isMaster) {
    if (controller->lastHeartbeat_ms > 0) {
      if (timestamp_ms - controller->lastHeartbeat_ms >
          ECH_LIN_TIMEOUT_HEARTBEAT) {
        controller->heartbeatTimeout = true;
        controller->lastError = ECH_LIN_STATUS_TIMEOUT;
      }
    }
  }
}

/* ============================================================================
 * 单元测试桩代码 (UNIT_TEST 宏定义时编译)
 * ============================================================================
 */

#ifdef UNIT_TEST

#include <assert.h>
#include <stdio.h>

void test_lin_init(void) {
  EchLinController_t lin;
  EchLinConfig_t config = {
      .baudrate = 19200, .nodeId = 1, .isMaster = true, .scheduleTableId = 0};

  int32_t result = EchLin_Init(&lin, &config);
  assert(result == 0);
  assert(lin.initialized == true);
  assert(lin.state == ECH_LIN_STATE_ACTIVE);
  assert(lin.config.isMaster == true);
  assert(lin.config.baudrate == 19200);

  printf("✓ test_lin_init passed\n");
}

void test_lin_checksum(void) {
  /* 测试校验和计算 */
  const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
  uint8_t checksum = EchLin_CalculateChecksum(data, 4, 0x01);

  /* 校验和应该是有效的 */
  assert(checksum != 0);
  printf("  Checksum for [01 02 03 04] ID=0x01: 0x%02X\n", checksum);

  /* 验证校验和 */
  EchLinFrame_t frame;
  frame.id = 0x01;
  frame.length = 4;
  memcpy(frame.data, data, 4);
  frame.checksum = checksum;

  assert(VerifyChecksum(&frame) == true);
  printf("  Checksum verification: OK\n");

  /* 破坏校验和，验证失败 */
  frame.checksum = checksum ^ 0xFF;
  assert(VerifyChecksum(&frame) == false);
  printf("  Bad checksum detection: OK\n");

  printf("✓ test_lin_checksum passed\n");
}

void test_lin_heartbeat(void) {
  EchLinController_t lin;
  EchLin_Init(&lin, NULL);

  /* 发送心跳 */
  int32_t result = EchLin_SendHeartbeat(&lin, 100);
  assert(result == 0);
  assert(lin.heartbeatCounter == 1);
  assert(lin.lastHeartbeat_ms == 100);
  printf("  Heartbeat sent, counter=%u\n", lin.heartbeatCounter);

  /* 再次发送 */
  EchLin_SendHeartbeat(&lin, 600);
  assert(lin.heartbeatCounter == 2);
  printf("  Second heartbeat, counter=%u\n", lin.heartbeatCounter);

  printf("✓ test_lin_heartbeat passed\n");
}

void test_lin_schedule(void) {
  EchLinController_t lin;
  EchLin_Init(&lin, NULL);

  /* 调度测试 */
  int32_t result = EchLin_Schedule(&lin, 100);
  assert(result == 0);

  /* 心跳应该自动发送 (500ms 间隔) */
  uint32_t heartbeatCount = lin.heartbeatCounter;

  EchLin_Schedule(&lin, 600); /* 500ms 后 */
  assert(lin.heartbeatCounter > heartbeatCount);
  printf("  Scheduled heartbeat sent\n");

  printf("✓ test_lin_schedule passed\n");
}

void test_lin_sleep_wakeup(void) {
  EchLinController_t lin;
  EchLin_Init(&lin, NULL);

  assert(lin.state == ECH_LIN_STATE_ACTIVE);

  /* 进入休眠 */
  int32_t result = EchLin_EnterSleep(&lin);
  assert(result == 0);
  assert(lin.state == ECH_LIN_STATE_SLEEP);
  printf("  Entered sleep mode\n");

  /* 唤醒 */
  result = EchLin_WakeUp(&lin, 1000);
  assert(result == 0);
  assert(lin.state == ECH_LIN_STATE_ACTIVE);
  printf("  Woke up from sleep\n");

  printf("✓ test_lin_sleep_wakeup passed\n");
}

void test_lin_diagnostic(void) {
  EchLinController_t lin;
  EchLin_Init(&lin, NULL);

  /* 发送一些帧 */
  EchLin_SendHeartbeat(&lin, 100);
  EchLin_SendHeartbeat(&lin, 600);

  /* 获取诊断信息 */
  EchLinDiagnostic_t diag;
  EchLin_GetDiagnostic(&lin, &diag);

  assert(diag.state == ECH_LIN_STATE_ACTIVE);
  assert(diag.isMaster == true);
  assert(diag.baudrate == 19200);
  assert(diag.frameCount == 2);
  assert(diag.errorCount == 0);

  printf("  Diagnostic: state=%d, frames=%u, errors=%u\n", diag.state,
         diag.frameCount, diag.errorCount);

  printf("✓ test_lin_diagnostic passed\n");
}

int main(void) {
  printf("=== ECH LIN Module Unit Tests ===\n\n");

  test_lin_init();
  test_lin_checksum();
  test_lin_heartbeat();
  test_lin_schedule();
  test_lin_sleep_wakeup();
  test_lin_diagnostic();

  printf("\n=== All tests passed! ===\n");
  return 0;
}

#endif /* UNIT_TEST */
