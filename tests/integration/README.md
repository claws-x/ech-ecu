# ECH ECU 集成测试套件

本目录包含 ECH ECU 项目的集成测试 (SWE.5) 实现代码。

## 目录结构

```
tests/integration/
├── Integration_Test_Plan.md    # 集成测试计划文档
├── README.md                   # 本文件
├── suites/                     # 测试套件 (按集成阶段组织)
│   ├── phase1_temp_pwm/        # Phase 1: 温度控制 + PWM 驱动
│   ├── phase2_temp_sampling/   # Phase 2: 温度采集集成
│   ├── phase3_state_machine/   # Phase 3: 状态机集成
│   ├── phase4_lin_comm/        # Phase 4: LIN 通信集成
│   └── phase5_fault_diag/      # Phase 5: 故障诊断集成
├── fixtures/                   # 测试夹具和桩代码
│   ├── stub_pwm.c/h            # PWM 硬件桩
│   ├── stub_adc.c/h            # ADC 硬件桩
│   ├── stub_lin.c/h            # LIN 硬件桩
│   ├── stub_dio.c/h            # DIO 硬件桩
│   ├── stub_timer.c/h          # 定时器桩
│   ├── fixture_temp_control.c/h
│   ├── fixture_state_machine.c/h
│   └── fixture_lin_comm.c/h
├── reports/                    # 测试报告
│   └── template.md             # 报告模板
└── scripts/                    # 测试脚本
    ├── run_integration_tests.sh
    ├── run_phase1_tests.sh
    ├── generate_coverage_report.sh
    └── hil_test_runner.py
```

## 快速开始

### SIL 环境运行测试

```bash
# 运行全部集成测试
./scripts/run_integration_tests.sh

# 运行特定阶段测试
./scripts/run_phase1_tests.sh

# 生成覆盖率报告
./scripts/generate_coverage_report.sh
```

### HIL 环境运行测试

```bash
# 需要 dSPACE/NI 实时仿真器环境
python3 scripts/hil_test_runner.py --phase 4
```

## 测试框架

- **SIL**: CMocka (C 语言单元测试框架)
- **HIL**: Python + pytest (控制实时仿真器)

## 编写新测试用例

1. 在对应 `suites/phaseX_*/` 目录下创建测试文件
2. 遵循命名规范：`test_<feature>_<scenario>.c`
3. 在 `CMakeLists.txt` 中注册测试
4. 更新 `Integration_Test_Plan.md` 中的用例列表

## 参考文档

- `Integration_Test_Plan.md` - 集成测试计划和场景定义
- `docs/test/System_Test_Plan.md` - 系统测试计划
- `docs/software/SRSW_ECH.md` - 软件需求规格
