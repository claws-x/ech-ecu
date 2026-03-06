/**
 * @file fixture_temp_control.h
 * @brief 温度控制模块测试夹具头文件
 */

#ifndef FIXTURE_TEMP_CONTROL_H
#define FIXTURE_TEMP_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化温度控制模块
 */
void temp_control_init(void);

/**
 * @brief 设置目标温度
 * @param temp 目标温度 (°C)
 */
void temp_control_set_target(float temp);

/**
 * @brief 获取目标温度
 * @return 目标温度 (°C)
 */
float temp_control_get_target(void);

/**
 * @brief 执行温度控制循环
 */
void temp_control_cycle(void);

/**
 * @brief 获取当前占空比输出
 * @return 占空比 (0-100%)
 */
float temp_control_get_duty_output(void);

/**
 * @brief 复位积分器
 */
void temp_control_reset_integrator(void);

/**
 * @brief 设置 PID 参数
 * @param kp 比例增益
 * @param ki 积分增益
 * @param kd 微分增益
 */
void temp_control_set_pid_params(float kp, float ki, float kd);

#ifdef __cplusplus
}
#endif

#endif /* FIXTURE_TEMP_CONTROL_H */
