/**
 * @file ech_adc.h
 * @brief ECH 水加热器 ECU - ADC 驱动模块
 * 
 * @copyright Copyright (c) 2026 ECH ECU Development Team
 * @license MIT License
 * 
 * @version 0.1
 * @date 2026-03-04
 * 
 * @details
 * 本模块提供 ADC (模数转换器) 驱动功能，用于 ECH 水加热器的模拟信号采集。
 * 主要采集对象：温度传感器 (NTC/PTC)、母线电压、电机电流等。
 * 
 * 过程域: SWE.3 (软件详细设计与单元实现)
 * 安全等级: ASIL B (根据 HARA 报告 SG-001/SG-002)
 * 
 * 需求追溯:
 * - SWE-F-002: 温度信号采集
 * - SWE-F-009: 电压/电流监控
 * - SWE-S-004: 传感器断线检测
 * - SWE-P-004: 采样周期 10ms
 */

#ifndef ECH_ADC_H
#define ECH_ADC_H

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

/** ADC 模块版本号 */
#define ECH_ADC_VERSION_MAJOR    (0)
#define ECH_ADC_VERSION_MINOR    (1)
#define ECH_ADC_VERSION_PATCH    (0)

/** ADC 分辨率 (12-bit) */
#define ECH_ADC_RESOLUTION       (4095)
#define ECH_ADC_BITS             (12)

/** ADC 参考电压 (mV) - 典型值 3.3V */
#define ECH_ADC_REF_VOLTAGE_MV   (3300)

/** 通道定义 */
#define ECH_ADC_CHANNEL_TEMP_INLET    (0)   /**< 进水温度传感器 */
#define ECH_ADC_CHANNEL_TEMP_OUTLET   (1)   /**< 出水温度传感器 */
#define ECH_ADC_CHANNEL_TEMP_AMBIENT  (2)   /**< 环境温度传感器 */
#define ECH_ADC_CHANNEL_VBUS          (3)   /**< 母线电压 */
#define ECH_ADC_CHANNEL_CURRENT       (4)   /**< 电机电流 */
#define ECH_ADC_CHANNEL_COUNT         (5)   /**< 总通道数 */

/** 滑动平均滤波器窗口大小 */
#define ECH_ADC_FILTER_WINDOW_SIZE   (8)

/** 断线检测阈值 */
#define ECH_ADC_OPEN_CIRCUIT_THRESHOLD  (4090)  /**< 开路阈值 (接近满量程) */
#define ECH_ADC_SHORT_CIRCUIT_THRESHOLD (5)     /**< 短路阈值 (接近 0) */

/** 温度传感器参数 (NTC 10kΩ @ 25°C) */
#define ECH_NTC_NOMINAL_RESISTANCE   (10000.0f)  /**< 标称电阻 (Ω) */
#define ECH_NTC_NOMINAL_TEMP         (25.0f)     /**< 标称温度 (°C) */
#define ECH_NTC_B_COEFFICIENT        (3950.0f)   /**< B 常数 */
#define ECH_NTC_SERIES_RESISTANCE    (10000.0f)  /**< 串联电阻 (Ω) */

/** 有效数据标志 */
#define ECH_ADC_DATA_VALID           (0x5A)
#define ECH_ADC_DATA_INVALID         (0x00)

/* ============================================================================
 * 类型定义
 * ============================================================================ */

/**
 * @brief ADC 通道 ID 枚举
 */
typedef enum {
    ECH_ADC_CH_TEMP_INLET = 0,      /**< 进水温度 */
    ECH_ADC_CH_TEMP_OUTLET = 1,     /**< 出水温度 */
    ECH_ADC_CH_TEMP_AMBIENT = 2,    /**< 环境温度 */
    ECH_ADC_CH_VBUS = 3,            /**< 母线电压 */
    ECH_ADC_CH_CURRENT = 4,         /**< 电机电流 */
    ECH_ADC_CH_MAX                  /**< 通道数量 */
} EchAdcChannelId_t;

/**
 * @brief ADC 通道状态枚举
 */
typedef enum {
    ECH_ADC_STATUS_OK = 0,          /**< 正常 */
    ECH_ADC_STATUS_OPEN = 1,        /**< 开路/断线 */
    ECH_ADC_STATUS_SHORT = 2,       /**< 短路 */
    ECH_ADC_STATUS_OVER_RANGE = 3,  /**< 超量程 */
    ECH_ADC_STATUS_INVALID = 4      /**< 无效数据 */
} EchAdcStatus_t;

/**
 * @brief ADC 通道配置结构体
 */
typedef struct {
    EchAdcChannelId_t channelId;    /**< 通道 ID */
    bool enabled;                   /**< 通道使能 */
    bool filterEnabled;             /**< 滤波器使能 */
    uint8_t filterWindowSize;       /**< 滤波器窗口大小 */
    float calibrationOffset;        /**< 校准偏移 */
    float calibrationGain;          /**< 校准增益 */
} EchAdcChannelConfig_t;

/**
 * @brief ADC 原始数据结构体
 */
typedef struct {
    uint16_t rawValue;              /**< 原始 ADC 值 (0-4095) */
    uint32_t timestamp_ms;          /**< 采样时间戳 (ms) */
    EchAdcStatus_t status;          /**< 通道状态 */
    uint8_t validFlag;              /**< 有效标志 */
} EchAdcRawData_t;

/**
 * @brief ADC 工程数据结构体
 */
