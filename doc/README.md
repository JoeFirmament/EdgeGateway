# 🎥 深视边缘视觉平台 (DeepVision Edge Platform)

基于RK3588开发板的智能视觉处理平台，采用Crow框架实现WebSocket视频推流和API驱动架构。

## 📋 项目概述

深视边缘视觉平台是一个专为RK3588平台设计的高性能智能视觉处理系统，采用模块化架构，支持多摄像头管理、视频录制、帧提取、AI视觉处理等功能，具备完整的Web界面和API扩展能力。

### 🎯 核心功能
- ✅ **WebSocket视频推流** - 基于Crow框架的实时MJPEG视频传输
- ✅ **双摄像头支持** - 支持多个USB摄像头设备同时工作
- ✅ **Web控制界面** - 现代化的HTML5前端界面
- ✅ **设备管理工具** - 完整的摄像头检测和管理工具
- 🔄 **API驱动架构** - RESTful API框架（设计完成，待集成）
- 🔄 **录制和拍照** - 视频录制和图像捕获功能（架构已设计）
- 🔄 **AI视觉处理** - YOLO检测、透视变换等AI算法（框架已设计）

### 🛠️ 技术栈
- **后端框架**: Crow (C++ HTTP/WebSocket)
- **视频处理**: V4L2 + FFmpeg + OpenCV
- **前端技术**: HTML5 + JavaScript + 统一导航栏组件
- **开发平台**: RK3588 (ARM64) + Debian12 bookworm
- **构建系统**: Makefile + C++17
- **架构特点**: 模块化设计 + API驱动 + 组件化前端

## 🚀 快速开始

### 📋 环境要求
- **硬件平台**: RK3588开发板（推荐4GB+内存）
- **操作系统**: Debian12 bookworm (ARM64架构)
- **摄像头设备**: USB摄像头（支持MJPEG格式）
- **网络连接**: 以太网或WiFi

### 📦 依赖安装
```bash
# 更新系统包
sudo apt update && sudo apt upgrade -y

# 安装核心依赖
sudo apt install -y \
    build-essential cmake git \
    libopencv-dev libv4l-dev \
    libjsoncpp-dev libfmt-dev \
    v4l-utils ffmpeg

# 安装Crow框架依赖
sudo apt install -y \
    libasio-dev libssl-dev

# 安装RK3588硬件加速库（可选）
sudo apt install -y \
    librga-dev rockchip-mpp-dev
```

### 🔨 构建项目
```bash
# 克隆项目
git clone <repository-url>
cd cam_server_cpp

# 使用 Makefile 构建（推荐）
make clean && make -j8

# 调试版本
make debug

# 发布版本
make release
```

### 🚀 启动服务
```bash
# 启动摄像头服务器
./main_server -p 8081

# 查看帮助信息
./main_server --help
```

### 🌐 访问Web界面
```bash
# 服务器启动后，在浏览器中访问：
http://<RK3588-IP-地址>:8081

# 主要功能页面：
http://192.168.124.12:8081/                    # 主页
http://192.168.124.12:8081/video_recording.html # 视频录制
http://192.168.124.12:8081/frame_extraction.html # 帧提取
http://192.168.124.12:8081/photo_capture.html   # 拍照功能
http://192.168.124.12:8081/system_info.html     # 系统信息
http://192.168.124.12:8081/serial_info.html     # 串口信息
```

## 📷 摄像头设备管理

### 🔍 设备检测
```bash
# 查看所有视频设备
ls -la /dev/video*

# 查看设备详细信息
v4l2-ctl --list-devices

# 检查设备支持的格式
v4l2-ctl --device=/dev/video0 --list-formats-ext
```

### 🛠️ 摄像头管理工具
项目提供了专门的摄像头管理工具：
```bash
# 使用摄像头管理工具
./tools/camera_manager.sh

# 常用命令
./tools/camera_manager.sh list      # 列出所有设备
./tools/camera_manager.sh check     # 检查占用情况
./tools/camera_manager.sh release   # 释放占用的设备
./tools/camera_manager.sh test      # 测试设备功能
```

### ⚠️ 重要说明
**设备节点映射**：每个物理USB摄像头会创建两个设备节点
- `/dev/video0`, `/dev/video2` - ✅ 主设备（支持视频捕获）
- `/dev/video1`, `/dev/video3` - ❌ 元数据设备（仅用于元数据）

**推荐使用**：`/dev/video0` 和 `/dev/video2`

## 🏗️ 系统架构

### 🎯 单摄像头多标签页设计
- **核心理念**：每个页面管理一个摄像头，简单高效
- **使用方式**：用户根据需要打开多个浏览器标签页
- **技术优势**：
  - 开发简单，维护容易
  - 资源按需分配，内存优化
  - 故障隔离，一个摄像头问题不影响其他
  - 完美适配4GB内存限制

