/**
 * @file ech_diag.h
 * @brief ECH 水加热器 ECU - 诊断服务模块
 * 
 * @copyright Copyright (c) 2026 ECH ECU Development Team
 * @license MIT License
 * 
 * @version 0.1
 * @date 2026-03-04
 * 
 * @details
 * 本模块提供 UDS (Unified Diagnostic Services) 诊断服务功能。
 * 支持 DTC 故障码管理、诊断会话控制、ECU 信息读取等。
 * 
 * 过程域: SWE.3 (软件详细设计与单元实现)
 * 安全等级: ASIL B (根据 HARA 报告 SG-003)
 * 
 * 需求追溯:
 * - SWE-D-001: UDS 诊断服务支持
 * - SWE-D-002: DTC 故障码管理
 * - SWE-D-003: ECU 信息读取
 * - SWE-D-004: 诊断会话控制
 */

#ifndef ECH_DIAG_H
#define ECH_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 包含头文件
 * ============================================================================ */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 * 宏定义
 * ============================================================================ */

/** 诊断模块版本号 */
#define ECH_DIAG_VERSION_MAJOR    (0)
#define ECH_DIAG_VERSION_MINOR    (1)
#define ECH_DIAG_VERSION_PATCH    (0)

/** 最大 DTC 数量 */
#define ECH_DIAG_MAX_DTC_COUNT    (32)

/** 最大冻结帧数据长度 */
#define ECH_DIAG_FREEZE_FRAME_SIZE  (8)

/** 诊断会话类型 */
#define ECH_DIAG_SESSION_DEFAULT    (0x01)
#define ECH_DIAG_SESSION_PROGRAMMING (0x02)
#define ECH_DIAG_SESSION_EXTENDED   (0x03)

/** DTC 状态掩码 */
#define ECH_DTC_STATUS_PENDING      (0x01)
#define ECH_DTC_STATUS_CONFIRMED    (0x02)
#define ECH_DTC_STATUS_PERMANENT    (0x04)
#define ECH_DTC_STATUS_TEST_FAILED  (0x08)
#define ECH_DTC_STATUS_TEST_PASSED  (0x10)

/** UDS 服务 ID */
#define ECH_UDS_SID_READ_DID        (0x22)
#define ECH_UDS_SID_WRITE_DID       (0x2E)
#define ECH_UDS_SID_DIAG_CTRL       (0x10)
#define ECH_UDS_SID_READ_DTC        (0x19)
#define ECH_UDS_SID_CLEAR_DTC       (0x14)
#define ECH_UDS_SID_ROUTINE_CTRL    (0x31)

/* ============================================================================
 * 类型定义
 * ============================================================================ */

/**
 * @brief DTC 故障码类型枚举
 */
typedef enum {
    ECH_DTC_NONE = 0,
    ECH_DTC_P0001 = 1,    /**< 动力系统故障 */
    ECH_DTC_P0002 = 2,    /**< 传感器电路故障 */
    ECH_DTC_U0001 = 3,    /**< 通信总线故障 */
    ECH_DTC_B0001 = 4,    /**< 车身系统故障 */
    ECH_DTC_C0001 = 5,    /**< 底盘系统故障 */
    ECH_DTC_CUSTOM_START = 100  /**< 自定义 DTC 起始 */
} EchDtcCode_t;

/**
 * @brief DTC 严重等级枚举
 */
typedef enum {
    ECH_DTC_SEVERITY_INFO = 0,    /**< 信息级 */
    ECH_DTC_SEVERITY_WARNING = 1, /**< 警告级 */
    ECH_DTC_SEVERITY_ERROR = 2,   /**< 错误级 */
    ECH_DTC_SEVERITY_CRITICAL = 3 /**< 严重级 */
} EchDtcSeverity_t;

/**
 * @brief DTC 记录结构体
 */
typedef struct {
    EchDtcCode_t dtcCode;           /**< DTC 故障码 */
    uint8_t status;                 /**< DTC 状态 */
    EchDtcSeverity_t severity;      /**< 严重等级 */
    uint32_t occurrenceCount;       /**< 发生次数 */
    uint32_t firstOccurTime;        /**< 首次发生时间 */
    uint32_t lastOccurTime;         /**< 最后发生时间 */
    uint8_t freezeFrame[ECH_DIAG_FREEZE_FRAME_SIZE]; /**< 冻结帧数据 */
    bool hasFreezeFrame;            /**< 是否有冻结帧 */
} EchDtcRecord_t;

/**
 * @brief 诊断会话类型枚举
 */
typedef enum {
    ECH_DIAG_SESSION_DEFAULT_TYPE = 1,
    ECH_DIAG_SESSION_PROGRAMMING_TYPE = 2,
    ECH_DIAG_SESSION_EXTENDED_TYPE = 3
} EchDiagSessionType_t;

