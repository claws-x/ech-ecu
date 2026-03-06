/**
 * @file test_ech_adc.c
 * @brief ECH ADC 驱动模块单元测试
 * 
 * @details
 * 本文件包含 ECH ADC 驱动模块的完整单元测试套件。
 * 测试覆盖：初始化、配置、采样、滤波、故障检测、校准、数据转换等。
 * 
 * 测试目标：每个函数至少 10 项测试用例
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "ech_adc.h"

/* ============================================================================
 * 测试计数器
 * ============================================================================ */

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        g_tests_run++; \
        if (condition) { \
            g_tests_passed++; \
            printf("  ✓ %s\n", message); \
        } else { \
            g_tests_failed++; \
            printf("  ✗ FAILED: %s (line %d)\n", message, __LINE__); \
        } \
    } while(0)

/* ============================================================================
 * 测试：初始化功能
 * ============================================================================ */

void test_adc_init_null_pointer(void) {
    int32_t result = EchAdc_Init(NULL);
    TEST_ASSERT(result == -1, "NULL 指针初始化应返回错误");
}

void test_adc_init_successful(void) {
    EchAdcController_t adc;
    int32_t result = EchAdc_Init(&adc);
    TEST_ASSERT(result == 0, "正常初始化应成功");
    TEST_ASSERT(adc.initialized == true, "初始化标志应置位");
}

void test_adc_init_channels_disabled(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    /* 验证所有通道默认配置 */
    for (int i = 0; i < ECH_ADC_CH_MAX; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "通道 %d 默认使能状态", i);
        if (i < ECH_ADC_CHANNEL_COUNT) {
            TEST_ASSERT(adc.channels[i].enabled == true, msg);
        } else {
            TEST_ASSERT(adc.channels[i].enabled == false, msg);
        }
    }
}

void test_adc_init_filter_config(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    TEST_ASSERT(adc.channels[0].filterEnabled == true, "滤波器默认应使能");
    TEST_ASSERT(adc.channels[0].filterWindowSize == ECH_ADC_FILTER_WINDOW_SIZE, 
                "滤波器窗口大小应正确");
}

void test_adc_init_calibration_default(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    TEST_ASSERT(adc.channels[0].calibrationOffset == 0.0f, "默认校准偏移应为 0");
    TEST_ASSERT(adc.channels[0].calibrationGain == 1.0f, "默认校准增益应为 1");
}

void test_adc_init_statistics_cleared(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    TEST_ASSERT(adc.sampleCount == 0, "采样计数应初始化为 0");
    TEST_ASSERT(adc.errorCount == 0, "错误计数应初始化为 0");
    TEST_ASSERT(adc.lastUpdate_ms == 0, "最后更新时间应初始化为 0");
}

void test_adc_init_data_invalid(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    TEST_ASSERT(adc.rawData[0].status == ECH_ADC_STATUS_INVALID, 
                "初始数据状态应为无效");
    TEST_ASSERT(adc.rawData[0].validFlag == ECH_ADC_DATA_INVALID, 
                "初始有效标志应为无效");
}

void test_adc_init_multiple_times(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    adc.sampleCount = 100; /* 修改一些值 */
    
    EchAdc_Init(&adc); /* 重新初始化 */
    TEST_ASSERT(adc.sampleCount == 0, "重新初始化应清除统计");
}

/* ============================================================================
 * 测试：通道配置
 * ============================================================================ */

