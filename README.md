# RK3588摄像头服务器 (C++版本)

基于RK3588开发板的摄像头服务器与Web客户端，支持视频采集、录制和拆分为静态图像帧。

## 项目概述

本项目旨在利用瑞芯微RK3588开发板的硬件能力，构建一个功能强大的摄像头服务器。该服务器能够连接摄像头，实现视频流的采集、编码和录制，并支持将已录制的视频文件拆解为一系列静态图像帧。项目同时包含一个基于Web浏览器的客户端界面，用户可以通过任何设备的浏览器远程访问、控制服务器的功能并获取其状态信息。

## 主要功能

- **视频采集**：摄像头开启和关闭控制，从连接的摄像头获取视频流
- **视频录制**：将视频流编码并保存为视频文件，视频文件管理
- **视频拆分**：将视频文件拆分为静态图像帧，静态帧文件夹管理和打包
- **实时预览**：通过Web浏览器实时查看摄像头视频流
- **远程控制**：通过Web界面远程控制和管理上述功能，摄像头参数控制
- **系统监控**：监控和显示系统状态信息（CPU、内存、存储、网络）

## 技术栈

- **编程语言**：C++17/20
- **构建系统**：CMake
- **视频处理**：FFmpeg/libav
- **摄像头接口**：V4L2
- **Web服务器**：Mongoose（嵌入式HTTP/WebSocket服务器）
- **前端技术**：HTML5, CSS3, JavaScript（原生）
- **实时流**：MJPEG流通过HTTP

## 项目结构

```
cam_server_cpp/
├── CMakeLists.txt                # 主CMake配置文件
├── cmake/                        # CMake模块和配置
├── dev_env.md                    # 开发环境配置文档
├── docs/                         # 文档
├── include/                      # 头文件
│   ├── camera/                   # 摄像头模块头文件
│   │   ├── camera_device.h       # 摄像头设备接口
│   │   └── v4l2_camera.h         # V4L2摄像头实现
│   ├── video/                    # 视频处理模块头文件
│   │   ├── i_video_recorder.h    # 视频录制接口
│   │   ├── video_recorder.h      # 视频录制相关结构体定义
│   │   ├── i_video_splitter.h    # 视频分割接口
│   │   └── video_splitter.h      # 视频分割相关结构体定义
│   ├── api/                      # API服务模块头文件
│   ├── storage/                  # 存储管理模块头文件
│   ├── monitor/                  # 系统监控模块头文件
│   ├── system/                   # 系统信息模块头文件
│   └── utils/                    # 工具类和通用功能
├── third_party/                  # 第三方库
│   └── mongoose/                 # Mongoose嵌入式Web服务器
├── referenceDoc/                 # 参考文档（原Rust项目）
├── scripts/                      # 构建和部署脚本
├── src/                          # 源文件
│   ├── camera/                   # 摄像头模块实现
│   │   ├── camera_manager.cpp    # 摄像头管理器
│   │   └── v4l2_camera.cpp       # V4L2摄像头实现
│   ├── video/                    # 视频处理模块实现
│   │   ├── ffmpeg_recorder.cpp   # FFmpeg视频录制实现
│   │   └── ffmpeg_splitter.cpp   # FFmpeg视频分割实现
│   ├── api/                      # API服务模块实现
│   ├── storage/                  # 存储管理模块实现
│   ├── monitor/                  # 系统监控模块实现
│   ├── system/                   # 系统信息模块实现
│   ├── utils/                    # 工具类和通用功能实现
│   └── main.cpp                  # 主程序入口
├── tests/                        # 测试
└── web_client/                   # Web客户端
```

## 环境要求

- **操作系统**：Armbian（基于Debian/Ubuntu的RK3588优化版本）
- **开发板**：RK3588（如香橙派5 Plus）
- **依赖库**：
  - FFmpeg开发库（libavcodec-dev, libavformat-dev, libavutil-dev, libswscale-dev）
  - V4L2开发库（libv4l-dev）
  - 可选：RK3588 MPP（Media Process Platform）
  - 可选：LibRGA（Rockchip Graphics Acceleration）

