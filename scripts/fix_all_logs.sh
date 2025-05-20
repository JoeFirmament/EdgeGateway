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

# 检查目录
if [ ! -d "src" ]; then
    echo -e "${RED}错误: 未找到源码目录 src${NC}"
    exit 1
fi

# 检查是否已备份
BACKUP_DIR="src.bak.$(date +%Y%m%d%H%M%S)"
if [ ! -d "$BACKUP_DIR" ]; then
    echo -e "${YELLOW}正在创建备份目录: $BACKUP_DIR${NC}"
    cp -r src $BACKUP_DIR
fi

echo -e "${YELLOW}开始扫描所有日志语句...${NC}"

# 查找所有使用日志宏的文件
echo -e "\n${BLUE}查找所有使用LOG_XXX宏的文件...${NC}"
FILES_WITH_LOGS=$(grep -r "LOG_" --include="*.cpp" --include="*.h" src | cut -d: -f1 | sort | uniq)

if [ -z "$FILES_WITH_LOGS" ]; then
    echo -e "${GREEN}未找到使用LOG_XXX宏的文件${NC}"
    exit 0
fi

echo -e "${YELLOW}找到以下文件使用了LOG_XXX宏:${NC}"
for file in $FILES_WITH_LOGS; do
    echo -e "  - ${BLUE}$file${NC}"
done

# 查找所有使用流式输出与LOG_XXX宏的组合的文件
echo -e "\n${BLUE}查找所有使用流式输出与LOG_XXX宏的组合的文件...${NC}"
FILES_WITH_STREAM_LOGS=$(grep -r "LOG_.*<<" --include="*.cpp" --include="*.h" src | cut -d: -f1 | sort | uniq)

if [ -z "$FILES_WITH_STREAM_LOGS" ]; then
    echo -e "${GREEN}未找到使用流式输出与LOG_XXX宏的组合的文件${NC}"
else
    echo -e "${YELLOW}以下文件使用了流式输出与LOG_XXX宏的组合:${NC}"
    for file in $FILES_WITH_STREAM_LOGS; do
        echo -e "  - ${RED}$file${NC}"
        
        # 显示每一行错误的日志使用
        echo -e "    ${YELLOW}错误的日志使用:${NC}"
        grep -n "LOG_.*<<" $file | while read -r line; do
            line_num=$(echo "$line" | cut -d: -f1)
            line_content=$(echo "$line" | cut -d: -f2-)
            echo -e "    ${BLUE}$line_num:${NC} $line_content"
            
            # 提取日志级别
            log_level=$(echo "$line_content" | grep -o "LOG_[A-Z]*")
            
            # 提取消息模板
            message=$(echo "$line_content" | sed -n 's/.*'"$log_level"'[ ]*(\([^,]*\).*/\1/p' | tr -d '"')
            
            # 提取模块名
            module=$(echo "$line_content" | sed -n 's/.*'"$log_level"'[ ]*([^,]*,[ ]*\([^)]*\).*/\1/p' | tr -d '"')
            
            # 提取流式输出的变量
            var=$(echo "$line_content" | sed -n 's/.*<<[ ]*\([^ ]*\).*/\1/p')
            
            echo -e "      ${GREEN}修改建议: ${NC}"
            echo -e "      ${GREEN}原始日志:${NC} $log_level(\"$message\", $module) << $var"
            echo -e "      ${GREEN}修改为:${NC} std::string log_msg = \"$message\" + std::to_string($var);"
            echo -e "      ${GREEN}        ${NC}$log_level(log_msg, $module);"
            echo
        done
    done
fi

echo -e "\n${BLUE}==============================================${NC}"
echo -e "${YELLOW}建议手动修改文件，确保每一处流式输出的日志都使用正确的方式${NC}"
echo -e "${YELLOW}修改方法:${NC}"
echo -e "1. 对于简单的LOG_XXX << var 语句，修改为:"
echo -e "   LOG_XXX(\"消息: \" + std::to_string(var), \"模块名\");"
echo -e ""
echo -e "2. 对于复杂的多行流式输出，修改为:"
echo -e "   std::string log_msg = \"消息:\"; "
echo -e "   log_msg += \"\\n  - 值1: \" + std::to_string(val1);"
echo -e "   log_msg += \"\\n  - 值2: \" + std::to_string(val2);"
echo -e "   LOG_XXX(log_msg, \"模块名\");"
echo -e "${BLUE}==============================================${NC}" 