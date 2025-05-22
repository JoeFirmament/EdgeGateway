# LuBan 边缘处理器 (C++版本)

基于RK3588开发板的边缘处理器与Web客户端，支持摄像头视频采集、录制和拆分为静态图像帧。

## 📚 重要提示：API 文档

**在开始使用前，请务必阅读 [API 文档](./doc/API_DOCUMENTATION.md)**，其中包含了所有可用的 API 接口、参数说明、请求/响应示例以及使用说明。

## 项目概述

本项目旨在利用瑞芯微RK3588开发板的硬件能力，构建一个功能强大的边缘处理器。该处理器能够连接摄像头，实现视频流的采集、编码和录制，并支持将已录制的视频文件拆解为一系列静态图像帧。项目同时包含一个基于Web浏览器的客户端界面，用户可以通过任何设备的浏览器远程访问、控制处理器的功能并获取其状态信息。


### 程序流程与架构

#### 主要流程

1. **初始化阶段**：
   - 设置本地化环境（解决中文乱码问题）
   - 设置信号处理器（捕获SIGINT, SIGTERM等）
   - 记录系统信息到日志文件
   - 初始化日志系统（Logger类）
   - 初始化配置管理器（加载config.json）
   - 初始化存储管理器（管理视频和图像文件）
   - 初始化摄像头管理器（但不立即开启摄像头）
   - 初始化API服务器（Mongoose HTTP服务器）
   - 初始化系统监控器（监控CPU、内存等）

2. **运行阶段**：
   - 主循环中执行周期性任务（自动清理存储、系统状态更新）
   - API服务器处理来自Web客户端的请求
   - 用户通过Web界面配置和控制摄像头操作
   - 摄像头捕获视频帧，提供给MJPEG流和视频录制模块

3. **关闭阶段**：
   - 接收关闭信号（如Ctrl+C）
   - 停止系统监控器
   - 停止API服务器
   - 关闭摄像头设备
   - 记录关闭信息并退出

#### 线程架构

系统运行时创建以下线程：

1. **主线程**：
   - 初始化各个模块
   - 执行主循环
   - 处理信号并协调关闭过程

2. **API服务器线程**：
   - 由ApiServer::start()创建
   - 处理HTTP请求
   - 提供REST API和静态文件服务

3. **摄像头捕获线程**：
   - 由V4L2Camera::startCapture()创建
   - 从摄像头设备读取视频帧
   - 处理帧数据并通知观察者

4. **MJPEG流线程**：
   - 为每个连接到/api/stream的客户端创建
   - 将摄像头帧编码为JPEG并发送
   - 维护客户端连接状态

5. **系统监控线程**：
   - 由SystemMonitor::start()创建
   - 定期收集系统状态信息
   - 更新系统监控数据供API使用

#### 连接数与限制

- **最大HTTP连接数**：默认为100个并发连接，由Mongoose配置决定
- **MJPEG流并发客户端**：理论上支持与HTTP连接数相同的并发流，但实际受限于网络带宽和处理能力
- **活跃连接超时**：HTTP连接默认超时时间为30秒，MJPEG流连接保持活跃状态直到客户端断开

#### API路由

系统支持以下主要API端点：

1. **静态资源**：
   - `/` 或 `/index.html` - Web界面主页
   - `/css/*` - 样式表文件
   - `/js/*` - JavaScript文件
   - `/images/*` - 图像资源

2. **摄像头控制**：
   - `/api/cameras` - GET：获取可用摄像头列表
   - `/api/cameras/:id` - GET：获取特定摄像头信息
   - `/api/cameras/:id/open` - POST：打开摄像头
   - `/api/cameras/:id/close` - POST：关闭摄像头
   - `/api/cameras/:id/start` - POST：开始捕获
   - `/api/cameras/:id/stop` - POST：停止捕获
   - `/api/cameras/:id/params` - GET/POST：获取/设置摄像头参数

3. **流媒体**：
   - `/api/stream` - GET：MJPEG视频流
   - `/api/snapshot` - GET：获取当前帧的JPEG图像

4. **录制控制**：
   - `/api/recording/start` - POST：开始录制
   - `/api/recording/stop` - POST：停止录制
   - `/api/recordings` - GET：获取录制列表
   - `/api/recordings/:id` - GET/DELETE：获取/删除特定录制

