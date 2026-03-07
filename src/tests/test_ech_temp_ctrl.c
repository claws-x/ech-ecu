/**
 * @file test_ech_temp_ctrl.c
 * @brief 温度控制模块单元测试
 * @version 0.1
 * @date 2026-03-07
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

/* 模拟时间戳 */
static uint32_t mock_timestamp_ms = 0;
uint32_t Ech_GetTimestamp_ms(void) { return mock_timestamp_ms; }

/* 包含被测模块 */
#include "../app/ech_temp_ctrl.c"

/* ============================================================================
 * 测试框架
 * ============================================================================ */

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(condition, message, ...) \
    do { \
        g_tests_run++; \
        if (condition) { \
            g_tests_passed++; \
            printf("  ✓ %s\n", #condition); \
        } else { \
            printf("  ✗ FAILED: %s - " message "\n", #condition, ##__VA_ARGS__); \
        } \
    } while(0)

/* ============================================================================
 * 测试：初始化功能
 * ============================================================================ */

void test_temp_init_null_controller(void) {
    int32_t result = EchTempCtrl_Init(NULL);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_temp_init_successful(void) {
    EchTempCtrlController_t ctrl;
    int32_t result = EchTempCtrl_Init(&ctrl);
    
    TEST_ASSERT(result == 0, "初始化应成功");
    TEST_ASSERT(ctrl.initialized == true, "初始化标志应置位");
}

void test_temp_init_default_parameters(void) {
    EchTempCtrlController_t ctrl;
    EchTempCtrl_Init(&ctrl);
    
    TEST_ASSERT(ctrl.targetTemp == 65.0f, "默认目标温度应为 65°C");
    TEST_ASSERT(ctrl.deadband == 2.0f, "默认死区应为 2°C");
    TEST_ASSERT(ctrl.maxDutyCycle == 100.0f, "默认最大占空比应为 100%");
}

/* ============================================================================
 * 测试：目标温度设置
 * ============================================================================ */

void test_temp_set_target_null_controller(void) {
    int32_t result = EchTempCtrl_SetTargetTemp(NULL, 70.0f);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_temp_set_target_within_range(void) {
    EchTempCtrlController_t ctrl;
    EchTempCtrl_Init(&ctrl);
    
    int32_t result = EchTempCtrl_SetTargetTemp(&ctrl, 70.0f);
    
    TEST_ASSERT(result == 0, "设置目标温度应成功");
    TEST_ASSERT(ctrl.targetTemp == 70.0f, "目标温度应正确设置");
}

void test_temp_set_target_below_min(void) {
    EchTempCtrlController_t ctrl;
    EchTempCtrl_Init(&ctrl);
    
    int32_t result = EchTempCtrl_SetTargetTemp(&ctrl, 30.0f);
    
    TEST_ASSERT(result == -2, "低于最小值应返回错误");
    TEST_ASSERT(ctrl.targetTemp == 65.0f, "目标温度应保持默认值");
}

void test_temp_set_target_above_max(void) {
    EchTempCtrlController_t ctrl;
    EchTempCtrl_Init(&ctrl);
    
    int32_t result = EchTempCtrl_SetTargetTemp(&ctrl, 95.0f);
    
    TEST_ASSERT(result == -2, "高于最大值应返回错误");
    TEST_ASSERT(ctrl.targetTemp == 65.0f, "目标温度应保持默认值");
}

/* ============================================================================
 * 测试：温度控制计算
 * ============================================================================ */

void test_temp_calculate_null_controller(void) {
    float duty = EchTempCtrl_Calculate(NULL, 60.0f, 100);
    TEST_ASSERT(duty == 0.0f, "NULL 控制器应返回 0");
}

void test_temp_calculate_uninitialized(void) {
    EchTempCtrlController_t ctrl = {0};
    float duty = EchTempCtrl_Calculate(&ctrl, 60.0f, 100);
    TEST_ASSERT(duty == 0.0f, "未初始化应返回 0");
}

void test_temp_calculate_below_target(void) {
    EchTempCtrlController_t ctrl;
    EchTempCtrl_Init(&ctrl);
    
    /* 当前温度 60°C，目标 65°C，误差 5°C */
    float duty = EchTempCtrl_Calculate(&ctrl, 60.0f, 100);
    
    TEST_ASSERT(duty > 0.0f, "低于目标温度应有输出");
    TEST_ASSERT(duty <= 100.0f, "占空比不应超过 100%");
}

void test_temp_calculate_above_target(void) {
    EchTempCtrlController_t ctrl;
    EchTempCtrl_Init(&ctrl);
    
    /* 当前温度 70°C，目标 65°C，误差 -5°C */
    float duty = EchTempCtrl_Calculate(&ctrl, 70.0f, 100);
    
    TEST_ASSERT(duty == 0.0f, "高于目标温度应关闭加热");
}

void test_temp_calculate_within_deadband(void) {
    EchTempCtrlController_t ctrl;
    EchTempCtrl_Init(&ctrl);
    
    /* 当前温度 64°C，目标 65°C，误差 1°C (在死区内) */
    float duty = EchTempCtrl_Calculate(&ctrl, 64.0f, 100);
    
    TEST_ASSERT(duty == 0.0f, "死区内应关闭加热");
}

void test_temp_calculate_with_pid(void) {
    EchTempCtrlController_t ctrl;
    EchTempCtrl_Init(&ctrl);
    
    /* 设置 PID 参数 */
    ctrl.pid.kp = 2.0f;
    ctrl.pid.ki = 0.1f;
    ctrl.pid.kd = 0.5f;
    
    /* 持续低温，积分应累积 */
    EchTempCtrl_Calculate(&ctrl, 60.0f, 100);
    mock_timestamp_ms = 1000;
    EchTempCtrl_Calculate(&ctrl, 60.0f, 100);
    
    TEST_ASSERT(ctrl.pid.integral > 0, "积分项应累积");
}

/* ============================================================================
 * 测试：安全保护
 * ============================================================================ */

void test_temp_check_over_temp_protection(void) {
    EchTempCtrlController_t ctrl;
    EchTempCtrl_Init(&ctrl);
    
    /* 温度超过过温保护阈值 (85°C) */
    bool protected = EchTempCtrl_CheckOverTemp(&ctrl, 90.0f);
    
    TEST_ASSERT(protected == true, "应触发过温保护");
}

void test_temp_check_over_temp_normal(void) {
    EchTempCtrlController_t ctrl;
    EchTempCtrl_Init(&ctrl);
    
    bool protected = EchTempCtrl_CheckOverTemp(&ctrl, 70.0f);
    
    TEST_ASSERT(protected == false, "正常温度不应触发保护");
}

void test_temp_check_sensor_fault_open(void) {
    EchTempCtrlController_t ctrl;
    EchTempCtrl_Init(&ctrl);
    
    /* 传感器开路 (-999°C) */
    bool fault = EchTempCtrl_CheckSensorFault(&ctrl, -999.0f);
    
    TEST_ASSERT(fault == true, "应检测到传感器开路故障");
}

void test_temp_check_sensor_fault_short(void) {
    EchTempCtrlController_t ctrl;
    EchTempCtrl_Init(&ctrl);
    
    /* 传感器短路 (150°C+) */
    bool fault = EchTempCtrl_CheckSensorFault(&ctrl, 150.0f);
    
    TEST_ASSERT(fault == true, "应检测到传感器短路故障");
}

void test_temp_check_sensor_fault_normal(void) {
    EchTempCtrlController_t ctrl;
    EchTempCtrl_Init(&ctrl);
    
    bool fault = EchTempCtrl_CheckSensorFault(&ctrl, 65.0f);
    
    TEST_ASSERT(fault == false, "正常温度不应有故障");
}

/* ============================================================================
 * 测试：状态获取
 * ============================================================================ */

void test_temp_get_status_null_controller(void) {
    EchTempCtrl_GetStatus(NULL, NULL, NULL);
    TEST_ASSERT(true, "NULL 控制器应处理");
}

void test_temp_get_status_success(void) {
    EchTempCtrlController_t ctrl;
    EchTempCtrl_Init(&ctrl);
    
    ctrl.currentTemp = 62.5f;
    ctrl.targetTemp = 65.0f;
    ctrl.outputDuty = 45.0f;
    
    float current, target, duty;
    EchTempCtrl_GetStatus(&ctrl, &current, &target, &duty);
    
    TEST_ASSERT(current == 62.5f, "当前温度应正确");
    TEST_ASSERT(target == 65.0f, "目标温度应正确");
    TEST_ASSERT(duty == 45.0f, "占空比应正确");
}

/* ============================================================================
 * 测试：诊断功能
 * ============================================================================ */

void test_temp_get_diagnostic_null_controller(void) {
    EchTempCtrlDiagnostic_t diag;
    EchTempCtrl_GetDiagnostic(NULL, &diag);
    TEST_ASSERT(true, "NULL 控制器应处理");
}

void test_temp_get_diagnostic_success(void) {
    EchTempCtrlController_t ctrl;
    EchTempCtrl_Init(&ctrl);
    
    /* 模拟一些运行 */
    ctrl.cycleCount = 100;
    ctrl.totalRunTime_ms = 3600000;
    
    EchTempCtrlDiagnostic_t diag;
    EchTempCtrl_GetDiagnostic(&ctrl, &diag);
    
    TEST_ASSERT(diag.cycleCount == 100, "周期计数应正确");
    TEST_ASSERT(diag.totalRunTime_ms == 3600000, "总运行时间应正确");
}

/* ============================================================================
 * 测试：版本号
 * ============================================================================ */

void test_temp_version_not_null(void) {
    uint8_t major, minor, patch;
    EchTempCtrl_GetVersion(&major, &minor, &patch);
    
    TEST_ASSERT(major != 0 || minor != 0 || patch != 0, "版本号不应全为 0");
}

void test_temp_version_null_params(void) {
    EchTempCtrl_GetVersion(NULL, NULL, NULL);
    TEST_ASSERT(true, "NULL 参数应处理");
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

int main(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  ECH ECU 温度控制模块单元测试                            ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    printf("【1. 初始化功能测试】\n");
    test_temp_init_null_controller();
    test_temp_init_successful();
    test_temp_init_default_parameters();
    printf("\n");
    
    printf("【2. 目标温度设置测试】\n");
    test_temp_set_target_null_controller();
    test_temp_set_target_within_range();
    test_temp_set_target_below_min();
    test_temp_set_target_above_max();
    printf("\n");
    
    printf("【3. 温度控制计算测试】\n");
    test_temp_calculate_null_controller();
    test_temp_calculate_uninitialized();
    test_temp_calculate_below_target();
    test_temp_calculate_above_target();
    test_temp_calculate_within_deadband();
    test_temp_calculate_with_pid();
    printf("\n");
    
    printf("【4. 安全保护测试】\n");
    test_temp_check_over_temp_protection();
    test_temp_check_over_temp_normal();
    test_temp_check_sensor_fault_open();
    test_temp_check_sensor_fault_short();
    test_temp_check_sensor_fault_normal();
    printf("\n");
    
    printf("【5. 状态获取测试】\n");
    test_temp_get_status_null_controller();
    test_temp_get_status_success();
    printf("\n");
    
    printf("【6. 诊断功能测试】\n");
    test_temp_get_diagnostic_null_controller();
    test_temp_get_diagnostic_success();
    printf("\n");
    
    printf("【7. 版本号测试】\n");
    test_temp_version_not_null();
    test_temp_version_null_params();
    printf("\n");
    
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  测试结果汇总                                            ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  总测试数：%3d                                           ║\n", g_tests_run);
    printf("║  通过：%3d                                               ║\n", g_tests_passed);
    printf("║  失败：%3d                                               ║\n", g_tests_run - g_tests_passed);
    printf("║  通过率：%5.1f%%                                         ║\n", 
           g_tests_run > 0 ? (float)g_tests_passed / g_tests_run * 100 : 0);
    printf("╚══════════════════════════════════════════════════════════╝\n");
    
    return (g_tests_run == g_tests_passed) ? 0 : 1;
}
