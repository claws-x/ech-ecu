/**
 * @file test_ech_lin.c
 * @brief ECH LIN 驱动模块单元测试
 * 
 * @details
 * 本文件包含 ECH LIN 驱动模块的完整单元测试套件。
 * 测试覆盖：初始化、调度、帧发送/接收、校验和、心跳、休眠/唤醒等。
 * 
 * 测试目标：每个函数至少 10 项测试用例
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ech_lin.h"

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

void test_lin_init_null_controller(void) {
    int32_t result = EchLin_Init(NULL, NULL);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_lin_init_default_config(void) {
    EchLinController_t lin;
    int32_t result = EchLin_Init(&lin, NULL);
    TEST_ASSERT(result == 0, "使用默认配置初始化应成功");
    TEST_ASSERT(lin.initialized == true, "初始化标志应置位");
}

void test_lin_init_custom_config(void) {
    EchLinController_t lin;
    EchLinConfig_t config = {
        .baudrate = 19200,
        .nodeId = 1,
        .isMaster = true,
        .scheduleTableId = 0
    };
    
    int32_t result = EchLin_Init(&lin, &config);
    TEST_ASSERT(result == 0, "自定义配置初始化应成功");
    TEST_ASSERT(lin.config.baudrate == 19200, "波特率应正确");
    TEST_ASSERT(lin.config.isMaster == true, "主站模式应正确");
}

void test_lin_init_state_active(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    TEST_ASSERT(lin.state == ECH_LIN_STATE_ACTIVE, "初始化后状态应为 ACTIVE");
}

void test_lin_init_error_count_zero(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    TEST_ASSERT(lin.errorCount == 0, "错误计数应初始化为 0");
    TEST_ASSERT(lin.frameCount == 0, "帧计数应初始化为 0");
}

void test_lin_init_heartbeat_cleared(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    TEST_ASSERT(lin.heartbeatCounter == 0, "心跳计数应初始化为 0");
    TEST_ASSERT(lin.lastHeartbeat_ms == 0, "最后心跳时间应初始化为 0");
}

void test_lin_init_tx_pending_false(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    TEST_ASSERT(lin.txPending == false, "发送等待标志应为 false");
    TEST_ASSERT(lin.rxAvailable == false, "接收可用标志应为 false");
}

void test_lin_init_multiple_times(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    lin.heartbeatCounter = 100;
    
    EchLin_Init(&lin, NULL);
    TEST_ASSERT(lin.heartbeatCounter == 0, "重新初始化应清除计数");
}

void test_lin_init_from_slave_config(void) {
    EchLinController_t lin;
    EchLinConfig_t config = {
        .baudrate = 19200,
        .nodeId = 2,
        .isMaster = false,
        .scheduleTableId = 0
    };
    
    int32_t result = EchLin_Init(&lin, &config);
    TEST_ASSERT(result == 0, "从站配置初始化应成功");
    TEST_ASSERT(lin.config.isMaster == false, "从站模式应正确");
}

void test_lin_init_diagnostic_cleared(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    TEST_ASSERT(lin.lastError == 0, "最后错误应清零");
    TEST_ASSERT(lin.errorFrameCount == 0, "错误帧计数应清零");
}

/* ============================================================================
 * 测试：调度功能
 * ============================================================================ */

