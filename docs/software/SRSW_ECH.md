# ECH 水加热器 ECU 软件需求规格 (SRSW)

**文档版本**: 0.1  
**创建日期**: 2026-03-03  
**过程域**: SWE.1 软件需求分析  
**状态**: 草案  
**关联文档**: `docs/system/SyRS_ECH.md`, `docs/software/SW_Architecture_ECH.md`, `docs/trace-matrix/Trace_Matrix_SYS-SWE.md`

---

## 1. 引言

### 1.1 目的

本软件需求规格 (SRSW) 定义 ECH 水加热器 ECU 的软件需求，基于系统需求规格 (SyRS) 进行细化，为软件架构设计 (SWE.2) 和详细设计 (SWE.3) 提供输入。

### 1.2 范围

本规格适用于 ECH ECU 的所有软件组件，包括：
- 应用层：温度控制、状态管理、通信处理
- 服务层：PID 控制、故障诊断、数据记录
- 驱动层：PWM、ADC、CAN、DIO 驱动
- 平台层：初始化、中断管理、看门狗

### 1.3 需求编写规范

本规格采用 **EARS 模式** 编写需求，确保需求满足以下特性：
- ✅ 正确性、可理解性、单一性、无歧义性
- ✅ 一致性、必要性、完整性、可行性
- ✅ 可验证性、不提前限定设计、合规性

**EARS 句式模板**:
| 类型 | 句式 |
|------|------|
| 通用型 | The <system> shall <requirement> |
| 事件驱动 | When <trigger>, the <system> shall <requirement> |
| 异常行为 | If <fault condition>, the <system> shall <response> |
| 状态驱动 | While in <state>, the <system> shall <requirement> |

---

## 2. 软件需求总览

### 2.1 需求分类与统计

| 类别 | 需求数量 | 追溯 SyRS |
|------|----------|-----------|
| 功能需求 (SWE-F) | 14 | SYS-F-001~009 |
| 安全需求 (SWE-S) | 10 | SYS-S-001~006 |
| 性能需求 (SWE-P) | 8 | SYS-P-001~005 |
| 接口需求 (SWE-I) | 15 | SYS-F-006/007, SYS-D-001~004 |
| **合计** | **47** | - |

> **注**: 
> - 2026-03-03 22:30 根据 Get 笔记知识库 MD 文档更新，新增 PTC 自动恢复、标定接口、LIN 监控需求
> - 2026-03-03 22:40 CAN → LIN 通信变更，新增 LIN 主站、LIN 2.1 协议、休眠唤醒需求

### 2.2 需求优先级定义

| 优先级 | 定义 | 占比要求 |
|--------|------|----------|
| P0 - 关键 | 安全相关/核心功能，必须实现 | ≥30% |
| P1 - 高 | 主要功能，应实现 | ≥50% |
| P2 - 中 | 辅助功能，可实现 | ≤20% |

---

## 3. 功能需求 (SWE-F)

### 3.1 温度控制功能

| ID | 需求描述 (EARS 模式) | 优先级 | 追溯 SyRS | 验证方法 |
|----|---------------------|--------|-----------|----------|
| SWE-F-001 | The software shall calculate PWM duty cycle based on target temperature and actual temperature using PID algorithm | P0 | SYS-F-001, SYS-F-004 | 单元测试 |
| SWE-F-002 | When target temperature changes, the software shall respond within 100ms | P0 | SYS-F-001 | 时序测试 |
| SWE-F-003 | The software shall limit duty cycle ramp rate to maximum 10%/s during soft-start | P1 | SYS-F-008 | 功能测试 |
| SWE-F-004 | The software shall maintain steady-state temperature error within ±2°C | P0 | SYS-F-004 | 闭环测试 |
| SWE-F-005 | While in STANDBY state, the software shall output 0% duty cycle | P1 | SYS-F-009 | 状态测试 |
| SWE-F-006 | While in RUNNING state, the software shall output PID-calculated duty cycle | P0 | SYS-F-009 | 状态测试 |
| SWE-F-015 | When PTC temperature exceeds configurable limit, the software shall automatically reduce power output | P0 | SYS-F-001 | 功能测试 |
| SWE-F-016 | When PTC temperature returns to normal range and heating request is valid, the software shall resume normal operation automatically | P1 | SYS-F-001 | 功能测试 |

