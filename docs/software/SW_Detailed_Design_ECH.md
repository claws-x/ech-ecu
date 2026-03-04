# 软件详细设计 (SDD)

**文档版本**: 0.1  
**创建日期**: 2026-03-03  
**过程域**: SWE.3 软件详细设计  
**项目**: ECH 水加热器 ECU 控制系统

---

## 1. 文档概述

### 1.1 目的

本文档定义 ECH ECU 的软件详细设计，包括：
- 模块详细设计
- 接口详细定义
- 数据结构设计
- 算法设计

### 1.2 范围

本文档适用于：
- 应用层软件开发
- 驱动层软件开发
- 单元测试用例设计

---

## 2. 软件架构概览

### 2.1 分层架构

```
┌─────────────────────────────────────┐
│         应用层 (Application)         │
│  ┌───────────┐  ┌─────────────────┐ │
│  │Temp Ctrl  │  │Safety Monitor   │ │
│  └───────────┘  └─────────────────┘ │
├─────────────────────────────────────┤
│         服务层 (Services)            │
│  ┌───────────┐  ┌─────────────────┐ │
│  │  PID      │  │  Diagnostic     │ │
│  └───────────┘  └─────────────────┘ │
├─────────────────────────────────────┤
│         驱动层 (Drivers)             │
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌───────┐ │
│  │ PWM │ │ ADC │ │ CAN │ │ GPIO  │ │
│  └─────┘ └─────┘ └─────┘ └───────┘ │
├─────────────────────────────────────┤
│         硬件层 (Hardware)            │
│     MCU (RH850 / AURIX)             │
└─────────────────────────────────────┘
```

### 2.2 模块清单

| 模块 | 层级 | 文件 | ASIL |
|------|------|------|------|
| 温度控制 | 应用层 | ech_temp_ctrl.c | ASIL-B |
| 安全监控 | 应用层 | ech_safety_monitor.c | ASIL-B |
| PID 服务 | 服务层 | ech_pid.c | ASIL-B |
| 诊断服务 | 服务层 | ech_diagnostic.c | ASIL-B |
| PWM 驱动 | 驱动层 | ech_pwm.c/.h | ASIL-B |
| ADC 驱动 | 驱动层 | ech_adc.c/.h | ASIL-B |
| CAN 驱动 | 驱动层 | ech_can.c/.h | ASIL-B |
| GPIO 驱动 | 驱动层 | ech_gpio.c/.h | QM |

---

## 3. 模块详细设计

### 3.1 温度控制模块 (ech_temp_ctrl.c)

#### 3.1.1 功能描述

实现水加热器的温度闭环控制，包括：
- PID 温度控制算法
- 温度安全监控
- PWM 输出控制

#### 3.1.2 接口定义

```c
// 初始化
void TempCtrl_Init(void);

// 主控制循环 (周期：10ms)
void TempCtrl_MainCycle(void);

// 设置目标温度 (单位：0.1°C)
void TempCtrl_SetTarget(uint16_t targetTemp);

// 获取当前温度 (单位：0.1°C)
uint16_t TempCtrl_GetCurrentTemp(void);

// 紧急停止
void TempCtrl_EmergencyStop(void);
```

#### 3.1.3 数据结构

```c
typedef struct {
    uint16_t targetTemp;      // 目标温度
    uint16_t currentTemp;     // 当前温度
    int16_t  error;           // 温度误差
    uint16_t pwmDuty;         // PWM 占空比
    uint8_t  state;           // 控制状态
} TempCtrl_t;

typedef enum {
    TEMP_CTRL_IDLE = 0,       // 空闲状态
    TEMP_CTRL_HEATING,        // 加热状态
    TEMP_CTRL_HOLD,           // 保温状态
    TEMP_CTRL_FAULT           // 故障状态
} TempCtrlState_t;
```

#### 3.1.4 控制算法

```c
// 伪代码
void TempCtrl_MainCycle(void) {
    // 1. 读取当前温度
    currentTemp = TempSensor_Read();
    
    // 2. 计算温度误差
    error = targetTemp - currentTemp;
    
    // 3. PID 计算
    pwmDuty = PID_Calculate(error);
    
    // 4. 安全检查
    if (currentTemp > TEMP_MAX_LIMIT) {
        TempCtrl_EmergencyStop();
        return;
    }
    
    // 5. 更新 PWM 输出
    PWM_SetDuty(pwmDuty);
}
```

#### 3.1.5 错误处理

| 错误代码 | 错误类型 | 响应 |
|----------|----------|------|
| 0x01 | 温度传感器故障 | 进入安全状态 |
| 0x02 | 温度超限 (>85°C) | 关闭 PWM |
| 0x03 | PID 计算溢出 | 使用默认值 |

---

### 3.2 安全监控模块 (ech_safety_monitor.c)

#### 3.2.1 功能描述

