# ECH 水加热器 ECU 软件架构设计

**文档版本**: 0.1  
**创建日期**: 2026-03-02  
**过程域**: SWE.2 软件架构设计  
**状态**: 草案  
**关联文档**: `docs/system/SyRS_ECH.md`

---

## 1. 软件架构概述

### 1.1 设计原则
- **分层架构**: 清晰分离硬件抽象、服务层、应用层
- **模块化**: 每个模块单一职责，便于单元测试 (SWE.4)
- **可配置性**: 通过配置头文件适配不同硬件平台
- **安全性**: 关键路径实现冗余检查和故障安全

### 1.2 软件组件总览

```
┌─────────────────────────────────────────────────────────────────┐
│                        应用层 (Application)                      │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │
│  │ 温度控制    │  │ 状态管理    │  │ 通信处理    │              │
│  │ Controller  │  │ State Mgr   │  │ Comm Handler│              │
│  └─────────────┘  └─────────────┘  └─────────────┘              │
├─────────────────────────────────────────────────────────────────┤
│                        服务层 (Services)                         │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │
│  │ PID 控制    │  │ 故障诊断    │  │ 数据记录    │              │
│  │ PID Service │  │ Diag Service│  │ Data Logger │              │
│  └─────────────┘  └─────────────┘  └─────────────┘              │
├─────────────────────────────────────────────────────────────────┤
│                    硬件抽象层 (HAL / Drivers)                     │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────┐ │
│  │ PWM Driver  │  │ ADC Driver  │  │ CAN Driver  │  │ DIO     │ │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                        平台层 (Platform)                         │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │
│  │ 启动初始化  │  │ 中断管理    │  │ 看门狗      │              │
│  │ Startup     │  │ ISR Mgr     │  │ Watchdog    │              │
│  └─────────────┘  └─────────────┘  └─────────────┘              │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. 软件需求 (SRSW)

### 2.1 功能需求

| ID | 需求描述 | 追踪 SyRS | 优先级 |
|----|----------|-----------|--------|
| SWE-F-001 | 软件应实现温度闭环控制算法，根据目标温度计算 PWM 占空比 | SYS-F-001, SYS-F-004 | 高 |
| SWE-F-002 | 软件应生成 20kHz PWM 信号，占空比 0-100% 可调 | SYS-F-002 | 高 |
| SWE-F-003 | 软件应周期性采集温度传感器 ADC 值并转换为温度 | SYS-F-003 | 高 |
| SWE-F-004 | 软件应实现 PID 控制算法，参数可标定 | SYS-F-004 | 高 |
| SWE-F-005 | 软件应控制水泵继电器输出 | SYS-F-005 | 高 |
| SWE-F-006 | 软件应实现 CAN 协议栈接口，收发指定信号 | SYS-F-006 | 高 |
| SWE-F-007 | 软件应实现软启动逻辑，功率爬升 ≤ 10%/s | SYS-F-008 | 中 |
| SWE-F-008 | 软件应实现四状态状态机（待机/运行/故障/维护） | SYS-F-009 | 高 |

### 2.2 安全需求

| ID | 需求描述 | 追踪 SyRS | ASIL |
|----|----------|-----------|------|
| SWE-S-001 | 软件应监控出水温度，≥85°C 时立即切断 PWM | SYS-S-001 | B |
| SWE-S-002 | 软件应实现过流检测逻辑，10ms 内响应 | SYS-S-002 | B |
| SWE-S-003 | 软件应监控 CAN 通信超时，≥500ms 进入安全状态 | SYS-S-003 | B |
| SWE-S-004 | 软件应检测温度传感器故障（开路/短路） | SYS-S-004 | B |
| SWE-S-005 | 软件应实现周期性看门狗喂狗 | SYS-S-005 | B |
| SWE-S-006 | 软件应实现 DTC 存储与管理 | SYS-S-006 | A |

### 2.3 性能需求

| ID | 需求描述 | 追踪 SyRS | 验收标准 |
|----|----------|-----------|----------|
| SWE-P-001 | 主控制循环周期 ≤ 10ms | SYS-P-001 | 时序分析 |
| SWE-P-002 | 温度采样周期 ≤ 50ms | SYS-P-002 | 时序分析 |
| SWE-P-003 | CAN 发送任务周期 100ms | SYS-P-005 | 总线测试 |
| SWE-P-004 | 启动初始化 ≤ 500ms | SYS-P-004 | 启动测试 |

---

## 3. 模块详细设计

### 3.1 模块列表与接口

| 模块名称 | 文件路径 | 职责 | 关键接口 |
|----------|----------|------|----------|
| `ech_main` | `src/app/ech_main.c` | 主循环、任务调度 | `ECH_Init()`, `ECH_Cycle()` |
| `ech_temp_ctrl` | `src/app/ech_temp_ctrl.c` | 温度闭环控制 | `TempCtrl_CalcDuty(target, actual)` |
| `ech_state` | `src/app/ech_state.c` | 状态机管理 | `StateMgr_Transit(event)` |
| `ech_comm` | `src/app/ech_comm.c` | CAN 通信处理 | `Comm_ProcessRx()`, `Comm_BuildTx()` |
| `drv_pwm` | `src/drivers/ech_pwm.c` | PWM 硬件驱动 | `PWM_Init()`, `PWM_SetDuty()` |
| `drv_adc` | `src/drivers/ech_adc.c` | ADC 采样驱动 | `ADC_Init()`, `ADC_ReadChannel()` |
| `drv_can` | `src/drivers/ech_can.c` | CAN 驱动接口 | `CAN_Init()`, `CAN_Send()`, `CAN_Receive()` |
| `drv_dio` | `src/drivers/ech_dio.c` | 数字 IO 驱动 | `DIO_Write()`, `DIO_Read()` |
| `svc_pid` | `src/services/ech_pid.c` | PID 算法服务 | `PID_Init()`, `PID_Calc()` |
| `svc_diag` | `src/services/ech_diag.c` | 故障诊断服务 | `Diag_ReportFault()`, `Diag_ClearDTC()` |
| `svc_wdg` | `src/services/ech_watchdog.c` | 看门狗服务 | `Wdg_Init()`, `Wdg_Feed()` |
| `platform_init` | `src/platform/ech_init.c` | 系统初始化 | `Platform_Init()` |

### 3.2 关键模块接口定义

#### 3.2.1 温度控制模块 (`ech_temp_ctrl.h`)

```c
#ifndef ECH_TEMP_CTRL_H
#define ECH_TEMP_CTRL_H

