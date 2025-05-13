# RK3588摄像头服务器

基于RK3588开发板的摄像头服务器与Web客户端，支持视频采集、录制和拆分为静态图像帧。

## 项目概述

本项目旨在利用瑞芯微RK3588开发板的硬件能力，构建一个功能强大的摄像头服务器。该服务器能够连接摄像头，实现视频流的采集、编码和录制，并支持将已录制的视频文件拆解为一系列静态图像帧。项目同时包含一个基于Web浏览器的客户端界面，用户可以通过任何设备的浏览器远程访问、控制服务器的功能并获取其状态信息。

## 主要功能

- **视频采集**：摄像头开启和关闭控制，从连接的摄像头获取视频流
- **视频录制**：将视频流编码并保存为视频文件，视频文件管理
- **视频拆分**：将视频文件拆分为静态图像帧，静态帧文件夹管理和打包
- **远程控制**：通过Web界面远程控制和管理上述功能，摄像头参数控制
- **系统监控**：监控和显示系统状态信息

## 技术栈

- **编程语言**：Rust（服务器端）、HTML/CSS/JavaScript（Web客户端）
- **操作系统**：Radxa OS或Armbian（基于Debian/Ubuntu的RK3588优化版本）
- **Web框架**：Actix-web（Rust）
- **视频处理**：FFmpeg（通过ffmpeg-next绑定）
- **摄像头接口**：V4L2（通过v4l2-rs库）

## 项目结构

```
camera-server/
├── Cargo.toml (workspace)
├── camera-core/       # 摄像头采集和视频处理核心功能
├── camera-api/        # API服务和Web服务器
├── camera-storage/    # 存储管理功能
├── camera-monitor/    # 系统监控功能
├── camera-app/        # 主应用，集成其他crate
└── web-client/        # Web客户端（HTML/CSS/JS）
```

## 开发指南

### 环境设置

#### Mac开发环境

```bash
# 安装Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# 安装交叉编译工具链
rustup target add aarch64-unknown-linux-gnu

# 安装必要的系统依赖
brew install pkg-config openssl ffmpeg
```

#### RK3588开发环境

```bash
# 安装Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# 安装必要的系统依赖
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    pkg-config \
    libssl-dev \
    libv4l-dev \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev
```

### 混合开发流程

我们采用混合开发模式，结合Mac开发环境和RK3588目标平台的优势：

1. **在Mac上进行基础开发**：
   - 搭建项目框架和基础结构
   - 开发与硬件无关的功能（如API服务、存储管理、基本业务逻辑）
   - 编写单元测试和模拟测试

2. **在RK3588上进行硬件相关开发和最终适配**：
   - 实现摄像头接口和视频处理等硬件相关功能
   - 进行性能优化和硬件加速适配
   - 进行系统集成测试和稳定性测试

3. **代码同步流程**：
   - 在Mac上开发并提交到GitHub
   - 在RK3588上拉取最新代码并继续开发
   - 将RK3588上的开发成果推送回GitHub

### 构建和运行

```bash
# 构建整个工作空间
cargo build --workspace

# 运行主应用
cargo run -p camera-app

# 交叉编译（在Mac上）
cargo build --target aarch64-unknown-linux-gnu --release
```

## 许可证

[待定]

## 贡献指南

[待定]