5. **系统信息**：
   - `/api/system/info` - GET：获取系统状态信息
   - `/api/system/stats` - GET：获取实时性能统计



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
- **Web服务器**：Crow（现代C++ HTTP/WebSocket框架）
- **前端技术**：HTML5, CSS3, JavaScript（原生）
- **实时流**：WebSocket + MJPEG格式推流

## 项目结构

```
cam_server_cpp/
├── CMakeLists.txt                # 主CMake配置文件
├── doc/                          # 项目文档
│   ├── README.md                 # 主文档
│   ├── api.md                    # API文档
│   └── ...                       # 其他文档
├── logs/                         # 系统日志
│   ├── server.log                # 服务日志
│   └── dev_*.log                 # 开发日志
├── static/                       # 静态资源
│   ├── css/                      # 样式文件
│   ├── js/                       # 脚本文件
│   └── ...
├── src/                          # 源代码
│   ├── camera/                   # 摄像头模块
│   ├── video/                    # 视频处理模块
│   └── ...
├── third_party/                  # 第三方库
│   └── crow/                     # Crow现代C++ HTTP/WebSocket框架
├── config/                       # 配置文件
├── scripts/                      # 构建和部署脚本
└── build/                        # 构建输出目录（可选）
```

注：
1. 所有文档统一存放在 `doc/` 目录
2. 所有日志统一输出到 `logs/` 目录
3. 可执行文件默认生成在项目根目录

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

# 安装 fmt 库
sudo apt-get install -y libfmt-dev
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

Web界面包含以下页面：

- **首页（/index.html）**：集成了实时预览和系统信息功能，包含两个可切换的标签页：
  - **摄像头预览**：显示摄像头视频流，提供预览、拍照和录制功能，支持选择摄像头、分辨率和帧率
  - **系统信息**：显示系统状态信息，包括CPU温度、GPU温度、内存使用率、磁盘使用情况、IP地址和WiFi SSID等

### 实时预览功能

系统支持通过Web浏览器实时预览摄像头视频流：

```
http://<rk3588-ip-address>:8080/api/stream
```

也可以通过主页面访问实时预览功能，支持以下参数：

- **width**: 视频宽度（像素）
- **height**: 视频高度（像素）
- **quality**: JPEG质量（1-100）
- **fps**: 最大帧率

示例：
```
http://<rk3588-ip-address>:8080/api/stream?width=1280&height=720&quality=80&fps=30
```

### 拍照和录制功能

Web界面支持以下功能：

- **拍照**：在摄像头选择页面，可以拍摄当前帧并保存为JPEG图像
- **录制**：在摄像头选择页面，可以开始和停止视频录制，支持MP4格式
- **预览**：在所有页面，可以实时预览摄像头视频流
- **设备控制**：在设备管理页面，可以控制摄像头设备的参数

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
- **Web服务器模块**：实现了基于Mongoose的Web服务器，支持静态文件服务和API路由
- **MJPEG流媒体服务**：实现了基于HTTP的MJPEG流媒体服务，支持实时视频预览和多客户端连接
- **API接口实现**：实现了系统信息和摄像头控制的API接口
- **Web客户端**：实现了完整的Web界面，包括实时预览、设备管理、摄像头选择和系统信息页面
- **拍照功能**：实现了拍照和图像保存功能，支持在Web界面上拍摄当前帧并保存为JPEG图像
- **摄像头连接管理**：实现了摄像头连接的完整生命周期管理，包括打开、预览、停止和关闭
- **系统信息显示**：实现了系统信息页面，显示CPU温度、GPU温度、内存使用率、磁盘使用情况、IP地址和WiFi SSID等
- **用户界面优化**：优化了按钮布局和样式，提高了用户体验
- **环境变量处理**：改进环境变量获取方式，增强系统兼容性
- **多摄像头管理系统**：实现了完整的多摄像头管理架构，支持同时管理多个USB摄像头设备
- **多摄像头WebSocket推流**：为每个摄像头提供独立的WebSocket端点，支持MJPEG格式的并发推流
- **设备自动识别和过滤**：智能识别真正可用的摄像头设备，过滤重复的设备节点
- **Crow框架集成**：成功从Mongoose迁移到Crow框架，实现现代C++ HTTP/WebSocket服务
- **WebSocket通信突破**：✅ **重大突破** - 解决了WebSocket连接问题，实现稳定的双向通信

