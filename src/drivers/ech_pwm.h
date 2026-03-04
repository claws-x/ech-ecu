/**
 * @file ech_pwm.h
 * @brief PTC 加热器 PWM 驱动头文件
 * @process SWE.3 软件详细设计与单元实现
 * @version 0.1
 * @date 2026-03-02
 */

#ifndef ECH_PWM_H
#define ECH_PWM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 类型定义
 *============================================================================*/

/**
 * @brief PWM 初始化配置结构
 */
typedef struct {
  uint32_t frequency_hz; ///< PWM 频率 (Hz)，推荐 20000
  uint8_t channel;       ///< PWM 通道号 (0-3)
} PWM_Config_t;

/*============================================================================
 * 公有函数接口
 *============================================================================*/

/**
 * @brief 初始化 PWM 驱动
 * @param config PWM 配置参数
 * @return true 成功，false 失败
 *
 * @pre 系统时钟已初始化
 * @post PWM 模块处于禁用状态，占空比为 0%
 */
bool PWM_Init(const PWM_Config_t *config);

/**
 * @brief 设置 PWM 占空比
 * @param duty_percent 占空比百分比 (0.0 - 100.0)
 * @return true 成功，false 失败
 *
 * @note 超出范围的值会被自动钳位到 0-100
 */
bool PWM_SetDuty(float duty_percent);

/**
 * @brief 获取当前占空比
 * @return 当前占空比百分比 (0.0 - 100.0)
 */
float PWM_GetDuty(void);

/**
 * @brief 禁用 PWM 输出
 *
 * @note 禁用后 PWM 引脚输出低电平
 */
void PWM_Disable(void);

/**
 * @brief 启用 PWM 输出
 *
 * @pre PWM_Init 已成功调用
 */
void PWM_Enable(void);

/**
 * @brief 运行所有单元测试（仅测试模式）
 */
#ifdef UNIT_TEST
void PWM_UT_RunAll(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // ECH_PWM_H