### 3.2 PWM 控制功能

| ID | 需求描述 (EARS 模式) | 优先级 | 追溯 SyRS | 验证方法 |
|----|---------------------|--------|-----------|----------|
| SWE-F-007 | The software shall generate PWM signal with frequency 20kHz ±5% | P0 | SYS-F-002 | 示波器测试 |
| SWE-F-008 | The software shall support duty cycle adjustment from 0% to 100% with 1% resolution | P0 | SYS-F-002 | 功能测试 |
| SWE-F-009 | When emergency stop is triggered, the software shall set PWM output to 0% within 10ms | P0 | SYS-S-001, SYS-S-002 | 安全测试 |

### 3.3 温度采集功能

| ID | 需求描述 (EARS 模式) | 优先级 | 追溯 SyRS | 验证方法 |
|----|---------------------|--------|-----------|----------|
| SWE-F-010 | The software shall sample outlet temperature sensor every 50ms | P0 | SYS-F-003 | 时序测试 |
| SWE-F-011 | The software shall sample return temperature sensor every 50ms | P0 | SYS-F-003 | 时序测试 |
| SWE-F-012 | The software shall convert ADC value to temperature with accuracy ±2°C | P0 | SYS-F-003 | 精度测试 |

### 3.4 水泵控制功能

| ID | 需求描述 (EARS 模式) | 优先级 | 追溯 SyRS | 验证方法 |
|----|---------------------|--------|-----------|----------|
| SWE-F-013 | When entering RUNNING state, the software shall turn ON water pump relay | P1 | SYS-F-005 | 状态测试 |
| SWE-F-014 | When exiting RUNNING state, the software shall turn OFF water pump relay | P1 | SYS-F-005 | 状态测试 |

### 3.5 状态管理功能

| ID | 需求描述 (EARS 模式) | 优先级 | 追溯 SyRS | 验证方法 |
|----|---------------------|--------|-----------|----------|
| SWE-F-014 | The software shall implement four states: STANDBY, RUNNING, FAULT, MAINTENANCE | P0 | SYS-F-009 | 状态机测试 |
| SWE-F-015 | When enable request is received in STANDBY state, the software shall transition to RUNNING state | P0 | SYS-F-009 | 状态转换测试 |
| SWE-F-016 | When fault is detected in any state, the software shall transition to FAULT state | P0 | SYS-F-009 | 状态转换测试 |
| SWE-F-017 | When maintenance request is received in STANDBY state, the software shall transition to MAINTENANCE state | P1 | SYS-F-009 | 状态转换测试 |

---

## 4. 安全需求 (SWE-S)

### 4.1 过温保护

| ID | 需求描述 (EARS 模式) | 优先级 | 追溯 SyRS | ASIL | 验证方法 |
|----|---------------------|--------|-----------|------|----------|
| SWE-S-001 | If outlet temperature ≥ 85°C, the software shall cut off PWM output immediately | P0 | SYS-S-001 | B | 故障注入 |
| SWE-S-002 | If outlet temperature ≥ 80°C, the software shall reduce duty cycle to maximum 50% | P1 | SYS-S-001 | B | 功能测试 |
| SWE-S-003 | When over-temperature fault is cleared, the software shall require manual reset before resuming | P1 | SYS-S-001 | B | 恢复测试 |

### 4.2 过流保护

| ID | 需求描述 (EARS 模式) | 优先级 | 追溯 SyRS | ASIL | 验证方法 |
|----|---------------------|--------|-----------|------|----------|
| SWE-S-004 | If current ≥ 120% of rated value, the software shall cut off PWM output within 10ms | P0 | SYS-S-002 | B | 故障注入 |
| SWE-S-005 | The software shall sample current sensor every 10ms for over-current detection | P0 | SYS-S-002 | B | 时序测试 |

