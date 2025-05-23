#!/bin/bash

echo "🔨 编译WebSocket视频流测试程序 (websocket_video_stream_test.cpp)..."

# 设置编译参数
CXX=g++
CXXFLAGS="-std=c++17 -Wall -Wextra -O2"
INCLUDES="-I. -Iinclude -Ithird_party/crow -I/usr/include/opencv4"
LIBS="-lpthread -lv4l2 -lavcodec -lavformat -lavutil -lswscale -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_dnn -lfmt"

# 基础源文件
SOURCES="websocket_video_stream_test.cpp \
         src/camera/camera_manager.cpp \
         src/camera/v4l2_camera.cpp \
         src/camera/frame.cpp \
         src/camera/format_utils.cpp \
         src/vision/processing_pipeline.cpp \
         src/vision/homography_processor.cpp \
         src/monitor/logger.cpp \
         src/utils/file_utils.cpp \
         src/utils/config_manager.cpp \
         src/utils/string_utils.cpp"

echo "🔍 检查源文件是否存在..."
for file in $SOURCES; do
    if [ ! -f "$file" ]; then
        echo "❌ 文件不存在: $file"
        exit 1
    fi
done

echo "✅ 所有源文件都存在"

# 编译 (使用并行编译加速)
echo "📦 编译中... (使用 $(nproc) 个CPU核心)"
$CXX $CXXFLAGS $INCLUDES $SOURCES $LIBS -o websocket_video_stream_test

if [ $? -eq 0 ]; then
    echo "✅ 编译成功！"
    echo ""
    echo "🚀 使用方法:"
    echo "  ./websocket_video_stream_test"
    echo ""
    echo "📱 然后在浏览器中访问:"
    echo "  http://localhost:8081/test_video_stream.html"
    echo "  或连接WebSocket: ws://localhost:8081/ws/video"
    echo ""
    echo "💡 测试命令:"
    echo '  启动摄像头: {"action":"start_camera","device":"/dev/video0"}'
    echo '  停止摄像头: {"action":"stop_camera"}'
    echo '  获取状态:   {"action":"get_status"}'
else
    echo "❌ 编译失败"
    exit 1
fi
