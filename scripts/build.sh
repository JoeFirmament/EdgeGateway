#!/bin/bash

# 构建脚本

# 设置颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 设置构建类型
BUILD_TYPE="Debug"
BUILD_TESTS=ON
USE_OPENCV=OFF
USE_HARDWARE_ACCEL=ON

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    key="$1"
    case $key in
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --no-tests)
            BUILD_TESTS=OFF
            shift
            ;;
        --with-opencv)
            USE_OPENCV=ON
            shift
            ;;
        --no-hardware-accel)
            USE_HARDWARE_ACCEL=OFF
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --help)
            echo -e "${BLUE}摄像头服务器构建脚本${NC}"
            echo "用法: $0 [选项]"
            echo "选项:"
            echo "  --release             构建发布版本"
            echo "  --debug               构建调试版本 (默认)"
            echo "  --no-tests            不构建测试"
            echo "  --with-opencv         使用 OpenCV"
            echo "  --no-hardware-accel   不使用硬件加速"
            echo "  --clean               清理构建目录"
            echo "  --help                显示此帮助信息"
            exit 0
            ;;
        *)
            echo -e "${RED}未知选项: $key${NC}"
            exit 1
            ;;
    esac
done

# 显示构建信息
echo -e "${BLUE}构建类型: ${BUILD_TYPE}${NC}"
echo -e "${BLUE}构建测试: ${BUILD_TESTS}${NC}"
echo -e "${BLUE}使用 OpenCV: ${USE_OPENCV}${NC}"
echo -e "${BLUE}使用硬件加速: ${USE_HARDWARE_ACCEL}${NC}"

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build"

# 如果需要清理构建目录
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}清理构建目录...${NC}"
    rm -rf "${BUILD_DIR}"
fi

# 创建构建目录
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}" || exit 1

# 配置项目
echo -e "${YELLOW}配置项目...${NC}"
cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      -DBUILD_TESTS="${BUILD_TESTS}" \
      -DUSE_OPENCV="${USE_OPENCV}" \
      -DUSE_HARDWARE_ACCEL="${USE_HARDWARE_ACCEL}" \
      "${PROJECT_DIR}"

# 构建项目
echo -e "${YELLOW}构建项目...${NC}"
cmake --build . -- -j"$(nproc)"

# 检查构建结果
if [ $? -eq 0 ]; then
    echo -e "${GREEN}构建成功!${NC}"
    echo -e "${BLUE}可执行文件位于: ${BUILD_DIR}/bin/cam_server${NC}"
    
    # 如果构建了测试，运行测试
    if [ "${BUILD_TESTS}" = "ON" ]; then
        echo -e "${YELLOW}运行测试...${NC}"
        ctest --output-on-failure
        
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}测试通过!${NC}"
        else
            echo -e "${RED}测试失败!${NC}"
            exit 1
        fi
    fi
else
    echo -e "${RED}构建失败!${NC}"
    exit 1
fi

exit 0
