# ECH ECU 项目 ASPICE CL2 差距分析报告

**文档版本**: 0.1  
**创建日期**: 2026-03-02  
**过程域**: SUP.1 质量保证 / SUP.8 配置管理  
**状态**: 草案  
**评估基准**: Automotive SPICE 3.1 CL2

---

## 1. 评估范围

### 1.1 评估对象
- ECH 水加热器 ECU 控制系统
- 当前项目阶段：需求分析 → 详细设计 → 单元实现（早期）

### 1.2 评估过程域
本次评估聚焦以下过程域：

| 过程组 | 过程域 | 评估状态 |
|--------|--------|----------|
| 系统工程 | SYS.1 系统需求分析 | ✅ 已覆盖 |
| 系统工程 | SYS.2 系统架构设计 | ⚠️ 部分覆盖 |
| 系统工程 | SYS.3 系统集成 | ❌ 未开始 |
| 系统工程 | SYS.4 系统集成测试 | ✅ 已规划 |
| 系统工程 | SYS.5 系统确认测试 | ✅ 已规划 |
| 软件工程 | SWE.1 软件需求分析 | ✅ 已覆盖 |
| 软件工程 | SWE.2 软件架构设计 | ✅ 已覆盖 |
| 软件工程 | SWE.3 软件详细设计 | ✅ 已覆盖 |
| 软件工程 | SWE.4 软件单元测试 | ⚠️ 部分覆盖 |
| 软件工程 | SWE.5 软件集成测试 | ⚠️ 部分覆盖 |
| 软件工程 | SWE.6 软件确认测试 | ✅ 已规划 |
| 支持过程 | SUP.1 质量保证 | ⚠️ 部分覆盖 |
| 支持过程 | SUP.8 配置管理 | ❌ 未建立 |
| 支持过程 | SUP.9 问题解决 | ❌ 未建立 |
| 支持过程 | SUP.10 变更请求 | ❌ 未建立 |
| 管理过程 | MAN.3 项目管理 | ⚠️ 部分覆盖 |

---

## 2. 已有工作产品清单

### 2.1 系统工程 (SYS)

| 工作产品 | 文件路径 | 状态 | 备注 |
|----------|----------|------|------|
| SyRS (系统需求规格) | `docs/system/SyRS_ECH.md` | ✅ 完整 | 覆盖 SYS.1 |
| 系统架构草案 | `docs/system/SyRS_ECH.md` §3 | ⚠️ 简化 | 需独立文档 (SYS.2) |
| 系统测试计划 | `docs/test/System_Test_Plan.md` | ✅ 完整 | 覆盖 SYS.4/5 |
| 需求追踪矩阵 | `docs/system/SyRS_ECH.md` §5 | ⚠️ 初步 | 需独立维护 |

### 2.2 软件工程 (SWE)

| 工作产品 | 文件路径 | 状态 | 备注 |
|----------|----------|------|------|
| SRSW (软件需求规格) | `docs/software/SW_Architecture_ECH.md` §2 | ✅ 完整 | 覆盖 SWE.1 |
| 软件架构设计 | `docs/software/SW_Architecture_ECH.md` | ✅ 完整 | 覆盖 SWE.2 |
| 软件详细设计 | `docs/software/SW_Architecture_ECH.md` §3 | ✅ 完整 | 覆盖 SWE.3 |
| 源代码 | `src/drivers/ech_pwm.c`, `src/app/ech_temp_ctrl.c` | ⚠️ 部分 | 仅示例模块 |
| 单元测试代码 | 嵌入源文件 `#ifdef UNIT_TEST` | ⚠️ 初步 | 需独立测试框架 |
| 软件测试计划 | `docs/test/System_Test_Plan.md` §3-4 | ✅ 完整 | 覆盖 SWE.4/5/6 |

### 2.3 支持过程 (SUP)

| 工作产品 | 文件路径 | 状态 | 备注 |
|----------|----------|------|------|
| 质量保证计划 | - | ❌ 缺失 | 需创建 |
| 配置管理计划 | - | ❌ 缺失 | 需创建 |
| 问题跟踪记录 | - | ❌ 缺失 | 需创建 |
| 变更请求记录 | - | ❌ 缺失 | 需创建 |

### 2.4 管理过程 (MAN)

| 工作产品 | 文件路径 | 状态 | 备注 |
|----------|----------|------|------|
| 项目计划 | - | ❌ 缺失 | 需创建 |
| 项目进度报告 | - | ❌ 缺失 | 需创建 |
| 风险评估记录 | - | ❌ 缺失 | 需创建 |

---

## 3. 差距分析详情

### 3.1 高优先级差距 (必须满足 CL2)

