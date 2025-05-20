#!/bin/bash

# 确保脚本在发生错误时停止
set -e

echo "===== 开始段错误调试 ====="

# 设置核心转储
ulimit -c unlimited
echo "核心转储已设置为无限制"

# 清理旧的核心转储和日志文件
rm -f core.* gdb_output.log
echo "已清理旧的核心转储和日志文件"

# 在执行程序的目录下编译程序（添加调试信息）
cd build
echo "开始编译程序..."
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-g -O0 -fsanitize=address -fno-omit-frame-pointer" ..
make -j4
echo "编译完成"

# 设置内存检查环境变量
export MALLOC_CHECK_=3
export MALLOC_PERTURB_=42

# 导出ASAN选项
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1:halt_on_error=1:print_stats=1:verbosity=1

echo "===== 开始GDB调试 ====="
cd ..
gdb -x scripts/gdb_command.txt ./cam_server

# 如果程序崩溃并生成核心转储，分析它
if [ -f core.* ]; then
    echo "===== 分析核心转储 ====="
    gdb ./cam_server core.* -x scripts/gdb_core_analysis.txt
fi

echo "===== 调试完成 =====" 