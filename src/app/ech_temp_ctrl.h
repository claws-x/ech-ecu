/**
 * @file ech_temp_ctrl.h
 * @brief ECH 水加热器 ECU - 温度控制模块头文件
 */

#ifndef ECH_TEMP_CTRL_H
#define ECH_TEMP_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* 温度控制配置 */
typedef struct {
    float targetTemp;
    float hysteresis;
    bool enabled;
} EchTempCtrlConfig_t;

/* 初始化温度控制 */
int32_t EchTempCtrl_Init(const EchTempCtrlConfig_t* config);

/* 更新温度控制 */
float EchTempCtrl_Update(float currentTemp, float targetTemp, uint32_t timestamp_ms);

/* 获取版本 */
void EchTempCtrl_GetVersion(uint8_t* major, uint8_t* minor, uint8_t* patch);

#ifdef __cplusplus
}
#endif

#endif /* ECH_TEMP_CTRL_H */