| ID | 差距描述 | 影响过程域 | 建议行动 | 优先级 |
|----|----------|------------|----------|--------|
| GAP-001 | 缺少独立的系统架构设计文档 | SYS.2 | 创建 `docs/system/System_Architecture_ECH.md`，包含硬件/软件接口、状态机、信号流 | 高 |
| GAP-002 | 缺少配置管理计划与版本控制策略 | SUP.8 | 创建 `docs/process/Configuration_Management_Plan.md`，定义 Git 分支策略、版本命名规则 | 高 |
| GAP-003 | 缺少质量保证计划 | SUP.1 | 创建 `docs/process/Quality_Assurance_Plan.md`，定义评审计划、审计节点 | 高 |
| GAP-004 | 单元测试未使用独立测试框架 | SWE.4 | 集成 CMocka/Unity 框架，将单元测试从源文件分离到 `tests/unit/` | 高 |
| GAP-005 | 需求追踪矩阵未独立维护 | SYS.1/SWE.1 | 创建 `docs/trace-matrix/Trace_Matrix_SYS-SWE.xlsx` 或 Markdown 版本 | 高 |
| GAP-006 | 缺少项目计划与进度跟踪 | MAN.3 | 创建 `docs/process/Project_Plan.md`，定义里程碑、资源、风险 | 高 |

### 3.2 中优先级差距 (建议完善)

| ID | 差距描述 | 影响过程域 | 建议行动 | 优先级 |
|----|----------|------------|----------|--------|
| GAP-007 | 缺少问题跟踪系统 | SUP.9 | 建立 GitHub Issues 或 JIRA 工作流，创建 `docs/process/Issue_Log.md` | 中 |
| GAP-008 | 缺少变更请求管理流程 | SUP.10 | 创建变更请求模板 `docs/process/Change_Request_Template.md` | 中 |
| GAP-009 | 软件集成测试用例不够详细 | SWE.5 | 扩充 `docs/test/System_Test_Plan.md` §4，增加接口测试、数据流测试 | 中 |
| GAP-010 | 缺少编码规范引用 | SWE.3 | 在架构文档中引用 MISRA-C:2012 规范，创建 `docs/coding/MISRA_Checklist.md` | 中 |
| GAP-011 | 缺少静态分析计划 | SWE.4 | 集成 PC-lint/Cppcheck，创建 `docs/test/Static_Analysis_Plan.md` | 中 |

### 3.3 低优先级差距 (可选增强)

| ID | 差距描述 | 影响过程域 | 建议行动 | 优先级 |
|----|----------|------------|----------|--------|
| GAP-012 | 缺少代码审查记录模板 | SUP.1 | 创建 `docs/process/Code_Review_Template.md` | 低 |
| GAP-013 | 缺少度量指标定义 | MAN.3 | 定义 KPI（需求稳定性、缺陷密度、测试覆盖率等） | 低 |
| GAP-014 | 缺少工具资质记录 | SUP.8 | 对编译器、测试工具进行资质评估 | 低 |

---

## 4. 建议行动路线图

### 4.1 第 1 周（立即执行）

| 行动项 | 负责人 | 输出物 | 验收标准 |
|--------|--------|--------|----------|
| 创建系统架构独立文档 | 系统工程师 | `docs/system/System_Architecture_ECH.md` | 包含硬件接口、信号定义、状态机 |
| 创建配置管理计划 | 项目经理 | `docs/process/Configuration_Management_Plan.md` | 定义 Git 流、版本规则、基线策略 |
| 建立需求追踪矩阵 | 系统工程师 | `docs/trace-matrix/Trace_Matrix_v0.1.md` | 覆盖所有 SYS-F/SYS-S 需求 |
| 搭建单元测试框架 | 开发工程师 | `tests/unit/` 目录 + CMakeLists.txt | 可编译运行现有单元测试 |

### 4.2 第 2 周

| 行动项 | 负责人 | 输出物 | 验收标准 |
|--------|--------|--------|----------|
| 创建质量保证计划 | 质量工程师 | `docs/process/Quality_Assurance_Plan.md` | 定义评审节点、审计计划 |
| 创建项目计划 | 项目经理 | `docs/process/Project_Plan.md` | 包含里程碑、资源、风险清单 |
| 完善集成测试用例 | 测试工程师 | `docs/test/Integration_Test_Plan.md` | 覆盖所有模块接口 |
| 建立问题跟踪流程 | 质量工程师 | `docs/process/Issue_Management_Process.md` | 定义问题分类、优先级、关闭标准 |

### 4.3 第 3 周

| 行动项 | 负责人 | 输出物 | 验收标准 |
|--------|--------|--------|----------|
| 补充完整驱动代码 | 开发工程师 | `src/drivers/` 下所有驱动 | ADC、CAN、DIO、Watchdog |
| 执行静态分析 | 开发工程师 | `docs/test/Static_Analysis_Report_v1.md` | 无 MISRA 关键违规 |
| 创建变更管理流程 | 质量工程师 | `docs/process/Change_Management_Process.md` | 定义 CR 提交、评估、批准流程 |
| 第一次内部评审 | 全体 | `docs/process/Review_Record_001.md` | 评审 SyRS、SRSW、架构 |