实现系统安全监控，包括：
- 看门狗管理
- 故障检测
- 安全状态管理

#### 3.2.2 接口定义

```c
// 初始化
void SafetyMonitor_Init(void);

// 主监控循环 (周期：10ms)
void SafetyMonitor_MainCycle(void);

// 喂狗
void SafetyMonitor_FeedDog(void);

// 获取安全状态
SafeState_t SafetyMonitor_GetState(void);

// 报告故障
void SafetyMonitor_ReportFault(FaultCode_t faultCode);
```

#### 3.2.3 数据结构

```c
typedef struct {
    SafeState_t currentState;    // 当前安全状态
    uint32_t faultMask;          // 故障掩码
    uint32_t watchdogCounter;    // 看门狗计数
    uint32_t lastFeedTime;       // 上次喂狗时间
} SafetyMonitor_t;
```

#### 3.2.4 状态机

```
正常状态 (NORMAL)
  │
  ├─ 检测到警告故障
  ↓
警告状态 (WARNING)
  │
  ├─ 故障恢复
  ├─ 检测到严重故障
  ↓              ↓
正常状态     故障状态 (FAULT)
              │
              ├─ 故障恢复 (需手动复位)
              ↓
           正常状态
```

---

### 3.3 PID 服务模块 (ech_pid.c)

#### 3.3.1 功能描述

实现通用 PID 控制算法，包括：
- 位置式 PID
- 增量式 PID
- 抗积分饱和

#### 3.3.2 接口定义

```c
// PID 控制器句柄
typedef struct PID_Handle PID_Handle;

// 创建 PID 控制器
PID_Handle* PID_Create(PID_Config_t* config);

// 销毁 PID 控制器
void PID_Destroy(PID_Handle* handle);

// PID 计算
int32_t PID_Calculate(PID_Handle* handle, int32_t error);

// 重置 PID
void PID_Reset(PID_Handle* handle);

// 设置参数
void PID_SetTunings(PID_Handle* handle, float Kp, float Ki, float Kd);
```

#### 3.3.3 数据结构

```c
typedef struct {
    float Kp;           // 比例增益
    float Ki;           // 积分增益
    float Kd;           // 微分增益
    int32_t maxOutput;  // 最大输出
    int32_t minOutput;  // 最小输出
} PID_Config_t;

struct PID_Handle {
    PID_Config_t config;
    int32_t lastError;
    int32_t integral;
    uint32_t timestamp;
};
```

#### 3.3.4 算法实现

```c
// 位置式 PID
int32_t PID_Calculate(PID_Handle* handle, int32_t error) {
    // 比例项
    float P = handle->config.Kp * error;
    
    // 积分项 (带抗饱和)
    handle->integral += error;
    if (handle->integral > MAX_INTEGRAL) {
        handle->integral = MAX_INTEGRAL;
    }
    float I = handle->config.Ki * handle->integral;
    
    // 微分项
    float D = handle->config.Kd * (error - handle->lastError);
    
    handle->lastError = error;
    
    // 计算输出
    float output = P + I + D;
    
    // 输出限幅
    if (output > handle->config.maxOutput) {
        output = handle->config.maxOutput;
    }
    if (output < handle->config.minOutput) {
        output = handle->config.minOutput;
    }
    
    return (int32_t)output;
}
```

---

### 3.4 PWM 驱动模块 (ech_pwm.c)

#### 3.4.1 功能描述

实现 PTC 加热器的 PWM 控制，包括：
- PWM 初始化
- 占空比设置
- 紧急停止

#### 3.4.2 接口定义

```c
// 初始化
void PWM_Init(void);

// 设置占空比 (0-100%)
void PWM_SetDuty(uint16_t duty);

// 获取占空比
uint16_t PWM_GetDuty(void);

// 启用 PWM
void PWM_Enable(bool enable);

// 紧急停止
void PWM_EmergencyStop(void);
```

#### 3.4.3 配置参数

```c
#define PWM_FREQUENCY       20000       // 20kHz
#define PWM_PERIOD_US       50          // 50μs
#define PWM_MAX_DUTY        100         // 100%
#define PWM_MIN_DUTY        0           // 0%
```

---

## 4. 接口设计

### 4.1 内部接口

| 源模块 | 目标模块 | 接口函数 | 数据类型 |
|--------|----------|----------|----------|
| TempCtrl | PWM | PWM_SetDuty() | uint16_t |
| TempCtrl | ADC | ADC_ReadTemp() | uint16_t |
| SafetyMonitor | PWM | PWM_EmergencyStop() | void |
| SafetyMonitor | TempCtrl | TempCtrl_EmergencyStop() | void |

### 4.2 外部接口 (CAN)

| 消息 ID | 名称 | 方向 | 周期 |
|---------|------|------|------|
| 0x100 | ECU_Status | TX | 100ms |
| 0x101 | ECU_Temp | TX | 100ms |
| 0x200 | VCU_Command | RX | 100ms |
| 0x201 | VCU_Config | RX | 事件 |

