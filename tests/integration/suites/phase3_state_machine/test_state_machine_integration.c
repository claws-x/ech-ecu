/**
 * @file test_state_machine_integration.c
 * @brief Phase 3: 状态机集成测试
 * 
 * 测试场景:
 * - ITS-002: 状态机与使能控制集成
 * - ITS-006: 软启动集成
 * - ITS-007: 水泵联动控制
 * 
 * 追溯需求:
 * - SWE-F-005: STANDBY 状态 PWM 输出 0%
 * - SWE-F-006: RUNNING 状态 PWM 输出 PID 计算值
 * - SWE-F-014: 状态机四状态实现
 * - SWE-F-015: STANDBY → RUNNING 转换
 * - SWE-F-016: 故障状态转换
 * - SWE-F-017: STANDBY → MAINTENANCE 转换
 * - SWE-F-013: RUNNING 状态水泵 ON
 * - SWE-F-014: 退出 RUNNING 状态水泵 OFF
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmocka.h>

/* 被测试模块 */
#include "app/state_machine.h"
#include "app/temperature_control.h"
#include "drivers/ech_pwm.h"
#include "drivers/ech_dio.h"

/* 测试桩 */
#include "../fixtures/stub_pwm.h"
#include "../fixtures/stub_dio.h"
#include "../fixtures/fixture_state_machine.h"

/* 状态定义 (与 state_machine.h 一致) */
typedef enum {
    STATE_STANDBY = 0,
    STATE_RUNNING = 1,
    STATE_FAULT = 2,
    STATE_MAINTENANCE = 3
} system_state_t;

/* 测试常量 */
#define STATE_TRANSITION_TIMEOUT_MS  50    /* 状态转换超时 (ms) */

/* ============================================================================
 * 测试桩实现
 * ============================================================================ */

/* PWM 桩 */
static float g_pwm_duty = 0.0f;
static int g_pwm_init_called = 0;

void pwm_init(void) {
    g_pwm_duty = 0.0f;
    g_pwm_init_called = 1;
}

int pwm_set_duty_cycle(float duty) {
    g_pwm_duty = duty;
    return 0;
}

float pwm_get_current_duty(void) {
    return g_pwm_duty;
}

/* DIO 桩 - 水泵继电器 */
static int g_pump_relay_state = 0;  /* 0=OFF, 1=ON */

void dio_init(void) {
    g_pump_relay_state = 0;
}

int dio_set_pump_relay(int state) {
    g_pump_relay_state = state;
    return 0;
}

int dio_get_pump_relay_state(void) {
    return g_pump_relay_state;
}

/* 温度控制桩 */
static float g_temp_target = 25.0f;

void temp_control_init(void) {
    g_temp_target = 25.0f;
}

void temp_control_set_target(float temp) {
    g_temp_target = temp;
}

void temp_control_cycle(void) {
    /* 简化实现 */
}

/* ============================================================================
 * 测试夹具
 * ============================================================================ */

static int setup_state_machine_fixture(void **state) {
    /* 初始化所有模块 */
    state_machine_init();
    pwm_init();
    dio_init();
    temp_control_init();
    
    return 0;
}

static int teardown_state_machine_fixture(void **state) {
    return 0;
}

/* ============================================================================
 * 测试用例: ITC-004 - 状态机 STANDBY 到 RUNNING 转换
 * ============================================================================ */

static void test_itc_004_standby_to_running(void **state) {
    /* 前置条件: 初始状态应为 STANDBY */
    system_state_t initial_state = state_machine_get_current_state();
    assert_int_equal(initial_state, STATE_STANDBY);
    
    /* 验证: STANDBY 状态下 PWM 为 0% */
    float standby_duty = pwm_get_current_duty();
    assert_true(standby_duty == 0.0f);
    
    /* 验证: STANDBY 状态下水泵为 OFF */
    int standby_pump = dio_get_pump_relay_state();
    assert_int_equal(standby_pump, 0);
    
    /* 执行: 发送使能请求 */
    state_machine_enable_request(1);
    state_machine_cycle();  /* 执行状态机循环 */
    
    /* 验证: 状态转换为 RUNNING */
    system_state_t new_state = state_machine_get_current_state();
    assert_int_equal(new_state, STATE_RUNNING);
    
    /* 验证: RUNNING 状态下水泵为 ON */
    int running_pump = dio_get_pump_relay_state();
    assert_int_equal(running_pump, 1);
}