---

## 5. ASPICE CL2 符合性检查表

### 5.1 系统工程

| 实践 | 检查项 | 状态 | 证据 |
|------|--------|------|------|
| SYS.1.BP1 | 定义系统需求范围 | ✅ | SyRS_ECH.md §1 |
| SYS.1.BP2 | 定义系统功能需求 | ✅ | SyRS_ECH.md §2.1 |
| SYS.1.BP3 | 定义系统非功能需求 | ✅ | SyRS_ECH.md §2.2-2.5 |
| SYS.1.BP4 | 需求可测试性 | ✅ | SyRS_ECH.md §2 含验收标准 |
| SYS.1.BP5 | 需求双向追踪 | ⚠️ | 追踪矩阵初步，需完善 |
| SYS.1.BP6 | 需求评审 | ❌ | 无评审记录 |
| SYS.2.BP1 | 定义系统架构 | ⚠️ | 架构在 SyRS 中简化描述 |
| SYS.2.BP2 | 分配需求到架构元素 | ⚠️ | 需明确分配关系 |
| SYS.2.BP3 | 定义接口 | ✅ | SyRS_ECH.md §3 |
| SYS.2.BP4 | 架构评审 | ❌ | 无评审记录 |

### 5.2 软件工程

| 实践 | 检查项 | 状态 | 证据 |
|------|--------|------|------|
| SWE.1.BP1 | 定义软件需求 | ✅ | SW_Architecture_ECH.md §2 |
| SWE.1.BP2 | 需求可测试性 | ✅ | 含验收标准 |
| SWE.1.BP3 | 需求双向追踪 | ⚠️ | 追踪矩阵初步 |
| SWE.2.BP1 | 定义软件架构 | ✅ | SW_Architecture_ECH.md §1 |
| SWE.2.BP2 | 分配需求到模块 | ✅ | SW_Architecture_ECH.md §3.1 |
| SWE.2.BP3 | 定义接口 | ✅ | SW_Architecture_ECH.md §3.2 |
| SWE.2.BP4 | 架构评审 | ❌ | 无评审记录 |
| SWE.3.BP1 | 详细设计 | ✅ | SW_Architecture_ECH.md §3.2 |
| SWE.3.BP2 | 设计可测试性 | ✅ | 含单元测试桩 |
| SWE.4.BP1 | 单元测试策略 | ⚠️ | 单元测试嵌入源文件 |
| SWE.4.BP2 | 测试用例设计 | ✅ | System_Test_Plan.md §3 |
| SWE.4.BP3 | 测试执行 | ❌ | 未执行 |
| SWE.4.BP4 | 测试覆盖率分析 | ❌ | 无覆盖率工具集成 |

### 5.3 支持过程

| 实践 | 检查项 | 状态 | 证据 |
|------|--------|------|------|
| SUP.1.BP1 | 质量保证计划 | ❌ | 无 QA 计划 |
| SUP.1.BP2 | 过程符合性检查 | ❌ | 无审计记录 |
| SUP.1.BP3 | 不符合项管理 | ❌ | 无问题跟踪 |
| SUP.8.BP1 | 配置管理计划 | ❌ | 无 CM 计划 |
| SUP.8.BP2 | 配置标识 | ⚠️ | 有文件结构，无版本规则 |
| SUP.8.BP3 | 变更控制 | ❌ | 无变更流程 |
| SUP.8.BP4 | 基线管理 | ❌ | 无基线定义 |

---

## 6. 风险评估

| 风险 ID | 风险描述 | 影响 | 可能性 | 缓解措施 |
|---------|----------|------|--------|----------|
| RISK-001 | 缺少正式评审流程，设计缺陷可能遗留到高阶段 | 高 | 中 | 立即建立评审计划，第 3 周执行首次评审 |
| RISK-002 | 单元测试框架未独立，难以自动化和集成 CI | 中 | 高 | 第 1 周完成 Unity/CMocka 集成 |
| RISK-003 | 配置管理缺失，版本混乱风险 | 高 | 中 | 第 1 周创建 CM 计划，定义 Git 流 |
| RISK-004 | 需求追踪不完整，ASPICE 审核不通过风险 | 高 | 中 | 第 1 周完善追踪矩阵 |

---

## 7. 结论

### 7.1 当前状态总结