#include <stdint.h>
#include <stdbool.h>

// 温度控制配置
typedef struct {
    float kp;           // 比例系数
    float ki;           // 积分系数
    float kd;           // 微分系数
    float max_duty;     // 最大占空比 (%)
    float min_duty;     // 最小占空比 (%)
    float ramp_rate;    // 爬升速率 (%/s)
} TempCtrl_Config_t;

// 初始化
void TempCtrl_Init(const TempCtrl_Config_t* config);

// 计算占空比
// 输入：目标温度 (°C), 实际温度 (°C)
// 输出：占空比 (0.0 - 100.0)
float TempCtrl_CalcDuty(float target_temp, float actual_temp);

// 获取当前占空比
float TempCtrl_GetCurrentDuty(void);

// 紧急停止
void TempCtrl_EmergencyStop(void);

#endif // ECH_TEMP_CTRL_H
```

#### 3.2.2 PWM 驱动模块 (`ech_pwm.h`)

```c
#ifndef ECH_PWM_H
#define ECH_PWM_H

#include <stdint.h>
#include <stdbool.h>

// PWM 初始化配置
typedef struct {
    uint32_t frequency_hz;  // PWM 频率 (Hz)
    uint8_t channel;        // PWM 通道号
} PWM_Config_t;

// 初始化
bool PWM_Init(const PWM_Config_t* config);

// 设置占空比 (0.0 - 100.0)
bool PWM_SetDuty(float duty_percent);

// 获取当前占空比
float PWM_GetDuty(void);

// 禁用 PWM 输出
void PWM_Disable(void);

// 启用 PWM 输出
void PWM_Enable(void);

#endif // ECH_PWM_H
```

#### 3.2.3 状态管理模块 (`ech_state.h`)

```c
#ifndef ECH_STATE_H
#define ECH_STATE_H

