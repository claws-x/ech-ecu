#!/bin/bash
# run_integration_tests.sh - ECH ECU 集成测试执行脚本
# 
# 用法:
#   ./run_integration_tests.sh [phase] [options]
#
# 参数:
#   phase: 测试阶段 (1|2|3|4|5|all), 默认: all
#   options: -v (详细输出), -c (生成覆盖率报告)

set -e

# 配置
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="$PROJECT_ROOT/build"
TEST_REPORTS_DIR="$SCRIPT_DIR/reports"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 显示用法
show_usage() {
    echo "ECH ECU 集成测试执行脚本"
    echo ""
    echo "用法: $0 [phase] [options]"
    echo ""
    echo "参数:"
    echo "  phase    测试阶段 (1|2|3|4|5|all), 默认：all"
    echo "  options:"
    echo "    -v     详细输出模式"
    echo "    -c     生成代码覆盖率报告"
    echo "    -h     显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  $0                  # 运行全部测试"
    echo "  $0 1                # 运行 Phase 1 测试"
    echo "  $0 3 -v             # 运行 Phase 3 测试 (详细模式)"
    echo "  $0 all -c           # 运行全部测试并生成覆盖率报告"
}

# 构建测试
build_tests() {
    local phase=$1
    
    log_info "构建集成测试 (Phase $phase)..."
    
    cd "$PROJECT_ROOT"
    
    if [ ! -d "$BUILD_DIR" ]; then
        mkdir -p "$BUILD_DIR"
    fi
    
    cd "$BUILD_DIR"
    
    # 配置 CMake (如果尚未配置)
    if [ ! -f "CMakeCache.txt" ]; then
        cmake .. -DBUILD_INTEGRATION_TESTS=ON
    fi
    
    # 编译
    if [ "$phase" = "all" ]; then
        make integration_tests -j$(nproc)
    else
        make integration_tests_phase${phase} -j$(nproc)
    fi
    
    log_info "构建完成"
}

# 运行单个阶段测试
run_phase_tests() {
    local phase=$1
    local verbose=$2
    
    log_info "运行 Phase $phase 集成测试..."
    
    local test_binary="$BUILD_DIR/tests/integration/phase${phase}/integration_test_phase${phase}"
    
    if [ ! -f "$test_binary" ]; then
        log_error "测试可执行文件不存在：$test_binary"
        return 1
    fi
    
    # 运行测试
    if [ "$verbose" = "true" ]; then
        "$test_binary" -v
    else
        "$test_binary"
    fi
    
    local result=$?
    
    if [ $result -eq 0 ]; then
        log_info "Phase $phase 测试通过 ✓"
    else
        log_error "Phase $phase 测试失败 ✗"
    fi
    
    return $result
}

# 生成覆盖率报告
generate_coverage() {
    log_info "生成代码覆盖率报告..."
    
    cd "$BUILD_DIR"
    
    # 使用 gcov/lcov 生成报告
    if command -v lcov &> /dev/null; then
        lcov --capture --directory . --output-file coverage.info
        genhtml coverage.info --output-directory coverage_report
        
        log_info "覆盖率报告已生成：$BUILD_DIR/coverage_report/index.html"
    else
        log_warn "lcov 未安装，跳过覆盖率报告生成"
        log_warn "安装方法：brew install lcov (macOS) 或 apt-get install lcov (Ubuntu)"
    fi
}

# 主函数
main() {
    local phase="all"
    local verbose="false"
    local coverage="false"
    
    # 解析参数
    while [[ $# -gt 0 ]]; do
        case $1 in
            1|2|3|4|5)
                phase="$1"
                shift
                ;;
            all)
                phase="all"
                shift
                ;;
            -v|--verbose)
                verbose="true"
                shift
                ;;
            -c|--coverage)
                coverage="true"
                shift
                ;;
            -h|--help)
                show_usage
                exit 0
                ;;
            *)
                log_error "未知参数：$1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    # 显示配置
    echo "========================================"
    echo "  ECH ECU 集成测试"
    echo "========================================"
    echo "测试阶段：$phase"
    echo "详细模式：$verbose"
    echo "覆盖率：$coverage"
    echo "========================================"
    echo ""
    
    # 构建测试
    build_tests "$phase"
    
    # 运行测试
    local overall_result=0
    
    if [ "$phase" = "all" ]; then
        for p in 1 2 3 4 5; do
            if ! run_phase_tests "$p" "$verbose"; then
                overall_result=1
            fi
            echo ""
        done
    else
        if ! run_phase_tests "$phase" "$verbose"; then
            overall_result=1
        fi
    fi
    
    # 生成覆盖率报告
    if [ "$coverage" = "true" ] && [ $overall_result -eq 0 ]; then
        generate_coverage
    fi
    
    # 总结
    echo ""
    echo "========================================"
    if [ $overall_result -eq 0 ]; then
        log_info "全部集成测试通过 ✓"
    else
        log_error "部分集成测试失败 ✗"
    fi
    echo "========================================"
    
    exit $overall_result
}

# 执行
main "$@"
