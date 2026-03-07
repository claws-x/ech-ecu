/**
 * @file test_ech_watchdog.c
 * @brief 看门狗服务单元测试
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
#include "../services/ech_watchdog.c"

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

void test_wdg_init_null_controller(void) {
    int32_t result = EchWdg_Init(NULL, 1000);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_wdg_init_successful(void) {
    EchWdgController_t wdg;
    int32_t result = EchWdg_Init(&wdg, 1000);
    
    TEST_ASSERT(result == 0, "初始化应成功");
    TEST_ASSERT(wdg.initialized == true, "初始化标志应置位");
    TEST_ASSERT(wdg.timeout_ms == 1000, "超时时间应正确设置");
    TEST_ASSERT(wdg.state == ECH_WDG_STATE_RUNNING, "状态应为运行中");
}

void test_wdg_init_parameters_set(void) {
    EchWdgController_t wdg;
    EchWdg_Init(&wdg, 2000);
    
    TEST_ASSERT(wdg.timeout_ms == 2000, "超时时间应正确");
    TEST_ASSERT(wdg.feedCount == 0, "喂狗计数应清零");
    TEST_ASSERT(wdg.lastFeedTime_ms == 0, "最后喂狗时间应清零");
    TEST_ASSERT(wdg.resetCount == 0, "复位计数应清零");
}

void test_wdg_init_timestamp_set(void) {
    EchWdgController_t wdg;
    mock_timestamp_ms = 5000;
    
    EchWdg_Init(&wdg, 1000);
    
    TEST_ASSERT(wdg.lastFeedTime_ms == 5000, "初始时间戳应正确");
}

/* ============================================================================
 * 测试：喂狗功能
 * ============================================================================ */

