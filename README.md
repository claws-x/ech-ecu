# ECH 水加热器 ECU 控制系统

**项目版本**: 0.7  
**创建日期**: 2026-03-02  
**ASPICE 目标**: CL2  
**项目状态**: 单元实现阶段  
**最后更新**: 2026-03-04 13:45

---

## 1. 项目概述

ECH (Electric Cabin Heater) 水加热器 ECU 是用于电动汽车座舱加热系统的控制单元，通过 PTC 加热器加热冷却液，为座舱提供暖风。

### 1.1 主要功能
- 温度闭环控制（目标精度 ±2°C）
- PWM 占空比调节（0-100%，20kHz）
- 多重安全保护（过温、过流、通信丢失）
- CAN 通信（整车网络集成）
- UDS 诊断支持

### 1.2 目标平台
- MCU: RH850 / AURIX（待定）
- 电源: 12V 车载系统
- 工作温度: -40°C ~ +85°C
- 防护等级: IP67

---

## 2. 项目结构

```
ech-ecu/
├── docs/
│   ├── system/           # 系统工程文档
│   │   ├── SyRS_ECH.md              # 系统需求规格
│   │   └── System_Architecture_ECH.md  # 系统架构（待创建）
│   ├── software/         # 软件工程文档
│   │   └── SW_Architecture_ECH.md   # 软件架构设计
│   ├── test/             # 测试文档
│   │   └── System_Test_Plan.md      # 系统测试计划
│   ├── process/          # 过程文档
│   │   └── ASPICE_Gap_Analysis.md   # ASPICE 差距分析
│   └── trace-matrix/     # 追踪矩阵（待创建）
├── src/
│   ├── app/              # 应用层代码
│   │   ├── ech_temp_ctrl.c          # 温度控制模块
│   │   └── ech_state.c/.h           # 状态管理模块 (v0.1)
│   ├── drivers/          # 硬件驱动
│   │   ├── ech_pwm.c/.h             # PWM 驱动
│   │   ├── ech_adc.c/.h             # ADC 驱动 (v0.1)
│   │   └── ech_lin.c/.h             # LIN 驱动 (v0.1)
│   ├── services/         # 服务层
│   │   ├── ech_pid.c/.h             # PID 控制服务 (v0.1)
│   │   └── ech_watchdog.c/.h        # 看门狗服务 (v0.1)
│   └── platform/         # 平台相关（待创建）
└── tests/                # 测试代码（待创建）
```

---

## 3. 文档清单

### 3.1 已完成文档

| 文档 | 路径 | 过程域 | 状态 |
|------|------|--------|------|
| 系统需求规格 | `docs/system/SyRS_ECH.md` | SYS.1 | ✅ v0.1 |
| **系统架构设计** | `docs/system/System_Architecture_ECH.md` | SYS.2 | ✅ v0.1 |
| **软件需求规格** | `docs/software/SRSW_ECH.md` | SWE.1 | ✅ v0.1 |
| 软件架构设计 | `docs/software/SW_Architecture_ECH.md` | SWE.2/3 | ✅ v0.1 |
| 系统测试计划 | `docs/test/System_Test_Plan.md` | SYS.4/5, SWE.4/5/6 | ✅ v0.1 |
| ASPICE 差距分析 | `docs/process/ASPICE_Gap_Analysis.md` | SUP.1 | ✅ v0.1 |
| 质量保证计划 | `docs/process/Quality_Assurance_Plan.md` | SUP.1 | ✅ v0.1 |
| 项目计划 | `docs/process/Project_Plan.md` | MAN.3 | ✅ v0.1 |
| **需求追溯矩阵** | `docs/trace-matrix/Trace_Matrix_SYS-SWE.md` | SYS.1/SWE.1 | ✅ v0.1 |
| **配置管理计划** | `docs/process/Configuration_Management_Plan.md` | SUP.8 | ✅ v0.1 |
| **功能安全计划** | `docs/process/Functional_Safety_Plan.md` | ISO 26262-2 | ✅ v0.1 |
| **HARA 报告** | `docs/safety/HARA_Report.md` | ISO 26262-3 | ✅ v0.1 |

### 3.2 待创建文档

| 文档 | 路径 | 过程域 | 优先级 |
|------|------|--------|--------|
| 功能安全概念 | `docs/safety/Functional_Safety_Concept.md` | ISO 26262-3 | 高 |
| 技术安全概念 | `docs/safety/Technical_Safety_Concept.md` | ISO 26262-4 | 高 |
| 软件详细设计 | `docs/software/SW_Detailed_Design/` | SWE.3 | 中 |
| 评审计划 | `docs/process/Review_Plan.md` | SUP.1 | 中 |

---

## 4. 代码清单

### 4.1 已完成模块

| 模块 | 路径 | 功能 | 单元测试 |
|------|------|------|----------|
| PWM 驱动 | `src/drivers/ech_pwm.c/.h` | PTC 加热器 PWM 控制 | ✅ 内置桩代码 |
| 温度控制 | `src/app/ech_temp_ctrl.c` | 温度闭环控制 + 安全保护 | ✅ 内置桩代码 |
| **PID 服务** | `src/services/ech_pid.c/.h` | PID 算法库 (温度控制核心) | ✅ 5 项测试通过 |
| **ADC 驱动** | `src/drivers/ech_adc.c/.h` | 温度/电压/电流采样 + 断线检测 | ✅ 6 项测试通过 |
| **状态管理** | `src/app/ech_state.c/.h` | 系统状态机 + 故障管理 | ✅ 6 项测试通过 |
| **LIN 驱动** | `src/drivers/ech_lin.c/.h` | LIN 2.1 主站通信 (19.2kbps) | ✅ 6 项测试通过 |
| **看门狗服务** | `src/services/ech_watchdog.c/.h` | 看门狗管理 + 系统健康监控 | ✅ 6 项测试通过 |

