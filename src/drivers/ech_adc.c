/**
 * @file ech_adc.c
 * @brief ECH 水加热器 ECU - ADC 驱动模块实现
 *
 * @copyright Copyright (c) 2026 ECH ECU Development Team
 * @license MIT License
 *
 * @version 0.1
 * @date 2026-03-04
 *
 * @details
 * 本文件实现 ADC (模数转换器) 驱动功能，用于 ECH 水加热器的模拟信号采集。
 *
 * 过程域: SWE.3 (软件详细设计与单元实现)
 * 安全等级: ASIL B (根据 HARA 报告 SG-001/SG-002)
 *
 * 需求追溯:
 * - SWE-F-002: 温度信号采集
 * - SWE-F-009: 电压/电流监控
 * - SWE-S-004: 传感器断线检测
 * - SWE-P-004: 采样周期 10ms
 *
 * 单元测试: 见本文件末尾 UNIT_TEST 宏定义部分
 */

/* ============================================================================
 * 包含头文件
 * ============================================================================
 */

#include "ech_adc.h"
#include <math.h>
#include <string.h>

/* ============================================================================
 * 局部宏定义
 * ============================================================================
 */

/** 最小采样周期 (ms) */
#define MIN_SAMPLE_PERIOD_MS (5)

/** 默认校准参数 */
#define DEFAULT_CALIBRATION_OFFSET (0.0f)
#define DEFAULT_CALIBRATION_GAIN (1.0f)

/* ============================================================================
 * 局部函数声明
 * ============================================================================
 */

/**
 * @brief 滑动平均滤波
 */
static uint16_t ApplyFilter(EchAdcController_t *controller,
                            EchAdcChannelId_t channelId, uint16_t newValue);

/**
 * @brief 检测通道故障 (开路/短路)
 */
static EchAdcStatus_t DetectFault(uint16_t rawValue);

/**
 * @brief ADC 原始值转电压 (mV)
 */
static float RawToVoltage(uint16_t rawValue);

/**
 * @brief ADC 原始值转温度 (°C) - NTC 热敏电阻
 */
static float RawToTemperature(uint16_t rawValue);

/**
 * @brief ADC 原始值转电流 (A)
 */
static float RawToCurrent(uint16_t rawValue);

/**
 * @brief 应用校准参数
 */
static float ApplyCalibration(float value, float offset, float gain);

/* ============================================================================
 * 函数实现
 * ============================================================================
 */

/**
 * @brief 初始化 ADC 控制器
 */
int32_t EchAdc_Init(EchAdcController_t *controller) {
  /* 参数检查 */
  if (controller == NULL) {
    return -1;
  }

  /* 清零结构体 */
  memset(controller, 0, sizeof(EchAdcController_t));

  /* 初始化所有通道配置 */
  for (int i = 0; i < ECH_ADC_CH_MAX; i++) {
    controller->channels[i].channelId = (EchAdcChannelId_t)i;
    controller->channels[i].enabled =
        (i < ECH_ADC_CHANNEL_COUNT) ? true : false;
    controller->channels[i].filterEnabled = true;
    controller->channels[i].filterWindowSize = ECH_ADC_FILTER_WINDOW_SIZE;
    controller->channels[i].calibrationOffset = DEFAULT_CALIBRATION_OFFSET;
    controller->channels[i].calibrationGain = DEFAULT_CALIBRATION_GAIN;

    /* 初始化状态 */
    controller->rawData[i].status = ECH_ADC_STATUS_INVALID;
    controller->rawData[i].validFlag = ECH_ADC_DATA_INVALID;

    controller->engineeredData[i].status = ECH_ADC_STATUS_INVALID;
  }

  /* 运行统计 */
  controller->sampleCount = 0;
  controller->errorCount = 0;
  controller->lastUpdate_ms = 0;

  controller->initialized = true;

  return 0;
}

/**
 * @brief 配置 ADC 通道
 */
int32_t EchAdc_ConfigureChannel(EchAdcController_t *controller,
                                const EchAdcChannelConfig_t *config) {
  if (controller == NULL || config == NULL) {
    return -1;
  }

  if (config->channelId >= ECH_ADC_CH_MAX) {
    return -2; /* 通道 ID 无效 */
  }

  controller->channels[config->channelId] = *config;

  return 0;
}

/**
 * @brief 执行 ADC 采样 (所有使能通道)
 */
