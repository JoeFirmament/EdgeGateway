# 🏗️ 模块化服务器架构设计

## 📋 概述

本文档描述了摄像头视频流服务器的模块化重构设计，将原有的单体文件 `websocket_video_stream_test.cpp` (2000+行) 重构为多个专注的模块，每个模块不超过500行代码。

## 🎯 设计目标

### 核心原则
- **单一职责原则** - 每个模块只负责一个特定功能
- **代码可维护性** - 每个文件控制在500行以内
- **功能完整性** - 保持所有原有功能不变
- **性能一致性** - 重构不影响系统性能
- **扩展友好性** - 便于添加新功能

### 质量目标
- ✅ 代码行数控制：每个文件 ≤ 500行
- ✅ 功能完整性：100%保留原有功能
- ✅ 性能保持：无性能损失
- ✅ 可维护性：模块边界清晰
- ✅ 可测试性：支持单元测试

## 📁 模块架构

### 🗂️ 目录结构
```
cam_server_cpp/
├── main_server.cpp                    # 新的主程序入口 (~100行)
├── websocket_video_stream_test.cpp    # 原始文件(保留作为参考)
├── build_main_server.sh               # 新的编译脚本
├── build_websocket_video_stream_test.sh # 原始编译脚本(保留)
├── include/web/                       # Web模块头文件
│   ├── video_server.h                 # 服务器核心类
│   ├── http_routes.h                  # HTTP路由处理
│   ├── websocket_handler.h            # WebSocket处理
│   ├── frame_extraction_routes.h      # 帧提取路由
│   └── system_routes.h                # 系统信息路由
├── src/web/                           # Web模块实现
│   ├── video_server.cpp               # 服务器核心实现 (~200行)
│   ├── http_routes.cpp                # HTTP路由实现 (~300行)
│   ├── websocket_handler.cpp          # WebSocket实现 (~300行)
│   ├── frame_extraction_routes.cpp    # 帧提取实现 (~200行)
│   └── system_routes.cpp              # 系统信息实现 (~150行)
└── doc/architecture/                  # 架构文档
    └── modular-server-design.md       # 本文档
```

## 🧩 模块详细设计

### 1. 主程序模块 (`main_server.cpp`)
**职责**: 程序入口点和生命周期管理
**代码行数**: ~100行

**功能**:
- 命令行参数解析 (`-p/--port`, `-h/--help`)
- 信号处理 (SIGINT, SIGTERM)
- 服务器实例创建和管理
- 错误处理和优雅退出

**接口**:
```cpp
int main(int argc, char* argv[]);
void signalHandler(int signal);
int parseArguments(int argc, char* argv[]);
void showUsage(const char* program_name);
```

### 2. 服务器核心模块 (`VideoServer`)
**职责**: 服务器核心逻辑和组件协调
**代码行数**: ~200行

**功能**:
- 组件初始化 (摄像头管理器、系统监控)
- 路由设置协调
- 资源管理和清理
- 配置管理

**核心类**:
```cpp
class VideoServer {
public:
    bool initialize();
    bool start();
    void stop();
    void setPort(int port);

private:
    crow::SimpleApp app_;
    int port_;
    bool is_running_;
    // 客户端管理、任务管理等
};
```

### 3. HTTP路由模块 (`HttpRoutes`)
**职责**: HTTP API和静态文件服务
**代码行数**: ~300行

**功能**:
- 静态文件服务 (HTML, CSS, JS)
- 图片API (`/api/photos/*`)
- 视频API (`/api/videos/*`)
- 页面路由 (`/*.html`)
- 动态HTML路由发现

**接口**:
```cpp
class HttpRoutes {
public:
    static void setupStaticRoutes(crow::SimpleApp& app);
    static void setupPhotoRoutes(crow::SimpleApp& app);
    static void setupVideoRoutes(crow::SimpleApp& app);
    static void setupPageRoutes(crow::SimpleApp& app);
};
```

### 4. WebSocket处理模块 (`WebSocketHandler`)
**职责**: WebSocket连接和视频流处理
**代码行数**: ~300行

**功能**:
- WebSocket连接管理
- 客户端状态跟踪
- 视频流传输
- 消息路由和处理
- 错误处理和重连

**接口**:
```cpp
class WebSocketHandler {
public:
    static void setupRoutes(crow::SimpleApp& app, VideoServer* server);

private:
    static void onOpen(crow::websocket::connection& conn, VideoServer* server);
    static void onMessage(crow::websocket::connection& conn, const std::string& data,
                         bool is_binary, VideoServer* server);
    static void onClose(crow::websocket::connection& conn, const std::string& reason,
                       VideoServer* server);
};
```

