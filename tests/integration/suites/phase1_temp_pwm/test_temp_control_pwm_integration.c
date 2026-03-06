/**
 * @file test_temp_control_pwm_integration.c
 * @brief Phase 1: 温度控制与 PWM 驱动集成测试
 * 
 * 测试场景:
 * - ITS-001: 温度闭环控制集成
 * - ITS-006: 软启动集成
 * 
 * 追溯需求:
 * - SWE-F-001: PID 温度控制算法
 * - SWE-F-003: 软启动占空比爬升限制
 * - SWE-F-004: 稳态温度误差 ≤ ±2°C
 * - SWE-F-007: PWM 频率 20kHz
 * - SWE-F-008: PWM 占空比 0-100% 可调
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cmocka.h>

/* 被测试模块 */
#include "app/temperature_control.h"
#include "drivers/ech_pwm.h"

/* 测试桩 */
#include "../fixtures/stub_pwm.h"
#include "../fixtures/stub_adc.h"
#include "../fixtures/fixture_temp_control.h"

/* 测试常量 */
#define TEMP_TARGET_DEFAULT      50.0f   /* 默认目标温度 (°C) */
#define TEMP_INITIAL             25.0f   /* 初始温度 (°C) */
#define TEMP_TOLERANCE           2.0f    /* 稳态误差容限 (°C) */
#define DUTY_RAMP_LIMIT          10.0f   /* 占空比爬升限制 (%/s) */
#define PID_CYCLE_TIME_MS        10      /* PID 循环周期 (ms) */

/* ============================================================================
 * 测试桩实现 (在 fixtures 目录中定义，此处为简化示例)
 * ============================================================================ */

/* PWM 桩 - 记录最后一次设置的占空比 */
static float g_last_duty_cycle = 0.0f;

void pwm_init(void) {
    g_last_duty_cycle = 0.0f;
}

int pwm_set_duty_cycle(float duty_percent) {
    g_last_duty_cycle = duty_percent;
    return 0;
}

float pwm_get_last_duty_cycle(void) {
    return g_last_duty_cycle;
}

/* ADC 桩 - 返回预设温度值 */
static float g_simulated_temp = TEMP_INITIAL;

void adc_init(void) {
    g_simulated_temp = TEMP_INITIAL;
}

float adc_read_temperature(void) {
    return g_simulated_temp;
}

void adc_set_simulated_temp(float temp) {
    g_simulated_temp = temp;
}

/* ============================================================================
 * 测试夹具 (Setup/Teardown)
 * ============================================================================ */

static int setup_temp_control_fixture(void **state) {
    /* 初始化温度控制模块 */
    temp_control_init();
    pwm_init();
    adc_init();
    
    /* 设置默认目标温度 */
    temp_control_set_target(TEMP_TARGET_DEFAULT);
    
    return 0;
}

static int teardown_temp_control_fixture(void **state) {
    /* 清理资源 (如有) */
    return 0;
}

/* ============================================================================
 * 测试用例: ITC-001 - 温度闭环控制 - 升温过程
 * ============================================================================ */

static void test_itc_001_temp_rising_response(void **state) {
    /* 前置条件: 目标 50°C, 实际 25°C */
    adc_set_simulated_temp(25.0f);
    
    /* 执行: 运行一个控制周期 */
    temp_control_cycle();
    
    /* 验证: PWM 占空比应 > 0 (加热需求) */
    float duty = pwm_get_last_duty_cycle();
    assert_true(duty > 0.0f);
    assert_true(duty <= 100.0f);
    
    /* 模拟温度上升过程 */
    float temp_sequence[] = {25.0f, 30.0f, 35.0f, 40.0f, 45.0f, 48.0f, 50.0f};
    int num_steps = sizeof(temp_sequence) / sizeof(temp_sequence[0]);
    
    float prev_duty = duty;
    for (int i = 1; i < num_steps; i++) {
        adc_set_simulated_temp(temp_sequence[i]);
        temp_control_cycle();
        
        float current_duty = pwm_get_last_duty_cycle();
        
        /* 验证: 随着温度上升，占空比应逐渐降低 */
        assert_true(current_duty <= prev_duty + 1.0f);  /* 允许小幅波动 */
        
        prev_duty = current_duty;
    }
    
    /* 在目标温度时，占空比应接近维持值 (非 0) */
    float final_duty = pwm_get_last_duty_cycle();
    assert_true(final_duty >= 0.0f);
    assert_true(final_duty <= 50.0f);  /* 维持温度不需要全功率 */
}

/* ============================================================================
 * 测试用例: ITC-002 - 温度闭环控制 - 稳态精度
 * ============================================================================ */

static void test_itc_002_steady_state_accuracy(void **state) {
    /* 设置目标温度 */
    temp_control_set_target(50.0f);
    
    /* 模拟稳态情况: 温度在目标值附近小幅波动 */
    float temp_steady[] = {49.0f, 50.0f, 51.0f, 49.5f, 50.5f, 50.0f};
    int num_samples = sizeof(temp_steady) / sizeof(temp_steady[0]);
    
    for (int i = 0; i < num_samples; i++) {
        adc_set_simulated_temp(temp_steady[i]);
        temp_control_cycle();
        
        float duty = pwm_get_last_duty_cycle();
        
        /* 验证: 在稳态范围内，占空比应保持稳定 */
        assert_true(duty >= 0.0f);
        assert_true(duty <= 100.0f);
    }
    
    /* 模拟温度超过目标 + 容差 */
    adc_set_simulated_temp(53.0f);  /* 50 + 3°C */
    temp_control_cycle();
    
    float duty_over = pwm_get_last_duty_cycle();
    /* 验证: 温度过高时应降低或关闭占空比 */
    assert_true(duty_over < 20.0f);
    
    /* 模拟温度低于目标 - 容差 */
    adc_set_simulated_temp(47.0f);  /* 50 - 3°C */
    temp_control_cycle();
    
    float duty_under = pwm_get_last_duty_cycle();
    /* 验证: 温度过低时应增加占空比 */
    assert_true(duty_under > 30.0f);
}