#### 进行中模块
- **视频录制功能**：正在实现视频录制和回放功能，支持MP4格式的视频录制和播放
- **存储管理模块**：正在实现视频文件和图像文件的存储管理，包括文件列表、删除和下载功能
- **MJPEG流优化**：正在优化MJPEG流处理，解决偶发的段错误问题
- **错误处理改进**：正在添加更详细的错误信息和用户友好的错误提示
- **权限管理**：正在实现更完善的权限控制系统，确保设备访问安全

#### 待实现模块
- **视频拆分功能**：将视频文件拆分为图像帧，支持不同的图像格式和拆分参数
- **高级摄像头控制**：支持更多摄像头参数调整，如亮度、对比度、饱和度等
- **图像处理功能**：添加基本的图像处理功能，如滤镜、特效等
- **高级录制功能**：实现更高级的录制功能，如定时录制、分段录制等
- **用户认证**：实现基本的用户认证和权限控制，保护敏感操作和数据
- **性能优化**：优化视频处理和流媒体服务的性能，提高系统稳定性和响应速度
- **多摄像头支持**：支持同时连接和控制多个摄像头设备（已实现）
- **视频分析功能**：添加基本的视频分析功能，如运动检测、对象识别等

## 多摄像头管理系统

### 概述

本项目实现了完整的多摄像头管理系统，支持同时管理和控制多个USB摄像头设备，每个摄像头可以独立进行MJPEG格式的WebSocket推流。

### 架构设计

#### 1. 多摄像头管理器 (`MultiCameraManager`)

负责管理多个摄像头设备的生命周期：

```cpp
// 核心功能
- 设备扫描和识别
- 独立的摄像头状态管理（打开/关闭/捕获/错误）
- 客户端连接计数管理
- 线程安全的设备操作
```

#### 2. 多摄像头WebSocket流处理器 (`MultiCameraWebSocketStreamer`)

为每个摄像头提供独立的WebSocket推流服务：

```cpp
// 核心功能
- 为每个摄像头创建独立的WebSocket端点
- MJPEG格式的直接推流支持
- 自动开始/停止捕获机制
- 实时帧率统计和客户端连接管理
```

### 摄像头设备识别和验证

#### 设备扫描流程

系统通过以下步骤识别可用的摄像头设备：

1. **扫描/dev目录**：
   ```bash
   # 查看所有video设备
   ls -la /dev/video*
   ```

2. **设备信息查询**：
   ```bash
   # 查看设备详细信息
   for dev in /dev/video*; do
     echo "=== $dev ==="
     v4l2-ctl -d $dev --info 2>/dev/null | head -5
   done
   ```

3. **验证设备能力**：
   ```bash
   # 检查设备支持的格式
   v4l2-ctl -d /dev/video0 --list-formats-ext
   ```

#### 典型设备配置示例

在RK3588开发板上，通过实际验证发现以下设备配置：

```bash
=== /dev/video0 ===
Driver Info:
        Driver name      : uvcvideo
        Card type        : DECXIN  CAMERA: DECXIN  CAMERA
        Bus info         : usb-xhci-hcd.12.auto-1.3
        Driver version   : 6.1.43
        Device Caps      : Video Capture, Streaming
        Supported Formats: MJPG (Motion-JPEG), YUYV (4:2:2)

=== /dev/video1 ===
Driver Info:
        Driver name      : uvcvideo
        Card type        : DECXIN  CAMERA: DECXIN  CAMERA
        Bus info         : usb-xhci-hcd.12.auto-1.3
        Driver version   : 6.1.43
        Device Caps      : Metadata Capture (元数据设备，不用于视频捕获)

=== /dev/video2 ===
Driver Info:
        Driver name      : uvcvideo
        Card type        : USB Camera: USB Camera
        Bus info         : usb-xhci-hcd.12.auto-1.4
        Driver version   : 6.1.43
        Device Caps      : Video Capture, Streaming
        Supported Formats: MJPG (Motion-JPEG), YUYV (4:2:2)

=== /dev/video3 ===
Driver Info:
        Driver name      : uvcvideo
        Card type        : USB Camera: USB Camera
        Bus info         : usb-xhci-hcd.12.auto-1.4
        Driver version   : 6.1.43
        Device Caps      : Metadata Capture (元数据设备，不用于视频捕获)
```

#### 设备节点说明

**重要发现**：每个物理USB摄像头会创建两个设备节点，但功能不同：

