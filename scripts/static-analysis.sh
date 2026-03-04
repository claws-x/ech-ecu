#!/bin/bash
# ECH ECU 静态分析脚本
# 用法：./static-analysis.sh [--full]

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC_DIR="$PROJECT_ROOT/src"
REPORT_DIR="$PROJECT_ROOT/docs/process-reports"
DATE=$(date +%Y%m%d)

echo "🔍 ECH ECU 静态分析"
echo "=================="
echo ""

# 创建报告目录
mkdir -p "$REPORT_DIR"

# 检查工具
check_tool() {
    if ! command -v "$1" &> /dev/null; then
        echo "⚠️  未安装：$1"
        return 1
    fi
    echo "✅ 已安装：$1"
    return 0
}

echo "📋 工具检查:"
check_tool cppcheck || true
check_tool clang-tidy || true
check_tool clang-format || true
echo ""

# Cppcheck 分析
if command -v cppcheck &> /dev/null; then
    echo "🔎 运行 cppcheck..."
    CPPCHECK_REPORT="$REPORT_DIR/cppcheck-report-$DATE.txt"
    
    cppcheck \
        --enable=all \
        --std=c99 \
        --platform=unix64 \
        --suppress=missingIncludeSystem \
        --output-format=gcc \
        -i"$PROJECT_ROOT/build" \
        "$SRC_DIR" \
        2> "$CPPCHECK_REPORT" || true
    
    ISSUE_COUNT=$(wc -l < "$CPPCHECK_REPORT" | tr -d ' ')
    echo "  发现问题：$ISSUE_COUNT"
    echo "  报告文件：$CPPCHECK_REPORT"
    echo ""
else
    echo "⚠️  跳过 cppcheck（未安装）"
fi

# Clang-tidy 分析
if command -v clang-tidy &> /dev/null; then
    echo "🔎 运行 clang-tidy..."
    # 需要 compile_commands.json
    if [[ -f "$PROJECT_ROOT/build/compile_commands.json" ]]; then
        find "$SRC_DIR" -name "*.c" | head -5 | while read -r src; do
            clang-tidy "$src" -- -I"$SRC_DIR" || true
        done
    else
        echo "  ⚠️  缺少 compile_commands.json，跳过"
    fi
    echo ""
else
    echo "⚠️  跳过 clang-tidy（未安装）"
fi

# 代码风格检查
if command -v clang-format &> /dev/null; then
    echo "📐 检查代码风格..."
    STYLE_ISSUES=0
    find "$SRC_DIR" -name "*.c" -o -name "*.h" | while read -r src; do
        if ! clang-format --dry-run --Werror "$src" 2>/dev/null; then
            STYLE_ISSUES=$((STYLE_ISSUES + 1))
            echo "  格式问题：$src"
        fi
    done
    echo ""
else
    echo "⚠️  跳过 clang-format（未安装）"
fi

echo "✅ 静态分析完成"
echo ""
echo "📄 报告位置：$REPORT_DIR/"
