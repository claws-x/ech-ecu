/**
 * @file fixture_temp_control.c
 * @brief 温度控制模块测试夹具
 * 
 * 提供温度控制模块集成测试所需的辅助函数和初始化例程。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fixture_temp_control.h"

/* 温度控制模块测试桩实现 */

/* PID 参数 (示例值，实际值需根据系统标定) */
#define KP_DEFAULT    2.0f
#define KI_DEFAULT    0.5f
#define KD_DEFAULT    0.1f

/* 内部状态 */
typedef struct {
    float target_temp;
    float integral;
    float prev_error;
    float duty_output;
} temp_control_state_t;

static temp_control_state_t g_temp_ctrl = {0};

/**
 * @brief 初始化温度控制模块
 */
void temp_control_init(void) {
    memset(&g_temp_ctrl, 0, sizeof(g_temp_ctrl));
    printf("[TempControl] Initialized\n");
}

/**
 * @brief 设置目标温度
 * @param temp 目标温度 (°C)
 */
void temp_control_set_target(float temp) {
    /* 限制目标温度范围 */
    if (temp < -40.0f) temp = -40.0f;
    if (temp > 100.0f) temp = 100.0f;
    
    g_temp_ctrl.target_temp = temp;
    printf("[TempControl] Target set to %.1f°C\n", temp);
}

/**
 * @brief 获取目标温度
 * @return 目标温度 (°C)
 */
float temp_control_get_target(void) {
    return g_temp_ctrl.target_temp;
}

/**
 * @brief 执行温度控制循环 (PID 计算)
 * 
 * 简化实现，实际应读取 ADC 温度值并计算 PID 输出。
 */
void temp_control_cycle(void) {
    /* 简化实现：返回固定占空比 */
    /* 实际实现应包含完整的 PID 算法 */
    g_temp_ctrl.duty_output = 50.0f;  /* 示例值 */
}

/**
 * @brief 获取当前占空比输出
 * @return 占空比 (0-100%)
 */
float temp_control_get_duty_output(void) {
    return g_temp_ctrl.duty_output;
}

/**
 * @brief 复位积分器 (用于测试)
 */
void temp_control_reset_integrator(void) {
    g_temp_ctrl.integral = 0.0f;
}

/**
 * @brief 设置 PID 参数 (用于测试调参)
 * @param kp 比例增益
 * @param ki 积分增益
 * @param kd 微分增益
 */
void temp_control_set_pid_params(float kp, float ki, float kd) {
    printf("[TempControl] PID params: Kp=%.2f, Ki=%.2f, Kd=%.2f\n", kp, ki, kd);
    /* 实际实现应保存参数 */
}
