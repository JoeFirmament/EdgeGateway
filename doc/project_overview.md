# 📋 项目概览
## 深视边缘视觉平台 (DeepVision Edge Platform)

## 🎯 项目简介
基于 RK3588 的智能视觉处理平台，采用现代化的模块化架构，支持多摄像头管理、视频录制、帧提取、AI视觉处理、系统监控等功能。

## 🏗️ 核心架构

### 编译系统
- **构建工具**: Makefile (支持并行编译)
- **编译命令**: `make clean && make -j8`
- **源文件组织**: 模块化分离，增量编译

### 静态页面系统
- **主页位置**: `static/index.html`
- **功能页面**: `static/pages/*.html`
- **统一导航**: `static/components/navigation.html`
- **动态加载**: JavaScript 组件化管理

### 后端架构
- **Web框架**: Crow (C++ HTTP/WebSocket)
- **模块分离**: Web、Camera、System、Utils
- **API驱动**: RESTful 接口设计

## 📁 关键目录结构

```
cam_server_cpp/
├── 📄 main_server.cpp          # 主程序入口
├── 📄 Makefile                 # 编译配置（核心）
├── 📁 src/                     # 源代码
│   ├── 📁 web/                 # Web服务模块
│   ├── 📁 camera/              # 摄像头管理
│   ├── 📁 system/              # 系统监控
│   └── 📁 utils/               # 工具类
├── 📁 static/                  # 前端资源
│   ├── 📄 index.html           # 主页
│   ├── 📁 pages/               # 功能页面
│   ├── 📁 components/          # 可复用组件
│   ├── 📁 css/                 # 样式文件
│   └── 📁 js/                  # JavaScript
└── 📁 doc/                     # 项目文档
```

## 🚀 快速操作

### 编译运行
```bash
# 编译
make clean && make -j8

# 启动
./main_server -p 8081

# 访问
http://localhost:8081
```

### 主要页面
- 🏠 **主页**: `/`
- 🎬 **视频录制**: `/video_recording.html`
- 🖼️ **帧提取**: `/frame_extraction.html`
- 📸 **拍照功能**: `/photo_capture.html`
- 🖥️ **系统信息**: `/system_info.html`
- 🔌 **串口信息**: `/serial_info.html`

## 🧭 导航栏系统

### 统一管理
- **组件文件**: `static/components/navigation.html`
- **管理器**: `static/js/navigation.js`
- **优势**: 修改一处，全局更新

### 使用方式
```html
<!-- 页面中添加容器 -->
<div id="navigation-container"></div>

<!-- 引入管理器 -->
<script src="/static/js/navigation.js"></script>
```

### 添加新页面
1. 创建页面文件
2. 更新 `navigation.html`
3. 更新 `navigation.js` 页面映射
4. 所有页面自动显示新导航

## 🔧 开发工作流

### 添加新功能页面
```bash
# 1. 创建页面
touch static/pages/new_feature.html

# 2. 编辑导航组件
vim static/components/navigation.html

# 3. 更新页面映射
vim static/js/navigation.js
```

### 添加新API
```bash
# 1. 创建路由文件
touch src/web/new_routes.cpp
touch src/web/new_routes.h

# 2. 更新Makefile
vim Makefile  # 添加到WEB_SOURCES

# 3. 注册路由
vim src/web/video_server.cpp
```

### 修改样式
```bash
# 全局样式
vim static/css/unified-style.css

# 页面特定样式
# 在页面内添加<style>标签
```

## 📦 模块说明

### Web模块 (`src/web/`)
- `video_server.cpp` - 主服务器
- `http_routes.cpp` - HTTP路由
- `websocket_handler.cpp` - WebSocket处理
- `frame_extraction_routes.cpp` - 帧提取API
- `system_routes.cpp` - 系统信息API
- `serial_routes.cpp` - 串口信息API

### 前端组件 (`static/`)
- `index.html` - 主页入口
- `pages/` - 功能页面目录
- `components/navigation.html` - 导航栏组件
- `js/navigation.js` - 导航管理器
- `css/unified-style.css` - 统一样式

## 🎨 设计原则

### 前端设计
- **统一样式**: 所有页面使用 `unified-style.css`
- **组件化**: 导航栏等可复用组件
- **响应式**: 网格布局适配不同屏幕
- **现代化**: 米白色背景，低饱和度配色

### 后端设计
- **模块化**: 功能分离，便于维护
- **API驱动**: RESTful接口设计
- **可扩展**: 易于添加新功能
- **性能优化**: 增量编译，并行处理

## 📋 API接口

### 摄像头API
- `GET /api/cameras` - 摄像头列表
- `POST /api/camera/start_recording` - 开始录制
- `POST /api/camera/stop_recording` - 停止录制
- `POST /api/camera/capture` - 拍照

### 系统API
- `GET /api/system/info` - 系统信息
- `GET /api/serial/devices` - 串口设备

### 帧提取API
- `POST /api/frame-extraction/start` - 开始提取
- `GET /api/frame-extraction/status` - 提取状态

## 🔍 调试技巧

### 编译调试
```bash
make debug          # 调试版本
make test-compile   # 语法检查
make stats          # 代码统计
```

### 运行调试
```bash
./main_server -p 8081  # 启动服务器
curl -s http://localhost:8081/api/cameras  # 测试API
```

### 前端调试
```javascript
// 浏览器控制台
window.navigationManager.getCurrentPage()
window.navigationManager.isLoaded()
```

## 📚 文档索引

### 核心文档
- `doc/project_architecture.md` - 详细架构说明
- `doc/quick_dev_guide.md` - 快速开发指南
- `README.md` - 项目总览

### 开发文档
- `dev_log.md` - 开发日志
- `api_documentation.md` - API文档

## 💡 最佳实践

### 开发建议
1. **遵循模块化**: 新功能按模块组织
2. **使用统一样式**: 保持界面一致性
3. **API优先**: 后端提供API，前端调用
4. **组件复用**: 利用现有组件和样式
5. **增量开发**: 小步快跑，逐步完善

### 代码规范
- **C++**: 使用 snake_case，类名 PascalCase
- **HTML**: 语义化标签，统一缩进
- **CSS**: BEM命名，模块化组织
- **JavaScript**: camelCase，ES6+语法

## 🎯 项目特色

### 技术亮点
- ✅ **统一导航栏系统** - 组件化管理
- ✅ **模块化编译** - Makefile增量编译
- ✅ **动态页面扫描** - 自动路由注册
- ✅ **API驱动架构** - 前后端分离
- ✅ **响应式设计** - 现代化界面

### 开发优势
- **易于扩展** - 添加新功能简单
- **维护友好** - 模块化架构清晰
- **性能优化** - 并行编译，按需加载
- **用户体验** - 统一界面，直观操作

---

**快速开始**: `make clean && make -j8 && ./main_server -p 8081`
**主要访问**: `http://localhost:8081`
**文档位置**: `doc/` 目录