| 类别 | 已完成 | 进行中 | 未开始 | 完成率 |
|------|--------|--------|--------|--------|
| 系统工程 (SYS.1-5) | 3 | 1 | 1 | 60% |
| 软件工程 (SWE.1-6) | 4 | 2 | 0 | 67% |
| 支持过程 (SUP.1/8/9/10) | 0 | 0 | 4 | 0% |
| 管理过程 (MAN.3) | 0 | 0 | 1 | 0% |
| **总体** | **7** | **3** | **6** | **44%** |

### 7.2 ASPICE CL2 就绪度评估

- **当前评级**: **CL1 部分满足**（工作产品已创建，但过程证据不足）
- **目标评级**: CL2
- **主要差距**: 
  1. 缺少过程定义文档（QA 计划、CM 计划、项目计划）
  2. 缺少评审与审计记录
  3. 缺少配置管理与变更控制流程
  4. 单元测试框架需完善

### 7.3 建议

1. **立即行动**（第 1 周）: 完成 GAP-001 至 GAP-006 高优先级差距
2. **过程建立**（第 2 周）: 建立 QA、CM、问题跟踪流程
3. **执行证据**（第 3 周+）: 执行评审、测试、静态分析，保留记录
4. **持续改进**: 每周更新差距分析，跟踪关闭率

---

## 8. 修订历史

| 版本 | 日期 | 作者 | 变更描述 |
|------|------|------|----------|
| 0.1 | 2026-03-02 | AI Assistant | 初始差距分析 |

---

**附录 A: 推荐文档结构**

```
ech-ecu/
├── docs/
│   ├── system/
│   │   ├── SyRS_ECH.md              # ✅ 已创建
│   │   └── System_Architecture_ECH.md  # ❌ 待创建
│   ├── software/
│   │   ├── SRSW_ECH.md                 # ⚠️ 合并到架构文档，建议分离
│   │   ├── SW_Architecture_ECH.md      # ✅ 已创建
│   │   └── SW_Detailed_Design/         # ❌ 待创建（关键模块详细设计）
│   ├── test/
│   │   ├── System_Test_Plan.md         # ✅ 已创建
│   │   ├── SW_Test_Plan.md             # ❌ 待创建（软件测试专项）
│   │   ├── Integration_Test_Plan.md    # ❌ 待创建
│   │   └── Test_Reports/               # ❌ 待创建
│   ├── trace-matrix/
│   │   ├── Trace_Matrix_SYS-SWE.md     # ❌ 待创建
│   │   └── Trace_Matrix_SWE-Test.md    # ❌ 待创建
│   ├── process/
│   │   ├── Project_Plan.md             # ❌ 待创建
│   │   ├── Quality_Assurance_Plan.md   # ❌ 待创建
│   │   ├── Configuration_Management_Plan.md  # ❌ 待创建
│   │   ├── Issue_Management_Process.md # ❌ 待创建
│   │   ├── Change_Management_Process.md # ❌ 待创建
│   │   ├── Daily_Report_Template.md    # ❌ 待创建
│   │   └── Weekly_Report_Template.md   # ❌ 待创建
│   └── coding/
│       └── MISRA_Checklist.md          # ❌ 待创建
├── src/
│   ├── app/
│   │   ├── ech_main.c                  # ❌ 待创建
│   │   └── ech_temp_ctrl.c             # ✅ 已创建
│   ├── drivers/
│   │   ├── ech_pwm.c/.h                # ✅ 已创建
│   │   ├── ech_adc.c/.h                # ❌ 待创建
│   │   ├── ech_can.c/.h                # ❌ 待创建
│   │   └── ech_dio.c/.h                # ❌ 待创建
│   ├── services/
│   │   ├── ech_pid.c/.h                # ❌ 待创建
│   │   ├── ech_diag.c/.h               # ❌ 待创建
│   │   └── ech_watchdog.c/.h           # ❌ 待创建
│   └── platform/
│       └── ech_init.c                  # ❌ 待创建
└── tests/
    ├── unit/                           # ❌ 待创建（独立单元测试）
    ├── integration/                    # ❌ 待创建
    └── coverage/                       # ❌ 待创建
```

---

**任务完成总结**:

✅ **已完成**:
1. `/sys` — 系统需求规格 (SyRS_ECH.md)
2. `/swe-arch` — 软件架构设计 (SW_Architecture_ECH.md)
3. `/dev` — PWM 驱动 + 温度控制示例代码
4. `/test` — 系统测试计划 (System_Test_Plan.md)
5. `/sqa` — ASPICE CL2 差距分析报告

📁 **项目结构已创建**: `ech-ecu/` 目录及子目录

📋 **下一步建议**:
1. 根据差距分析 GAP-001~GAP-006，优先创建缺失的过程文档
2. 补充完整驱动代码（ADC、CAN、DIO、Watchdog）
3. 搭建独立单元测试框架（Unity/CMocka）
4. 执行首次设计评审并保留记录

有任何需要调整或深化的部分，请告诉我！
