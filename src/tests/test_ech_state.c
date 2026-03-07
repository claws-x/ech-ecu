/**
 * @file test_ech_state.c
 * @brief 状态管理服务单元测试
 * @version 0.1
 * @date 2026-03-07
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

/* 模拟时间戳 */
static uint32_t mock_timestamp_ms = 0;
uint32_t Ech_GetTimestamp_ms(void) { return mock_timestamp_ms; }

/* 包含被测模块 */
#include "../app/ech_state.c"

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

void test_state_init_null_controller(void) {
    int32_t result = EchState_Init(NULL);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_state_init_successful(void) {
    EchStateController_t state;
    int32_t result = EchState_Init(&state);
    
    TEST_ASSERT(result == 0, "初始化应成功");
    TEST_ASSERT(state.initialized == true, "初始化标志应置位");
    TEST_ASSERT(state.currentState == ECH_STATE_STANDBY, "初始状态应为待机");
}

void test_state_init_parameters_set(void) {
    EchStateController_t state;
    EchState_Init(&state);
    
    TEST_ASSERT(state.stateCount == 0, "状态计数应清零");
    TEST_ASSERT(state.faultCount == 0, "故障计数应清零");
    TEST_ASSERT(state.lastTransitionTime_ms == 0, "最后转换时间应清零");
}

/* ============================================================================
 * 测试：状态转换
 * ============================================================================ */

void test_state_transition_null_controller(void) {
    int32_t result = EchState_Transition(NULL, ECH_STATE_RUNNING);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_state_transition_standy_to_running(void) {
    EchStateController_t state;
    EchState_Init(&state);
    
    mock_timestamp_ms = 1000;
    int32_t result = EchState_Transition(&state, ECH_STATE_RUNNING);
    
    TEST_ASSERT(result == 0, "状态转换应成功");
    TEST_ASSERT(state.currentState == ECH_STATE_RUNNING, "当前状态应为运行中");
    TEST_ASSERT(state.stateCount == 1, "状态计数应增加");
}

void test_state_transition_running_to_fault(void) {
    EchStateController_t state;
    EchState_Init(&state);
    EchState_Transition(&state, ECH_STATE_RUNNING);
    
    mock_timestamp_ms = 2000;
    int32_t result = EchState_Transition(&state, ECH_STATE_FAULT);
    
    TEST_ASSERT(result == 0, "状态转换应成功");
    TEST_ASSERT(state.currentState == ECH_STATE_FAULT, "当前状态应为故障");
    TEST_ASSERT(state.faultCount == 1, "故障计数应增加");
}

void test_state_transition_fault_to_maintenance(void) {
    EchStateController_t state;
    EchState_Init(&state);
    EchState_Transition(&state, ECH_STATE_RUNNING);
    EchState_Transition(&state, ECH_STATE_FAULT);
    
    mock_timestamp_ms = 3000;
    int32_t result = EchState_Transition(&state, ECH_STATE_MAINTENANCE);
    
    TEST_ASSERT(result == 0, "状态转换应成功");
    TEST_ASSERT(state.currentState == ECH_STATE_MAINTENANCE, "当前状态应为维护");
}

void test_state_transition_maintenance_to_standy(void) {
    EchStateController_t state;
    EchState_Init(&state);
    EchState_Transition(&state, ECH_STATE_RUNNING);
    EchState_Transition(&state, ECH_STATE_FAULT);
    EchState_Transition(&state, ECH_STATE_MAINTENANCE);
    
    mock_timestamp_ms = 4000;
    int32_t result = EchState_Transition(&state, ECH_STATE_STANDBY);
    
    TEST_ASSERT(result == 0, "状态转换应成功");
    TEST_ASSERT(state.currentState == ECH_STATE_STANDBY, "当前状态应为待机");
}

/* ============================================================================
 * 测试：状态获取
 * ============================================================================ */

void test_state_get_current_null_controller(void) {
    EchState_t state = EchState_GetCurrent(NULL);
    TEST_ASSERT(state == ECH_STATE_ERROR, "NULL 控制器应返回错误状态");
}

void test_state_get_current_after_init(void) {
    EchStateController_t ctrl;
    EchState_Init(&ctrl);
    
    EchState_t state = EchState_GetCurrent(&ctrl);
    TEST_ASSERT(state == ECH_STATE_STANDBY, "初始化后状态应为待机");
}

void test_state_get_current_after_transition(void) {
    EchStateController_t ctrl;
    EchState_Init(&ctrl);
    EchState_Transition(&ctrl, ECH_STATE_RUNNING);
    
    EchState_t state = EchState_GetCurrent(&ctrl);
    TEST_ASSERT(state == ECH_STATE_RUNNING, "状态应为运行中");
}

/* ============================================================================
 * 测试：故障管理
 * ============================================================================ */

void test_state_add_fault_null_controller(void) {
    int32_t result = EchState_AddFault(NULL, ECH_FAULT_OVER_TEMP);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_state_add_fault_successful(void) {
    EchStateController_t ctrl;
    EchState_Init(&ctrl);
    
    mock_timestamp_ms = 1000;
    int32_t result = EchState_AddFault(&ctrl, ECH_FAULT_OVER_TEMP);
    
    TEST_ASSERT(result == 0, "添加故障应成功");
    TEST_ASSERT(ctrl.activeFaults[0].faultCode == ECH_FAULT_OVER_TEMP, "故障码应正确");
    TEST_ASSERT(ctrl.activeFaults[0].timestamp_ms == 1000, "故障时间戳应正确");
}

void test_state_add_fault_multiple(void) {
    EchStateController_t ctrl;
    EchState_Init(&ctrl);
    
    EchState_AddFault(&ctrl, ECH_FAULT_OVER_TEMP);
    EchState_AddFault(&ctrl, ECH_FAULT_OVER_CURRENT);
    EchState_AddFault(&ctrl, ECH_FAULT_LIN_ERROR);
    
    TEST_ASSERT(ctrl.faultCount == 3, "故障计数应正确");
}

void test_state_clear_fault_null_controller(void) {
    int32_t result = EchState_ClearFault(NULL, ECH_FAULT_OVER_TEMP);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_state_clear_fault_successful(void) {
    EchStateController_t ctrl;
    EchState_Init(&ctrl);
    
    EchState_AddFault(&ctrl, ECH_FAULT_OVER_TEMP);
    EchState_AddFault(&ctrl, ECH_FAULT_OVER_CURRENT);
    
    int32_t result = EchState_ClearFault(&ctrl, ECH_FAULT_OVER_TEMP);
    
    TEST_ASSERT(result == 0, "清除故障应成功");
    TEST_ASSERT(ctrl.faultCount == 1, "故障计数应减少");
}

void test_state_clear_all_faults(void) {
    EchStateController_t ctrl;
    EchState_Init(&ctrl);
    
    EchState_AddFault(&ctrl, ECH_FAULT_OVER_TEMP);
    EchState_AddFault(&ctrl, ECH_FAULT_OVER_CURRENT);
    
    int32_t result = EchState_ClearAllFaults(&ctrl);
    
    TEST_ASSERT(result == 0, "清除所有故障应成功");
    TEST_ASSERT(ctrl.faultCount == 0, "故障计数应清零");
}

/* ============================================================================
 * 测试：状态机运行
 * ============================================================================ */

void test_state_run_null_controller(void) {
    int32_t result = EchState_Run(NULL);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_state_run_standy_state(void) {
    EchStateController_t ctrl;
    EchState_Init(&ctrl);
    
    mock_timestamp_ms = 1000;
    int32_t result = EchState_Run(&ctrl);
    
    TEST_ASSERT(result == 0, "运行应成功");
    TEST_ASSERT(ctrl.currentState == ECH_STATE_STANDBY, "状态应保持待机");
}

void test_state_run_with_fault_should_transition(void) {
    EchStateController_t ctrl;
    EchState_Init(&ctrl);
    EchState_Transition(&ctrl, ECH_STATE_RUNNING);
    
    EchState_AddFault(&ctrl, ECH_FAULT_CRITICAL);
    
    mock_timestamp_ms = 2000;
    EchState_Run(&ctrl);
    
    TEST_ASSERT(ctrl.currentState == ECH_STATE_FAULT, "有关键故障应转换到故障状态");
}

/* ============================================================================
 * 测试：诊断功能
 * ============================================================================ */

void test_state_get_diagnostic_null_controller(void) {
    EchStateDiagnostic_t diag;
    EchState_GetDiagnostic(NULL, &diag);
    TEST_ASSERT(true, "NULL 控制器应处理");
}

void test_state_get_diagnostic_success(void) {
    EchStateController_t ctrl;
    EchState_Init(&ctrl);
    
    EchState_Transition(&ctrl, ECH_STATE_RUNNING);
    EchState_Transition(&ctrl, ECH_STATE_FAULT);
    
    EchStateDiagnostic_t diag;
    EchState_GetDiagnostic(&ctrl, &diag);
    
    TEST_ASSERT(diag.currentState == ECH_STATE_FAULT, "当前状态应正确");
    TEST_ASSERT(diag.stateCount == 2, "状态计数应正确");
    TEST_ASSERT(diag.faultCount == 1, "故障计数应正确");
}

/* ============================================================================
 * 测试：版本号
 * ============================================================================ */

void test_state_version_not_null(void) {
    uint8_t major, minor, patch;
    EchState_GetVersion(&major, &minor, &patch);
    
    TEST_ASSERT(major != 0 || minor != 0 || patch != 0, "版本号不应全为 0");
}

void test_state_version_null_params(void) {
    EchState_GetVersion(NULL, NULL, NULL);
    TEST_ASSERT(true, "NULL 参数应处理");
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

int main(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  ECH ECU 状态管理服务单元测试                            ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    printf("【1. 初始化功能测试】\n");
    test_state_init_null_controller();
    test_state_init_successful();
    test_state_init_parameters_set();
    printf("\n");
    
    printf("【2. 状态转换测试】\n");
    test_state_transition_null_controller();
    test_state_transition_standy_to_running();
    test_state_transition_running_to_fault();
    test_state_transition_fault_to_maintenance();
    test_state_transition_maintenance_to_standy();
    printf("\n");
    
    printf("【3. 状态获取测试】\n");
    test_state_get_current_null_controller();
    test_state_get_current_after_init();
    test_state_get_current_after_transition();
    printf("\n");
    
    printf("【4. 故障管理测试】\n");
    test_state_add_fault_null_controller();
    test_state_add_fault_successful();
    test_state_add_fault_multiple();
    test_state_clear_fault_null_controller();
    test_state_clear_fault_successful();
    test_state_clear_all_faults();
    printf("\n");
    
    printf("【5. 状态机运行测试】\n");
    test_state_run_null_controller();
    test_state_run_standy_state();
    test_state_run_with_fault_should_transition();
    printf("\n");
    
    printf("【6. 诊断功能测试】\n");
    test_state_get_diagnostic_null_controller();
    test_state_get_diagnostic_success();
    printf("\n");
    
    printf("【7. 版本号测试】\n");
    test_state_version_not_null();
    test_state_version_null_params();
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