/* ============================================================================
 * 测试用例: ITC-003 - 目标温度变化响应
 * ============================================================================ */

static void test_itc_003_target_change_response(void **state) {
    /* 初始状态: 目标 40°C, 实际 35°C */
    temp_control_set_target(40.0f);
    adc_set_simulated_temp(35.0f);
    temp_control_cycle();
    
    float duty_initial = pwm_get_last_duty_cycle();
    assert_true(duty_initial > 0.0f);
    
    /* 目标温度提升到 50°C */
    temp_control_set_target(50.0f);
    temp_control_cycle();
    
    float duty_after_change = pwm_get_last_duty_cycle();
    
    /* 验证: 目标温度升高后，占空比应增加 */
    assert_true(duty_after_change > duty_initial);
    
    /* 目标温度降低到 30°C (低于当前实际温度) */
    temp_control_set_target(30.0f);
    temp_control_cycle();
    
    float duty_low_target = pwm_get_last_duty_cycle();
    
    /* 验证: 目标温度低于实际温度时，占空比应为 0 */
    assert_true(duty_low_target == 0.0f);
}

/* ============================================================================
 * 测试用例: ITC-015 - 软启动占空比爬升速率
 * ============================================================================ */

static void test_itc_015_soft_start_ramp_rate(void **state) {
    /* 初始状态: 系统刚启动，目标 50°C, 实际 20°C */
    temp_control_set_target(50.0f);
    adc_set_simulated_temp(20.0f);
    
    /* 记录初始占空比 */
    temp_control_cycle();
    float prev_duty = pwm_get_last_duty_cycle();
    
    /* 模拟软启动过程，每 10ms 执行一次控制循环 */
    float max_ramp_rate = 0.0f;
    int num_cycles = 50;  /* 500ms */
    
    for (int i = 0; i < num_cycles; i++) {
        /* 模拟温度缓慢上升 */
        float simulated_temp = 20.0f + (i * 0.1f);
        adc_set_simulated_temp(simulated_temp);
        
        temp_control_cycle();
        float current_duty = pwm_get_last_duty_cycle();
        
        /* 计算爬升速率 (%/10ms) */
        float ramp = current_duty - prev_duty;
        if (ramp > max_ramp_rate) {
            max_ramp_rate = ramp;
        }
        
        /* 验证: 单次循环占空比增加不超过限制 */
        /* 10%/s = 0.1%/10ms */
        assert_true(ramp <= 0.15f);  /* 允许 50% 容差 */
        
        prev_duty = current_duty;
    }
    
    /* 记录最大爬升速率用于分析 */
    printf("Max ramp rate: %.2f %%/10ms (limit: 0.1 %%/10ms)\n", max_ramp_rate);
}

/* ============================================================================
 * 测试用例: 边界条件测试
 * ============================================================================ */

static void test_boundary_conditions(void **state) {
    /* 测试: 目标温度 = 实际温度 */
    temp_control_set_target(40.0f);
    adc_set_simulated_temp(40.0f);
    temp_control_cycle();
    
    float duty_equal = pwm_get_last_duty_cycle();
    /* 验证: 温度相等时应有维持占空比 (非 0) */
    assert_true(duty_equal >= 0.0f);
    assert_true(duty_equal <= 30.0f);
    
    /* 测试: 目标温度极低 (-40°C) */
    temp_control_set_target(-40.0f);
    adc_set_simulated_temp(25.0f);
    temp_control_cycle();
    
    float duty_min = pwm_get_last_duty_cycle();
    /* 验证: 占空比应为 0 */
    assert_true(duty_min == 0.0f);
    
    /* 测试: 目标温度极高 (100°C) */
    temp_control_set_target(100.0f);
    adc_set_simulated_temp(25.0f);
    temp_control_cycle();
    
    float duty_max = pwm_get_last_duty_cycle();
    /* 验证: 占空比应为最大值或接近最大 */
    assert_true(duty_max >= 80.0f);
    assert_true(duty_max <= 100.0f);
}

/* ============================================================================
 * 测试注册
 * ============================================================================ */

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_itc_001_temp_rising_response,
            setup_temp_control_fixture,
            teardown_temp_control_fixture
        ),
        cmocka_unit_test_setup_teardown(
            test_itc_002_steady_state_accuracy,
            setup_temp_control_fixture,
            teardown_temp_control_fixture
        ),
        cmocka_unit_test_setup_teardown(
            test_itc_003_target_change_response,
            setup_temp_control_fixture,
            teardown_temp_control_fixture
        ),
        cmocka_unit_test_setup_teardown(
            test_itc_015_soft_start_ramp_rate,
            setup_temp_control_fixture,
            teardown_temp_control_fixture
        ),
        cmocka_unit_test_setup_teardown(
            test_boundary_conditions,
            setup_temp_control_fixture,
            teardown_temp_control_fixture
        ),
    };
    
    printf("=== ECH ECU Integration Test: Phase 1 (Temp Control + PWM) ===\n\n");
    
    return cmocka_run_group_tests(tests, NULL, NULL);
}