typedef struct {
    float value;                    /**< 工程单位值 */
    const char* unit;               /**< 单位字符串 */
    EchAdcStatus_t status;          /**< 通道状态 */
    uint32_t timestamp_ms;          /**< 采样时间戳 */
} EchAdcEngineeredData_t;

/**
 * @brief ADC 控制器状态结构体
 */
typedef struct {
    /* 配置 */
    EchAdcChannelConfig_t channels[ECH_ADC_CH_MAX]; /**< 通道配置 */
    
    /* 原始数据 */
    EchAdcRawData_t rawData[ECH_ADC_CH_MAX];        /**< 原始数据缓冲 */
    
    /* 滤波器状态 */
    uint16_t filterBuffer[ECH_ADC_CH_MAX][ECH_ADC_FILTER_WINDOW_SIZE]; /**< 滤波缓冲 */
    uint8_t filterIndex[ECH_ADC_CH_MAX];            /**< 滤波器索引 */
    uint8_t filterCount[ECH_ADC_CH_MAX];            /**< 滤波器计数 */
    
    /* 工程数据 */
    EchAdcEngineeredData_t engineeredData[ECH_ADC_CH_MAX]; /**< 工程数据 */
    
    /* 运行统计 */
    uint32_t sampleCount;           /**< 总采样次数 */
    uint32_t errorCount;            /**< 错误次数 */
    uint32_t lastUpdate_ms;         /**< 上次更新时间 */
    
    /* 初始化标志 */
    bool initialized;               /**< 初始化完成标志 */
} EchAdcController_t;

/* ============================================================================
 * 函数声明
 * ============================================================================ */

/**
 * @brief 初始化 ADC 控制器
 * 
 * @param controller 指向 ADC 控制器结构体的指针
 * @return int32_t 0=成功，负值=错误码
 */
int32_t EchAdc_Init(EchAdcController_t* controller);

/**
 * @brief 配置 ADC 通道
 * 
 * @param controller 指向 ADC 控制器结构体的指针
 * @param config 指向通道配置结构体的指针
 * @return int32_t 0=成功，负值=错误码
 */
int32_t EchAdc_ConfigureChannel(EchAdcController_t* controller, const EchAdcChannelConfig_t* config);

/**
 * @brief 执行 ADC 采样 (所有使能通道)
 * 
 * @param controller 指向 ADC 控制器结构体的指针
 * @param timestamp_ms 当前时间戳 (ms)
 * @return int32_t 0=成功，负值=错误码
 */
int32_t EchAdc_Sample(EchAdcController_t* controller, uint32_t timestamp_ms);

/**
 * @brief 获取原始 ADC 值
 * 
 * @param controller 指向 ADC 控制器结构体的指针
 * @param channelId 通道 ID
 * @return uint16_t 原始 ADC 值 (0-4095)
 */
uint16_t EchAdc_GetRawValue(const EchAdcController_t* controller, EchAdcChannelId_t channelId);

/**
 * @brief 获取工程单位数据
 * 
 * @param controller 指向 ADC 控制器结构体的指针
 * @param channelId 通道 ID
 * @return EchAdcEngineeredData_t 工程数据
 */
EchAdcEngineeredData_t EchAdc_GetEngineeredData(const EchAdcController_t* controller, EchAdcChannelId_t channelId);

/**
 * @brief 获取温度值 (°C)
 * 
 * @param controller 指向 ADC 控制器结构体的指针
 * @param channelId 温度通道 ID
 * @return float 温度值 (°C)，无效时返回 -999.0f
 */
float EchAdc_GetTemperature(const EchAdcController_t* controller, EchAdcChannelId_t channelId);

/**
 * @brief 获取电压值 (V)
 * 
 * @param controller 指向 ADC 控制器结构体的指针
 * @param channelId 电压通道 ID
 * @return float 电压值 (V)，无效时返回 -1.0f
 */
float EchAdc_GetVoltage(const EchAdcController_t* controller, EchAdcChannelId_t channelId);

/**
 * @brief 获取电流值 (A)
 * 
 * @param controller 指向 ADC 控制器结构体的指针
 * @param channelId 电流通道 ID
 * @return float 电流值 (A)，无效时返回 -1.0f
 */
float EchAdc_GetCurrent(const EchAdcController_t* controller, EchAdcChannelId_t channelId);

/**
 * @brief 检查通道状态
 * 
 * @param controller 指向 ADC 控制器结构体的指针
 * @param channelId 通道 ID
 * @return EchAdcStatus_t 通道状态
 */
EchAdcStatus_t EchAdc_GetChannelStatus(const EchAdcController_t* controller, EchAdcChannelId_t channelId);

/**
 * @brief 设置通道校准参数
 * 
 * @param controller 指向 ADC 控制器结构体的指针
 * @param channelId 通道 ID
 * @param offset 校准偏移
 * @param gain 校准增益
 * @return int32_t 0=成功，负值=错误码
 */
int32_t EchAdc_SetCalibration(EchAdcController_t* controller, EchAdcChannelId_t channelId, float offset, float gain);

/**
 * @brief 重置 ADC 控制器
 * 
 * @param controller 指向 ADC 控制器结构体的指针
 */
void EchAdc_Reset(EchAdcController_t* controller);

/**
 * @brief 获取 ADC 模块版本号
 */
void EchAdc_GetVersion(uint8_t* major, uint8_t* minor, uint8_t* patch);

#ifdef __cplusplus
}
#endif

#endif /* ECH_ADC_H */
