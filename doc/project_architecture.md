# 📁 项目架构文档

## 🎯 项目概述
深视边缘视觉平台 (DeepVision Edge Platform) - 基于 RK3588 的智能视觉处理平台，采用 Crow 框架 + WebSocket 实现 Web 服务器，支持多摄像头录制、帧提取、AI视觉处理、系统监控等功能。

## 🏗️ 目录结构

```
cam_server_cpp/
├── 📁 src/                     # 源代码目录
│   ├── 📁 web/                 # Web服务模块
│   ├── 📁 camera/              # 摄像头管理模块
│   ├── 📁 system/              # 系统监控模块
│   ├── 📁 monitor/             # 日志监控模块
│   ├── 📁 utils/               # 工具类模块
│   └── 📁 vision/              # 视觉处理模块（占位）
├── 📁 include/                 # 头文件目录
├── 📁 static/                  # 静态资源目录
│   ├── 📁 pages/               # HTML页面
│   ├── 📁 css/                 # 样式文件
│   ├── 📁 js/                  # JavaScript文件
│   └── 📁 components/          # 可复用组件
├── 📁 third_party/            # 第三方库
├── 📁 config/                 # 配置文件
├── 📁 build/                  # 编译输出目录
├── 📁 doc/                    # 项目文档
├── main_server.cpp            # 主程序入口
├── Makefile                   # 编译配置
└── build_main_server.sh       # 编译脚本（已弃用）
```

## 🔧 编译系统

### Makefile 架构
- **位置**: 项目根目录 `Makefile`
- **特点**: 模块化编译，支持增量编译，并行编译
- **目标**:
  - `make` - 编译默认版本
  - `make debug` - 编译调试版本
  - `make clean` - 清理编译文件
  - `make -j8` - 并行编译（推荐）

### 源文件组织
```makefile
# Web模块
WEB_SOURCES = src/web/video_server.cpp
              src/web/http_routes.cpp
              src/web/websocket_handler.cpp
              src/web/frame_extraction_routes.cpp
              src/web/system_routes.cpp
              src/web/serial_routes.cpp

# 核心模块
CORE_SOURCES = src/camera/v4l2_camera.cpp
               src/camera/camera_manager.cpp
               src/system/system_monitor.cpp
               src/monitor/logger.cpp
               src/utils/config_manager.cpp
```

## 🌐 静态页面架构

### 页面组织结构
```
static/
├── index.html                  # 主页（项目根目录）
├── 📁 pages/                   # 功能页面目录
│   ├── video_recording.html    # 视频录制页面
│   ├── frame_extraction.html   # 帧提取页面
│   ├── photo_capture.html      # 拍照功能页面
│   ├── system_info.html        # 系统信息页面
│   └── serial_info.html        # 串口信息页面
├── 📁 css/
│   └── unified-style.css       # 统一样式文件
├── 📁 js/
│   └── navigation.js           # 导航栏管理器
└── 📁 components/
    └── navigation.html         # 导航栏组件
```

### 动态加载方式

#### 🧭 统一导航栏系统
- **组件文件**: `static/components/navigation.html`
- **管理器**: `static/js/navigation.js`
- **加载方式**: JavaScript 动态加载
- **优势**: 统一管理，修改一处即可更新所有页面

#### 使用方法
```html
<!-- 1. 在页面中添加导航容器 -->
<div id="navigation-container"></div>

<!-- 2. 引入导航管理器 -->
<script src="/static/js/navigation.js"></script>

<!-- 3. 自动初始化（或手动调用） -->
<script>
// 自动检测并初始化
// 或手动调用: initNavigation();
</script>
```

#### 页面映射
```javascript
const pageMap = {
    '/': 'home',
    '/video_recording.html': 'video_recording',
    '/frame_extraction.html': 'frame_extraction',
    '/photo_capture.html': 'photo_capture',
    '/system_info.html': 'system_info',
    '/serial_info.html': 'serial_info'
};
```

## 📦 模块架构

