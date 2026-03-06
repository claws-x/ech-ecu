/**
 * @file test_ech_pid.c
 * @brief ECH PID 控制算法模块单元测试
 * 
 * @details
 * 本文件包含 ECH PID 控制算法模块的完整单元测试套件。
 * 测试覆盖：初始化、计算、死区、使能/禁用、诊断等。
 * 
 * 测试目标：每个函数至少 10 项测试用例
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ech_pid.h"

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

void test_pid_init_null_controller(void) {
    EchPidConfig_t config = {.kp = 1.0f, .ki = 0.1f, .kd = 0.5f};
    int32_t result = EchPid_Init(NULL, &config);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_pid_init_null_config(void) {
    EchPidController_t pid;
    int32_t result = EchPid_Init(&pid, NULL);
    TEST_ASSERT(result == -1, "NULL 配置应返回错误");
}

void test_pid_init_successful(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {
        .kp = 2.0f, .ki = 0.1f, .kd = 0.5f,
        .setpoint = 50.0f, .enabled = true
    };
    
    int32_t result = EchPid_Init(&pid, &config);
    TEST_ASSERT(result == 0, "正常初始化应成功");
}

void test_pid_init_parameters_set(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {
        .kp = 2.5f, .ki = 0.2f, .kd = 0.8f,
        .setpoint = 60.0f, .enabled = true
    };
    
    EchPid_Init(&pid, &config);
    
    TEST_ASSERT(pid.kp == 2.5f, "Kp 应正确设置");
    TEST_ASSERT(pid.ki == 0.2f, "Ki 应正确设置");
    TEST_ASSERT(pid.kd == 0.8f, "Kd 应正确设置");
}

void test_pid_init_setpoint_set(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {.kp = 1.0f, .ki = 0.1f, .kd = 0.5f, .setpoint = 75.0f};
    
    EchPid_Init(&pid, &config);
    TEST_ASSERT(pid.setpoint == 75.0f, "设定值应正确设置");
}

void test_pid_init_enabled_state(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {.kp = 1.0f, .ki = 0.1f, .kd = 0.5f, .enabled = true};
    
    EchPid_Init(&pid, &config);
    TEST_ASSERT(pid.enabled == true, "使能状态应正确");
}

void test_pid_init_disabled_state(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {.kp = 1.0f, .ki = 0.1f, .kd = 0.5f, .enabled = false};
    
    EchPid_Init(&pid, &config);
    TEST_ASSERT(pid.enabled == false, "禁用状态应正确");
}

void test_pid_init_integral_cleared(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {.kp = 1.0f, .ki = 0.1f, .kd = 0.5f};
    
    EchPid_Init(&pid, &config);
    TEST_ASSERT(pid.integral == 0.0f, "积分项应初始化为 0");
    TEST_ASSERT(pid.prev_error == 0.0f, "上次误差应初始化为 0");
}

void test_pid_init_output_zero(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {.kp = 1.0f, .ki = 0.1f, .kd = 0.5f};
    
    EchPid_Init(&pid, &config);
    TEST_ASSERT(pid.output == 0.0f, "初始输出应为 0");
}

void test_pid_init_cycle_count_zero(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {.kp = 1.0f, .ki = 0.1f, .kd = 0.5f};
    
    EchPid_Init(&pid, &config);
    TEST_ASSERT(pid.cycle_count == 0, "周期计数应初始化为 0");
}

/* ============================================================================
 * 测试：PID 计算
 * ============================================================================ */

void test_pid_calculate_null_controller(void) {
    float output = EchPid_Calculate(NULL, 25.0f, 100);
    TEST_ASSERT(output == 0.0f, "NULL 控制器应返回 0");
}

void test_pid_calculate_disabled(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {.kp = 1.0f, .ki = 0.1f, .kd = 0.5f, .enabled = false};
    
    EchPid_Init(&pid, &config);
    float output = EchPid_Calculate(&pid, 25.0f, 100);
    TEST_ASSERT(output == 0.0f, "禁用状态应返回 0 输出");
}

