#!/bin/bash
# 启用核心转储
ulimit -c unlimited

# 清理旧的核心转储文件
rm -f core.*

# 创建构建目录（如果不存在）
mkdir -p build
cd build

# 编译程序（添加调试信息和内存检查）
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j4

# 设置内存检查环境变量
export MALLOC_CHECK_=3
export MALLOC_PERTURB_=42

# 返回项目根目录
cd ..

# 使用GDB运行程序
gdb -x scripts/gdb_command.txt ./cam_server