### Web 服务模块 (`src/web/`)
- **video_server.cpp** - 主服务器类，协调各模块
- **http_routes.cpp** - HTTP 路由处理，静态文件服务
- **websocket_handler.cpp** - WebSocket 连接管理
- **frame_extraction_routes.cpp** - 帧提取 API
- **system_routes.cpp** - 系统信息 API
- **serial_routes.cpp** - 串口信息 API

### 摄像头模块 (`src/camera/`)
- **v4l2_camera.cpp** - V4L2 摄像头驱动封装
- **camera_manager.cpp** - 摄像头管理器
- **frame.cpp** - 帧数据结构
- **format_utils.cpp** - 格式转换工具

### 系统模块 (`src/system/`)
- **system_monitor.cpp** - 系统资源监控

### 工具模块 (`src/utils/`)
- **config_manager.cpp** - 配置管理
- **file_utils.cpp** - 文件操作工具
- **string_utils.cpp** - 字符串处理工具

## 🎨 前端架构

### 设计系统
- **统一样式**: `unified-style.css` 提供一致的视觉风格
- **组件化**: 导航栏、卡片、按钮等可复用组件
- **响应式**: 网格布局适配不同屏幕尺寸
- **主题**: 米白色背景，现代化排版，低饱和度配色

### 页面结构模板
```html
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <link rel="stylesheet" href="/static/css/unified-style.css">
</head>
<body>
    <div class="app-container">
        <!-- 统一导航栏 -->
        <div id="navigation-container"></div>

        <!-- 页面标题 -->
        <header class="page-header">
            <h1 class="page-title">Page Title</h1>
            <p class="page-subtitle">Page description</p>
        </header>

        <!-- 主要内容 -->
        <div class="grid grid-2">
            <div class="card fade-in">
                <!-- 卡片内容 -->
            </div>
        </div>
    </div>

    <script src="/static/js/navigation.js"></script>
</body>
</html>
```

## 🚀 部署与运行

### 编译命令
```bash
# 清理并编译
make clean && make -j8

# 调试版本
make debug

# 发布版本
make release
```

### 运行命令
```bash
# 启动服务器
./main_server -p 8081

# 访问地址
http://localhost:8081
```

### 主要功能页面
- 🏠 主页: `/`
- 🎬 视频录制: `/video_recording.html`
- 🖼️ 帧提取: `/frame_extraction.html`
- 📸 拍照功能: `/photo_capture.html`
- 🖥️ 系统信息: `/system_info.html`
- 🔌 串口信息: `/serial_info.html`

## 📋 API 接口

### 摄像头 API
- `GET /api/cameras` - 获取摄像头列表
- `POST /api/camera/start_recording` - 开始录制
- `POST /api/camera/stop_recording` - 停止录制
- `POST /api/camera/capture` - 拍照

### 系统 API
- `GET /api/system/info` - 获取系统信息
- `GET /api/serial/devices` - 获取串口设备列表

### 帧提取 API
- `POST /api/frame-extraction/start` - 开始帧提取
- `GET /api/frame-extraction/status` - 获取提取状态

## 🔄 开发工作流

### 添加新页面
1. 在 `static/pages/` 创建 HTML 文件
2. 在 `static/components/navigation.html` 添加导航项
3. 在 `static/js/navigation.js` 添加页面映射
4. 所有现有页面自动显示新导航项

### 添加新 API
1. 在对应模块创建路由文件（如 `src/web/new_routes.cpp`）
2. 在 `Makefile` 的 `WEB_SOURCES` 中添加源文件
3. 在 `src/web/video_server.cpp` 中注册路由

### 修改样式
- 全局样式: 修改 `static/css/unified-style.css`
- 页面特定样式: 在页面内添加 `<style>` 标签

## 💡 设计原则

- **模块化**: 功能分离，便于维护
- **统一性**: 统一的样式和交互模式
- **可扩展**: 易于添加新功能和页面
- **性能优化**: 增量编译，并行处理
- **用户友好**: 直观的界面和清晰的导航
