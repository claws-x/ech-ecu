#!/bin/bash
# ECH ECU 构建脚本
# 用法：./build.sh [--clean]

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
SRC_DIR="$PROJECT_ROOT/src"

echo "🔨 ECH ECU 构建脚本"
echo "=================="
echo "项目根目录：$PROJECT_ROOT"
echo ""

# 清理选项
if [[ "$1" == "--clean" ]]; then
    echo "🧹 清理构建目录..."
    rm -rf "$BUILD_DIR"
fi

# 创建构建目录
mkdir -p "$BUILD_DIR"

# 检测编译器
if command -v arm-none-eabi-gcc &> /dev/null; then
    CC="arm-none-eabi-gcc"
    echo "✅ 使用交叉编译器：$CC"
elif command -v gcc &> /dev/null; then
    CC="gcc"
    echo "⚠️  使用本地编译器：$CC (仅用于单元测试)"
else
    echo "❌ 错误：未找到编译器"
    exit 1
fi

# 编译各模块
echo ""
echo "📦 编译源文件..."

# 构建所有子目录的包含路径
INCLUDES="-I$SRC_DIR -I$SRC_DIR/drivers -I$SRC_DIR/services -I$SRC_DIR/app -I$SRC_DIR/platform"

find "$SRC_DIR" -name "*.c" | while read -r src; do
    obj="${src/$SRC_DIR/$BUILD_DIR}.o"
    mkdir -p "$(dirname "$obj")"
    echo "  → $src"
    $CC -c "$src" -o "$obj" $INCLUDES -Wall -Wextra
done

# 链接（如果有 main）
if [[ -f "$BUILD_DIR/app/ech_main.c.o" ]]; then
    echo ""
    echo "🔗 链接可执行文件..."
    # 收集所有 .o 文件
    OBJ_FILES=$(find "$BUILD_DIR" -name "*.o" | tr '\n' ' ')
    $CC $OBJ_FILES -o "$BUILD_DIR/ech_main.elf"
    echo "✅ 构建完成：$BUILD_DIR/ech_main.elf"
else
    echo "✅ 编译完成（无 main 入口）"
fi

echo ""
echo "📊 构建统计:"
echo "  源文件数：$(find "$SRC_DIR" -name "*.c" | wc -l | tr -d ' ')"
echo "  目标文件：$(find "$BUILD_DIR" -name "*.o" | wc -l | tr -d ' ')"