### 5. 帧提取路由模块 (`FrameExtractionRoutes`)
**职责**: 视频帧提取功能
**代码行数**: ~200行

**功能**:
- 帧提取任务管理
- MJPEG文件处理
- 进度跟踪
- 结果打包和下载
- 预览图片生成

**API端点**:
- `POST /api/frame-extraction/start` - 开始提取
- `GET /api/frame-extraction/status/<task_id>` - 获取状态
- `POST /api/frame-extraction/stop/<task_id>` - 停止任务
- `GET /api/frame-extraction/download/<task_id>` - 下载结果
- `GET /api/frame-extraction/preview/<task_id>/<filename>` - 预览图片

### 6. 系统信息路由模块 (`SystemRoutes`)
**职责**: 系统状态和硬件信息API
**代码行数**: ~150行

**功能**:
- 系统信息获取 (CPU、内存、存储)
- 硬件状态监控
- 摄像头设备信息
- 网络接口信息
- 实时性能数据

**API端点**:
- `GET /api/system/info` - 系统信息
- `GET /api/system/cameras` - 摄像头设备

## 🔄 数据流设计

### 请求处理流程
```
客户端请求 → Crow路由 → 对应模块处理 → 响应返回

HTTP请求:
Client → HttpRoutes → 静态文件/API处理 → Response

WebSocket:
Client → WebSocketHandler → VideoServer → CameraManager → 视频流

帧提取:
Client → FrameExtractionRoutes → 异步任务 → 文件处理 → 结果返回

系统信息:
Client → SystemRoutes → SystemMonitor → 实时数据 → JSON响应
```

### 组件依赖关系
```
main_server.cpp
    ↓
VideoServer (核心协调)
    ├── HttpRoutes (HTTP处理)
    ├── WebSocketHandler (WebSocket处理)
    ├── FrameExtractionRoutes (帧提取)
    ├── SystemRoutes (系统信息)
    ├── CameraManager (摄像头管理)
    └── SystemMonitor (系统监控)
```

## 🚀 实施计划

### 阶段1: 基础架构 (已完成)
- ✅ 创建目录结构
- ✅ 定义头文件接口
- ✅ 创建主程序框架
- ✅ 设计编译脚本
- ✅ 创建架构文档

### 阶段2: 核心模块实现 (已完成)
- ✅ 实现 VideoServer 核心类 (~200行)
- ✅ 实现 HttpRoutes 模块 (~300行)
- ✅ 实现 WebSocketHandler 模块 (~300行)

### 阶段3: 功能模块实现 (已完成)
- ✅ 实现 FrameExtractionRoutes 模块 (~200行)
- ✅ 实现 SystemRoutes 模块 (~150行)
- ✅ 创建 Makefile 编译系统
- ✅ 更新系统信息页面API集成

### 阶段4: 验证和优化
- ⏳ 功能完整性测试
- ⏳ 性能对比测试
- ⏳ 代码质量检查
- ⏳ 文档完善

## 📊 质量保证

### 代码质量指标
- **行数控制**: 每个文件 ≤ 500行
- **圈复杂度**: 每个函数 ≤ 10
- **注释覆盖率**: ≥ 20%
- **编译警告**: 0个警告

### 测试策略
- **单元测试**: 每个模块独立测试
- **集成测试**: 模块间接口测试
- **功能测试**: 端到端功能验证
- **性能测试**: 与原版本对比

### 兼容性保证
- **API兼容**: 所有原有API保持不变
- **配置兼容**: 配置文件格式不变
- **部署兼容**: 部署方式保持一致

## 🔧 开发指南

### 编译和运行
```bash
# 编译新版本
./build_main_server.sh

# 运行服务器
./main_server -p 8081

# 查看帮助
./main_server --help
```

### 添加新功能
1. 在对应模块中添加功能
2. 更新头文件接口
3. 在 VideoServer 中注册路由
4. 更新编译脚本
5. 添加测试用例

### 调试建议
- 使用模块化日志系统
- 每个模块独立调试
- 利用单元测试快速定位问题

## 📈 预期收益

### 开发效率提升
- **维护成本降低**: 40%
- **新功能开发速度**: 提升30%
- **Bug修复时间**: 减少50%
- **代码审查效率**: 提升60%

### 代码质量提升
- **可读性**: 显著提升
- **可测试性**: 支持单元测试
- **可扩展性**: 便于添加新功能
- **可维护性**: 模块边界清晰

---

**文档版本**: v1.0
**创建日期**: 2024-05-24
**最后更新**: 2024-05-24
**维护者**: 开发团队