void test_wdg_feed_null_controller(void) {
    int32_t result = EchWdg_Feed(NULL);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_wdg_feed_uninitialized(void) {
    EchWdgController_t wdg = {0};
    int32_t result = EchWdg_Feed(&wdg);
    TEST_ASSERT(result == -1, "未初始化应返回错误");
}

void test_wdg_feed_successful(void) {
    EchWdgController_t wdg;
    EchWdg_Init(&wdg, 1000);
    
    mock_timestamp_ms = 1000;
    int32_t result = EchWdg_Feed(&wdg);
    
    TEST_ASSERT(result == 0, "喂狗应成功");
    TEST_ASSERT(wdg.feedCount == 1, "喂狗计数应增加");
    TEST_ASSERT(wdg.lastFeedTime_ms == 1000, "最后喂狗时间应更新");
}

void test_wdg_feed_multiple_times(void) {
    EchWdgController_t wdg;
    EchWdg_Init(&wdg, 1000);
    
    mock_timestamp_ms = 1000;
    EchWdg_Feed(&wdg);
    
    mock_timestamp_ms = 2000;
    EchWdg_Feed(&wdg);
    
    mock_timestamp_ms = 3000;
    EchWdg_Feed(&wdg);
    
    TEST_ASSERT(wdg.feedCount == 3, "喂狗计数应正确");
    TEST_ASSERT(wdg.lastFeedTime_ms == 3000, "最后喂狗时间应正确");
}

/* ============================================================================
 * 测试：监控功能
 * ============================================================================ */

void test_wdg_monitor_null_controller(void) {
    EchWdgStatus_t status;
    int32_t result = EchWdg_Monitor(NULL, &status);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_wdg_monitor_within_timeout(void) {
    EchWdgController_t wdg;
    EchWdg_Init(&wdg, 1000);
    EchWdg_Feed(&wdg);
    
    mock_timestamp_ms = 500; /* 500ms 后，未超时 */
    
    EchWdgStatus_t status;
    int32_t result = EchWdg_Monitor(&wdg, &status);
    
    TEST_ASSERT(result == 0, "监控应成功");
    TEST_ASSERT(status == ECH_WDG_STATUS_OK, "状态应正常");
}

void test_wdg_monitor_timeout(void) {
    EchWdgController_t wdg;
    EchWdg_Init(&wdg, 1000);
    EchWdg_Feed(&wdg);
    
    mock_timestamp_ms = 1500; /* 1500ms 后，已超时 */
    
    EchWdgStatus_t status;
    int32_t result = EchWdg_Monitor(&wdg, &status);
    
    TEST_ASSERT(result == 0, "监控应成功");
    TEST_ASSERT(status == ECH_WDG_STATUS_TIMEOUT, "状态应超时");
}

void test_wdg_monitor_time_remaining(void) {
    EchWdgController_t wdg;
    EchWdg_Init(&wdg, 1000);
    EchWdg_Feed(&wdg);
    
    mock_timestamp_ms = 600; /* 600ms 后 */
    
    EchWdgStatus_t status;
    EchWdg_Monitor(&wdg, &status);
    
    uint32_t remaining = EchWdg_GetTimeRemaining(&wdg);
    TEST_ASSERT(remaining == 400, "剩余时间应正确，实际=%d", remaining);
}

/* ============================================================================
 * 测试：复位功能
 * ============================================================================ */

void test_wdg_reset_null_controller(void) {
    int32_t result = EchWdg_Reset(NULL);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_wdg_reset_uninitialized(void) {
    EchWdgController_t wdg = {0};
    int32_t result = EchWdg_Reset(&wdg);
    TEST_ASSERT(result == -1, "未初始化应返回错误");
}

void test_wdg_reset_successful(void) {
    EchWdgController_t wdg;
    EchWdg_Init(&wdg, 1000);
    
    /* 先喂几次狗 */
    EchWdg_Feed(&wdg);
    EchWdg_Feed(&wdg);
    
    int32_t result = EchWdg_Reset(&wdg);
    
    TEST_ASSERT(result == 0, "复位应成功");
    TEST_ASSERT(wdg.feedCount == 0, "喂狗计数应清零");
    TEST_ASSERT(wdg.resetCount == 1, "复位计数应增加");
}

/* ============================================================================
 * 测试：状态获取
 * ============================================================================ */

void test_wdg_get_state_null_controller(void) {
    EchWdgState_t state = EchWdg_GetState(NULL);
    TEST_ASSERT(state == ECH_WDG_STATE_ERROR, "NULL 控制器应返回错误状态");
}

void test_wdg_get_state_after_init(void) {
    EchWdgController_t wdg;
    EchWdg_Init(&wdg, 1000);
    
    EchWdgState_t state = EchWdg_GetState(&wdg);
    TEST_ASSERT(state == ECH_WDG_STATE_RUNNING, "初始化后状态应为运行中");
}

void test_wdg_get_state_after_timeout(void) {
    EchWdgController_t wdg;
    EchWdg_Init(&wdg, 1000);
    EchWdg_Feed(&wdg);
    
    mock_timestamp_ms = 1500; /* 超时 */
    EchWdg_Monitor(&wdg, NULL);
    
    EchWdgState_t state = EchWdg_GetState(&wdg);
    TEST_ASSERT(state == ECH_WDG_STATE_TIMEOUT, "超时后状态应为超时");
}

/* ============================================================================
 * 测试：诊断功能
 * ============================================================================ */

void test_wdg_get_diagnostic_null_controller(void) {
    EchWdgDiagnostic_t diag;
    EchWdg_GetDiagnostic(NULL, &diag);
    TEST_ASSERT(true, "NULL 控制器应处理");
}

void test_wdg_get_diagnostic_success(void) {
    EchWdgController_t wdg;
    EchWdg_Init(&wdg, 1000);
    
    EchWdg_Feed(&wdg);
    EchWdg_Feed(&wdg);
    
    EchWdgDiagnostic_t diag;
    EchWdg_GetDiagnostic(&wdg, &diag);
    
    TEST_ASSERT(diag.feedCount == 2, "喂狗计数应正确");
    TEST_ASSERT(diag.timeout_ms == 1000, "超时时间应正确");
}

void test_wdg_get_diagnostic_after_reset(void) {
    EchWdgController_t wdg;
    EchWdg_Init(&wdg, 1000);
    
    EchWdg_Feed(&wdg);
    EchWdg_Feed(&wdg);
    EchWdg_Reset(&wdg);
    
    EchWdgDiagnostic_t diag;
    EchWdg_GetDiagnostic(&wdg, &diag);
    
    TEST_ASSERT(diag.feedCount == 0, "喂狗计数应清零");
    TEST_ASSERT(diag.resetCount == 1, "复位计数应正确");
}

/* ============================================================================
 * 测试：版本号
 * ============================================================================ */

void test_wdg_version_not_null(void) {
    uint8_t major, minor, patch;
    EchWdg_GetVersion(&major, &minor, &patch);
    
    TEST_ASSERT(major != 0 || minor != 0 || patch != 0, "版本号不应全为 0");
}

void test_wdg_version_null_params(void) {
    /* 传递 NULL 不应崩溃 */
    EchWdg_GetVersion(NULL, NULL, NULL);
    TEST_ASSERT(true, "NULL 参数应处理");
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

int main(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  ECH ECU 看门狗服务单元测试                              ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    printf("【1. 初始化功能测试】\n");
    test_wdg_init_null_controller();
    test_wdg_init_successful();
    test_wdg_init_parameters_set();
    test_wdg_init_timestamp_set();
    printf("\n");
    
    printf("【2. 喂狗功能测试】\n");
    test_wdg_feed_null_controller();
    test_wdg_feed_uninitialized();
    test_wdg_feed_successful();
    test_wdg_feed_multiple_times();
    printf("\n");
    
    printf("【3. 监控功能测试】\n");
    test_wdg_monitor_null_controller();
    test_wdg_monitor_within_timeout();
    test_wdg_monitor_timeout();
    test_wdg_monitor_time_remaining();
    printf("\n");
    
    printf("【4. 复位功能测试】\n");
    test_wdg_reset_null_controller();
    test_wdg_reset_uninitialized();
    test_wdg_reset_successful();
    printf("\n");
    
    printf("【5. 状态获取测试】\n");
    test_wdg_get_state_null_controller();
    test_wdg_get_state_after_init();
    test_wdg_get_state_after_timeout();
    printf("\n");
    
    printf("【6. 诊断功能测试】\n");
    test_wdg_get_diagnostic_null_controller();
    test_wdg_get_diagnostic_success();
    test_wdg_get_diagnostic_after_reset();
    printf("\n");
    
    printf("【7. 版本号测试】\n");
    test_wdg_version_not_null();
    test_wdg_version_null_params();
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