## 构建说明

### 安装依赖

```bash
# 安装基本开发工具
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git

# 安装FFmpeg开发库
sudo apt-get install -y \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev

# 安装V4L2开发库
sudo apt-get install -y \
    libv4l-dev \
    v4l-utils

# 安装RK3588特定的开发库（可选）
sudo apt-get install -y \
    librga-dev \
    rockchip-mpp-dev
```

### 构建项目

```bash
# 克隆仓库
git clone <repository-url>
cd cam_server_cpp

# 使用构建脚本
./scripts/build.sh

# 或者手动构建
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

### 构建选项

构建脚本支持以下选项：

- `--release`：构建发布版本（默认为调试版本）
- `--debug`：构建调试版本
- `--no-tests`：不构建测试
- `--with-opencv`：使用OpenCV（可选）
- `--no-hardware-accel`：不使用硬件加速
- `--clean`：清理构建目录
- `--help`：显示帮助信息

## 使用说明

### 启动服务器

```bash
# 使用默认配置
./build/bin/cam_server

# 指定配置文件
./build/bin/cam_server --config config.json
```

### 访问Web界面

启动服务器后，可以通过浏览器访问Web界面：

```
http://<rk3588-ip-address>:8080
```

### 实时预览功能

系统支持通过Web浏览器实时预览摄像头视频流：

```
http://<rk3588-ip-address>:8080/api/stream/mjpeg
```

也可以通过主页面访问实时预览功能，支持以下参数：

- **width**: 视频宽度（像素）
- **height**: 视频高度（像素）
- **quality**: JPEG质量（1-100）
- **fps**: 最大帧率

示例：
```
http://<rk3588-ip-address>:8080/api/stream/mjpeg?width=1280&height=720&quality=80&fps=30
```

### 开发日志和环境配置

在开发过程中，请及时更新以下文档：

- **查看开发日志**：`cat dev_log.md` 或使用文本编辑器打开
- **更新开发日志**：在 `dev_log.md` 中添加新的开发记录，包括完成的工作、遇到的问题和解决方案
- **查看环境配置**：`cat dev_env.md` 或使用文本编辑器打开
- **更新环境配置**：当开发环境有变化时，在 `dev_env.md` 中更新相关配置信息

这些文档对于项目的长期维护和新开发人员的加入非常重要，请确保它们始终保持最新状态。

### 当前开发进度

#### 已完成模块
- **摄像头模块**：实现了CameraDevice接口和V4L2Camera类，支持摄像头设备的基本操作
- **视频录制模块**：实现了IVideoRecorder接口和FFmpegRecorder类，支持视频流的编码和录制
- **视频分割模块**：实现了IVideoSplitter接口和FFmpegSplitter类，支持将视频文件拆分为静态图像帧
- **日志系统**：实现了Logger类，支持多级别日志、文件和控制台输出、异步日志等功能
- **文件工具类**：实现了FileUtils类，提供文件和目录操作的通用功能
- **字符串工具类**：实现了StringUtils类，提供字符串处理的通用功能
- **配置管理器**：实现了ConfigManager类，支持配置的加载、保存和访问
- **REST API处理器**：实现了RestHandler类，支持HTTP请求的路由和处理

#### 进行中模块
- **Web服务器模块**：正在实现基于Mongoose的Web服务器
- **MJPEG流媒体服务**：正在实现基于HTTP的MJPEG流媒体服务
- **API接口实现**：正在实现各功能模块的REST API接口

#### 待实现模块
- **系统监控模块**：监控系统资源使用情况
- **存储管理模块**：管理视频文件和图像文件的存储
- **Web客户端**：实现基于浏览器的用户界面

## 项目文档

### 核心文档

- **README.md**: 项目概述、功能介绍、构建和使用说明，是项目的主要入口文档
- **dev_log.md**: 开发日志，记录项目开发过程中的进展、问题和解决方案，按时间顺序组织，便于追踪项目历史
- **dev_env.md**: 开发环境配置文档，详细记录硬件环境、软件环境、依赖库和工具配置，帮助新开发者快速搭建开发环境
- **dev_board_log.md**: 开发板环境日志，记录RK3588开发板的硬件配置、系统信息和性能测试结果，为性能优化提供基准数据

### 技术文档

- **docs/api.md**: API文档，包含所有可用的API端点、参数说明和使用示例，是Web客户端和服务器交互的接口规范
- **docs/development.md**: 开发指南，包含项目架构、模块设计、代码规范和开发流程等信息，是开发者必读的指导文档
- **docs/camera.md**: 摄像头模块文档，包含摄像头接口、参数配置和使用示例，详细说明如何与不同类型的摄像头设备交互
- **docs/video.md**: 视频处理模块文档，包含视频录制和拆分功能说明，以及如何利用RK3588硬件加速能力
- **docs/storage.md**: 存储管理模块文档，包含文件管理、存储策略和数据备份方案说明
- **docs/streaming.md**: 视频流模块文档，包含MJPEG流实现、WebSocket通信和实时预览功能说明
- **docs/system.md**: 系统信息模块文档，包含系统监控、资源使用统计和性能优化建议

### 参考文档

- **referenceDoc/**: 原Rust项目的参考文档，包含项目需求和设计思路，是本C++项目的功能参考和设计依据
  - **referenceDoc/requirements.md**: 原项目的需求文档，详细说明了项目的功能需求和非功能需求
  - **referenceDoc/design.md**: 原项目的设计文档，包含架构设计和模块划分
  - **referenceDoc/api_spec.md**: 原项目的API规范，定义了服务器和客户端之间的通信接口

### 用户文档

- **docs/user_manual.md**: 用户手册，面向最终用户，详细说明如何安装、配置和使用本系统
- **docs/troubleshooting.md**: 故障排除指南，包含常见问题和解决方案
- **docs/faq.md**: 常见问题解答，收集用户经常提出的问题及其答案

### 如何使用这些文档

1. **新开发者入门**：
   - 首先阅读README.md了解项目概况
   - 然后查看dev_env.md配置开发环境
   - 查看dev_board_log.md了解开发板硬件和性能特性
   - 接着阅读docs/development.md了解开发流程
   - 最后查看dev_log.md了解项目当前状态

2. **功能开发参考**：
   - 查看相应模块的技术文档（如docs/camera.md）
   - 参考referenceDoc/中的原始需求和设计
   - 在dev_log.md中记录开发进展

3. **问题排查**：
   - 查看docs/troubleshooting.md寻找常见问题的解决方案
   - 在dev_log.md中记录遇到的问题和解决方法

4. **环境配置更新**：
   - 当开发环境有变化时，更新dev_env.md
   - 确保新增的依赖库和工具都有详细的安装说明

5. **性能优化**：
   - 参考dev_board_log.md中的性能测试结果作为基准
   - 在优化后重新运行性能测试，并更新测试结果
   - 记录性能改进和优化方法

## 许可证

[待定]

## 贡献指南

### 代码贡献流程
1. Fork项目仓库
2. 创建功能分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add some amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建Pull Request

### 开发规范
- 遵循C++17/20标准
- 使用4空格缩进
- 类名使用PascalCase命名法
- 方法和变量使用snake_case命名法
- 常量使用UPPER_CASE命名法
- 所有代码必须包含适当的注释
- 所有公共API必须有文档注释
- 提交前运行单元测试确保代码质量

### 文档贡献
- 更新技术文档以反映代码变更
- 改进用户文档使其更加清晰易懂
- 添加示例和教程帮助用户理解系统

### 问题报告
- 使用GitHub Issues报告bug
- 包含详细的复现步骤
- 如果可能，提供日志和截图
- 标记相关的标签以便分类
