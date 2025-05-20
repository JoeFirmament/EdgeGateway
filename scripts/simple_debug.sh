#!/bin/bash

# 设置核心转储
ulimit -c unlimited
echo "核心转储已设置为无限制"

# 清理旧的核心转储和日志文件
rm -f core.* debug_output.log

# 编译程序（添加调试信息）
cd build
echo "开始编译程序..."
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j4
echo "编译完成"
cd ..

# 运行服务器，并将输出重定向到日志文件
echo "启动服务器..."
echo "请在另一个终端窗口打开Web浏览器访问服务器"
echo "按Ctrl+C终止服务器"

# 运行服务器并记录输出
./cam_server 2>&1 | tee debug_output.log 