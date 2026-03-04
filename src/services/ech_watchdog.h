/**
 * @file ech_watchdog.h
 * @brief ECH 水加热器 ECU - 看门狗服务模块
 * 
 * @copyright Copyright (c) 2026 ECH ECU Development Team
 * @license MIT License
 * 
 * @version 0.1
 * @date 2026-03-04
 * 
 * @details
 * 本模块提供看门狗 (Watchdog) 管理功能，用于 ECH 水加热器 ECU 的系统健康监控。
 * 支持独立看门狗和窗口看门狗模式。
 * 
 * 过程域: SWE.3 (软件详细设计与单元实现)
 * 安全等级: ASIL C (根据 HARA 报告 SG-001)
 * 
 * 需求追溯:
 * - SWE-S-005: 看门狗喂狗策略
 * - SWE-S-006: 系统健康监控
 * - SWE-F-011: 故障安全恢复
 */

#ifndef ECH_WATCHDOG_H
#define ECH_WATCHDOG_H

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

/** 看门狗模块版本号 */
#define ECH_WDG_VERSION_MAJOR    (0)
#define ECH_WDG_VERSION_MINOR    (1)
#define ECH_WDG_VERSION_PATCH    (0)

/** 看门狗超时时间 (ms) */
#define ECH_WDG_TIMEOUT_DEFAULT      (1000)  /**< 默认超时 1 秒 */
#define ECH_WDG_TIMEOUT_MIN          (100)   /**< 最小超时 100ms */
#define ECH_WDG_TIMEOUT_MAX          (5000)  /**< 最大超时 5 秒 */

/** 窗口看门狗时间窗口 */
#define ECH_WDG_WINDOW_EARLY         (200)   /**< 最早喂狗时间 (提前量) */

/** 喂狗容差 (ms) */
#define ECH_WDG_FEED_TOLERANCE       (50)

/** 最大重置次数 */
#define ECH_WDG_MAX_RESET_COUNT      (3)

/* ============================================================================
 * 类型定义
 * ============================================================================ */

/**
 * @brief 看门狗模式枚举
 */
typedef enum {
    ECH_WDG_MODE_INDEPENDENT = 0,   /**< 独立看门狗 */
    ECH_WDG_MODE_WINDOW = 1,        /**< 窗口看门狗 */
    ECH_WDG_MODE_DISABLED = 2       /**< 禁用 */
} EchWdgMode_t;

/**
 * @brief 看门狗状态枚举
 */
typedef enum {
    ECH_WDG_STATE_STOPPED = 0,      /**< 停止 */
    ECH_WDG_STATE_RUNNING = 1,      /**< 运行中 */
    ECH_WDG_STATE_WARNING = 2,      /**< 警告 (接近超时) */
    ECH_WDG_STATE_EXPIRED = 3,      /**< 已超时 */
    ECH_WDG_STATE_ERROR = 4         /**< 错误 */
} EchWdgState_t;

/**
 * @brief 看门狗任务结构体
 */
typedef struct {
    uint8_t taskId;                 /**< 任务 ID */
    const char* taskName;           /**< 任务名称 */
    bool enabled;                   /**< 任务使能 */
    uint32_t lastFeed_ms;           /**< 最后喂狗时间 */
    uint32_t timeout_ms;            /**< 超时时间 */
    uint32_t feedCount;             /**< 喂狗次数 */
    uint32_t missCount;             /**< 错过喂狗次数 */
} EchWdgTask_t;

/**
 * @brief 看门狗配置结构体
 */
typedef struct {
    EchWdgMode_t mode;              /**< 看门狗模式 */
    uint32_t timeout_ms;            /**< 超时时间 */
    uint32_t window_ms;             /**< 窗口时间 (窗口模式) */
    bool autoResetEnabled;          /**< 自动重置使能 */
    uint8_t maxResetCount;          /**< 最大重置次数 */
} EchWdgConfig_t;

/**
 * @brief 看门狗控制器结构体
 */
