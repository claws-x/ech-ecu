/**
 * @file ech_state.h
 * @brief ECH 水加热器 ECU - 系统状态管理模块
 * 
 * @copyright Copyright (c) 2026 ECH ECU Development Team
 * @license MIT License
 * 
 * @version 0.1
 * @date 2026-03-04
 * 
 * @details
 * 本模块提供 ECH 水加热器 ECU 的系统状态机管理功能。
 * 负责系统状态转换、模式管理、故障处理和安全状态监控。
 * 
 * 过程域: SWE.3 (软件详细设计与单元实现)
 * 安全等级: ASIL B (根据 HARA 报告 SG-001/SG-003)
 * 
 * 需求追溯:
 * - SWE-F-003: 系统状态管理
 * - SWE-F-010: 故障安全处理
 * - SWE-S-001: 状态转换逻辑
 * - SWE-S-005: 看门狗喂狗策略
 */

#ifndef ECH_STATE_H
#define ECH_STATE_H

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

/** 状态管理模块版本号 */
#define ECH_STATE_VERSION_MAJOR    (0)
#define ECH_STATE_VERSION_MINOR    (1)
#define ECH_STATE_VERSION_PATCH    (0)

/** 状态转换超时时间 (ms) */
#define ECH_STATE_TRANSITION_TIMEOUT_MS    (5000)

/** 故障去抖时间 (ms) */
#define ECH_STATE_FAULT_DEBOUNCE_MS        (100)

/** 系统就绪检查周期 (ms) */
#define ECH_STATE_READY_CHECK_PERIOD_MS    (100)

/** 最大故障历史记录数 */
#define ECH_STATE_MAX_FAULT_HISTORY       (10)

/* ============================================================================
 * 类型定义
 * ============================================================================ */

/**
 * @brief 系统主状态枚举
 * 
 * @details
 * 定义 ECH 水加热器 ECU 的主要运行状态。
 * 状态转换必须符合安全要求，避免非法跳转。
 */
typedef enum {
    ECH_STATE_INIT = 0,           /**< 初始化状态 */
    ECH_STATE_STANDBY = 1,        /**< 待机状态 (系统就绪) */
    ECH_STATE_RUNNING = 2,        /**< 运行状态 (加热中) */
    ECH_STATE_FAULT = 3,          /**< 故障状态 */
    ECH_STATE_SHUTDOWN = 4,       /**< 关机状态 */
    ECH_STATE_INVALID = 0xFF      /**< 无效状态 */
} EchSystemState_t;

/**
 * @brief 系统子状态枚举
 */
typedef enum {
    ECH_SUBSTATE_NONE = 0,        /**< 无子状态 */
    ECH_SUBSTATE_SELFTEST = 1,    /**< 自检中 */
    ECH_SUBSTATE_PREHEAT = 2,     /**< 预热中 */
    ECH_SUBSTATE_NORMAL = 3,      /**< 正常运行 */
    ECH_SUBSTATE_COOLDOWN = 4,    /**< 冷却中 */
    ECH_SUBSTATE_RECOVER = 5      /**< 故障恢复中 */
} EchSystemSubState_t;

/**
 * @brief 故障类型枚举
 */
typedef enum {
    ECH_FAULT_NONE = 0,           /**< 无故障 */
    ECH_FAULT_OVER_TEMP = 1,      /**< 过温故障 */
    ECH_FAULT_OVER_CURRENT = 2,   /**< 过流故障 */
    ECH_FAULT_SENSOR_OPEN = 3,    /**< 传感器开路 */
    ECH_FAULT_SENSOR_SHORT = 4,   /**< 传感器短路 */
    ECH_FAULT_COMM_LOST = 5,      /**< 通信丢失 */
    ECH_FAULT_WATCHDOG = 6,       /**< 看门狗故障 */
    ECH_FAULT_LOW_VOLTAGE = 7,    /**< 低电压故障 */
    ECH_FAULT_HIGH_VOLTAGE = 8    /**< 高电压故障 */
} EchFaultType_t;

/**
 * @brief 故障严重等级枚举
 */
typedef enum {
    ECH_FAULT_LEVEL_INFO = 0,     /**< 信息级 (可自动恢复) */
    ECH_FAULT_LEVEL_WARNING = 1,  /**< 警告级 (需监控) */
    ECH_FAULT_LEVEL_ERROR = 2,    /**< 错误级 (需停机) */
    ECH_FAULT_LEVEL_CRITICAL = 3  /**< 严重级 (紧急停机) */
} EchFaultLevel_t;

/**
 * @brief 故障记录结构体
 */
typedef struct {
    EchFaultType_t faultType;     /**< 故障类型 */
    EchFaultLevel_t faultLevel;   /**< 故障等级 */
    uint32_t timestamp_ms;        /**< 故障发生时间戳 */
    EchSystemState_t stateAtFault; /**< 故障发生时系统状态 */
    float temperature;            /**< 故障时温度值 */
    bool isCleared;               /**< 故障是否已清除 */
} EchFaultRecord_t;