#include <stdint.h>

// 系统状态枚举
typedef enum {
    STATE_STANDBY = 0,
    STATE_RUNNING = 1,
    STATE_FAULT = 2,
    STATE_MAINTENANCE = 3
} ECH_SystemState_t;

// 状态转换事件
typedef enum {
    EVT_ENABLE_REQ = 0,
    EVT_DISABLE_REQ = 1,
    EVT_FAULT_DETECTED = 2,
    EVT_FAULT_CLEARED = 3,
    EVT_MAINT_ENTER = 4,
    EVT_MAINT_EXIT = 5
} ECH_StateEvent_t;

// 初始化
void StateMgr_Init(void);

// 状态转换
ECH_SystemState_t StateMgr_Transit(ECH_StateEvent_t event);

// 获取当前状态
ECH_SystemState_t StateMgr_GetCurrentState(void);

// 检查状态是否允许运行
bool StateMgr_IsRunningAllowed(void);

#endif // ECH_STATE_H
```

---

## 4. 数据流图

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  CAN 接收   │────▶│  状态管理   │────▶│  温度控制   │
│  ReqTemp    │     │  State Mgr  │     │  Controller │
│  ReqEnable  │     └─────────────┘     └──────┬──────┘
└─────────────┘                                │
                                               │ duty_cmd
                                               ▼
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  温度采集   │────▶│  PID 计算    │     │  PWM 驱动   │
│  ADC Read   │     │  PID Calc   │     │  PWM Output │
└─────────────┘     └─────────────┘     └─────────────┘
       │                                       │
       │ actual_temp                           │ PTC Heater
       └───────────────────────────────────────┘
                    反馈回路
```

---

## 5. 任务调度设计

| 任务名称 | 周期 (ms) | 优先级 | 职责 |
|----------|-----------|--------|------|
| `Task_Main` | 10 | 高 | 主控制循环、状态机、温度控制 |
| `Task_ADC` | 50 | 中 | 温度传感器采样 |
| `Task_CAN_Tx` | 100 | 低 | CAN 发送帧打包与发送 |
| `Task_Diag` | 200 | 低 | 故障诊断、DTC 管理 |
| `Task_Wdg` | 50 | 高 | 看门狗喂狗 |

---

## 6. 配置与标定参数

```c
// ech_config.h - 可标定参数
#define ECH_CONFIG_TEMP_TARGET_MAX      80.0f   // 最大目标温度 °C
#define ECH_CONFIG_TEMP_CUTOFF          85.0f   // 过温切断阈值 °C
#define ECH_CONFIG_PWM_FREQUENCY        20000   // PWM 频率 Hz
#define ECH_CONFIG_RAMP_RATE            10.0f   // 功率爬升速率 %/s
#define ECH_CONFIG_CAN_TIMEOUT_MS       500     // CAN 超时阈值 ms
#define ECH_CONFIG_PID_KP               2.5f    // PID 比例系数
#define ECH_CONFIG_PID_KI               0.1f    // PID 积分系数
#define ECH_CONFIG_PID_KD               0.05f   // PID 微分系数
```

---

## 7. 追踪矩阵

| 软件需求 | 软件模块 | 单元测试 | 状态 |
|----------|----------|----------|------|
| SWE-F-001 | `ech_temp_ctrl` | UTC-001 | 待链接 |
| SWE-F-002 | `drv_pwm` | UTC-002 | 待链接 |
| SWE-F-003 | `drv_adc` | UTC-003 | 待链接 |
| SWE-F-004 | `svc_pid` | UTC-004 | 待链接 |
| SWE-S-001 | `ech_temp_ctrl` | UTC-011 | 待链接 |
| SWE-S-002 | `svc_diag` | UTC-012 | 待链接 |
| SWE-S-003 | `ech_comm` | UTC-013 | 待链接 |

---

## 8. 修订历史

| 版本 | 日期 | 作者 | 变更描述 |
|------|------|------|----------|
| 0.1 | 2026-03-02 | AI Assistant | 初始草案 |

---

**下一步**: 嵌入式实现工程师 (`/dev`) 将基于此架构编写最小可运行示例代码。