void test_pid_calculate_positive_error(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {
        .kp = 2.0f, .ki = 0.1f, .kd = 0.5f,
        .setpoint = 50.0f, .enabled = true
    };
    
    EchPid_Init(&pid, &config);
    float output = EchPid_Calculate(&pid, 30.0f, 100); /* 误差 = 20 */
    
    TEST_ASSERT(output > 0.0f, "正误差应产生正输出");
}

void test_pid_calculate_negative_error(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {
        .kp = 2.0f, .ki = 0.1f, .kd = 0.5f,
        .setpoint = 30.0f, .enabled = true
    };
    
    EchPid_Init(&pid, &config);
    float output = EchPid_Calculate(&pid, 50.0f, 100); /* 误差 = -20 */
    
    TEST_ASSERT(output < 0.0f, "负误差应产生负输出");
}

void test_pid_calculate_zero_error(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {
        .kp = 2.0f, .ki = 0.1f, .kd = 0.5f,
        .setpoint = 40.0f, .enabled = true
    };
    
    EchPid_Init(&pid, &config);
    
    /* 先运行几次建立状态 */
    EchPid_Calculate(&pid, 35.0f, 100);
    float output = EchPid_Calculate(&pid, 40.0f, 200); /* 误差 = 0 */
    
    /* 由于死区，输出应接近 0 */
    TEST_ASSERT(output == 0.0f || output < 5.0f, "零误差应产生接近 0 输出");
}

void test_pid_calculate_increments_cycle_count(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {.kp = 1.0f, .ki = 0.1f, .kd = 0.5f, .enabled = true};
    
    EchPid_Init(&pid, &config);
    EchPid_Calculate(&pid, 30.0f, 100);
    
    TEST_ASSERT(pid.cycle_count == 1, "周期计数应增加");
}

void test_pid_calculate_updates_timestamp(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {.kp = 1.0f, .ki = 0.1f, .kd = 0.5f, .enabled = true};
    
    EchPid_Init(&pid, &config);
    EchPid_Calculate(&pid, 30.0f, 500);
    
    TEST_ASSERT(pid.last_update_ms == 500, "时间戳应更新");
}

void test_pid_calculate_period_too_short(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {.kp = 1.0f, .ki = 0.1f, .kd = 0.5f, .enabled = true};
    
    EchPid_Init(&pid, &config);
    EchPid_Calculate(&pid, 30.0f, 100);
    float output1 = pid.output;
    
    /* 仅 10ms 后，小于最小周期 50ms */
    float output2 = EchPid_Calculate(&pid, 35.0f, 110);
    
    TEST_ASSERT(output2 == output1, "周期过短应返回上次输出");
}

void test_pid_calculate_integral_accumulation(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {
        .kp = 1.0f, .ki = 0.5f, .kd = 0.0f,
        .setpoint = 50.0f, .enabled = true
    };
    
    EchPid_Init(&pid, &config);
    
    /* 连续多次计算，积分项应累积 */
    EchPid_Calculate(&pid, 30.0f, 100);
    float integral1 = pid.integral;
    
    EchPid_Calculate(&pid, 30.0f, 200);
    float integral2 = pid.integral;
    
    TEST_ASSERT(integral2 > integral1, "积分项应累积");
}

void test_pid_calculate_output_saturation(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {
        .kp = 10.0f, .ki = 0.0f, .kd = 0.0f,
        .setpoint = 100.0f, .enabled = true
    };
    
    EchPid_Init(&pid, &config);
    
    /* 大误差应导致输出饱和 */
    float output = EchPid_Calculate(&pid, 0.0f, 100);
    
    TEST_ASSERT(output <= ECH_PID_OUTPUT_MAX, "输出不应超过最大值");
    TEST_ASSERT(output >= ECH_PID_OUTPUT_MIN, "输出不应低于最小值");
}

/* ============================================================================
 * 测试：死区功能
 * ============================================================================ */

void test_pid_deadband_small_error(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {
        .kp = 2.0f, .ki = 0.0f, .kd = 0.0f,
        .setpoint = 50.0f, .enabled = true
    };
    
    EchPid_Init(&pid, &config);
    
    /* 误差 0.2°C，在死区±0.5°C 内 */
    float output = EchPid_Calculate(&pid, 49.8f, 100);
    TEST_ASSERT(output == 0.0f, "死区内误差应输出 0");
}