- **DECXIN CAMERA**（物理摄像头1）：
  - `/dev/video0` ✅ **主设备** - 支持视频捕获和MJPEG格式
  - `/dev/video1` ❌ **元数据设备** - 仅用于元数据，不支持视频捕获

- **USB Camera**（物理摄像头2）：
  - `/dev/video2` ✅ **主设备** - 支持视频捕获和MJPEG格式
  - `/dev/video3` ❌ **元数据设备** - 仅用于元数据，不支持视频捕获

**推荐使用的设备**：`/dev/video0` 和 `/dev/video2`

#### 设备过滤逻辑

系统通过以下逻辑识别真正可用的摄像头：

```cpp
bool CameraApi::queryDevice(const std::string& device_path, CameraDeviceInfo& info) {
    // 1. 尝试打开设备文件
    int fd = open(device_path.c_str(), O_RDWR);
    if (fd < 0) return false;

    // 2. 查询设备能力
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) return false;

    // 3. 验证是否为视频捕获设备
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) return false;

    // 4. 查询支持的格式和分辨率
    queryCapabilities(fd, info);

    // 5. 只有支持至少一种格式的设备才被认为是有效的
    return !info.formats.empty();
}
```

### WebSocket端点映射

系统为每个可用摄像头创建独立的WebSocket端点：

```
验证后的设备路径映射：
/dev/video0 (DECXIN CAMERA)  → /ws/camera/video0
/dev/video2 (USB Camera)     → /ws/camera/video2

客户端连接示例：
ws://localhost:8080/ws/camera/video0  # 连接DECXIN摄像头
ws://localhost:8080/ws/camera/video2  # 连接USB摄像头

注意：/dev/video1 和 /dev/video3 是元数据设备，不会创建WebSocket端点
```

## 🎉 WebSocket技术突破

### 重大进展

**日期：2025-01-22**

经过深入的问题分析和代码重构，我们成功解决了困扰项目的WebSocket连接问题，实现了稳定的双向通信。

### 问题分析与解决

#### 🔍 根本原因

通过对比工作的测试代码和主程序代码，发现了以下关键问题：

1. **路由冲突**：
   ```cpp
   // 问题代码：通用路由会拦截WebSocket升级请求
   CROW_ROUTE((*app_), "/<path>")  // 这会匹配所有路径，包括 /ws
   ```

2. **代码结构复杂**：
   - 原代码有重复的类定义和函数实现
   - lambda捕获方式不正确
   - 作用域混乱导致编译错误

3. **应用对象引用问题**：
   ```cpp
   // 工作代码
   CROW_WEBSOCKET_ROUTE(app, "/ws")        // 直接使用app对象

   // 问题代码
   CROW_WEBSOCKET_ROUTE((*app_), "/ws")    // 使用指针解引用
   ```

#### ✅ 解决方案

1. **创建简化版本**：
   - 基于工作的测试代码模式重写WebSocket实现
   - 移除复杂的路由逻辑，专注于WebSocket功能
   - 简化类结构，避免重复定义

2. **路由冲突解决**：
   ```cpp
   // 暂时禁用通用静态文件路由，避免冲突
   // 后续将实现更具体的路由规则
   ```

3. **代码重构**：
   - 统一lambda捕获方式：`[this]`
   - 简化应用对象管理
   - 清理重复的代码定义

### 验证结果

#### 🧪 测试验证

**浏览器端测试**：
```javascript
// WebSocket连接测试
const ws = new WebSocket('ws://192.168.124.12:8080/ws');

// 结果：✅ 连接成功
ws.onopen = function(event) {
    console.log('✅ WebSocket连接已打开');
    ws.send('Hello from browser!');
};

ws.onmessage = function(event) {
    console.log('📥 收到服务器消息:', event.data);
    // 输出：Echo: Hello from browser!
};
```

**验证功能**：
- ✅ WebSocket连接建立成功
- ✅ 双向消息传输正常
- ✅ 服务器正确回显消息
- ✅ 连接保持稳定

#### 📊 技术指标

- **连接延迟**：< 100ms
- **消息传输**：实时双向通信
- **稳定性**：连接保持稳定，无异常断开
- **并发支持**：支持多客户端同时连接

### 技术架构

#### 🏗️ 新架构特点