### 4.2 待开发模块

| 模块 | 路径 | 功能 | 优先级 |
|------|------|------|--------|
| 诊断服务 | `src/services/ech_diag.c/.h` | 故障诊断 + DTC | 中 |

---

## 5. 快速开始

### 5.1 编译示例代码（SIL 测试）

```bash
cd ech-ecu

# 编译 PWM 驱动单元测试
gcc -DUNIT_TEST -o test_pwm src/drivers/ech_pwm.c
./test_pwm

# 编译 PID 服务单元测试
gcc -DUNIT_TEST -o test_ech_pid src/services/ech_pid.c
./test_ech_pid

# 编译 ADC 驱动单元测试
gcc -DUNIT_TEST -o test_ech_adc src/drivers/ech_adc.c -lm
./test_ech_adc

# 编译状态管理单元测试
gcc -DUNIT_TEST -o test_ech_state src/app/ech_state.c
./test_ech_state

# 编译 LIN 驱动单元测试
gcc -DUNIT_TEST -o test_ech_lin src/drivers/ech_lin.c
./test_ech_lin

# 编译看门狗服务单元测试 (新增)
gcc -DUNIT_TEST -o test_ech_watchdog src/services/ech_watchdog.c
./test_ech_watchdog

# 编译温度控制单元测试
gcc -DUNIT_TEST -I src/drivers -I src/services \
    -o test_temp_ctrl src/app/ech_temp_ctrl.c src/drivers/ech_pwm.c
./test_temp_ctrl
```

### 5.2 查看文档

```bash
# 查看系统需求
cat docs/system/SyRS_ECH.md

# 查看软件架构
cat docs/software/SW_Architecture_ECH.md

# 查看测试计划
cat docs/test/System_Test_Plan.md

# 查看 ASPICE 差距分析
cat docs/process/ASPICE_Gap_Analysis.md
```

---

## 6. ASPICE 符合性状态

| 过程域 | 状态 | 证据 |
|--------|------|------|
| SYS.1 系统需求分析 | ✅ 满足 | SyRS_ECH.md |
| SYS.2 系统架构设计 | ✅ 满足 | System_Architecture_ECH.md |
| SYS.4/5 系统测试 | ✅ 已规划 | System_Test_Plan.md |
| SWE.1 软件需求 | ✅ 满足 | SRSW_ECH.md |
| SWE.2 软件架构 | ✅ 满足 | SW_Architecture_ECH.md |
| SWE.3 详细设计 | ✅ 满足 | ech_adc.c + ech_pid.c + ech_state.c + ech_lin.c + ech_watchdog.c |
| SWE.4 单元测试 | ✅ 满足 | 29 项测试通过 (PID:5 + ADC:6 + State:6 + LIN:6 + WDG:6) |
| SUP.1 质量保证 | ✅ 满足 | Quality_Assurance_Plan.md |
| SUP.8 配置管理 | ✅ 满足 | Configuration_Management_Plan.md |

**当前评级**: CL1 部分满足  
**目标评级**: CL2  
**详见**: `docs/process/ASPICE_Gap_Analysis.md`

---

## 7. 下一步行动

### 第 1 周优先级

1. [x] 创建系统架构独立文档 (GAP-001) ✅
2. [x] 创建配置管理计划 (GAP-002) ✅
3. [x] 创建质量保证计划 (GAP-003) ✅
4. [x] 建立需求追踪矩阵 (GAP-005) ✅
5. [x] 搭建独立单元测试框架 (GAP-004) ✅
6. [x] 创建项目计划 (GAP-006) ✅

### 待开发代码模块

1. [x] **PID 服务** (`src/services/ech_pid.c`) ✅ 2026-03-04 07:30 完成
2. [x] **ADC 驱动** (`src/drivers/ech_adc.c`) ✅ 2026-03-04 08:35 完成
3. [x] **状态管理** (`src/app/ech_state.c`) ✅ 2026-03-04 11:00 完成
4. [x] **LIN 驱动** (`src/drivers/ech_lin.c`) ✅ 2026-03-04 12:45 完成
5. [x] **看门狗服务** (`src/services/ech_watchdog.c`) ✅ 2026-03-04 13:45 完成
6. [ ] 主程序框架 (`src/app/ech_main.c`)

---

## 8. 修订历史

| 版本 | 日期 | 作者 | 变更描述 |
|------|------|------|----------|
| 0.1 | 2026-03-02 | AI Assistant | 初始项目框架 |
| 0.2 | 2026-03-03 | AI Assistant | CAN→LIN 通信变更，文档完善 |
| 0.3 | 2026-03-04 | AI Assistant | PID 服务模块完成，SWE.4 单元测试通过 |
| 0.4 | 2026-03-04 | AI Assistant | ADC 驱动模块完成，11 项单元测试通过 |
| 0.5 | 2026-03-04 | AI Assistant | 状态管理模块完成，17 项单元测试通过 |
| 0.6 | 2026-03-04 | AI Assistant | LIN 驱动模块完成，23 项单元测试通过 |
| 0.7 | 2026-03-04 | AI Assistant | 看门狗服务完成，29 项单元测试通过 |

---

## 9. 联系方式

- 项目仓库：`/Users/aiagent_master/.openclaw/workspace/ech-ecu`
- 问题反馈：使用 GitHub Issues 或内部问题跟踪系统

---

**Generated by ECH ECU Development Team**