int32_t EchAdc_Sample(EchAdcController_t *controller, uint32_t timestamp_ms) {
  if (controller == NULL || !controller->initialized) {
    return -1;
  }

  /* 检查采样周期 */
  if (controller->lastUpdate_ms > 0) {
    uint32_t delta_ms = timestamp_ms - controller->lastUpdate_ms;
    if (delta_ms < MIN_SAMPLE_PERIOD_MS) {
      return 0; /* 周期过短，跳过 */
    }
  }

  /* 遍历所有通道 */
  for (int i = 0; i < ECH_ADC_CH_MAX; i++) {
    if (!controller->channels[i].enabled) {
      continue;
    }

    EchAdcChannelId_t channelId = (EchAdcChannelId_t)i;

    /* Step 1: 模拟读取原始值 (实际硬件中替换为真实 ADC 读取) */
    uint16_t rawValue = controller->rawData[i].rawValue; /* 保持上次值 */

    /* Step 2: 故障检测 */
    EchAdcStatus_t status = DetectFault(rawValue);

    /* Step 3: 应用滤波 */
    if (controller->channels[i].filterEnabled && status == ECH_ADC_STATUS_OK) {
      rawValue = ApplyFilter(controller, channelId, rawValue);
    }

    /* Step 4: 更新原始数据 */
    controller->rawData[i].rawValue = rawValue;
    controller->rawData[i].timestamp_ms = timestamp_ms;
    controller->rawData[i].status = status;
    controller->rawData[i].validFlag = (status == ECH_ADC_STATUS_OK)
                                           ? ECH_ADC_DATA_VALID
                                           : ECH_ADC_DATA_INVALID;

    /* Step 5: 转换为工程单位 */
    float engineeredValue = 0.0f;
    const char *unit = "";

    switch (channelId) {
    case ECH_ADC_CH_TEMP_INLET:
    case ECH_ADC_CH_TEMP_OUTLET:
    case ECH_ADC_CH_TEMP_AMBIENT:
      engineeredValue = RawToTemperature(rawValue);
      unit = "°C";
      break;

    case ECH_ADC_CH_VBUS:
      engineeredValue = RawToVoltage(rawValue) / 1000.0f; /* mV → V */
      unit = "V";
      break;

    case ECH_ADC_CH_CURRENT:
      engineeredValue = RawToCurrent(rawValue);
      unit = "A";
      break;

    default:
      break;
    }

    /* Step 6: 应用校准 */
    if (status == ECH_ADC_STATUS_OK) {
      engineeredValue = ApplyCalibration(
          engineeredValue, controller->channels[i].calibrationOffset,
          controller->channels[i].calibrationGain);
    }

    /* Step 7: 更新工程数据 */
    controller->engineeredData[i].value = engineeredValue;
    controller->engineeredData[i].unit = unit;
    controller->engineeredData[i].status = status;
    controller->engineeredData[i].timestamp_ms = timestamp_ms;
  }

  /* 更新统计 */
  controller->sampleCount++;
  controller->lastUpdate_ms = timestamp_ms;

  return 0;
}

/**
 * @brief 获取原始 ADC 值
 */
uint16_t EchAdc_GetRawValue(const EchAdcController_t *controller,
                            EchAdcChannelId_t channelId) {
  if (controller == NULL || channelId >= ECH_ADC_CH_MAX) {
    return 0;
  }

  return controller->rawData[channelId].rawValue;
}

/**
 * @brief 获取工程单位数据
 */
EchAdcEngineeredData_t
EchAdc_GetEngineeredData(const EchAdcController_t *controller,
                         EchAdcChannelId_t channelId) {
  EchAdcEngineeredData_t invalid = {0.0f, "", ECH_ADC_STATUS_INVALID, 0};

  if (controller == NULL || channelId >= ECH_ADC_CH_MAX) {
    return invalid;
  }

  return controller->engineeredData[channelId];
}

/**
 * @brief 获取温度值 (°C)
 */
float EchAdc_GetTemperature(const EchAdcController_t *controller,
                            EchAdcChannelId_t channelId) {
  if (controller == NULL || channelId >= ECH_ADC_CH_MAX) {
    return -999.0f;
  }

  if (controller->engineeredData[channelId].status != ECH_ADC_STATUS_OK) {
    return -999.0f;
  }

  /* 仅温度通道有效 */
  if (channelId != ECH_ADC_CH_TEMP_INLET &&
      channelId != ECH_ADC_CH_TEMP_OUTLET &&
      channelId != ECH_ADC_CH_TEMP_AMBIENT) {
    return -999.0f;
  }

  return controller->engineeredData[channelId].value;
}

/**
 * @brief 获取电压值 (V)
 */