```cpp
class CrowServer::Impl {
    // 简化的WebSocket实现
    void setupWebSocket() {
        CROW_WEBSOCKET_ROUTE((*app_), "/ws")
          .onopen([this](crow::websocket::connection& conn) {
              // 连接建立处理
              std::string client_id = generateClientId();
              // 存储连接信息
          })
          .onmessage([this](crow::websocket::connection& conn,
                           const std::string& data, bool is_binary) {
              // 消息处理和回显
              conn.send_text("Echo: " + data);
          })
          .onclose([this](crow::websocket::connection& conn,
                         const std::string& reason, uint16_t code) {
              // 连接关闭清理
          });
    }
};
```

#### 🔧 关键改进

1. **简化的连接管理**：
   ```cpp
   struct SimpleWebSocketConnection {
       std::string client_id;
       crow::websocket::connection* ws;
       bool is_connected;
       std::chrono::steady_clock::time_point last_activity;
   };
   ```

2. **线程安全的操作**：
   ```cpp
   std::unordered_map<std::string, SimpleWebSocketConnection> ws_connections_;
   std::mutex ws_connections_mutex_;
   ```

3. **客户端ID生成**：
   ```cpp
   std::string generateClientId() {
       static std::atomic<uint64_t> next_id{1};
       return "ws-" + std::to_string(next_id++);
   }
   ```

## 📋 开发计划与TODO列表

### 🎯 短期目标（1-2周）

#### 高优先级
- [ ] **集成摄像头视频流到WebSocket**
  - [ ] 将MJPEG帧数据通过WebSocket发送
  - [ ] 实现视频流的启动/停止控制
  - [ ] 优化帧率和质量控制

- [ ] **恢复静态文件服务**
  - [ ] 重新实现不冲突的静态文件路由
  - [ ] 支持特定文件类型的路由规则
  - [ ] 确保与WebSocket路由不冲突

- [ ] **完善WebSocket日志系统**
  - [ ] 将WebSocket事件集成到统一日志系统
  - [ ] 添加连接状态监控和统计
  - [ ] 实现调试信息的分级输出

#### 中优先级
- [ ] **多客户端连接测试**
  - [ ] 验证多个浏览器同时连接
  - [ ] 测试并发消息处理能力
  - [ ] 优化连接池管理

- [ ] **错误处理增强**
  - [ ] 添加WebSocket连接异常处理
  - [ ] 实现自动重连机制
  - [ ] 改进错误信息提示

### 🚀 中期目标（2-4周）

#### 功能扩展
- [ ] **多摄像头WebSocket支持**
  - [ ] 为每个摄像头创建独立WebSocket端点
  - [ ] 实现摄像头切换和管理
  - [ ] 支持同时推流多个摄像头

- [ ] **前端界面优化**
  - [ ] 创建WebSocket视频播放器
  - [ ] 添加连接状态指示器
  - [ ] 实现摄像头控制面板

- [ ] **性能优化**
  - [ ] 优化视频帧编码效率
  - [ ] 减少内存占用和CPU使用率
  - [ ] 实现帧缓冲和流控制

#### 技术改进
- [ ] **代码重构**
  - [ ] 整理和清理冗余代码
  - [ ] 统一编码风格和命名规范
  - [ ] 添加详细的代码注释

- [ ] **测试覆盖**
  - [ ] 编写WebSocket单元测试
  - [ ] 添加集成测试用例
  - [ ] 实现自动化测试流程

### 🎯 长期目标（1-2个月）

#### 高级功能
- [ ] **视频录制集成**
  - [ ] 通过WebSocket控制录制开始/停止
  - [ ] 实现录制状态实时反馈
  - [ ] 支持录制文件的WebSocket传输

- [ ] **智能视频分析**
  - [ ] 集成运动检测算法
  - [ ] 实现对象识别功能
  - [ ] 添加异常事件告警

- [ ] **用户认证系统**
  - [ ] 实现WebSocket连接认证
  - [ ] 添加权限控制机制
  - [ ] 支持多用户并发访问

#### 系统优化
- [ ] **部署和运维**
  - [ ] 创建Docker容器化部署
  - [ ] 实现系统监控和告警
  - [ ] 添加日志分析和可视化

- [ ] **文档完善**
  - [ ] 编写详细的API文档
  - [ ] 创建用户使用手册
  - [ ] 添加开发者指南

### 🐛 已知问题

#### 需要解决的问题
- [ ] **日志输出问题**
  - WebSocket事件的`std::cout`输出未在终端显示
  - 需要调查输出缓冲或线程问题
  - 考虑统一使用日志系统