typedef struct {
    /* 配置 */
    EchWdgConfig_t config;          /**< 看门狗配置 */
    
    /* 状态 */
    EchWdgState_t state;            /**< 当前状态 */
    bool initialized;               /**< 初始化标志 */
    bool started;                   /**< 已启动 */
    
    /* 计时 */
    uint32_t startTime_ms;          /**< 启动时间 */
    uint32_t lastFeed_ms;           /**< 最后喂狗时间 */
    uint32_t nextFeedDeadline_ms;   /**< 下次喂狗截止时间 */
    
    /* 任务管理 (多任务看门狗) */
    EchWdgTask_t tasks[8];          /**< 任务数组 (最多 8 个) */
    uint8_t taskCount;              /**< 任务数量 */
    
    /* 统计 */
    uint32_t totalFeedCount;        /**< 总喂狗次数 */
    uint32_t totalTimeoutCount;     /**< 总超时次数 */
    uint32_t resetCount;            /**< 重置次数 */
    
    /* 诊断 */
    uint32_t lastWarningTime_ms;    /**< 最后警告时间 */
    uint8_t consecutiveMisses;      /**< 连续错过次数 */
} EchWdgController_t;

/**
 * @brief 看门狗诊断信息结构体
 */
typedef struct {
    EchWdgState_t state;            /**< 当前状态 */
    EchWdgMode_t mode;              /**< 当前模式 */
    uint32_t timeout_ms;            /**< 超时时间 */
    uint32_t timeToTimeout_ms;      /**< 距离超时时间 */
    uint32_t totalFeedCount;        /**< 总喂狗次数 */
    uint32_t totalTimeoutCount;     /**< 总超时次数 */
    uint8_t resetCount;             /**< 重置次数 */
    bool isHealthy;                 /**< 健康状态 */
} EchWdgDiagnostic_t;

/* ============================================================================
 * 函数声明
 * ============================================================================ */

/**
 * @brief 初始化看门狗控制器
 */
int32_t EchWdg_Init(EchWdgController_t* controller, const EchWdgConfig_t* config);

/**
 * @brief 启动看门狗
 */
int32_t EchWdg_Start(EchWdgController_t* controller, uint32_t timestamp_ms);

/**
 * @brief 停止看门狗
 */
int32_t EchWdg_Stop(EchWdgController_t* controller);

/**
 * @brief 执行看门狗更新 (周期性调用)
 */
int32_t EchWdg_Update(EchWdgController_t* controller, uint32_t timestamp_ms);

/**
 * @brief 喂狗 (主任务)
 */
int32_t EchWdg_Feed(EchWdgController_t* controller, uint32_t timestamp_ms);

/**
 * @brief 注册看门狗任务
 */
int32_t EchWdg_RegisterTask(EchWdgController_t* controller, uint8_t taskId, const char* name, uint32_t timeout_ms);

/**
 * @brief 任务喂狗
 */
int32_t EchWdg_FeedTask(EchWdgController_t* controller, uint8_t taskId, uint32_t timestamp_ms);

/**
 * @brief 获取看门狗状态
 */
EchWdgState_t EchWdg_GetState(const EchWdgController_t* controller);

/**
 * @brief 获取剩余时间
 */
uint32_t EchWdg_GetTimeToTimeout(const EchWdgController_t* controller, uint32_t timestamp_ms);

/**
 * @brief 获取诊断信息
 */
void EchWdg_GetDiagnostic(const EchWdgController_t* controller, uint32_t timestamp_ms, EchWdgDiagnostic_t* diagnostic);

/**
 * @brief 重置看门狗
 */
int32_t EchWdg_Reset(EchWdgController_t* controller);

/**
 * @brief 检查系统健康状态
 */
bool EchWdg_IsHealthy(const EchWdgController_t* controller, uint32_t timestamp_ms);

/**
 * @brief 获取模块版本号
 */
void EchWdg_GetVersion(uint8_t* major, uint8_t* minor, uint8_t* patch);

#ifdef __cplusplus
}
#endif

#endif /* ECH_WATCHDOG_H */
