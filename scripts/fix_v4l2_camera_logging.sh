#!/bin/bash

# 定义颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}==============================================${NC}"
echo -e "${GREEN}修复 v4l2_camera.cpp 日志输出工具${NC}"
echo -e "${BLUE}==============================================${NC}"

V4L2_FILE="src/camera/v4l2_camera.cpp"

if [ ! -f "$V4L2_FILE" ]; then
    echo -e "${RED}错误: 未找到文件 $V4L2_FILE${NC}"
    exit 1
fi

# 备份原始文件
echo -e "${YELLOW}正在备份原始文件...${NC}"
cp "$V4L2_FILE" "${V4L2_FILE}.bak.$(date +%Y%m%d%H%M%S)"

echo -e "${YELLOW}开始修改文件...${NC}"

# 修改头文件部分，确保包含logger.h
sed -i '/#include.*iostream/a #include "monitor/logger.h"' "$V4L2_FILE"

# 修改日志输出，将std::cerr替换为LOG_DEBUG
# 注意：需要视情况调整正则表达式以匹配不同的日志格式
sed -i 's/std::cerr << "\[V4L2\]\[v4l2_camera\.cpp:\([^]]*\)\] \([^"]*\)" << std::endl;/LOG_DEBUG("\2", "V4L2Camera");/g' "$V4L2_FILE"
sed -i 's/std::cerr << "\[V4L2\]\[v4l2_camera\.cpp:\([^]]*\)\] \([^"]*\)"/LOG_DEBUG("\2", "V4L2Camera")/g' "$V4L2_FILE"

# 处理带有变量的行
# 这部分比较复杂，使用临时文件方式处理
grep -n "std::cerr.*\[V4L2\]" "$V4L2_FILE" | while read -r line; do
    line_num=$(echo "$line" | cut -d: -f1)
    line_content=$(echo "$line" | cut -d: -f2-)
    
    # 提取消息部分
    message=$(echo "$line_content" | sed -n 's/.*\[V4L2\]\[v4l2_camera\.cpp:[^]]*\] \([^<]*\).*/\1/p')
    
    # 如果有消息，提示用户手动修改
    if [ ! -z "$message" ]; then
        echo -e "${YELLOW}行 $line_num 需要手动修改:${NC}"
        echo -e "${RED}$line_content${NC}"
        echo -e "${GREEN}建议改为: LOG_DEBUG(\"$message...\", \"V4L2Camera\");${NC}"
        echo
    fi
done

echo -e "${GREEN}文件修改完成！${NC}"
echo -e "${YELLOW}注意：复杂的日志行需要手动修改，请参考上述建议。${NC}"
echo -e "${BLUE}==============================================${NC}"
echo -e "原始文件已备份为: ${GREEN}${V4L2_FILE}.bak.*${NC}"
echo -e "${BLUE}==============================================${NC}" 