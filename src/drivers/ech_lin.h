/**
 * @file ech_lin.h
 * @brief ECH 水加热器 ECU - LIN 驱动模块
 *
 * @copyright Copyright (c) 2026 ECH ECU Development Team
 * @license MIT License
 *
 * @version 0.1
 * @date 2026-03-04
 *
 * @details
 * 本模块提供 LIN (Local Interconnect Network) 总线驱动功能，用于 ECH
 * 水加热器与整车网络的通信。 LIN 2.1 协议，主站模式，波特率 19.2kbps。
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
 */

#ifndef ECH_LIN_H
#define ECH_LIN_H

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

/** LIN 模块版本号 */
#define ECH_LIN_VERSION_MAJOR (0)
#define ECH_LIN_VERSION_MINOR (1)
#define ECH_LIN_VERSION_PATCH (0)

/** LIN 协议版本 */
#define ECH_LIN_PROTOCOL_VERSION (0x21) /**< LIN 2.1 */

/** LIN 波特率 */
#define ECH_LIN_BAUDRATE (19200)

/** LIN 帧 ID 定义 */
#define ECH_LIN_FRAME_ID_HEARTBEAT (0x01)  /**< 心跳帧 */
#define ECH_LIN_FRAME_ID_COMMAND (0x02)    /**< 控制命令帧 */
#define ECH_LIN_FRAME_ID_STATUS (0x03)     /**< 状态上报帧 */
#define ECH_LIN_FRAME_ID_CONFIG (0x04)     /**< 配置帧 */
#define ECH_LIN_FRAME_ID_DIAGNOSTIC (0x05) /**< 诊断帧 */

/** LIN 数据长度 */
#define ECH_LIN_DATA_LENGTH_HEARTBEAT (2)
#define ECH_LIN_DATA_LENGTH_COMMAND (8)
#define ECH_LIN_DATA_LENGTH_STATUS (8)
#define ECH_LIN_DATA_LENGTH_CONFIG (8)
#define ECH_LIN_DATA_LENGTH_DIAGNOSTIC (8)

/** LIN 超时时间 (ms) */
#define ECH_LIN_TIMEOUT_HEARTBEAT (1000) /**< 心跳超时 1 秒 */
#define ECH_LIN_TIMEOUT_RESPONSE (100)   /**< 响应超时 100ms */
#define ECH_LIN_TIMEOUT_WAKEUP (50)      /**< 唤醒超时 50ms */

/** LIN 状态掩码 */
#define ECH_LIN_STATUS_OK (0x00)
#define ECH_LIN_STATUS_ERROR (0x01)
#define ECH_LIN_STATUS_TIMEOUT (0x02)
#define ECH_LIN_STATUS_CHECKSUM_ERROR (0x04)
#define ECH_LIN_STATUS_SYNC_ERROR (0x08)

/** LIN 休眠/唤醒 */
#define ECH_LIN_WAKEUP_PATTERN_LENGTH (6) /**< 唤醒脉冲宽度 */

/* ============================================================================
 * 类型定义
 * ============================================================================
 */

/**
 * @brief LIN 总线状态枚举
 */
typedef enum {
  ECH_LIN_STATE_INIT = 0,   /**< 初始化状态 */
  ECH_LIN_STATE_ACTIVE = 1, /**< 活动状态 */
  ECH_LIN_STATE_SLEEP = 2,  /**< 休眠状态 */
  ECH_LIN_STATE_ERROR = 3   /**< 错误状态 */
} EchLinState_t;

/**
 * @brief LIN 帧类型枚举
 */
typedef enum {
  ECH_LIN_FRAME_UNCONDITIONAL = 0, /**< 无条件帧 */
  ECH_LIN_FRAME_EVENT = 1,         /**< 事件触发帧 */
  ECH_LIN_FRAME_SPORADIC = 2,      /**< 偶发帧 */
  ECH_LIN_FRAME_DIAGNOSTIC = 3     /**< 诊断帧 */
} EchLinFrameType_t;

/**
 * @brief LIN 帧结构体
 */
typedef struct {
  uint8_t id;                  /**< 帧 ID (0-63) */
  uint8_t data[8];             /**< 数据域 */
  uint8_t length;              /**< 数据长度 */
  uint8_t checksum;            /**< 校验和 */
  EchLinFrameType_t frameType; /**< 帧类型 */
  uint32_t timestamp_ms;       /**< 时间戳 */
} EchLinFrame_t;

/**
 * @brief LIN 配置结构体
 */