float EchAdc_GetVoltage(const EchAdcController_t *controller,
                        EchAdcChannelId_t channelId) {
  if (controller == NULL || channelId != ECH_ADC_CH_VBUS) {
    return -1.0f;
  }

  if (controller->engineeredData[channelId].status != ECH_ADC_STATUS_OK) {
    return -1.0f;
  }

  return controller->engineeredData[channelId].value;
}

/**
 * @brief 获取电流值 (A)
 */
float EchAdc_GetCurrent(const EchAdcController_t *controller,
                        EchAdcChannelId_t channelId) {
  if (controller == NULL || channelId != ECH_ADC_CH_CURRENT) {
    return -1.0f;
  }

  if (controller->engineeredData[channelId].status != ECH_ADC_STATUS_OK) {
    return -1.0f;
  }

  return controller->engineeredData[channelId].value;
}

/**
 * @brief 检查通道状态
 */
EchAdcStatus_t EchAdc_GetChannelStatus(const EchAdcController_t *controller,
                                       EchAdcChannelId_t channelId) {
  if (controller == NULL || channelId >= ECH_ADC_CH_MAX) {
    return ECH_ADC_STATUS_INVALID;
  }

  return controller->engineeredData[channelId].status;
}

/**
 * @brief 设置通道校准参数
 */
int32_t EchAdc_SetCalibration(EchAdcController_t *controller,
                              EchAdcChannelId_t channelId, float offset,
                              float gain) {
  if (controller == NULL || channelId >= ECH_ADC_CH_MAX) {
    return -1;
  }

  controller->channels[channelId].calibrationOffset = offset;
  controller->channels[channelId].calibrationGain = gain;

  return 0;
}

/**
 * @brief 重置 ADC 控制器
 */
void EchAdc_Reset(EchAdcController_t *controller) {
  if (controller == NULL) {
    return;
  }

  /* 保留配置，清除数据 */
  for (int i = 0; i < ECH_ADC_CH_MAX; i++) {
    controller->rawData[i].rawValue = 0;
    controller->rawData[i].status = ECH_ADC_STATUS_INVALID;
    controller->rawData[i].validFlag = ECH_ADC_DATA_INVALID;

    controller->engineeredData[i].value = 0.0f;
    controller->engineeredData[i].status = ECH_ADC_STATUS_INVALID;

    /* 清除滤波器 */
    memset(controller->filterBuffer[i], 0, sizeof(controller->filterBuffer[i]));
    controller->filterIndex[i] = 0;
    controller->filterCount[i] = 0;
  }

  controller->sampleCount = 0;
  controller->errorCount = 0;
  controller->lastUpdate_ms = 0;
}

/**
 * @brief 获取 ADC 模块版本号
 */
void EchAdc_GetVersion(uint8_t *major, uint8_t *minor, uint8_t *patch) {
  if (major != NULL)
    *major = ECH_ADC_VERSION_MAJOR;
  if (minor != NULL)
    *minor = ECH_ADC_VERSION_MINOR;
  if (patch != NULL)
    *patch = ECH_ADC_VERSION_PATCH;
}

/* ============================================================================
 * 局部函数实现
 * ============================================================================
 */

/**
 * @brief 滑动平均滤波
 */
static uint16_t ApplyFilter(EchAdcController_t *controller,
                            EchAdcChannelId_t channelId, uint16_t newValue) {
  uint8_t idx = controller->filterIndex[channelId];
  uint8_t *count = &controller->filterCount[channelId];
  uint16_t *buffer = controller->filterBuffer[channelId];
  uint8_t windowSize = controller->channels[channelId].filterWindowSize;

  /* 添加新值 */
  buffer[idx] = newValue;
  idx = (idx + 1) % windowSize;
  controller->filterIndex[channelId] = idx;

  /* 更新计数 */
  if (*count < windowSize) {
    (*count)++;
  }

  /* 计算平均值 */
  uint32_t sum = 0;
  for (int i = 0; i < *count; i++) {
    sum += buffer[i];
  }

  return (uint16_t)(sum / *count);
}

/**
 * @brief 检测通道故障 (开路/短路)
 */
static EchAdcStatus_t DetectFault(uint16_t rawValue) {
  if (rawValue >= ECH_ADC_OPEN_CIRCUIT_THRESHOLD) {
    return ECH_ADC_STATUS_OPEN;
  }

  if (rawValue <= ECH_ADC_SHORT_CIRCUIT_THRESHOLD) {
    return ECH_ADC_STATUS_SHORT;
  }

  return ECH_ADC_STATUS_OK;
}

/**
 * @brief ADC 原始值转电压 (mV)
 */
