#!/bin/bash
# ECH ECU 版本发布脚本
# 用法：./release.sh --version v0.9.0

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION=""

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --version)
            VERSION="$2"
            shift 2
            ;;
        *)
            echo "未知参数：$1"
            echo "用法：./release.sh --version vX.Y.Z"
            exit 1
            ;;
    esac
done

if [[ -z "$VERSION" ]]; then
    echo "❌ 错误：必须指定版本号"
    echo "用法：./release.sh --version vX.Y.Z"
    exit 1
fi

echo "🚀 ECH ECU 版本发布"
echo "=================="
echo "版本号：$VERSION"
echo "日期：$(date +%Y-%m-%d)"
echo ""

# 1. 更新版本号
echo "📝 更新版本号..."
# 更新项目中的版本文件（如果存在）

# 2. 生成发布说明
RELEASE_NOTES="$PROJECT_ROOT/docs/process/Release_Notes_${VERSION}.md"
echo "📄 生成发布说明：$RELEASE_NOTES"

cat > "$RELEASE_NOTES" << EOF
# 发布说明 - $VERSION

**发布日期：** $(date +%Y-%m-%d)
**发布类型：** Minor Release

## 变更内容

### 新增功能
- 

### 改进
- 

### Bug 修复
- 

## 已知问题

## 升级步骤

1. 拉取最新代码：\`git pull\`
2. 清理构建：\`make clean\`
3. 重新编译：\`make\`

## 验证清单

- [ ] 构建成功
- [ ] 单元测试通过
- [ ] 集成测试通过
EOF

# 3. Git 操作提示
echo ""
echo "📌 下一步操作（手动执行）:"
echo "  1. git add -A"
echo "  2. git commit -m \"release: $VERSION\""
echo "  3. git tag $VERSION"
echo "  4. git push origin main --tags"
echo ""
echo "✅ 发布准备完成"
