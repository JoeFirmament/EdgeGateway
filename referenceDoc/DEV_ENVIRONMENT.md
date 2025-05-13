# 开发环境信息

## 当前开发环境

**日期**: 2025年5月13日

**设备**: RK3588开发板（如香橙派5 Plus）
**操作系统**: Armbian（基于Debian/Ubuntu的RK3588优化版本）
**CPU架构**: ARM64 (AArch64)
**Rust版本**: 1.86.0 (adf9b6ad1 2025-02-28)

## 开发工具

- **编辑器/IDE**: Visual Studio Code
- **版本控制**: Git
- **构建工具**: Cargo (Rust包管理器)
- **依赖管理**: Cargo.toml (Workspace)
- **代码仓库**: GitHub (git@github.com:JoeFirmament/cam_server.git)

## 主要依赖库

- **Web框架**: Actix-web 4.3
- **摄像头接口**: nokhwa 0.10 (input-v4l feature)
- **视频处理**: ffmpeg-next 6.0
- **硬件加速**: RK3588 MPP (Media Process Platform)
- **图像处理**: image 0.24
- **序列化/反序列化**: serde 1.0
- **异步运行时**: tokio 1.28
- **日志**: log 0.4, env_logger 0.10

## 开发模式

项目采用直接在RK3588上开发的模式：

1. 在RK3588上进行全栈开发
2. 使用Git进行版本控制和代码管理
3. 直接在RK3588上测试和部署

## 测试环境

- **单元测试**: Rust内置测试框架
- **集成测试**: 自定义测试工具 (tools/camera_capture_test, tools/camera_detector)
- **性能测试**: 待定

## 注意事项

1. **硬件特性**:
   - 充分利用RK3588的硬件加速能力
   - 优化V4L2摄像头接口性能
   - 使用MPP (Media Process Platform) 进行视频编解码

2. **性能优化**:
   - 使用硬件编码加速视频处理
   - 优化内存使用和CPU负载
   - 针对ARM架构优化代码

3. **依赖兼容性**:
   - 确保所有依赖库在ARM64架构上兼容
   - 使用特性标志处理平台特定代码
   - 避免使用不支持ARM64的依赖

4. **开发流程**:
   - 直接在RK3588上开发和测试所有功能
   - 使用Git进行版本控制
   - 定期备份代码到GitHub仓库

## 环境设置脚本

### RK3588开发环境设置

```bash
#!/bin/bash
# setup_rk3588_dev.sh

# 安装Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env

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
    libswscale-dev \
    v4l-utils \
    ffmpeg

# 安装RK3588特定的开发库
sudo apt-get install -y \
    librga-dev \
    rockchip-mpp-dev

# 克隆代码仓库
git clone git@github.com:JoeFirmament/cam_server.git
cd cam_server

# 构建项目
cargo build --workspace
```

---

*注：此文档将随着项目进展不断更新。*