### 4.3 通信安全

| ID | 需求描述 (EARS 模式) | 优先级 | 追溯 SyRS | ASIL | 验证方法 |
|----|---------------------|--------|-----------|------|----------|
| SWE-S-006 | If CAN communication is lost for ≥ 500ms, the software shall enter safe state | P0 | SYS-S-003 | B | 总线关闭测试 |
| SWE-S-007 | While in safe state due to CAN loss, the software shall output 0% duty cycle | P0 | SYS-S-003 | B | 状态测试 |

### 4.4 传感器故障

| ID | 需求描述 (EARS 模式) | 优先级 | 追溯 SyRS | ASIL | 验证方法 |
|----|---------------------|--------|-----------|------|----------|
| SWE-S-008 | If temperature sensor is open-circuit, the software shall report fault and reduce power | P0 | SYS-S-004 | B | 开路测试 |
| SWE-S-009 | If temperature sensor is short-circuit, the software shall report fault and reduce power | P0 | SYS-S-004 | B | 短路测试 |
| SWE-S-010 | The software shall detect sensor fault when ADC value is out of valid range | P0 | SYS-S-004 | B | 边界测试 |

### 4.5 看门狗监控

| ID | 需求描述 (EARS 模式) | 优先级 | 追溯 SyRS | ASIL | 验证方法 |
|----|---------------------|--------|-----------|------|----------|
| SWE-S-011 | The software shall feed watchdog every 100ms ±50ms | P0 | SYS-S-005 | B | 时序测试 |
| SWE-S-012 | If main loop exceeds 200ms, the software shall not feed watchdog | P0 | SYS-S-005 | B | 故障注入 |

### 4.6 故障记录

| ID | 需求描述 (EARS 模式) | 优先级 | 追溯 SyRS | ASIL | 验证方法 |
|----|---------------------|--------|-----------|------|----------|
| SWE-S-013 | The software shall store fault DTC in non-volatile memory | P1 | SYS-S-006 | A | 断电测试 |
| SWE-S-014 | The software shall support UDS service to read/clear DTC | P1 | SYS-S-006 | A | UDS 测试 |

---

## 5. 性能需求 (SWE-P)

| ID | 需求描述 | 优先级 | 追溯 SyRS | 验收标准 | 验证方法 |
|----|---------|--------|-----------|----------|----------|
| SWE-P-001 | 主控制循环周期 ≤ 10ms | P0 | SYS-P-001 | 时序分析 | 示波器/逻辑分析仪 |
| SWE-P-002 | 温度采样周期 ≤ 50ms | P0 | SYS-P-002 | 时序分析 | 示波器/逻辑分析仪 |
| SWE-P-003 | CAN 发送任务周期 100ms | P0 | SYS-P-005 | 总线负载测试 | CAN 分析仪 |
| SWE-P-004 | 启动初始化时间 ≤ 500ms | P1 | SYS-P-004 | 启动测试 | 示波器 |
| SWE-P-005 | PWM 占空比更新延迟 ≤ 5ms | P1 | SYS-P-001 | 阶跃响应测试 | 示波器 |
| SWE-P-006 | 状态转换响应时间 ≤ 50ms | P1 | SYS-F-009 | 状态转换测试 | 逻辑分析仪 |
| SWE-P-007 | 故障检测响应时间 ≤ 10ms | P0 | SYS-S-002 | 故障注入测试 | 示波器 |
| SWE-P-008 | RAM 使用率 ≤ 70% | P1 | - | 静态分析 | 编译器 Map 文件 |
| SWE-P-009 | ROM 使用率 ≤ 80% | P1 | - | 静态分析 | 编译器 Map 文件 |

---

## 6. 接口需求 (SWE-I)

### 6.1 LIN 通信接口