static float RawToVoltage(uint16_t rawValue) {
  return (float)rawValue * ECH_ADC_REF_VOLTAGE_MV / ECH_ADC_RESOLUTION;
}

/**
 * @brief ADC 原始值转温度 (°C) - NTC 热敏电阻
 *
 * @details
 * 使用 Steinhart-Hart 方程简化版 (B 参数方程):
 * 1/T = 1/T0 + (1/B) * ln(R/R0)
 *
 * 其中:
 * - T: 绝对温度 (K)
 * - T0: 标称温度 (298.15K = 25°C)
 * - B: B 常数 (3950)
 * - R: 当前电阻
 * - R0: 标称电阻 (10kΩ)
 */
static float RawToTemperature(uint16_t rawValue) {
  /* 计算 NTC 两端电压 */
  float vOut = (float)rawValue * ECH_ADC_REF_VOLTAGE_MV / ECH_ADC_RESOLUTION;

  /* 计算 NTC 电阻 (分压电路) */
  /* Vout = Vin * R_ntc / (R_ntc + R_series) */
  /* R_ntc = R_series * Vout / (Vin - Vout) */
  if (vOut >= ECH_ADC_REF_VOLTAGE_MV - 1.0f) {
    return -273.15f; /* 接近开路，返回绝对零度表示错误 */
  }

  float rNtc =
      ECH_NTC_SERIES_RESISTANCE * vOut / (ECH_ADC_REF_VOLTAGE_MV - vOut);

  /* 使用 B 参数方程计算温度 */
  float steinhart;
  steinhart = rNtc / ECH_NTC_NOMINAL_RESISTANCE;        /* (R/R0) */
  steinhart = logf(steinhart);                          /* ln(R/R0) */
  steinhart /= ECH_NTC_B_COEFFICIENT;                   /* (1/B) * ln(R/R0) */
  steinhart += 1.0f / (ECH_NTC_NOMINAL_TEMP + 273.15f); /* + 1/T0 */
  steinhart = 1.0f / steinhart;                         /* T = 1/(...) */

  return steinhart - 273.15f; /* 转换为摄氏度 */
}

/**
 * @brief ADC 原始值转电流 (A)
 *
 * @details
 * 假设电流传感器输出: 0-3.3V 对应 0-100A
 */
static float RawToCurrent(uint16_t rawValue) {
  float voltage = RawToVoltage(rawValue); /* mV */
  return voltage / 3300.0f * 100.0f;      /* 0-100A 量程 */
}

/**
 * @brief 应用校准参数
 */
static float ApplyCalibration(float value, float offset, float gain) {
  return (value * gain) + offset;
}

/* ============================================================================
 * 单元测试桩代码 (UNIT_TEST 宏定义时编译)
 * ============================================================================
 */

#ifdef UNIT_TEST

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* 模拟设置原始值用于测试 */
static void SetRawValue(EchAdcController_t *controller,
                        EchAdcChannelId_t channelId, uint16_t value) {
  controller->rawData[channelId].rawValue = value;
}

void test_adc_init(void) {
  EchAdcController_t adc;
  int32_t result = EchAdc_Init(&adc);

  assert(result == 0);
  assert(adc.initialized == true);
  assert(adc.sampleCount == 0);
  assert(adc.errorCount == 0);

  printf("✓ test_adc_init passed\n");
}

void test_adc_temperature_conversion(void) {
  EchAdcController_t adc;
  EchAdc_Init(&adc);

  /* 模拟 25°C 时的 ADC 值 (约 2048 = 1.65V) */
  SetRawValue(&adc, ECH_ADC_CH_TEMP_INLET, 2048);

  /* 执行采样 */
  EchAdc_Sample(&adc, 100);

  /* 获取温度 */
  float temp = EchAdc_GetTemperature(&adc, ECH_ADC_CH_TEMP_INLET);
  printf("  ADC=2048, Temperature=%.2f°C\n", temp);

  /* 25°C 附近应该合理 (允许一定误差) */
  assert(temp > 15.0f && temp < 35.0f);

  printf("✓ test_adc_temperature_conversion passed\n");
}

void test_adc_filter(void) {
  EchAdcController_t adc;
  EchAdc_Init(&adc);

  /* 连续采样多次，验证滤波效果 */
  SetRawValue(&adc, ECH_ADC_CH_TEMP_INLET, 2000);
  EchAdc_Sample(&adc, 100);

  SetRawValue(&adc, ECH_ADC_CH_TEMP_INLET, 2100);
  EchAdc_Sample(&adc, 200);

  SetRawValue(&adc, ECH_ADC_CH_TEMP_INLET, 2050);
  EchAdc_Sample(&adc, 300);

  uint16_t raw = EchAdc_GetRawValue(&adc, ECH_ADC_CH_TEMP_INLET);
  printf("  Filtered ADC value=%d (expected ~2050)\n", raw);

  /* 滤波后的值应该在输入值范围内 */
  assert(raw >= 2000 && raw <= 2100);

  printf("✓ test_adc_filter passed\n");
}

