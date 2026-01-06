#!/bin/bash
# TermKit 演示脚本

echo "================================================"
echo "        TermKit 终端多功能工具箱演示"
echo "================================================"
echo ""

# 设置颜色
RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
BLUE='\033[1;34m'
CYAN='\033[1;36m'
NC='\033[0m' # No Color

# 检查是否编译
if [ ! -f "termkit" ]; then
    echo -e "${YELLOW}正在编译项目...${NC}"
    make clean
    if ! make; then
        echo -e "${RED}编译失败！${NC}"
        exit 1
    fi
fi

echo -e "${GREEN}✓ 编译成功${NC}"
echo ""

# 显示版本信息
echo -e "${CYAN}[1] 显示版本和帮助信息${NC}"
echo "--------------------------------"
./termkit --version
echo ""
./termkit --help | head -20
echo ""

# 演示 tkls - 增强版ls
echo -e "${CYAN}[2] 演示 tkls - 增强版ls${NC}"
echo "--------------------------------"
echo -e "${BLUE}基本功能:${NC}"
./termkit tkls --version
echo ""
echo -e "${BLUE}查看当前目录 (默认):${NC}"
./termkit tkls | head -10
echo ""
echo -e "${BLUE}长格式显示:${NC}"
./termkit tkls -l | head -5
echo ""
echo -e "${BLUE}包含隐藏文件:${NC}"
./termkit tkls -a | head -5
echo ""

# 演示其他文件工具
echo -e "${CYAN}[3] 演示其他文件工具${NC}"
echo "--------------------------------"
for tool in tkcpmv tkfind tkdiff; do
    echo -e "${BLUE}$tool:${NC}"
    ./termkit $tool --help 2>&1 | head -3
    echo ""
done

# 演示文本处理工具
echo -e "${CYAN}[4] 演示文本处理工具${NC}"
echo "--------------------------------"
for tool in tkgrep tkstream tkview tkencode; do
    echo -e "${BLUE}$tool:${NC}"
    ./termkit $tool --help 2>&1 | head -3
    echo ""
done

# 演示系统工具
echo -e "${CYAN}[5] 演示系统信息工具${NC}"
echo "--------------------------------"
for tool in tkinfo tkmon tknet tkhw; do
    echo -e "${BLUE}$tool:${NC}"
    ./termkit $tool --help 2>&1 | head -3
    echo ""
done

# 演示开发工具
echo -e "${CYAN}[6] 演示开发辅助工具${NC}"
echo "--------------------------------"
for tool in tkcode tkconvert tkregex tkdebug; do
    echo -e "${BLUE}$tool:${NC}"
    ./termkit $tool --help 2>&1 | head -3
    echo ""
done

# 演示交互模式
echo -e "${CYAN}[7] 演示交互模式${NC}"
echo "--------------------------------"
echo -e "${BLUE}启动交互模式 (2秒后自动退出):${NC}"
timeout 2s ./termkit -i 2>/dev/null || true
echo ""

# 统计代码行数
echo -e "${CYAN}[8] 项目统计${NC}"
echo "--------------------------------"
echo -e "${BLUE}代码行数统计:${NC}"
if command -v cloc &> /dev/null; then
    cloc src/ --quiet | tail -5
else
    find src -name "*.c" -exec wc -l {} + | tail -1
fi
echo -e "${BLUE}总文件数:${NC}"
find src -name "*.c" | wc -l
echo ""

echo -e "${GREEN}================================================"
echo "        演示完成！"
echo ""
echo "主要功能:"
echo "  • tkls    - 彩色ls（带图标）"
echo "  • tkgrep  - 增强grep"
echo "  • tkinfo  - 系统信息"
echo "  • tkcode  - 代码统计"
echo ""
echo "使用命令:"
echo "  ./termkit [工具名]      # 使用单个工具"
echo "  ./termkit -i           # 进入交互模式"
echo "  ./termkit --help       # 查看所有工具"
echo "  make clean             # 清理编译文件"
echo "================================================"
echo -e "${NC}"