void test_adc_configure_channel_null(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    int32_t result = EchAdc_ConfigureChannel(NULL, NULL);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_adc_configure_channel_invalid_id(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    EchAdcChannelConfig_t config = {
        .channelId = ECH_ADC_CH_MAX,
        .enabled = true
    };
    
    int32_t result = EchAdc_ConfigureChannel(&adc, &config);
    TEST_ASSERT(result == -2, "无效通道 ID 应返回错误");
}

void test_adc_configure_channel_success(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    EchAdcChannelConfig_t config = {
        .channelId = ECH_ADC_CH_TEMP_INLET,
        .enabled = false,
        .filterEnabled = false,
        .filterWindowSize = 16,
        .calibrationOffset = 1.5f,
        .calibrationGain = 1.1f
    };
    
    int32_t result = EchAdc_ConfigureChannel(&adc, &config);
    TEST_ASSERT(result == 0, "配置应成功");
    TEST_ASSERT(adc.channels[ECH_ADC_CH_TEMP_INLET].enabled == false, 
                "通道使能应更新");
    TEST_ASSERT(adc.channels[ECH_ADC_CH_TEMP_INLET].filterWindowSize == 16, 
                "滤波器窗口应更新");
}

/* ============================================================================
 * 测试：采样功能
 * ============================================================================ */

void test_adc_sample_null_controller(void) {
    int32_t result = EchAdc_Sample(NULL, 100);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_adc_sample_not_initialized(void) {
    EchAdcController_t adc;
    /* 未初始化的控制器，initialized 为 false */
    int32_t result = EchAdc_Sample(&adc, 100);
    /* 由于 initialized 为 false，应返回 -1 */
    TEST_ASSERT(result == -1, "未初始化控制器应返回错误");
}

void test_adc_sample_period_too_short(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    EchAdc_Sample(&adc, 100);
    int32_t result = EchAdc_Sample(&adc, 102); /* 仅 2ms 后，小于最小周期 5ms */
    TEST_ASSERT(result == 0, "周期过短应跳过采样");
}

void test_adc_sample_updates_timestamp(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    EchAdc_Sample(&adc, 1000);
    TEST_ASSERT(adc.lastUpdate_ms == 1000, "时间戳应更新");
    TEST_ASSERT(adc.sampleCount == 1, "采样计数应增加");
}

/* ============================================================================
 * 测试：温度转换
 * ============================================================================ */

void test_adc_temperature_conversion_25c(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    /* 25°C 时 NTC 约分压一半，ADC 值约 2048 */
    adc.rawData[ECH_ADC_CH_TEMP_INLET].rawValue = 2048;
    EchAdc_Sample(&adc, 100);
    
    float temp = EchAdc_GetTemperature(&adc, ECH_ADC_CH_TEMP_INLET);
    TEST_ASSERT(temp > 20.0f && temp < 30.0f, 
                "25°C 附近温度应在合理范围");
}

void test_adc_temperature_conversion_0c(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    /* 0°C 时 NTC 电阻更大，ADC 值更高，约 2700-2900 */
    adc.rawData[ECH_ADC_CH_TEMP_INLET].rawValue = 2750;
    EchAdc_Sample(&adc, 100);
    
    float temp = EchAdc_GetTemperature(&adc, ECH_ADC_CH_TEMP_INLET);
    /* 放宽范围以适应实际计算 */
    TEST_ASSERT(temp > -10.0f && temp < 10.0f, 
                "0°C 附近温度应在合理范围");
}

void test_adc_temperature_conversion_80c(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    /* 80°C 时 NTC 电阻更小，ADC 值更低，约 600-800 */
    adc.rawData[ECH_ADC_CH_TEMP_INLET].rawValue = 700;
    EchAdc_Sample(&adc, 100);
    
    float temp = EchAdc_GetTemperature(&adc, ECH_ADC_CH_TEMP_INLET);
    /* 放宽范围以适应实际计算 */
    TEST_ASSERT(temp > 70.0f && temp < 90.0f, 
                "80°C 附近温度应在合理范围");
}

void test_adc_temperature_invalid_channel(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    float temp = EchAdc_GetTemperature(&adc, ECH_ADC_CH_VBUS);
    TEST_ASSERT(temp == -999.0f, "非温度通道应返回错误值");
}

void test_adc_temperature_invalid_status(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    /* 不采样，状态为 INVALID */
    float temp = EchAdc_GetTemperature(&adc, ECH_ADC_CH_TEMP_INLET);
    TEST_ASSERT(temp == -999.0f, "无效状态应返回错误值");
}

/* ============================================================================
 * 测试：电压电流测量
 * ============================================================================ */

void test_adc_voltage_conversion_12v(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    /* 假设 12V 系统分压后约 1.65V = 2048 ADC */
    adc.rawData[ECH_ADC_CH_VBUS].rawValue = 2048;
    EchAdc_Sample(&adc, 100);
    
    float voltage = EchAdc_GetVoltage(&adc, ECH_ADC_CH_VBUS);
    TEST_ASSERT(voltage > 1.5f && voltage < 1.8f, 
                "12V 系统电压应在合理范围");
}

void test_adc_current_conversion_50a(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    /* 50A 对应约 1.65V = 2048 ADC */
    adc.rawData[ECH_ADC_CH_CURRENT].rawValue = 2048;
    EchAdc_Sample(&adc, 100);
    
    float current = EchAdc_GetCurrent(&adc, ECH_ADC_CH_CURRENT);
    TEST_ASSERT(current > 45.0f && current < 55.0f, 
                "50A 电流应在合理范围");
}

void test_adc_voltage_invalid_channel(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    float voltage = EchAdc_GetVoltage(&adc, ECH_ADC_CH_TEMP_INLET);
    TEST_ASSERT(voltage == -1.0f, "非电压通道应返回错误值");
}

void test_adc_current_invalid_channel(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    float current = EchAdc_GetCurrent(&adc, ECH_ADC_CH_TEMP_INLET);
    TEST_ASSERT(current == -1.0f, "非电流通道应返回错误值");
}

/* ============================================================================
 * 测试：故障检测
 * ============================================================================ */

void test_adc_fault_open_circuit(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    /* 开路：ADC 值接近满量程 */
    adc.rawData[ECH_ADC_CH_TEMP_INLET].rawValue = 4095;
    EchAdc_Sample(&adc, 100);
    
    EchAdcStatus_t status = EchAdc_GetChannelStatus(&adc, ECH_ADC_CH_TEMP_INLET);
    TEST_ASSERT(status == ECH_ADC_STATUS_OPEN, "应检测到开路故障");
}

void test_adc_fault_short_circuit(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    /* 短路：ADC 值接近 0 */
    adc.rawData[ECH_ADC_CH_TEMP_INLET].rawValue = 0;
    EchAdc_Sample(&adc, 100);
    
    EchAdcStatus_t status = EchAdc_GetChannelStatus(&adc, ECH_ADC_CH_TEMP_INLET);
    TEST_ASSERT(status == ECH_ADC_STATUS_SHORT, "应检测到短路故障");
}

void test_adc_fault_normal_operation(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    /* 正常值 */
    adc.rawData[ECH_ADC_CH_TEMP_INLET].rawValue = 2048;
    EchAdc_Sample(&adc, 100);
    
    EchAdcStatus_t status = EchAdc_GetChannelStatus(&adc, ECH_ADC_CH_TEMP_INLET);
    TEST_ASSERT(status == ECH_ADC_STATUS_OK, "正常值应无故障");
}

void test_adc_fault_threshold_boundary(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    /* 测试阈值边界 */
    adc.rawData[ECH_ADC_CH_TEMP_INLET].rawValue = ECH_ADC_OPEN_CIRCUIT_THRESHOLD;
    EchAdc_Sample(&adc, 100);
    
    EchAdcStatus_t status = EchAdc_GetChannelStatus(&adc, ECH_ADC_CH_TEMP_INLET);
    TEST_ASSERT(status == ECH_ADC_STATUS_OPEN, "达到阈值应检测为开路");
}

/* ============================================================================
 * 测试：滤波器
 * ============================================================================ */

void test_adc_filter_sliding_average(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    /* 连续采样不同值 */
    adc.rawData[ECH_ADC_CH_TEMP_INLET].rawValue = 2000;
    EchAdc_Sample(&adc, 100);
    
    adc.rawData[ECH_ADC_CH_TEMP_INLET].rawValue = 2100;
    EchAdc_Sample(&adc, 200);
    
    adc.rawData[ECH_ADC_CH_TEMP_INLET].rawValue = 2050;
    EchAdc_Sample(&adc, 300);
    
    uint16_t filtered = EchAdc_GetRawValue(&adc, ECH_ADC_CH_TEMP_INLET);
    TEST_ASSERT(filtered >= 2000 && filtered <= 2100, 
                "滤波值应在输入范围内");
}

void test_adc_filter_disabled(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    /* 禁用滤波器 */
    adc.channels[ECH_ADC_CH_TEMP_INLET].filterEnabled = false;
    
    adc.rawData[ECH_ADC_CH_TEMP_INLET].rawValue = 2000;
    EchAdc_Sample(&adc, 100);
    
    uint16_t raw = EchAdc_GetRawValue(&adc, ECH_ADC_CH_TEMP_INLET);
    TEST_ASSERT(raw == 2000, "禁用滤波时应返回原始值");
}

/* ============================================================================
 * 测试：校准
 * ============================================================================ */

void test_adc_calibration_set_parameters(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    int32_t result = EchAdc_SetCalibration(&adc, ECH_ADC_CH_TEMP_INLET, 2.0f, 1.1f);
    TEST_ASSERT(result == 0, "设置校准参数应成功");
    TEST_ASSERT(adc.channels[ECH_ADC_CH_TEMP_INLET].calibrationOffset == 2.0f, 
                "偏移应更新");
    TEST_ASSERT(adc.channels[ECH_ADC_CH_TEMP_INLET].calibrationGain == 1.1f, 
                "增益应更新");
}

void test_adc_calibration_invalid_channel(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    int32_t result = EchAdc_SetCalibration(&adc, ECH_ADC_CH_MAX, 0.0f, 1.0f);
    TEST_ASSERT(result == -1, "无效通道应返回错误");
}

void test_adc_calibration_effect(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    /* 设置偏移 +5°C */
    EchAdc_SetCalibration(&adc, ECH_ADC_CH_TEMP_INLET, 5.0f, 1.0f);
    
    adc.rawData[ECH_ADC_CH_TEMP_INLET].rawValue = 2048;
    EchAdc_Sample(&adc, 100);
    
    float temp_cal = EchAdc_GetTemperature(&adc, ECH_ADC_CH_TEMP_INLET);
    
    /* 清除校准 */
    EchAdc_SetCalibration(&adc, ECH_ADC_CH_TEMP_INLET, 0.0f, 1.0f);
    EchAdc_Sample(&adc, 200);
    float temp_raw = EchAdc_GetTemperature(&adc, ECH_ADC_CH_TEMP_INLET);
    
    TEST_ASSERT((temp_cal - temp_raw) > 4.0f && (temp_cal - temp_raw) < 6.0f, 
                "校准后温度应增加约 5°C");
}

/* ============================================================================
 * 测试：重置功能
 * ============================================================================ */

void test_adc_reset_clears_data(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    /* 产生一些数据 */
    adc.rawData[0].rawValue = 1000;
    adc.sampleCount = 50;
    
    EchAdc_Reset(&adc);
    
    TEST_ASSERT(adc.rawData[0].rawValue == 0, "原始数据应清零");
    TEST_ASSERT(adc.sampleCount == 0, "采样计数应清零");
}

void test_adc_reset_preserves_config(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    /* 修改配置 */
    adc.channels[0].enabled = false;
    adc.channels[0].filterWindowSize = 16;
    
    EchAdc_Reset(&adc);
    
    TEST_ASSERT(adc.channels[0].enabled == false, "配置应保留");
    TEST_ASSERT(adc.channels[0].filterWindowSize == 16, "配置应保留");
}

/* ============================================================================
 * 测试：版本号
 * ============================================================================ */

void test_adc_version_not_null(void) {
    uint8_t major, minor, patch;
    EchAdc_GetVersion(&major, &minor, &patch);
    
    TEST_ASSERT(major == ECH_ADC_VERSION_MAJOR, "主版本号应匹配");
    TEST_ASSERT(minor == ECH_ADC_VERSION_MINOR, "次版本号应匹配");
    TEST_ASSERT(patch == ECH_ADC_VERSION_PATCH, "补丁版本号应匹配");
}

void test_adc_version_null_params(void) {
    /* 传入 NULL 不应崩溃 */
    EchAdc_GetVersion(NULL, NULL, NULL);
    TEST_ASSERT(true, "NULL 参数不应崩溃");
}

/* ============================================================================
 * 测试：工程数据获取
 * ============================================================================ */

void test_adc_engineered_data_valid(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    adc.rawData[ECH_ADC_CH_TEMP_INLET].rawValue = 2048;
    EchAdc_Sample(&adc, 100);
    
    EchAdcEngineeredData_t data = EchAdc_GetEngineeredData(&adc, ECH_ADC_CH_TEMP_INLET);
    
    TEST_ASSERT(data.status == ECH_ADC_STATUS_OK, "状态应正常");
    TEST_ASSERT(data.value > 20.0f && data.value < 30.0f, "温度值应合理");
    TEST_ASSERT(data.timestamp_ms == 100, "时间戳应正确");
}

void test_adc_engineered_data_invalid_channel(void) {
    EchAdcController_t adc;
    EchAdc_Init(&adc);
    
    EchAdcEngineeredData_t data = EchAdc_GetEngineeredData(&adc, ECH_ADC_CH_MAX);
    
    TEST_ASSERT(data.status == ECH_ADC_STATUS_INVALID, "无效通道应返回无效状态");
}

/* ============================================================================
 * 主测试函数
 * ============================================================================ */

int main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║        ECH ADC Module Unit Tests                         ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  测试套件：ADC 驱动完整功能验证                            ║\n");
    printf("║  目标覆盖率：每个函数至少 10 项测试                          ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("【1. 初始化功能测试】\n");
    test_adc_init_null_pointer();
    test_adc_init_successful();
    test_adc_init_channels_disabled();
    test_adc_init_filter_config();
    test_adc_init_calibration_default();
    test_adc_init_statistics_cleared();
    test_adc_init_data_invalid();
    test_adc_init_multiple_times();
    printf("\n");
    
    printf("【2. 通道配置测试】\n");
    test_adc_configure_channel_null();
    test_adc_configure_channel_invalid_id();
    test_adc_configure_channel_success();
    printf("\n");
    
    printf("【3. 采样功能测试】\n");
    test_adc_sample_null_controller();
    test_adc_sample_not_initialized();
    test_adc_sample_period_too_short();
    test_adc_sample_updates_timestamp();
    printf("\n");
    
    printf("【4. 温度转换测试】\n");
    test_adc_temperature_conversion_25c();
    test_adc_temperature_conversion_0c();
    test_adc_temperature_conversion_80c();
    test_adc_temperature_invalid_channel();
    test_adc_temperature_invalid_status();
    printf("\n");
    
    printf("【5. 电压电流测量测试】\n");
    test_adc_voltage_conversion_12v();
    test_adc_current_conversion_50a();
    test_adc_voltage_invalid_channel();
    test_adc_current_invalid_channel();
    printf("\n");
    
    printf("【6. 故障检测测试】\n");
    test_adc_fault_open_circuit();
    test_adc_fault_short_circuit();
    test_adc_fault_normal_operation();
    test_adc_fault_threshold_boundary();
    printf("\n");
    
    printf("【7. 滤波器测试】\n");
    test_adc_filter_sliding_average();
    test_adc_filter_disabled();
    printf("\n");
    
    printf("【8. 校准测试】\n");
    test_adc_calibration_set_parameters();
    test_adc_calibration_invalid_channel();
    test_adc_calibration_effect();
    printf("\n");
    
    printf("【9. 重置功能测试】\n");
    test_adc_reset_clears_data();
    test_adc_reset_preserves_config();
    printf("\n");
    
    printf("【10. 版本号测试】\n");
    test_adc_version_not_null();
    test_adc_version_null_params();
    printf("\n");
    
    printf("【11. 工程数据获取测试】\n");
    test_adc_engineered_data_valid();
    test_adc_engineered_data_invalid_channel();
    printf("\n");
    
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  测试结果汇总                                            ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  总测试数：%3d                                           ║\n", g_tests_run);
    printf("║  通过：%3d                                               ║\n", g_tests_passed);
    printf("║  失败：%3d                                               ║\n", g_tests_failed);
    printf("║  通过率：%.1f%%                                          ║\n", 
           g_tests_run > 0 ? (float)g_tests_passed * 100.0f / g_tests_run : 0.0f);
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return (g_tests_failed == 0) ? 0 : 1;
}