/* ============================================================================
 * 测试用例: ITC-005 - 状态机 RUNNING 到 STANDBY 转换
 * ============================================================================ */

static void test_itc_005_running_to_standby(void **state) {
    /* 前置条件: 系统处于 RUNNING 状态 */
    state_machine_enable_request(1);
    state_machine_cycle();
    
    system_state_t state = state_machine_get_current_state();
    assert_int_equal(state, STATE_RUNNING);
    
    /* 执行: 发送禁用请求 */
    state_machine_enable_request(0);
    state_machine_cycle();
    
    /* 验证: 状态转换回 STANDBY */
    state = state_machine_get_current_state();
    assert_int_equal(state, STATE_STANDBY);
    
    /* 验证: 水泵关闭 */
    int pump_state = dio_get_pump_relay_state();
    assert_int_equal(pump_state, 0);
}

/* ============================================================================
 * 测试用例: ITC-006 - 状态与 PWM 同步
 * ============================================================================ */

static void test_itc_006_state_pwm_synchronization(void **state) {
    /* 测试 STANDBY → PWM 0% */
    system_state_t state = state_machine_get_current_state();
    assert_int_equal(state, STATE_STANDBY);
    
    float duty = pwm_get_current_duty();
    assert_true(duty == 0.0f);
    
    /* 转换到 RUNNING */
    state_machine_enable_request(1);
    state_machine_cycle();
    
    state = state_machine_get_current_state();
    assert_int_equal(state, STATE_RUNNING);
    
    /* 模拟温度控制需要 50% 占空比 */
    temp_control_set_target(50.0f);
    temp_control_cycle();
    
    /* 验证: RUNNING 状态下 PWM 应为 PID 计算值 (非 0) */
    duty = pwm_get_current_duty();
    assert_true(duty > 0.0f);
    
    /* 转换回 STANDBY */
    state_machine_enable_request(0);
    state_machine_cycle();
    
    state = state_machine_get_current_state();
    assert_int_equal(state, STATE_STANDBY);
    
    /* 验证: STANDBY 状态下 PWM 应立即回到 0% */
    duty = pwm_get_current_duty();
    assert_true(duty == 0.0f);
}

/* ============================================================================
 * 测试用例: 故障状态转换
 * ============================================================================ */

static void test_fault_state_transition(void **state) {
    /* 前置条件: 系统运行中 */
    state_machine_enable_request(1);
    state_machine_cycle();
    
    system_state_t state = state_machine_get_current_state();
    assert_int_equal(state, STATE_RUNNING);
    
    /* 执行: 注入故障 (过温) */
    state_machine_inject_fault(FAULT_OVERTEMP);
    state_machine_cycle();
    
    /* 验证: 状态转换为 FAULT */
    state = state_machine_get_current_state();
    assert_int_equal(state, STATE_FAULT);
    
    /* 验证: PWM 切断 */
    float duty = pwm_get_current_duty();
    assert_true(duty == 0.0f);
    
    /* 验证: 水泵关闭 */
    int pump = dio_get_pump_relay_state();
    assert_int_equal(pump, 0);
    
    /* 验证: 故障状态下使能请求无效 */
    state_machine_enable_request(1);
    state_machine_cycle();
    
    state = state_machine_get_current_state();
    assert_int_equal(state, STATE_FAULT);  /* 应保持故障状态 */
}

/* ============================================================================
 * 测试用例: 维护状态转换
 * ============================================================================ */