typedef struct {
  uint32_t baudrate;       /**< 波特率 */
  uint8_t nodeId;          /**< 节点 ID (主站=1) */
  bool isMaster;           /**< 是否主站 */
  uint8_t scheduleTableId; /**< 调度表 ID */
} EchLinConfig_t;

/**
 * @brief LIN 控制器状态结构体
 */
typedef struct {
  /* 配置 */
  EchLinConfig_t config; /**< LIN 配置 */

  /* 状态 */
  EchLinState_t state;      /**< 当前状态 */
  uint32_t lastActivity_ms; /**< 最后活动时间 */
  uint8_t errorCount;       /**< 错误计数 */

  /* 发送缓冲 */
  EchLinFrame_t txFrame; /**< 发送帧 */
  bool txPending;        /**< 发送等待标志 */

  /* 接收缓冲 */
  EchLinFrame_t rxFrame; /**< 接收帧 */
  bool rxAvailable;      /**< 接收可用标志 */

  /* 心跳管理 */
  uint32_t heartbeatCounter; /**< 心跳计数 */
  uint32_t lastHeartbeat_ms; /**< 最后心跳时间 */
  bool heartbeatTimeout;     /**< 心跳超时标志 */

  /* 诊断 */
  uint8_t lastError;        /**< 最后错误代码 */
  uint32_t frameCount;      /**< 帧计数 */
  uint32_t errorFrameCount; /**< 错误帧计数 */

  /* 初始化标志 */
  bool initialized; /**< 初始化完成标志 */
} EchLinController_t;

/**
 * @brief LIN 诊断信息结构体
 */
typedef struct {
  EchLinState_t state;      /**< 当前状态 */
  uint8_t errorCount;       /**< 错误计数 */
  uint8_t lastError;        /**< 最后错误 */
  uint32_t frameCount;      /**< 总帧数 */
  uint32_t errorFrameCount; /**< 错误帧数 */
  bool isMaster;            /**< 是否主站 */
  uint32_t baudrate;        /**< 波特率 */
} EchLinDiagnostic_t;

/* ============================================================================
 * 函数声明
 * ============================================================================
 */

/**
 * @brief 初始化 LIN 控制器
 */
int32_t EchLin_Init(EchLinController_t *controller,
                    const EchLinConfig_t *config);

/**
 * @brief 执行 LIN 调度 (主站调用)
 */
int32_t EchLin_Schedule(EchLinController_t *controller, uint32_t timestamp_ms);

/**
 * @brief 发送 LIN 帧
 */
int32_t EchLin_SendFrame(EchLinController_t *controller,
                         const EchLinFrame_t *frame);

/**
 * @brief 接收 LIN 帧
 */
int32_t EchLin_ReceiveFrame(EchLinController_t *controller,
                            EchLinFrame_t *frame, uint32_t timestamp_ms);

/**
 * @brief 发送心跳帧
 */
int32_t EchLin_SendHeartbeat(EchLinController_t *controller,
                             uint32_t timestamp_ms);

/**
 * @brief 发送状态帧
 */
int32_t EchLin_SendStatus(EchLinController_t *controller,
                          const uint8_t *statusData, uint32_t timestamp_ms);

/**
 * @brief 接收命令帧
 */
int32_t EchLin_ReceiveCommand(EchLinController_t *controller,
                              uint8_t *commandData, uint32_t timestamp_ms);

/**
 * @brief 进入休眠模式
 */
int32_t EchLin_EnterSleep(EchLinController_t *controller);

/**
 * @brief 唤醒 LIN 总线
 */
int32_t EchLin_WakeUp(EchLinController_t *controller, uint32_t timestamp_ms);

/**
 * @brief 获取 LIN 状态
 */
EchLinState_t EchLin_GetState(const EchLinController_t *controller);

/**
 * @brief 获取诊断信息
 */
void EchLin_GetDiagnostic(const EchLinController_t *controller,
                          EchLinDiagnostic_t *diagnostic);

/**
 * @brief 清除错误状态
 */
void EchLin_ClearError(EchLinController_t *controller);

/**
 * @brief 计算 LIN 校验和 (增强型)
 */
uint8_t EchLin_CalculateChecksum(const uint8_t *data, uint8_t length,
                                 uint8_t id);

/**
 * @brief 获取模块版本号
 */
void EchLin_GetVersion(uint8_t *major, uint8_t *minor, uint8_t *patch);

#ifdef __cplusplus
}
#endif

#endif /* ECH_LIN_H */