| ID | 需求描述 | 优先级 | 追溯 SyRS | 验证方法 |
|----|---------|--------|-----------|----------|
| SWE-I-001 | The software shall receive ECH_ReqTemp signal via LIN frame | P0 | SYS-F-006 | LIN 测试 |
| SWE-I-002 | The software shall receive ECH_ReqEnable signal via LIN frame | P0 | SYS-F-006 | LIN 测试 |
| SWE-I-003 | The software shall transmit ECH_ActualTemp in LIN frame every 100ms | P0 | SYS-F-006 | LIN 测试 |
| SWE-I-004 | The software shall transmit ECH_DutyCycle in LIN frame every 100ms | P0 | SYS-F-006 | LIN 测试 |
| SWE-I-005 | The software shall transmit ECH_Status in LIN frame every 100ms | P0 | SYS-F-006 | LIN 测试 |
| SWE-I-006 | The software shall transmit ECH_FaultCode in LIN frame on fault event | P1 | SYS-F-006 | LIN 测试 |
| SWE-I-022 | The software shall implement LIN master node functionality | P0 | SYS-F-007 | LIN 测试 |
| SWE-I-023 | The software shall support LIN 2.1 protocol with 19.2kbps baud rate | P0 | SYS-F-007 | LIN 测试 |
| SWE-I-024 | The software shall support LIN sleep and wake-up functionality | P1 | SYS-F-007 | LIN 测试 |

### 6.2 UDS 诊断接口

| ID | 需求描述 | 优先级 | 追溯 SyRS | 验证方法 |
|----|---------|--------|-----------|----------|
| SWE-I-007 | The software shall support UDS Service 0x10 (Diagnostic Session Control) | P1 | SYS-D-001 | UDS 测试 |
| SWE-I-008 | The software shall support UDS Service 0x11 (ECU Reset) | P1 | SYS-D-004 | UDS 测试 |
| SWE-I-009 | The software shall support UDS Service 0x19 (Read DTC Information) | P1 | SYS-D-002 | UDS 测试 |
| SWE-I-010 | The software shall support UDS Service 0x14 (Clear DTC) | P1 | SYS-D-002 | UDS 测试 |
| SWE-I-011 | The software shall support UDS Service 0x22 (Read Data By Identifier) | P1 | SYS-D-003 | UDS 测试 |

### 6.3 硬件抽象接口

| ID | 需求描述 | 优先级 | 验证方法 |
|----|---------|--------|----------|
| SWE-I-012 | The software shall initialize PWM module during startup | P0 | 代码审查 |
| SWE-I-013 | The software shall initialize ADC module during startup | P0 | 代码审查 |
| SWE-I-014 | The software shall initialize CAN module during startup | P0 | 代码审查 |
| SWE-I-015 | The software shall configure system clock to target frequency | P0 | 代码审查 |

### 6.4 标定与调试接口

| ID | 需求描述 | 优先级 | 验证方法 |
|----|---------|--------|----------|
| SWE-I-016 | The software shall support environment variable binding for dynamic control (e.g., env_speed) | P1 | 功能测试 |
| SWE-I-017 | The software shall support runtime parameter adjustment via calibration interface | P1 | 功能测试 |
| SWE-I-018 | The software shall support A2L file interface with only necessary variables retained | P1 | 工具集成测试 |

### 6.5 LIN 总线监控

| ID | 需求描述 | 优先级 | 验证方法 |
|----|---------|--------|----------|
| SWE-I-019 | The software shall monitor LIN bus load during system integration | P1 | 集成测试 |
| SWE-I-020 | The software shall monitor LIN frame transmission integrity for stability assurance | P1 | 集成测试 |
| SWE-I-021 | The software shall support LIN configuration tool integration with LDF database | P1 | 工具集成测试 |

---

## 7. 软件需求追踪矩阵

### 7.1 正向追溯 (SWE → SYS)

