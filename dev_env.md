# 开发环境配置

本文档记录了RK3588摄像头服务器项目的开发环境配置，包括硬件环境、操作系统、依赖库和开发工具等信息。

## 硬件环境

### 开发板信息

- **开发板型号**：香橙派 5 Plus (Orange Pi 5 Plus)
- **处理器**：RK3588 八核 ARM Cortex-A76 + Cortex-A55
- **内存**：16GB LPDDR4
- **存储**：64GB eMMC + 128GB SD卡
- **网络**：千兆以太网 + WiFi 6
- **USB**：USB 3.0 x 2, USB 2.0 x 2
- **视频输出**：HDMI 2.1 x 2
- **摄像头接口**：MIPI CSI x 2

### 摄像头设备

- **型号**：Logitech C920 HD Pro
- **接口**：USB 2.0
- **分辨率**：最高支持 1080p/30fps
- **格式**：MJPEG, YUY2, H.264

## 软件环境

### 操作系统

- **系统**：Armbian 23.11.0
- **内核版本**：5.10.160-rockchip-rk3588
- **架构**：ARM64 (aarch64)

### 编译工具链

- **编译器**：GCC 11.4.0
- **构建系统**：CMake 3.22.1
- **调试工具**：GDB 12.1

### 依赖库

#### 基础依赖

```bash
# 安装基本开发工具
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git \
    gdb \
    valgrind \
    ccache

# 安装FFmpeg开发库
sudo apt-get install -y \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libavfilter-dev \
    libavdevice-dev

# 安装V4L2开发库
sudo apt-get install -y \
    libv4l-dev \
    v4l-utils

# 安装网络库
sudo apt-get install -y \
    libcurl4-openssl-dev \
    libssl-dev \
    libboost-all-dev

# 安装JSON库
sudo apt-get install -y \
    nlohmann-json3-dev

# 安装图像处理库
sudo apt-get install -y \
    libjpeg-dev \
    libpng-dev
```

#### RK3588特定依赖

```bash
# 安装RK3588 MPP (Media Process Platform)
sudo apt-get install -y \
    librga-dev \
    rockchip-mpp-dev
```

### 开发工具

- **IDE**：Visual Studio Code
- **版本控制**：Git
- **文档工具**：Markdown
- **API测试**：curl, Postman
- **性能分析**：perf, valgrind

## 开发环境设置

### 克隆代码仓库

```bash
git clone https://github.com/yourusername/cam_server_cpp.git
cd cam_server_cpp
```

### 配置VSCode

推荐安装以下VSCode扩展：

- C/C++ Extension Pack
- CMake Tools
- Git Graph
- Markdown All in One
- Remote SSH (如果远程开发)

### 配置文件

在项目根目录创建`.vscode/settings.json`文件：

```json
{
    "cmake.configureOnOpen": true,
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "cmake.installPrefix": "${workspaceFolder}/install",
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "editor.formatOnSave": true,
    "files.associations": {
        "*.h": "cpp",
        "*.cpp": "cpp"
    }
}
```

### 环境变量设置

将以下内容添加到`~/.bashrc`文件中：

```bash
# 设置MPP库路径
export MPP_DIR=/usr/local/rockchip/mpp
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$MPP_DIR/lib

# 设置RGA库路径
export RGA_DIR=/usr/local/rockchip/rga
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$RGA_DIR/lib

# 设置项目路径
export CAM_SERVER_DIR=$HOME/cam_server_cpp
```

## 摄像头设置

### 检查摄像头设备

```bash
# 列出视频设备
v4l2-ctl --list-devices

# 查看设备能力
v4l2-ctl -d /dev/video0 --all
v4l2-ctl -d /dev/video0 --list-formats-ext
```

### 测试摄像头

```bash
# 使用FFmpeg测试摄像头
ffmpeg -f v4l2 -framerate 30 -video_size 1280x720 -input_format mjpeg -i /dev/video0 -c:v libx264 -preset ultrafast -f matroska test.mkv

# 使用GStreamer测试摄像头
gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw,width=1280,height=720 ! videoconvert ! autovideosink
```

## 硬件加速设置

### 检查MPP状态

```bash
# 检查MPP库是否正确安装
ls -la /usr/local/rockchip/mpp/lib/

# 检查MPP服务是否运行
systemctl status rockchip-mpp.service
```

### 检查RGA状态

```bash
# 检查RGA库是否正确安装
ls -la /usr/local/rockchip/rga/lib/

# 测试RGA功能
rgaImDemo
```

## 网络设置

### 配置静态IP

为了方便远程访问，建议配置静态IP：

1. 编辑网络配置文件：
   ```bash
   sudo nano /etc/network/interfaces
   ```

2. 添加以下内容：
   ```
   auto eth0
   iface eth0 inet static
       address 192.168.1.100
       netmask 255.255.255.0
       gateway 192.168.1.1
       dns-nameservers 8.8.8.8 8.8.4.4
   ```

3. 重启网络服务：
   ```bash
   sudo systemctl restart networking
   ```

### 配置SSH

为了方便远程开发，建议配置SSH：

1. 安装SSH服务器：
   ```bash
   sudo apt-get install -y openssh-server
   ```

2. 配置SSH密钥认证：
   ```bash
   ssh-keygen -t rsa -b 4096
   ```

## 故障排除

### 常见问题

1. **摄像头无法识别**
   - 检查USB连接
   - 检查设备权限：`sudo chmod 666 /dev/video0`
   - 检查内核模块：`lsmod | grep uvcvideo`

2. **编译错误**
   - 检查依赖库是否正确安装
   - 检查CMake版本是否兼容
   - 检查编译器版本是否支持C++17

3. **硬件加速问题**
   - 检查MPP库是否正确安装
   - 检查内核是否支持硬件编解码
   - 检查设备树配置

## 参考资源

- [RK3588官方文档](https://wiki.t-firefly.com/en/Firefly-RK3588/index.html)
- [Orange Pi 5 Plus官方Wiki](http://www.orangepi.org/orangepiwiki/index.php/Orange_Pi_5_Plus)
- [V4L2编程指南](https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/v4l2.html)
- [FFmpeg文档](https://ffmpeg.org/documentation.html)
- [Rockchip MPP开发指南](https://github.com/rockchip-linux/mpp/blob/develop/readme.txt)

## 更新日志

- **2025-05-15**: 初始版本，记录基本开发环境配置
- **2025-05-16**: 添加摄像头设置和测试方法
- **2025-05-17**: 添加硬件加速配置和故障排除指南