void test_adc_fault_detection(void) {
  EchAdcController_t adc;
  EchAdc_Init(&adc);

  /* 测试开路检测 */
  SetRawValue(&adc, ECH_ADC_CH_TEMP_INLET, 4095);
  EchAdc_Sample(&adc, 100);
  EchAdcStatus_t status = EchAdc_GetChannelStatus(&adc, ECH_ADC_CH_TEMP_INLET);
  assert(status == ECH_ADC_STATUS_OPEN);
  printf("  Open circuit detected: OK\n");

  /* 测试短路检测 */
  SetRawValue(&adc, ECH_ADC_CH_TEMP_INLET, 0);
  EchAdc_Sample(&adc, 200);
  status = EchAdc_GetChannelStatus(&adc, ECH_ADC_CH_TEMP_INLET);
  assert(status == ECH_ADC_STATUS_SHORT);
  printf("  Short circuit detected: OK\n");

  /* 测试正常状态 */
  SetRawValue(&adc, ECH_ADC_CH_TEMP_INLET, 2048);
  EchAdc_Sample(&adc, 300);
  status = EchAdc_GetChannelStatus(&adc, ECH_ADC_CH_TEMP_INLET);
  assert(status == ECH_ADC_STATUS_OK);
  printf("  Normal status: OK\n");

  printf("✓ test_adc_fault_detection passed\n");
}

void test_adc_calibration(void) {
  EchAdcController_t adc;
  EchAdc_Init(&adc);

  /* 设置校准参数：偏移 +2°C，增益 1.0 */
  EchAdc_SetCalibration(&adc, ECH_ADC_CH_TEMP_INLET, 2.0f, 1.0f);

  SetRawValue(&adc, ECH_ADC_CH_TEMP_INLET, 2048);
  EchAdc_Sample(&adc, 100);

  float temp_calibrated = EchAdc_GetTemperature(&adc, ECH_ADC_CH_TEMP_INLET);

  /* 重置校准 */
  EchAdc_SetCalibration(&adc, ECH_ADC_CH_TEMP_INLET, 0.0f, 1.0f);
  EchAdc_Sample(&adc, 200);
  float temp_raw = EchAdc_GetTemperature(&adc, ECH_ADC_CH_TEMP_INLET);

  printf("  Raw temp=%.2f°C, Calibrated temp=%.2f°C (offset=+2°C)\n", temp_raw,
         temp_calibrated);

  /* 校准后的温度应该比原始值高约 2°C */
  assert((temp_calibrated - temp_raw) > 1.5f &&
         (temp_calibrated - temp_raw) < 2.5f);

  printf("✓ test_adc_calibration passed\n");
}

void test_adc_voltage_current(void) {
  EchAdcController_t adc;
  EchAdc_Init(&adc);

  /* 测试电压测量 (假设 12V 系统，分压后约 1.65V = 2048 ADC) */
  SetRawValue(&adc, ECH_ADC_CH_VBUS, 2048);
  EchAdc_Sample(&adc, 100);
  float voltage = EchAdc_GetVoltage(&adc, ECH_ADC_CH_VBUS);
  printf("  VBUS ADC=2048, Voltage=%.2fV\n", voltage);
  assert(voltage > 1.0f && voltage < 2.0f);

  /* 测试电流测量 (假设 50A = 1.65V = 2048 ADC) */
  SetRawValue(&adc, ECH_ADC_CH_CURRENT, 2048);
  EchAdc_Sample(&adc, 200);
  float current = EchAdc_GetCurrent(&adc, ECH_ADC_CH_CURRENT);
  printf("  Current ADC=2048, Current=%.2fA\n", current);
  assert(current > 40.0f && current < 60.0f);

  printf("✓ test_adc_voltage_current passed\n");
}

int main(void) {
  printf("=== ECH ADC Module Unit Tests ===\n\n");

  test_adc_init();
  test_adc_temperature_conversion();
  test_adc_filter();
  test_adc_fault_detection();
  test_adc_calibration();
  test_adc_voltage_current();

  printf("\n=== All tests passed! ===\n");
  return 0;
}

#endif /* UNIT_TEST */