| SWE 需求 ID | 追溯 SyRS ID | 追溯状态 |
|-------------|-------------|----------|
| SWE-F-001 ~ SWE-F-017 | SYS-F-001 ~ SYS-F-009 | ✅ 完整 |
| SWE-S-001 ~ SWE-S-014 | SYS-S-001 ~ SYS-S-006 | ✅ 完整 |
| SWE-P-001 ~ SWE-P-009 | SYS-P-001 ~ SYS-P-005 | ✅ 完整 |
| SWE-I-001 ~ SWE-I-015 | SYS-F-006, SYS-D-001~004 | ✅ 完整 |

### 7.2 反向追溯 (SYS → SWE)

| SyRS ID | 追溯 SWE 需求 ID | 覆盖状态 |
|---------|-----------------|----------|
| SYS-F-001 | SWE-F-001, SWE-F-002 | ✅ 覆盖 |
| SYS-F-002 | SWE-F-007, SWE-F-008 | ✅ 覆盖 |
| SYS-F-003 | SWE-F-010, SWE-F-011, SWE-F-012 | ✅ 覆盖 |
| SYS-F-004 | SWE-F-001, SWE-F-004 | ✅ 覆盖 |
| SYS-F-005 | SWE-F-013, SWE-F-014 | ✅ 覆盖 |
| SYS-F-006 | SWE-I-001 ~ SWE-I-006 | ✅ 覆盖 |
| SYS-F-007 | - | ⚠️ 可选功能 |
| SYS-F-008 | SWE-F-003 | ✅ 覆盖 |
| SYS-F-009 | SWE-F-005, SWE-F-006, SWE-F-014 ~ SWE-F-017 | ✅ 覆盖 |
| SYS-S-001 | SWE-S-001, SWE-S-002, SWE-S-003 | ✅ 覆盖 |
| SYS-S-002 | SWE-S-004, SWE-S-005 | ✅ 覆盖 |
| SYS-S-003 | SWE-S-006, SWE-S-007 | ✅ 覆盖 |
| SYS-S-004 | SWE-S-008, SWE-S-009, SWE-S-010 | ✅ 覆盖 |
| SYS-S-005 | SWE-S-011, SWE-S-012 | ✅ 覆盖 |
| SYS-S-006 | SWE-S-013, SWE-S-014 | ✅ 覆盖 |

---

## 8. 需求验证计划

### 8.1 验证方法分类

| 方法 | 适用需求 | 执行阶段 |
|------|---------|----------|
| 单元测试 | SWE-F, SWE-S | SWE.4 |
| 集成测试 | SWE-I, SWE-P | SWE.5 |
| 系统测试 | 全部 | SYS.4/5 |

### 8.2 验证覆盖率目标

| 需求类别 | 单元测试覆盖率 | 集成测试覆盖率 |
|----------|---------------|---------------|
| P0 关键需求 | 100% | 100% |
| P1 高优先级 | 100% | 100% |
| P2 中优先级 | ≥80% | ≥80% |

---

## 9. 修订历史

| 版本 | 日期 | 作者 | 变更描述 |
|------|------|------|----------|
| 0.1 | 2026-03-03 | AI Assistant | 初始版本，完成 SWE.1 过程域 |
| 0.2 | 2026-03-03 22:30 | AI Assistant | 根据 Get 笔记知识库 MD 文档更新：新增 PTC 自动恢复逻辑 (SWE-F-015/016)、标定接口 (SWE-I-016~018)、LIN 监控 (SWE-I-019~021) |
| 0.3 | 2026-03-03 22:40 | AI Assistant | CAN → LIN 通信变更：更新所有 CAN 接口为 LIN 接口，新增 LIN 主站功能 (SWE-I-022~024) |

---

## 10. 下一步行动

- [ ] 评审软件需求规格 (TL/PM/QA)
- [ ] 更新需求追溯矩阵，添加 SWE.1 详细追溯
- [ ] 基于 SRSW 开展软件架构详细设计 (SWE.2/3)
- [ ] 制定软件单元测试计划 (SWE.4)

---

**Generated by ECH ECU Development Team**