---

## 5. 数据结构设计

### 5.1 全局数据结构

```c
// 系统全局状态
typedef struct {
    TempCtrl_t tempCtrl;
    SafetyMonitor_t safetyMonitor;
    PID_Handle* pidHandle;
    uint32_t systemUptime;
    uint16_t faultCount;
} SystemState_t;

// 系统状态实例
static SystemState_t g_systemState;
```

### 5.2 配置数据结构

```c
typedef struct {
    uint16_t targetTemp;        // 目标温度
    uint16_t maxTemp;           // 最大温度
    uint16_t pwmFreq;           // PWM 频率
    uint16_t canTimeout;        // CAN 超时时间
    uint8_t  pidKp;             // PID 比例
    uint8_t  pidKi;             // PID 积分
    uint8_t  pidKd;             // PID 微分
} SystemConfig_t;
```

---

## 6. 错误处理设计

### 6.1 错误代码定义

```c
typedef enum {
    FAULT_NONE = 0x0000,        // 无故障
    FAULT_TEMP_SENSOR = 0x0001, // 温度传感器故障
    FAULT_TEMP_HIGH = 0x0002,   // 温度过高
    FAULT_FLOW_SWITCH = 0x0004, // 水流开关故障
    FAULT_CURRENT_HIGH = 0x0008,// 过流
    FAULT_VOLTAGE_HIGH = 0x0010,// 过压
    FAULT_CAN_LOST = 0x0020,    // CAN 丢失
    FAULT_WATCHDOG = 0x0040,    // 看门狗
} FaultCode_t;
```

### 6.2 错误处理流程

```c
void ErrorHandler_Process(FaultCode_t fault) {
    // 1. 记录故障码
    DTC_Store(fault);
    
    // 2. 根据故障等级响应
    if (IS_CRITICAL_FAULT(fault)) {
        // 紧急故障：立即关闭
        PWM_EmergencyStop();
        SafetyMonitor_ReportFault(fault);
    } else if (IS_WARNING_FAULT(fault)) {
        // 警告故障：降级运行
        PWM_SetDuty(0);
    }
    
    // 3. 通知上层
    VCU_NotifyFault(fault);
}
```

---

## 7. 单元测试设计

### 7.1 测试用例清单

| 模块 | 测试用例 | 测试方法 | 覆盖率 |
|------|----------|----------|--------|
| TempCtrl | TC_TEMP_01 | 单元测试 | 语句覆盖 |
| TempCtrl | TC_TEMP_02 | 单元测试 | 分支覆盖 |
| PID | TC_PID_01 | 单元测试 | 语句覆盖 |
| PID | TC_PID_02 | 单元测试 | 分支覆盖 |
| SafetyMonitor | TC_SAFETY_01 | 单元测试 | 语句覆盖 |

### 7.2 测试示例

```c
// 温度控制测试
void TC_TEMP_01_NormalControl(void) {
    // 初始化
    TempCtrl_Init();
    
    // 设置目标温度
    TempCtrl_SetTarget(600);  // 60.0°C
    
    // 运行控制循环
    for (int i = 0; i < 100; i++) {
        TempCtrl_MainCycle();
        Delay_ms(10);
    }
    
    // 验证结果
    TEST_ASSERT(TempCtrl_GetCurrentTemp() <= 620);
    TEST_ASSERT(PWM_GetDuty() > 0);
}

// 温度超限测试
void TC_TEMP_02_OverTempProtection(void) {
    // 模拟温度超限
    TempSensor_SetMockValue(900);  // 90.0°C
    
    // 运行控制循环
    TempCtrl_MainCycle();
    
    // 验证保护动作
    TEST_ASSERT(PWM_GetDuty() == 0);
    TEST_ASSERT(SafetyMonitor_GetState() == SAFE_STATE_FAULT);
}
```

---

## 8. 配置管理

### 8.1 版本控制

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|----------|
| 0.1 | 2026-03-03 | AI Assistant | 初稿 |

### 8.2 追溯矩阵

| 软件需求 | 详细设计 | 测试用例 | 状态 |
|----------|----------|----------|------|
| SRSW-001 | 3.1 | TC_TEMP_01 | ✅ |
| SRSW-002 | 3.2 | TC_SAFETY_01 | ✅ |
| SRSW-003 | 3.3 | TC_PID_01 | ✅ |
| SRSW-004 | 3.4 | TC_PWM_01 | ✅ |

---

## 9. 审批

| 角色 | 姓名 | 签名 | 日期 |
|------|------|------|------|
| 作者 | AI Assistant | - | 2026-03-03 |
| 审核 | - | - | - |
| 批准 | - | - | - |

---

**文档状态**: ✅ 已完成  
**下一步**: 评审计划文档开发

**AI 用起来，这里是六度空间学习之路。**
