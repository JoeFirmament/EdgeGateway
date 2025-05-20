#!/bin/bash

# 设置日志文件
LOG_FILE="mjpeg_monitor.log"

# 清空旧的日志文件
echo "开始MJPEG流监控..." > $LOG_FILE

echo "===== 开始MJPEG流监控 ====="

# 记录当前时间
date >> $LOG_FILE

# 监控进程
while true; do
    echo "==== $(date) ====" >> $LOG_FILE
    
    # 检查cam_server进程是否运行
    if pgrep cam_server > /dev/null; then
        echo "cam_server进程正在运行" >> $LOG_FILE
        
        # 列出所有线程
        echo "线程列表:" >> $LOG_FILE
        ps -T -p $(pgrep cam_server) >> $LOG_FILE
        
        # 检查网络连接
        echo "网络连接:" >> $LOG_FILE
        netstat -anp | grep cam_server >> $LOG_FILE
        
        # 检查内存使用情况
        echo "内存使用情况:" >> $LOG_FILE
        pmap $(pgrep cam_server) | tail -n 1 >> $LOG_FILE
        
        # 获取栈跟踪
        echo "栈跟踪:" >> $LOG_FILE
        gdb -p $(pgrep cam_server) -batch -ex "thread apply all bt" 2>/dev/null >> $LOG_FILE
    else
        echo "cam_server进程未运行" >> $LOG_FILE
    fi
    
    echo "" >> $LOG_FILE
    sleep 2
done 