/**
 * @brief 诊断配置结构体
 */
typedef struct {
    uint32_t sessionTimeout_ms;     /**< 会话超时时间 */
    uint32_t p2ServerTime_ms;       /**< P2 服务器响应时间 */
    bool securityEnabled;           /**< 安全访问使能 */
} EchDiagConfig_t;

/**
 * @brief 诊断控制器结构体
 */
typedef struct {
    /* 配置 */
    EchDiagConfig_t config;         /**< 诊断配置 */
    
    /* 会话状态 */
    EchDiagSessionType_t currentSession; /**< 当前会话 */
    uint32_t sessionStartTime_ms;   /**< 会话开始时间 */
    bool sessionActive;             /**< 会话激活标志 */
    
    /* DTC 管理 */
    EchDtcRecord_t dtcRecords[ECH_DIAG_MAX_DTC_COUNT]; /**< DTC 记录数组 */
    uint8_t dtcCount;               /**< DTC 数量 */
    uint32_t totalDtcSetCount;      /**< DTC 设置总次数 */
    uint32_t totalDtcClearCount;    /**< DTC 清除总次数 */
    
    /* 诊断数据 */
    uint8_t lastRequestId;          /**< 最后请求 ID */
    uint8_t lastResponseId;         /**< 最后响应 ID */
    uint32_t requestCount;          /**< 请求计数 */
    uint32_t errorCount;            /**< 错误计数 */
    
    /* 初始化标志 */
    bool initialized;               /**< 初始化完成标志 */
} EchDiagController_t;

/**
 * @brief 诊断响应结构体
 */
typedef struct {
    uint8_t serviceId;              /**< 服务 ID */
    uint8_t subFunction;            /**< 子功能 */
    uint8_t data[255];              /**< 响应数据 */
    uint16_t dataLength;            /**< 数据长度 */
    bool isPositive;                /**< 是否肯定响应 */
    uint8_t negativeCode;           /**< 否定响应码 */
} EchDiagResponse_t;

/* ============================================================================
 * 函数声明
 * ============================================================================ */

/**
 * @brief 初始化诊断控制器
 */
int32_t EchDiag_Init(EchDiagController_t* controller, const EchDiagConfig_t* config);

/**
 * @brief 更新诊断状态 (周期性调用)
 */
int32_t EchDiag_Update(EchDiagController_t* controller, uint32_t timestamp_ms);

/**
 * @brief 处理 UDS 请求
 */
int32_t EchDiag_ProcessRequest(EchDiagController_t* controller, const uint8_t* requestData, uint16_t requestLength, EchDiagResponse_t* response, uint32_t timestamp_ms);

/**
 * @brief 设置 DTC 故障码
 */
int32_t EchDiag_SetDtc(EchDiagController_t* controller, EchDtcCode_t dtcCode, const uint8_t* freezeFrame);

/**
 * @brief 清除 DTC 故障码
 */
int32_t EchDiag_ClearDtc(EchDiagController_t* controller, EchDtcCode_t dtcCode);

/**
 * @brief 清除所有 DTC
 */
int32_t EchDiag_ClearAllDtc(EchDiagController_t* controller);

/**
 * @brief 获取 DTC 记录
 */
const EchDtcRecord_t* EchDiag_GetDtcRecord(const EchDiagController_t* controller, EchDtcCode_t dtcCode);

/**
 * @brief 获取所有 DTC 列表
 */
uint8_t EchDiag_GetAllDtcList(const EchDiagController_t* controller, EchDtcCode_t* dtcList, uint8_t maxCount);

/**
 * @brief 切换诊断会话
 */
int32_t EchDiag_ChangeSession(EchDiagController_t* controller, EchDiagSessionType_t sessionType, uint32_t timestamp_ms);

/**
 * @brief 获取当前会话类型
 */
EchDiagSessionType_t EchDiag_GetCurrentSession(const EchDiagController_t* controller);

/**
 * @brief 读取数据标识符 (DID)
 */
int32_t EchDiag_ReadDid(EchDiagController_t* controller, uint16_t didId, uint8_t* data, uint16_t* dataLength);

/**
 * @brief 获取诊断统计信息
 */
void EchDiag_GetDiagnostic(const EchDiagController_t* controller, uint32_t* requestCount, uint32_t* errorCount, uint8_t* dtcCount);

/**
 * @brief 获取模块版本号
 */
void EchDiag_GetVersion(uint8_t* major, uint8_t* minor, uint8_t* patch);

#ifdef __cplusplus
}
#endif

#endif /* ECH_DIAG_H */