### 🌐 访问方式
```bash
# 单摄像头访问
http://192.168.124.12:8080/single_camera_stream.html?device=/dev/video0&name=Camera1

# 双摄像头使用（开启两个标签页）
标签页1: http://192.168.124.12:8080/single_camera_stream.html?device=/dev/video0&name=DECXIN
标签页2: http://192.168.124.12:8080/single_camera_stream.html?device=/dev/video2&name=USB
```

### 💡 性能特性
- **内存使用**：单摄像头约150MB，双摄像头按需分配
- **处理能力**：支持双摄像头640x480@30fps同时推流
- **硬件加速**：支持RK3588 MPP/RGA/NPU硬件加速



## 📚 核心架构文档

### 🏗️ 项目架构
- **[项目架构文档](./doc/project_architecture.md)** - 完整的项目结构和架构说明
- **[快速开发指南](./doc/quick_dev_guide.md)** - 开发者快速上手指南
- **[单摄像头多标签页架构](./single_camera_multi_tab_architecture.md)** - 核心架构设计文档
- **[AI视觉处理扩展指南](./ai_vision_processing_guide.md)** - AI算法集成和扩展

### 📖 开发过程文档
- **[开发日志](./dev_log.md)** - 完整的开发过程记录和技术决策
- **[API文档](./api_documentation.md)** - 完整的API接口文档

### 🔧 技术特性
- **统一导航栏系统** - 组件化前端架构，一处修改全局更新
- **模块化编译系统** - Makefile 支持增量编译和并行构建
- **动态页面加载** - 自动扫描和路由注册
- **串口设备管理** - 完整的串口信息监控和管理

## ✅ 当前可用功能

### 🎥 视频推流功能
- **WebSocket实时推流** - 基于Crow框架的MJPEG视频传输
- **双摄像头支持** - 支持多个USB摄像头同时工作
- **摄像头控制** - WebSocket命令控制摄像头启动/停止
- **系统信息响应** - 实时系统状态监控和反馈
- **智能设备选择** - 自动识别和选择可用的流捕获设备

### 🌐 Web界面访问
```bash
# WebSocket视频流测试页面
http://192.168.124.12:8081/test_video_stream.html

# 单摄像头推流页面
http://192.168.124.12:8080/single_camera_stream.html?device=/dev/video0&name=Camera1

# 双摄像头使用（开启两个标签页）
标签页1: http://192.168.124.12:8080/single_camera_stream.html?device=/dev/video0&name=DECXIN
标签页2: http://192.168.124.12:8080/single_camera_stream.html?device=/dev/video2&name=USB
```

### 🔧 WebSocket控制命令
```javascript
// 连接摄像头WebSocket
const ws = new WebSocket('ws://192.168.124.12:8081/ws/camera');

// 获取摄像头状态
ws.send(JSON.stringify({action: 'get_status'}));

// 启动摄像头
ws.send(JSON.stringify({action: 'start_camera', camera_id: 'default'}));

// 停止摄像头
ws.send(JSON.stringify({action: 'stop_camera', camera_id: 'default'}));
```

## 🔮 规划中的功能

### 🎬 录制和拍照功能
- **视频录制** - 高质量视频文件录制
- **图像捕获** - 高分辨率静态图像拍照
- **文件管理** - 录制文件的管理和下载

### 🤖 AI视觉处理
- **YOLO目标检测** - 实时物体识别和标注
- **OpenCV图像处理** - 透视变换、边缘检测等
- **NPU硬件加速** - 利用RK3588 NPU进行AI推理

### 🔌 API驱动架构
- **RESTful API** - 完整的API接口体系
- **现代化前端** - 基于API的响应式Web界面
- **第三方集成** - 支持外部系统集成


## 🎉 最新开发成果

### 🚀 WebSocket技术突破 (2025-01-22)
经过深入的问题分析和代码重构，我们成功解决了困扰项目的WebSocket连接问题，实现了稳定的双向通信。

**重大进展**：
- ✅ **WebSocket连接问题完全解决** - 稳定的浏览器与服务器双向通信
- ✅ **摄像头WebSocket控制功能完成** - 实现完整的摄像头控制系统
- ✅ **WebSocket视频流传输实现** - 实时MJPEG视频流通过WebSocket传输
- ✅ **智能设备选择逻辑** - 自动识别USB摄像头映射关系，优先选择流捕获设备

**技术指标**：
- **连接延迟**：< 100ms
- **视频流性能**：400+帧成功传输，每帧60-67KB
- **设备兼容性**：完美支持USB摄像头映射关系
- **稳定性**：连接保持稳定，无异常断开

### 🔧 摄像头管理工具完成 (2025-01-22)
开发了专门的摄像头管理工具 `tools/camera_manager.sh`，解决设备占用和管理问题。

**工具功能**：
- **设备检测** - 自动识别所有摄像头设备和类型
- **占用检查** - 检测设备被哪个进程占用
- **设备释放** - 优雅停止和强制释放占用的设备
- **功能测试** - 验证设备的MJPEG格式支持和流捕获能力
- **实时监控** - 动态显示设备状态变化