void test_pid_deadband_boundary(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {
        .kp = 2.0f, .ki = 0.0f, .kd = 0.0f,
        .setpoint = 50.0f, .enabled = true
    };
    
    EchPid_Init(&pid, &config);
    
    /* 误差 0.6°C，略超出死区 */
    float output = EchPid_Calculate(&pid, 49.4f, 100);
    TEST_ASSERT(output != 0.0f, "死区外误差应有输出");
}

/* ============================================================================
 * 测试：使能/禁用
 * ============================================================================ */

void test_pid_set_enabled_null(void) {
    EchPid_SetEnabled(NULL, false);
    TEST_ASSERT(true, "NULL 控制器不应崩溃");
}

void test_pid_set_enabled_disable_clears_integral(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {.kp = 1.0f, .ki = 0.1f, .kd = 0.5f, .enabled = true};
    
    EchPid_Init(&pid, &config);
    EchPid_Calculate(&pid, 30.0f, 100);
    
    /* 积分已有值 */
    float integral_before = pid.integral;
    
    EchPid_SetEnabled(&pid, false);
    
    TEST_ASSERT(pid.integral == 0.0f, "禁用应清除积分");
    TEST_ASSERT(pid.output == 0.0f, "禁用应清除输出");
}

void test_pid_set_enabled_re_enable(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {.kp = 1.0f, .ki = 0.1f, .kd = 0.5f, .enabled = false};
    
    EchPid_Init(&pid, &config);
    EchPid_SetEnabled(&pid, true);
    
    float output = EchPid_Calculate(&pid, 30.0f, 100);
    TEST_ASSERT(output != 0.0f, "重新使能后应正常计算");
}

/* ============================================================================
 * 测试：设定值设置
 * ============================================================================ */

void test_pid_set_setpoint_null(void) {
    EchPid_SetSetpoint(NULL, 50.0f);
    TEST_ASSERT(true, "NULL 控制器不应崩溃");
}

void test_pid_set_setpoint_success(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {.kp = 1.0f, .ki = 0.1f, .kd = 0.5f, .setpoint = 40.0f};
    
    EchPid_Init(&pid, &config);
    EchPid_SetSetpoint(&pid, 60.0f);
    
    TEST_ASSERT(pid.setpoint == 60.0f, "设定值应更新");
}

/* ============================================================================
 * 测试：参数调整
 * ============================================================================ */

void test_pid_tune_null_controller(void) {
    EchPid_Tune(NULL, 1.0f, 0.1f, 0.5f);
    TEST_ASSERT(true, "NULL 控制器不应崩溃");
}

void test_pid_tune_parameters(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {.kp = 1.0f, .ki = 0.1f, .kd = 0.5f};
    
    EchPid_Init(&pid, &config);
    EchPid_Tune(&pid, 3.0f, 0.3f, 0.8f);
    
    TEST_ASSERT(pid.kp == 3.0f, "Kp 应更新");
    TEST_ASSERT(pid.ki == 0.3f, "Ki 应更新");
    TEST_ASSERT(pid.kd == 0.8f, "Kd 应更新");
}

/* ============================================================================
 * 测试：重置功能
 * ============================================================================ */

void test_pid_reset_null_controller(void) {
    EchPid_Reset(NULL);
    TEST_ASSERT(true, "NULL 控制器不应崩溃");
}

void test_pid_reset_clears_state(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {.kp = 1.0f, .ki = 0.1f, .kd = 0.5f, .enabled = true};
    
    EchPid_Init(&pid, &config);
    EchPid_Calculate(&pid, 30.0f, 100);
    
    EchPid_Reset(&pid);
    
    TEST_ASSERT(pid.integral == 0.0f, "重置应清除积分");
    TEST_ASSERT(pid.prev_error == 0.0f, "重置应清除上次误差");
    TEST_ASSERT(pid.cycle_count == 0, "重置应清除周期计数");
}

/* ============================================================================
 * 测试：诊断功能
 * ============================================================================ */

void test_pid_get_diagnostic_null_controller(void) {
    EchPidDiagnostic_t diag;
    EchPid_GetDiagnostic(NULL, &diag);
    TEST_ASSERT(true, "NULL 控制器不应崩溃");
}