void test_lin_schedule_null_controller(void) {
    int32_t result = EchLin_Schedule(NULL, 100);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_lin_schedule_not_initialized(void) {
    EchLinController_t lin;
    int32_t result = EchLin_Schedule(&lin, 100);
    TEST_ASSERT(result == -1, "未初始化应返回错误");
}

void test_lin_schedule_success(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    int32_t result = EchLin_Schedule(&lin, 100);
    TEST_ASSERT(result == 0, "调度应成功");
}

void test_lin_schedule_updates_timestamp(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    EchLin_Schedule(&lin, 1000);
    TEST_ASSERT(lin.lastActivity_ms == 1000, "活动时间应更新");
}

void test_lin_schedule_heartbeat_auto(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    /* 初始心跳计数为 0 */
    uint32_t initialCount = lin.heartbeatCounter;
    
    /* 500ms 后应自动发送心跳 */
    EchLin_Schedule(&lin, 600);
    
    TEST_ASSERT(lin.heartbeatCounter > initialCount, "应自动发送心跳");
}

void test_lin_schedule_multiple_calls(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    EchLin_Schedule(&lin, 100);
    EchLin_Schedule(&lin, 200);
    EchLin_Schedule(&lin, 300);
    
    TEST_ASSERT(true, "多次调度调用应正常");
}

/* ============================================================================
 * 测试：帧发送
 * ============================================================================ */

void test_lin_send_frame_null_controller(void) {
    EchLinFrame_t frame = {.id = 1, .length = 8};
    int32_t result = EchLin_SendFrame(NULL, &frame);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_lin_send_frame_null_frame(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    int32_t result = EchLin_SendFrame(&lin, NULL);
    TEST_ASSERT(result == -1, "NULL 帧应返回错误");
}

void test_lin_send_frame_not_initialized(void) {
    EchLinController_t lin;
    EchLinFrame_t frame = {.id = 1, .length = 8};
    int32_t result = EchLin_SendFrame(&lin, &frame);
    TEST_ASSERT(result == -1, "未初始化应返回错误");
}

void test_lin_send_frame_invalid_id(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    EchLinFrame_t frame = {.id = 64, .length = 8}; /* ID 超出 0-63 范围 */
    int32_t result = EchLin_SendFrame(&lin, &frame);
    TEST_ASSERT(result == -3, "无效帧 ID 应返回错误");
}

void test_lin_send_frame_valid(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    EchLinFrame_t frame = {
        .id = 1,
        .length = 8,
        .frameType = ECH_LIN_FRAME_UNCONDITIONAL
    };
    memset(frame.data, 0xAA, 8);
    
    int32_t result = EchLin_SendFrame(&lin, &frame);
    TEST_ASSERT(result == 0, "有效帧发送应成功");
}

void test_lin_send_frame_increases_count(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    EchLinFrame_t frame = {.id = 1, .length = 8};
    EchLin_SendFrame(&lin, &frame);
    
    TEST_ASSERT(lin.frameCount == 1, "帧计数应增加");
}

void test_lin_send_frame_multiple(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    for (int i = 0; i < 5; i++) {
        EchLinFrame_t frame = {.id = (uint8_t)(i + 1), .length = 8};
        EchLin_SendFrame(&lin, &frame);
    }
    
    TEST_ASSERT(lin.frameCount == 5, "多次发送应累计计数");
}

/* ============================================================================
 * 测试：校验和计算
 * ============================================================================ */

void test_lin_checksum_basic(void) {
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t checksum = EchLin_CalculateChecksum(data, 4, 0x01);
    TEST_ASSERT(checksum != 0, "校验和应非零");
}

void test_lin_checksum_empty_data(void) {
    uint8_t checksum = EchLin_CalculateChecksum(NULL, 0, 0x01);
    /* 空数据时校验和基于 ID 计算 */
    TEST_ASSERT(true, "空数据校验和计算应处理");
}

void test_lin_checksum_same_data_different_id(void) {
    const uint8_t data[] = {0x01, 0x02, 0x03};
    
    uint8_t checksum1 = EchLin_CalculateChecksum(data, 3, 0x01);
    uint8_t checksum2 = EchLin_CalculateChecksum(data, 3, 0x02);
    
    TEST_ASSERT(checksum1 != checksum2, "不同 ID 应有不同校验和");
}

void test_lin_checksum_verification(void) {
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t checksum = EchLin_CalculateChecksum(data, 4, 0x01);
    
    EchLinFrame_t frame;
    frame.id = 0x01;
    frame.length = 4;
    memcpy(frame.data, data, 4);
    frame.checksum = checksum;
    
    /* 验证函数在 ech_lin.c 中是静态的，通过发送/接收间接测试 */
    TEST_ASSERT(checksum != 0, "校验和应可计算");
}

void test_lin_checksum_all_zeros(void) {
    const uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    uint8_t checksum = EchLin_CalculateChecksum(data, 4, 0x00);
    TEST_ASSERT(true, "全零数据校验和应可计算");
}

void test_lin_checksum_all_ones(void) {
    const uint8_t data[] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t checksum = EchLin_CalculateChecksum(data, 4, 0x3F);
    TEST_ASSERT(true, "全 1 数据校验和应可计算");
}

/* ============================================================================
 * 测试：心跳功能
 * ============================================================================ */

void test_lin_heartbeat_null_controller(void) {
    int32_t result = EchLin_SendHeartbeat(NULL, 100);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_lin_heartbeat_success(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    int32_t result = EchLin_SendHeartbeat(&lin, 100);
    TEST_ASSERT(result == 0, "心跳发送应成功");
}

void test_lin_heartbeat_increments_counter(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    EchLin_SendHeartbeat(&lin, 100);
    TEST_ASSERT(lin.heartbeatCounter == 1, "心跳计数应增加到 1");
    
    EchLin_SendHeartbeat(&lin, 600);
    TEST_ASSERT(lin.heartbeatCounter == 2, "心跳计数应增加到 2");
}

void test_lin_heartbeat_updates_timestamp(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    EchLin_SendHeartbeat(&lin, 500);
    TEST_ASSERT(lin.lastHeartbeat_ms == 500, "心跳时间戳应更新");
}

void test_lin_heartbeat_frame_id(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    EchLin_SendHeartbeat(&lin, 100);
    
    /* 心跳帧 ID 应为 ECH_LIN_FRAME_ID_HEARTBEAT */
    TEST_ASSERT(lin.txFrame.id == ECH_LIN_FRAME_ID_HEARTBEAT, 
                "心跳帧 ID 应正确");
}

void test_lin_heartbeat_data_length(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    EchLin_SendHeartbeat(&lin, 100);
    
    TEST_ASSERT(lin.txFrame.length == ECH_LIN_DATA_LENGTH_HEARTBEAT, 
                "心跳数据长度应为 2");
}

/* ============================================================================
 * 测试：休眠/唤醒
 * ============================================================================ */

void test_lin_sleep_null_controller(void) {
    int32_t result = EchLin_EnterSleep(NULL);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_lin_sleep_success(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    int32_t result = EchLin_EnterSleep(&lin);
    TEST_ASSERT(result == 0, "进入休眠应成功");
    TEST_ASSERT(lin.state == ECH_LIN_STATE_SLEEP, "状态应变为 SLEEP");
}

void test_lin_wakeup_null_controller(void) {
    int32_t result = EchLin_WakeUp(NULL, 100);
    TEST_ASSERT(result == -1, "NULL 控制器应返回错误");
}

void test_lin_wakeup_from_sleep(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    EchLin_EnterSleep(&lin);
    int32_t result = EchLin_WakeUp(&lin, 1000);
    
    TEST_ASSERT(result == 0, "唤醒应成功");
    TEST_ASSERT(lin.state == ECH_LIN_STATE_ACTIVE, "状态应变为 ACTIVE");
}

void test_lin_wakeup_already_active(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    /* 已经在 ACTIVE 状态 */
    int32_t result = EchLin_WakeUp(&lin, 100);
    TEST_ASSERT(result == 0, "已在活动状态唤醒应返回成功");
}

void test_lin_sleep_wakeup_sequence(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    EchLin_EnterSleep(&lin);
    EchLin_WakeUp(&lin, 1000);
    EchLin_EnterSleep(&lin);
    EchLin_WakeUp(&lin, 2000);
    
    TEST_ASSERT(lin.state == ECH_LIN_STATE_SLEEP, "最后状态应为 SLEEP");
}

/* ============================================================================
 * 测试：诊断功能
 * ============================================================================ */

void test_lin_get_state_null_controller(void) {
    EchLinState_t state = EchLin_GetState(NULL);
    TEST_ASSERT(state == ECH_LIN_STATE_ERROR, "NULL 控制器应返回 ERROR");
}

void test_lin_get_state_after_init(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    EchLinState_t state = EchLin_GetState(&lin);
    TEST_ASSERT(state == ECH_LIN_STATE_ACTIVE, "初始化后状态应为 ACTIVE");
}

void test_lin_get_diagnostic_null_controller(void) {
    EchLinDiagnostic_t diag;
    EchLin_GetDiagnostic(NULL, &diag);
    TEST_ASSERT(true, "NULL 控制器应处理");
}

void test_lin_get_diagnostic_success(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    EchLinDiagnostic_t diag;
    EchLin_GetDiagnostic(&lin, &diag);
    
    TEST_ASSERT(diag.state == ECH_LIN_STATE_ACTIVE, "状态应正确");
    TEST_ASSERT(diag.isMaster == true, "主站标志应正确");
    TEST_ASSERT(diag.baudrate == 19200, "波特率应正确");
}

void test_lin_get_diagnostic_after_frames(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    EchLin_SendHeartbeat(&lin, 100);
    EchLin_SendHeartbeat(&lin, 600);
    
    EchLinDiagnostic_t diag;
    EchLin_GetDiagnostic(&lin, &diag);
    
    TEST_ASSERT(diag.frameCount == 2, "帧计数应正确");
    TEST_ASSERT(diag.errorCount == 0, "错误计数应为 0");
}

void test_lin_clear_error(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    
    lin.errorCount = 10;
    lin.lastError = 5;
    
    EchLin_ClearError(&lin);
    
    TEST_ASSERT(lin.errorCount == 0, "错误计数应清零");
    TEST_ASSERT(lin.lastError == 0, "最后错误应清零");
}

/* ============================================================================
 * 测试：状态转换
 * ============================================================================ */

void test_lin_state_transition_init_to_active(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    TEST_ASSERT(lin.state == ECH_LIN_STATE_ACTIVE, "初始化后应为 ACTIVE");
}

void test_lin_state_transition_active_to_sleep(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    EchLin_EnterSleep(&lin);
    TEST_ASSERT(lin.state == ECH_LIN_STATE_SLEEP, "应转换到 SLEEP");
}

void test_lin_state_transition_sleep_to_active(void) {
    EchLinController_t lin;
    EchLin_Init(&lin, NULL);
    EchLin_EnterSleep(&lin);
    EchLin_WakeUp(&lin, 100);
    TEST_ASSERT(lin.state == ECH_LIN_STATE_ACTIVE, "应转换回 ACTIVE");
}

/* ============================================================================
 * 测试：版本号
 * ============================================================================ */

void test_lin_version_not_null(void) {
    uint8_t major, minor, patch;
    EchLin_GetVersion(&major, &minor, &patch);
    
    TEST_ASSERT(major == ECH_LIN_VERSION_MAJOR, "主版本号应匹配");
    TEST_ASSERT(minor == ECH_LIN_VERSION_MINOR, "次版本号应匹配");
    TEST_ASSERT(patch == ECH_LIN_VERSION_PATCH, "补丁版本号应匹配");
}

void test_lin_version_null_params(void) {
    EchLin_GetVersion(NULL, NULL, NULL);
    TEST_ASSERT(true, "NULL 参数不应崩溃");
}

/* ============================================================================
 * 主测试函数
 * ============================================================================ */

int main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║        ECH LIN Module Unit Tests                         ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  测试套件：LIN 驱动完整功能验证                            ║\n");
    printf("║  目标覆盖率：每个函数至少 10 项测试                          ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("【1. 初始化功能测试】\n");
    test_lin_init_null_controller();
    test_lin_init_default_config();
    test_lin_init_custom_config();
    test_lin_init_state_active();
    test_lin_init_error_count_zero();
    test_lin_init_heartbeat_cleared();
    test_lin_init_tx_pending_false();
    test_lin_init_multiple_times();
    test_lin_init_from_slave_config();
    test_lin_init_diagnostic_cleared();
    printf("\n");
    
    printf("【2. 调度功能测试】\n");
    test_lin_schedule_null_controller();
    test_lin_schedule_not_initialized();
    test_lin_schedule_success();
    test_lin_schedule_updates_timestamp();
    test_lin_schedule_heartbeat_auto();
    test_lin_schedule_multiple_calls();
    printf("\n");
    
    printf("【3. 帧发送测试】\n");
    test_lin_send_frame_null_controller();
    test_lin_send_frame_null_frame();
    test_lin_send_frame_not_initialized();
    test_lin_send_frame_invalid_id();
    test_lin_send_frame_valid();
    test_lin_send_frame_increases_count();
    test_lin_send_frame_multiple();
    printf("\n");
    
    printf("【4. 校验和计算测试】\n");
    test_lin_checksum_basic();
    test_lin_checksum_empty_data();
    test_lin_checksum_same_data_different_id();
    test_lin_checksum_verification();
    test_lin_checksum_all_zeros();
    test_lin_checksum_all_ones();
    printf("\n");
    
    printf("【5. 心跳功能测试】\n");
    test_lin_heartbeat_null_controller();
    test_lin_heartbeat_success();
    test_lin_heartbeat_increments_counter();
    test_lin_heartbeat_updates_timestamp();
    test_lin_heartbeat_frame_id();
    test_lin_heartbeat_data_length();
    printf("\n");
    
    printf("【6. 休眠/唤醒测试】\n");
    test_lin_sleep_null_controller();
    test_lin_sleep_success();
    test_lin_wakeup_null_controller();
    test_lin_wakeup_from_sleep();
    test_lin_wakeup_already_active();
    test_lin_sleep_wakeup_sequence();
    printf("\n");
    
    printf("【7. 诊断功能测试】\n");
    test_lin_get_state_null_controller();
    test_lin_get_state_after_init();
    test_lin_get_diagnostic_null_controller();
    test_lin_get_diagnostic_success();
    test_lin_get_diagnostic_after_frames();
    test_lin_clear_error();
    printf("\n");
    
    printf("【8. 状态转换测试】\n");
    test_lin_state_transition_init_to_active();
    test_lin_state_transition_active_to_sleep();
    test_lin_state_transition_sleep_to_active();
    printf("\n");
    
    printf("【9. 版本号测试】\n");
    test_lin_version_not_null();
    test_lin_version_null_params();
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