**重要发现**：
- 每个USB摄像头创建两个设备节点：流捕获设备（偶数）和元数据设备（奇数）
- 推荐使用：`/dev/video0` (DECXIN CAMERA) 和 `/dev/video2` (USB Camera)
- 避免使用：`/dev/video1` 和 `/dev/video3` (元数据设备，不支持视频流)

## 📋 摄像头操作笔记

### 🔍 设备检测和识别

#### 基础设备检测
```bash
# 查看所有视频设备
ls -la /dev/video*

# 查看设备详细信息
v4l2-ctl --list-devices

# 检查设备支持的格式
v4l2-ctl --device=/dev/video0 --list-formats-ext
```

#### 设备节点说明
**重要**：每个物理USB摄像头会创建两个设备节点
- `/dev/video0`, `/dev/video2` - ✅ **主设备**（支持视频捕获和MJPEG）
- `/dev/video1`, `/dev/video3` - ❌ **元数据设备**（仅用于元数据）

**推荐使用**：`/dev/video0` 和 `/dev/video2`

### 🛠️ 基础操作命令

#### 摄像头测试
```bash
# 测试摄像头基本功能
v4l2-ctl --device=/dev/video0 --set-fmt-video=width=640,height=480,pixelformat=MJPG
ffplay /dev/video0

# 查看当前配置
v4l2-ctl --device=/dev/video0 --get-fmt-video

# 测试设备流捕获
timeout 5s v4l2-ctl --device=/dev/video0 --stream-mmap --stream-count=3
```

#### 权限和占用检查
```bash
# 检查设备权限
ls -l /dev/video*
sudo chmod 666 /dev/video*

# 检查设备占用情况
lsof /dev/video*

# 释放占用的设备
pkill -f cam_server
kill -9 <PID>
```

### 🔧 故障排除

#### 常见问题解决
```bash
# 问题1：Device or resource busy
lsof /dev/video0          # 查看占用进程
kill -9 <PID>            # 强制释放

# 问题2：Permission denied
groups $USER              # 检查用户组
sudo usermod -a -G video $USER  # 添加到video组

# 问题3：格式不支持
v4l2-ctl --device=/dev/video0 --list-formats-ext  # 查看支持格式
```

#### 系统监控
```bash
# 实时监控设备状态
watch -n 1 'lsof /dev/video* 2>/dev/null || echo "设备空闲"'

# 查看系统资源
free -h
top -p $(pgrep cam_server)

# 查看系统温度
cat /sys/class/thermal/thermal_zone*/temp
```

### 🛡️ 摄像头管理工具

项目提供专门的管理工具：
```bash
# 使用摄像头管理工具
./tools/camera_manager.sh

# 常用命令
./tools/camera_manager.sh list      # 列出所有设备
./tools/camera_manager.sh check     # 检查占用情况
./tools/camera_manager.sh release   # 释放占用的设备
./tools/camera_manager.sh test      # 测试设备功能
./tools/camera_manager.sh monitor   # 实时监控
```

### 📊 设备信息示例

#### DECXIN CAMERA (/dev/video0)
```
Driver: uvcvideo
Card: DECXIN CAMERA
Formats: MJPEG, YUYV
Max Resolution: 1920x1200
Recommended: 640x480@30fps MJPEG
```

#### USB Camera (/dev/video2)
```
Driver: uvcvideo
Card: USB Camera
Formats: MJPEG, YUYV
Recommended: 640x480@30fps MJPEG
```

## 🔧 故障排除和调试

### 📝 日志查看
```bash
# 查看应用日志
tail -f logs/server.log

# 查看系统日志
journalctl -f

# 启动时指定日志级别
./cam_server --log-level debug
```

### 🌐 网络检查
```bash
# 检查服务器端口
netstat -tlnp | grep 8080

# 测试网络连通性
ping 192.168.124.12
```

---

**最后更新**: 2025-01-22
**项目状态**: 基础推流功能已实现，AI扩展架构已设计完成

## 📄 许可证

[待定]

## 🤝 贡献指南

### 代码贡献流程
1. Fork项目仓库
2. 创建功能分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add some amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建Pull Request

### 开发规范
- 遵循C++17标准
- 使用4空格缩进
- 类名使用PascalCase，变量使用snake_case
- 所有代码必须包含适当的注释
- 提交前运行测试确保代码质量

## 📁 项目结构说明

### 重要目录
- **`doc/`** - 项目文档（唯一文档来源）
- **`logs/`** - 运行时日志文件
- **`static/`** - Web静态资源文件
- **`tools/`** - 开发和管理工具

### 可执行文件位置
项目中的可执行文件（如`cam_server`）生成在**项目根目录**，这是为了方便访问`static/`目录中的Web资源文件。

## 🔧 日志系统

### 日志级别控制
```bash
# 设置日志级别
./cam_server --log-level debug
./cam_server -l info
```

### 日志格式
```
[MM-DD HH:MM:SS] [LEVEL] [MODULE] Message
```

### 最佳实践
- **开发环境**：使用`--log-level debug`获取详细日志
- **生产环境**：使用`--log-level info`减少日志量
- **问题排查**：临时提高到`trace`级别获取最详细信息