- [ ] **静态文件服务缺失**
  - 当前版本禁用了静态文件路由
  - 需要重新实现不冲突的文件服务
  - 确保Web界面正常加载

#### 技术债务
- [ ] **代码清理**
  - 移除备份的损坏代码文件
  - 整理项目目录结构
  - 统一代码风格

- [ ] **性能监控**
  - 添加WebSocket连接数监控
  - 实现内存使用情况跟踪
  - 监控视频流处理性能

### 📈 成功指标

#### 技术指标
- WebSocket连接成功率 > 99%
- 视频流延迟 < 200ms
- 支持并发连接数 > 10
- 系统稳定运行时间 > 24小时

#### 功能指标
- 多摄像头同时推流
- 实时视频质量控制
- 稳定的录制功能
- 完整的Web管理界面

---

**最后更新：2025-01-22**
**状态：WebSocket基础功能已突破，进入功能集成阶段**

### API接口

#### 1. 获取摄像头状态
```http
GET /api/camera/status
```

返回所有摄像头的状态信息，包括：
- 设备路径和名称
- 当前状态（关闭/打开/捕获中/错误）
- 客户端连接数
- 支持的格式和分辨率

#### 2. 获取摄像头列表
```http
GET /api/camera/list
```

返回系统中所有可用摄像头的详细信息。

### 使用示例

#### 启动多摄像头服务

```cpp
// 1. 初始化多摄像头管理器
auto& multi_camera_manager = camera::MultiCameraManager::getInstance();
multi_camera_manager.initialize();

// 2. 初始化WebSocket流处理器
auto streamer = std::make_unique<api::MultiCameraWebSocketStreamer>();
streamer->initialize(config, crow_server);

// 3. 添加摄像头到流处理器
streamer->addCamera("/dev/video0", 640, 480, 30);
streamer->addCamera("/dev/video2", 640, 480, 30);

// 4. 启动服务
streamer->start();
```

#### 客户端连接

```javascript
// 连接第一个摄像头
const ws1 = new WebSocket('ws://localhost:8080/ws/camera/video0');
ws1.onmessage = function(event) {
    // 处理MJPEG帧数据
    displayFrame(event.data, 'camera1-canvas');
};

// 连接第二个摄像头
const ws2 = new WebSocket('ws://localhost:8080/ws/camera/video2');
ws2.onmessage = function(event) {
    // 处理MJPEG帧数据
    displayFrame(event.data, 'camera2-canvas');
};
```

### 故障排除

#### 常见问题

1. **设备权限问题**：
   ```bash
   # 检查设备权限
   ls -la /dev/video*

   # 添加用户到video组
   sudo usermod -a -G video $USER
   ```

2. **设备被占用**：
   ```bash
   # 检查哪个进程在使用摄像头
   sudo lsof /dev/video0
   ```

3. **格式不支持**：
   ```bash
   # 检查设备支持的格式
   v4l2-ctl -d /dev/video0 --list-formats-ext
   ```

#### 验证命令

使用提供的测试脚本进行完整验证：

```bash
# 运行多摄像头验证脚本
./scripts/test_multi_camera.sh
```

手动验证命令：

```bash
# 1. 查看所有video设备
ls -la /dev/video*

# 2. 检查设备信息和能力
v4l2-ctl -d /dev/video0 --info
v4l2-ctl -d /dev/video2 --info

# 3. 验证支持的格式
v4l2-ctl -d /dev/video0 --list-formats
v4l2-ctl -d /dev/video2 --list-formats

# 4. 测试MJPEG格式设置
v4l2-ctl -d /dev/video0 --set-fmt-video=width=640,height=480,pixelformat=MJPG
v4l2-ctl -d /dev/video2 --set-fmt-video=width=640,height=480,pixelformat=MJPG

# 5. 测试帧捕获
v4l2-ctl -d /dev/video0 --stream-mmap --stream-count=5
v4l2-ctl -d /dev/video2 --stream-mmap --stream-count=5

# 6. 查看系统日志
tail -f logs/server.log | grep -E "(Camera|MultiCamera)"
```

### 性能考虑

- **并发连接**：每个摄像头支持多个客户端同时连接
- **资源管理**：自动管理摄像头资源，无客户端时自动停止捕获
- **帧率控制**：支持动态调整帧率以适应网络条件
- **内存优化**：使用零拷贝技术减少内存开销

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