void test_pid_get_diagnostic_success(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {
        .kp = 2.0f, .ki = 0.1f, .kd = 0.5f,
        .setpoint = 50.0f, .enabled = true
    };
    
    EchPid_Init(&pid, &config);
    EchPid_Calculate(&pid, 45.0f, 100);
    
    EchPidDiagnostic_t diag;
    EchPid_GetDiagnostic(&pid, &diag);
    
    TEST_ASSERT(diag.current_error == 5.0f, "当前误差应正确");
    TEST_ASSERT(diag.is_enabled == true, "使能状态应正确");
    TEST_ASSERT(diag.cycle_count == 1, "周期计数应正确");
}

void test_pid_get_diagnostic_saturation(void) {
    EchPidController_t pid;
    EchPidConfig_t config = {
        .kp = 100.0f, .ki = 0.0f, .kd = 0.0f,
        .setpoint = 100.0f, .enabled = true
    };
    
    EchPid_Init(&pid, &config);
    EchPid_Calculate(&pid, 0.0f, 100);
    
    EchPidDiagnostic_t diag;
    EchPid_GetDiagnostic(&pid, &diag);
    
    TEST_ASSERT(diag.is_saturated == true, "应检测到饱和");
}

/* ============================================================================
 * 测试：版本号
 * ============================================================================ */

void test_pid_version_not_null(void) {
    uint8_t major, minor, patch;
    EchPid_GetVersion(&major, &minor, &patch);
    
    TEST_ASSERT(major == ECH_PID_VERSION_MAJOR, "主版本号应匹配");
    TEST_ASSERT(minor == ECH_PID_VERSION_MINOR, "次版本号应匹配");
    TEST_ASSERT(patch == ECH_PID_VERSION_PATCH, "补丁版本号应匹配");
}

void test_pid_version_null_params(void) {
    EchPid_GetVersion(NULL, NULL, NULL);
    TEST_ASSERT(true, "NULL 参数不应崩溃");
}

/* ============================================================================
 * 主测试函数
 * ============================================================================ */

int main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║        ECH PID Module Unit Tests                         ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  测试套件：PID 控制算法完整功能验证                        ║\n");
    printf("║  目标覆盖率：每个函数至少 10 项测试                          ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("【1. 初始化功能测试】\n");
    test_pid_init_null_controller();
    test_pid_init_null_config();
    test_pid_init_successful();
    test_pid_init_parameters_set();
    test_pid_init_setpoint_set();
    test_pid_init_enabled_state();
    test_pid_init_disabled_state();
    test_pid_init_integral_cleared();
    test_pid_init_output_zero();
    test_pid_init_cycle_count_zero();
    printf("\n");
    
    printf("【2. PID 计算测试】\n");
    test_pid_calculate_null_controller();
    test_pid_calculate_disabled();
    test_pid_calculate_positive_error();
    test_pid_calculate_negative_error();
    test_pid_calculate_zero_error();
    test_pid_calculate_increments_cycle_count();
    test_pid_calculate_updates_timestamp();
    test_pid_calculate_period_too_short();
    test_pid_calculate_integral_accumulation();
    test_pid_calculate_output_saturation();
    printf("\n");
    
    printf("【3. 死区功能测试】\n");
    test_pid_deadband_small_error();
    test_pid_deadband_boundary();
    printf("\n");
    
    printf("【4. 使能/禁用测试】\n");
    test_pid_set_enabled_null();
    test_pid_set_enabled_disable_clears_integral();
    test_pid_set_enabled_re_enable();
    printf("\n");
    
    printf("【5. 设定值设置测试】\n");
    test_pid_set_setpoint_null();
    test_pid_set_setpoint_success();
    printf("\n");
    
    printf("【6. 参数调整测试】\n");
    test_pid_tune_null_controller();
    test_pid_tune_parameters();
    printf("\n");
    
    printf("【7. 重置功能测试】\n");
    test_pid_reset_null_controller();
    test_pid_reset_clears_state();
    printf("\n");
    
    printf("【8. 诊断功能测试】\n");
    test_pid_get_diagnostic_null_controller();
    test_pid_get_diagnostic_success();
    test_pid_get_diagnostic_saturation();
    printf("\n");
    
    printf("【9. 版本号测试】\n");
    test_pid_version_not_null();
    test_pid_version_null_params();
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