/**
 * @brief 状态机配置结构体
 */
typedef struct {
    bool autoRecoverEnabled;      /**< 自动恢复使能 */
    uint32_t recoverDelay_ms;     /**< 恢复延迟时间 */
    float overTempThreshold;      /**< 过温阈值 (°C) */
    float overCurrentThreshold;   /**< 过流阈值 (A) */
    float lowVoltageThreshold;    /**< 低电压阈值 (V) */
    float highVoltageThreshold;   /**< 高电压阈值 (V) */
} EchStateConfig_t;

/**
 * @brief 状态机控制器结构体
 */
typedef struct {
    /* 当前状态 */
    EchSystemState_t currentState;        /**< 当前主状态 */
    EchSystemSubState_t currentSubState;  /**< 当前子状态 */
    EchSystemState_t previousState;       /**< 上一状态 */
    
    /* 状态计时 */
    uint32_t stateEntryTime_ms;           /**< 状态进入时间 */
    uint32_t stateDuration_ms;            /**< 状态持续时间 */
    
    /* 故障管理 */
    EchFaultType_t activeFault;           /**< 当前活动故障 */
    EchFaultLevel_t activeFaultLevel;     /**< 当前故障等级 */
    uint32_t faultDebounceTimer_ms;       /**< 故障去抖计时器 */
    EchFaultRecord_t faultHistory[ECH_STATE_MAX_FAULT_HISTORY]; /**< 故障历史 */
    uint8_t faultHistoryIndex;            /**< 故障历史索引 */
    uint8_t faultCount;                   /**< 故障计数 */
    
    /* 配置 */
    EchStateConfig_t config;              /**< 状态机配置 */
    
    /* 运行统计 */
    uint32_t stateTransitionCount;        /**< 状态转换次数 */
    uint32_t totalRunTime_ms;             /**< 总运行时间 */
    uint32_t totalFaultCount;             /**< 总故障次数 */
    
    /* 初始化标志 */
    bool initialized;                     /**< 初始化完成标志 */
} EchStateController_t;

/**
 * @brief 状态转换事件结构体
 */
typedef struct {
    EchSystemState_t fromState;           /**< 源状态 */
    EchSystemState_t toState;             /**< 目标状态 */
    uint32_t timestamp_ms;                /**< 转换时间 */
    const char* reason;                   /**< 转换原因 */
} EchStateTransitionEvent_t;

/* ============================================================================
 * 函数声明
 * ============================================================================ */

/**
 * @brief 初始化状态机控制器
 */
int32_t EchState_Init(EchStateController_t* controller, const EchStateConfig_t* config);

/**
 * @brief 执行状态机更新
 */
EchSystemState_t EchState_Update(EchStateController_t* controller, uint32_t timestamp_ms);

/**
 * @brief 请求状态转换
 * 
 * @param controller 指向状态机控制器结构体的指针
 * @param targetState 目标状态
 * @param timestamp_ms 当前时间戳 (ms)
 * @return int32_t 0=成功，负值=错误码
 */
int32_t EchState_RequestTransition(EchStateController_t* controller, EchSystemState_t targetState, uint32_t timestamp_ms);

/**
 * @brief 报告故障
 */
int32_t EchState_ReportFault(EchStateController_t* controller, EchFaultType_t faultType, float currentValue);

/**
 * @brief 清除故障
 */
int32_t EchState_ClearFault(EchStateController_t* controller, EchFaultType_t faultType);

/**
 * @brief 获取当前状态
 */
EchSystemState_t EchState_GetCurrentState(const EchStateController_t* controller);

/**
 * @brief 获取当前子状态
 */
EchSystemSubState_t EchState_GetCurrentSubState(const EchStateController_t* controller);

/**
 * @brief 检查系统是否就绪
 */
bool EchState_IsSystemReady(const EchStateController_t* controller);

/**
 * @brief 检查系统是否运行中
 */
bool EchState_IsRunning(const EchStateController_t* controller);

/**
 * @brief 获取活动故障信息
 */
EchFaultType_t EchState_GetActiveFault(const EchStateController_t* controller);

/**
 * @brief 获取故障历史记录
 */
const EchFaultRecord_t* EchState_GetFaultHistory(const EchStateController_t* controller, uint8_t* count);

/**
 * @brief 获取状态字符串描述
 */
const char* EchState_GetStateString(EchSystemState_t state);

/**
 * @brief 重置状态机
 */
void EchState_Reset(EchStateController_t* controller);

/**
 * @brief 获取模块版本号
 */
void EchState_GetVersion(uint8_t* major, uint8_t* minor, uint8_t* patch);

#ifdef __cplusplus
}
#endif

#endif /* ECH_STATE_H */
