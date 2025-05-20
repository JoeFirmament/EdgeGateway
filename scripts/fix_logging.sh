#!/bin/bash

# 定义颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}==============================================${NC}"
echo -e "${GREEN}RK3588 摄像头服务器日志系统修复工具${NC}"
echo -e "${BLUE}==============================================${NC}"

# 检查源码目录
SRC_DIR="src"
if [ ! -d "$SRC_DIR" ]; then
    echo -e "${RED}错误: 未找到源码目录 $SRC_DIR${NC}"
    exit 1
fi

echo -e "${YELLOW}开始扫描直接使用 std::cerr 输出的代码...${NC}"

# 查找包含 std::cerr 的文件
echo -e "\n${BLUE}包含 std::cerr 的文件:${NC}"
FILES_WITH_CERR=$(grep -r "std::cerr" --include="*.cpp" --include="*.h" $SRC_DIR | grep -v "include <iostream>" | cut -d: -f1 | sort | uniq)

if [ -z "$FILES_WITH_CERR" ]; then
    echo -e "${GREEN}未找到使用 std::cerr 的代码${NC}"
else
    for file in $FILES_WITH_CERR; do
        CERR_COUNT=$(grep -c "std::cerr" $file)
        echo -e "${YELLOW}$file${NC} - 包含 ${RED}$CERR_COUNT${NC} 处 std::cerr 使用"
    done
    
    echo -e "\n${BLUE}示例修复:${NC}"
    echo -e "${YELLOW}替换前:${NC} std::cerr << \"[模块][文件:函数] 消息\" << std::endl;"
    echo -e "${GREEN}替换后:${NC} LOG_DEBUG(\"消息\", \"模块\");"
    
    echo -e "\n${BLUE}建议操作:${NC}"
    echo -e "1. 使用以下命令将 std::cerr 输出替换为 LOG_ 宏:"
    echo -e "   ${GREEN}grep -r \"std::cerr\" --include=\"*.cpp\" $SRC_DIR | less${NC}"
    echo -e "2. 逐个文件修改，确保将输出重定向到标准日志系统"
    echo -e "3. 根据日志级别选择合适的宏: LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR 或 LOG_CRITICAL"
fi

echo -e "\n${YELLOW}开始扫描直接使用 std::cout 输出的代码...${NC}"

# 查找包含 std::cout 的文件
echo -e "\n${BLUE}包含 std::cout 的文件:${NC}"
FILES_WITH_COUT=$(grep -r "std::cout" --include="*.cpp" --include="*.h" $SRC_DIR | grep -v "include <iostream>" | cut -d: -f1 | sort | uniq)

if [ -z "$FILES_WITH_COUT" ]; then
    echo -e "${GREEN}未找到使用 std::cout 的代码${NC}"
else
    for file in $FILES_WITH_COUT; do
        COUT_COUNT=$(grep -c "std::cout" $file)
        echo -e "${YELLOW}$file${NC} - 包含 ${RED}$COUT_COUNT${NC} 处 std::cout 使用"
    done
    
    echo -e "\n${BLUE}示例修复:${NC}"
    echo -e "${YELLOW}替换前:${NC} std::cout << \"[模块][文件:函数] 消息\" << std::endl;"
    echo -e "${GREEN}替换后:${NC} LOG_INFO(\"消息\", \"模块\");"
fi

echo -e "\n${BLUE}==============================================${NC}"
echo -e "${GREEN}扫描完成!${NC}"
echo -e "${BLUE}==============================================${NC}" 