static void test_maintenance_state_transition(void **state) {
    /* 前置条件: 系统处于 STANDBY */
    system_state_t state = state_machine_get_current_state();
    assert_int_equal(state, STATE_STANDBY);
    
    /* 执行: 发送维护请求 */
    state_machine_maintenance_request(1);
    state_machine_cycle();
    
    /* 验证: 状态转换为 MAINTENANCE */
    state = state_machine_get_current_state();
    assert_int_equal(state, STATE_MAINTENANCE);
    
    /* 验证: PWM 为 0% */
    float duty = pwm_get_current_duty();
    assert_true(duty == 0.0f);
    
    /* 验证: 水泵关闭 */
    int pump = dio_get_pump_relay_state();
    assert_int_equal(pump, 0);
    
    /* 清除维护请求 */
    state_machine_maintenance_request(0);
    state_machine_cycle();
    
    /* 验证: 返回 STANDBY */
    state = state_machine_get_current_state();
    assert_int_equal(state, STATE_STANDBY);
}

/* ============================================================================
 * 测试用例: 水泵联动控制 (ITC-016)
 * ============================================================================ */

static void test_itc_016_pump_synchronization(void **state) {
    /* 初始状态: STANDBY, 水泵 OFF */
    int pump_state = dio_get_pump_relay_state();
    assert_int_equal(pump_state, 0);
    
    /* 进入 RUNNING */
    state_machine_enable_request(1);
    state_machine_cycle();
    
    /* 验证: 水泵 ON */
    pump_state = dio_get_pump_relay_state();
    assert_int_equal(pump_state, 1);
    
    /* 模拟运行一段时间 */
    for (int i = 0; i < 10; i++) {
        state_machine_cycle();
        pump_state = dio_get_pump_relay_state();
        assert_int_equal(pump_state, 1);  /* 应保持 ON */
    }
    
    /* 退出 RUNNING */
    state_machine_enable_request(0);
    state_machine_cycle();
    
    /* 验证: 水泵 OFF */
    pump_state = dio_get_pump_relay_state();
    assert_int_equal(pump_state, 0);
}

/* ============================================================================
 * 测试用例: 状态转换响应时间
 * ============================================================================ */

static void test_state_transition_timing(void **state) {
    /* 测量 STANDBY → RUNNING 转换时间 */
    unsigned long start_time = state_machine_get_time_ms();
    
    state_machine_enable_request(1);
    state_machine_cycle();
    
    system_state_t state = state_machine_get_current_state();
    unsigned long end_time = state_machine_get_time_ms();
    
    unsigned long transition_time = end_time - start_time;
    
    /* 验证: 转换时间 ≤ 50ms */
    assert_true(transition_time <= STATE_TRANSITION_TIMEOUT_MS);
    
    printf("STANDBY→RUNNING transition time: %lu ms (limit: %d ms)\n", 
           transition_time, STATE_TRANSITION_TIMEOUT_MS);
}

/* ============================================================================
 * 测试注册
 * ============================================================================ */

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_itc_004_standby_to_running,
            setup_state_machine_fixture,
            teardown_state_machine_fixture
        ),
        cmocka_unit_test_setup_teardown(
            test_itc_005_running_to_standby,
            setup_state_machine_fixture,
            teardown_state_machine_fixture
        ),
        cmocka_unit_test_setup_teardown(
            test_itc_006_state_pwm_synchronization,
            setup_state_machine_fixture,
            teardown_state_machine_fixture
        ),
        cmocka_unit_test_setup_teardown(
            test_fault_state_transition,
            setup_state_machine_fixture,
            teardown_state_machine_fixture
        ),
        cmocka_unit_test_setup_teardown(
            test_maintenance_state_transition,
            setup_state_machine_fixture,
            teardown_state_machine_fixture
        ),
        cmocka_unit_test_setup_teardown(
            test_itc_016_pump_synchronization,
            setup_state_machine_fixture,
            teardown_state_machine_fixture
        ),
        cmocka_unit_test_setup_teardown(
            test_state_transition_timing,
            setup_state_machine_fixture,
            teardown_state_machine_fixture
        ),
    };
    
    printf("=== ECH ECU Integration Test: Phase 3 (State Machine) ===\n\n");
    
    return cmocka_run_group_tests(tests, NULL, NULL);
}
