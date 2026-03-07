/**
 * @file test_ech_diag.c
 * @brief 诊断服务单元测试
 * @version 0.1
 * @date 2026-03-07
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* 包含被测模块 */
#include "../services/ech_diag.c"

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

void test_diag_init_null_controller(void) {
    int32_t result = EchDiag_Init(NULL, NULL);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_diag_init_successful(void) {
    EchDiagController_t diag;
    int32_t result = EchDiag_Init(&diag, NULL);
    
    TEST_ASSERT(result == 0, "初始化应成功");
    TEST_ASSERT(diag.initialized == true, "初始化标志应置位");
}

void test_diag_init_parameters_set(void) {
    EchDiagController_t diag;
    EchDiag_Init(&diag, NULL);
    
    TEST_ASSERT(diag.dtcCount == 0, "DTC 计数应清零");
    TEST_ASSERT(diag.requestCount == 0, "请求计数应清零");
}

/* ============================================================================
 * 测试：DTC 管理
 * ============================================================================ */

void test_diag_set_dtc_null_controller(void) {
    int32_t result = EchDiag_SetDtc(NULL, ECH_DTC_P0001, NULL);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_diag_set_dtc_successful(void) {
    EchDiagController_t diag;
    EchDiag_Init(&diag, NULL);
    
    uint8_t freezeFrame[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    int32_t result = EchDiag_SetDtc(&diag, ECH_DTC_P0001, freezeFrame);
    
    TEST_ASSERT(result == 0, "设置 DTC 应成功");
    TEST_ASSERT(diag.dtcCount == 1, "DTC 计数应增加");
}

void test_diag_get_dtc_record_null_controller(void) {
    const EchDtcRecord_t* record = EchDiag_GetDtcRecord(NULL, ECH_DTC_P0001);
    TEST_ASSERT(record == NULL, "NULL 控制器应返回 NULL");
}

void test_diag_clear_dtc_null_controller(void) {
    int32_t result = EchDiag_ClearDtc(NULL, ECH_DTC_P0001);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_diag_clear_dtc_successful(void) {
    EchDiagController_t diag;
    EchDiag_Init(&diag, NULL);
    
    EchDiag_SetDtc(&diag, ECH_DTC_P0001, NULL);
    EchDiag_SetDtc(&diag, ECH_DTC_P0002, NULL);
    
    int32_t result = EchDiag_ClearDtc(&diag, ECH_DTC_P0001);
    
    TEST_ASSERT(result == 0, "清除 DTC 应成功");
    TEST_ASSERT(diag.dtcCount == 1, "DTC 计数应减少");
}

void test_diag_clear_all_dtc(void) {
    EchDiagController_t diag;
    EchDiag_Init(&diag, NULL);
    
    EchDiag_SetDtc(&diag, ECH_DTC_P0001, NULL);
    EchDiag_SetDtc(&diag, ECH_DTC_P0002, NULL);
    
    int32_t result = EchDiag_ClearAllDtc(&diag);
    
    TEST_ASSERT(result == 0, "清除所有 DTC 应成功");
    TEST_ASSERT(diag.dtcCount == 0, "DTC 计数应清零");
}

/* ============================================================================
 * 测试：UDS 请求处理
 * ============================================================================ */

void test_diag_process_request_null_controller(void) {
    EchDiagResponse_t response;
    uint8_t request[3] = {0x10, 0x01};
    int32_t result = EchDiag_ProcessRequest(NULL, request, 2, &response, 100);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_diag_process_request_session_control(void) {
    EchDiagController_t diag;
    EchDiag_Init(&diag, NULL);
    
    uint8_t request[2] = {0x10, 0x01}; /* 会话控制 */
    EchDiagResponse_t response;
    int32_t result = EchDiag_ProcessRequest(&diag, request, 2, &response, 100);
    
    TEST_ASSERT(result == 0, "处理请求应成功");
    TEST_ASSERT(response.isPositive == true, "响应应为肯定");
    TEST_ASSERT(response.serviceId == 0x50, "响应 SID 应正确");
}

void test_diag_process_request_read_did(void) {
    EchDiagController_t diag;
    EchDiag_Init(&diag, NULL);
    
    uint8_t request[3] = {0x22, 0xF1, 0x00}; /* 读 DID 0xF100 */
    EchDiagResponse_t response;
    int32_t result = EchDiag_ProcessRequest(&diag, request, 3, &response, 100);
    
    TEST_ASSERT(result == 0, "处理请求应成功");
    TEST_ASSERT(response.isPositive == true, "响应应为肯定");
}

/* ============================================================================
 * 测试：诊断信息获取
 * ============================================================================ */

void test_diag_get_diagnostic_null_controller(void) {
    EchDiag_GetDiagnostic(NULL, NULL, NULL, NULL);
    TEST_ASSERT(true, "NULL 控制器应处理");
}

void test_diag_get_diagnostic_success(void) {
    EchDiagController_t diag;
    EchDiag_Init(&diag, NULL);
    
    EchDiag_SetDtc(&diag, ECH_DTC_P0001, NULL);
    
    uint8_t request[2] = {0x10, 0x01};
    EchDiagResponse_t response;
    EchDiag_ProcessRequest(&diag, request, 2, &response, 100);
    
    uint32_t requestCount = 0, errorCount = 0;
    uint8_t dtcCount = 0;
    EchDiag_GetDiagnostic(&diag, &requestCount, &errorCount, &dtcCount);
    
    TEST_ASSERT(requestCount == 1, "请求计数应正确");
    TEST_ASSERT(dtcCount == 1, "DTC 计数应正确");
}

/* ============================================================================
 * 测试：版本号
 * ============================================================================ */

void test_diag_version_not_null(void) {
    uint8_t major, minor, patch;
    EchDiag_GetVersion(&major, &minor, &patch);
    
    TEST_ASSERT(major != 0 || minor != 0 || patch != 0, "版本号不应全为 0");
}

void test_diag_version_null_params(void) {
    EchDiag_GetVersion(NULL, NULL, NULL);
    TEST_ASSERT(true, "NULL 参数应处理");
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

int main(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  ECH ECU 诊断服务单元测试                                ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    printf("【1. 初始化功能测试】\n");
    test_diag_init_null_controller();
    test_diag_init_successful();
    test_diag_init_parameters_set();
    printf("\n");
    
    printf("【2. DTC 管理测试】\n");
    test_diag_set_dtc_null_controller();
    test_diag_set_dtc_successful();
    test_diag_get_dtc_record_null_controller();
    test_diag_clear_dtc_null_controller();
    test_diag_clear_dtc_successful();
    test_diag_clear_all_dtc();
    printf("\n");
    
    printf("【3. UDS 请求处理测试】\n");
    test_diag_process_request_null_controller();
    test_diag_process_request_session_control();
    test_diag_process_request_read_did();
    printf("\n");
    
    printf("【4. 诊断信息获取测试】\n");
    test_diag_get_diagnostic_null_controller();
    test_diag_get_diagnostic_success();
    printf("\n");
    
    printf("【5. 版本号测试】\n");
    test_diag_version_not_null();
    test_diag_version_null_params();
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