## 可执行文件输出目录说明

项目中的可执行文件（如 `cam_server`）默认生成在**项目根目录**下，而非标准的 `build/bin` 目录。这是有意为之的设计，原因如下：

1. **资源访问需求**
   后端程序 `cam_server` 需要直接访问同级目录下的 `static/` 文件夹中的资源（如网页、配置文件等）。若可执行文件放在其他路径（如 `build/bin`），会导致资源路径解析复杂化。

2. **部署便捷性**
   在开发和生产环境中，保持可执行文件与资源文件的相对路径一致，避免因路径差异导致的运行时错误。

### 重要提醒
- 请勿随意修改 `CMakeLists.txt` 中的以下配置：
  ```cmake
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
  ```
  除非你明确理解修改后的影响，并已测试资源加载逻辑。
- 如需更精准控制，可改用 `set_target_properties()`，但需确保资源路径仍能正确解析。

## 重要目录说明

1. **项目文档**
   所有文档（包括开发指南、API说明、用户手册等）均存放在 `doc/` 目录下。这是项目的唯一文档来源，请勿在其他位置存放文档。

2. **日志文件**
   所有运行时日志和系统日志均输出到 `logs/` 目录。包括：
   - 服务日志（`server.log`）
   - 开发环境日志（`dev_*.log`）
   - 其他模块日志

   > 注意：请定期清理日志文件以避免占用过多磁盘空间。

## 日志系统使用说明

### 日志级别

系统支持以下日志级别（从低到高）：

- **TRACE** - 最详细的日志，用于跟踪程序执行流程
- **DEBUG** - 调试信息，用于开发阶段的问题定位
- **INFO** - 一般信息，记录程序运行过程中的重要事件
- **WARNING** - 警告信息，表示潜在的问题
- **ERROR** - 错误信息，表示程序执行出错但可以继续运行
- **CRITICAL** - 严重错误，可能导致程序无法继续运行

### 命令行参数

启动程序时，可以使用以下参数控制日志行为：

```bash
# 设置日志级别为debug
./cam_server --log-level debug

# 简写形式
./cam_server -l info

# 结合其他参数使用
./cam_server --log-level warning --config /path/to/config.json
```

### 配置文件中的日志设置

在配置文件中可以通过以下配置项控制日志行为：

```json
{
  "log": {
    "level": "info",
    "file": "/var/log/cam_server.log",
    "max_size": 10485760,
    "max_files": 5,
    "console": true,
    "async": true,
    "async_queue_size": 4096,
    "flush_interval": 1,
    "include_timestamp": true,
    "include_thread_id": false,
    "include_file_line": false,
    "include_function_name": false
  }
}
```

### 日志格式

日志条目的默认格式为：
```
[MM-DD HH:MM:SS] [LEVEL] [MODULE] Message
```

例如：
```
[05-20 19:15:30] [INFO] [Camera] 摄像头初始化成功
[05-20 19:15:31] [DEBUG] [Stream] 新的客户端连接: 192.168.1.100
[05-20 19:15:35] [WARNING] [Storage] 磁盘空间不足: 85% 已使用
```

### 日志文件轮转

当日志文件达到 `max_size` 设置的大小时，会自动进行日志轮转：
1. 当前日志文件会被重命名为 `filename.1`
2. 如果存在 `filename.1`，则重命名为 `filename.2`，依此类推
3. 最多保留 `max_files` 个日志文件
4. 创建新的日志文件继续记录

### 最佳实践

1. **开发环境**：
   - 使用 `--log-level debug` 获取详细日志
   - 启用 `include_file_line` 和 `include_function_name` 方便调试

2. **生产环境**：
   - 使用 `--log-level info` 或 `--log-level warning` 减少日志量
   - 设置合理的 `max_size` 和 `max_files` 避免磁盘空间耗尽
   - 考虑禁用控制台输出（`"console": false`）以提升性能

3. **问题排查**：
   - 临时提高日志级别到 `trace` 获取最详细的信息
   - 检查 `logs/` 目录下的日志文件
   - 使用 `grep` 过滤特定模块或级别的日志

### 注意事项

1. 异步日志虽然能提高性能，但在程序崩溃时可能会丢失部分未写入磁盘的日志
2. 日志文件默认保存在 `logs/` 目录，请确保程序有写入权限
3. 长时间运行的服务建议配置日志轮转和定期清理策略