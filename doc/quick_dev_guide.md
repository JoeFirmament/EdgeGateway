# ⚡ 快速开发指南
## 深视边缘视觉平台 (DeepVision Edge Platform)

## 🚀 快速开始

### 编译运行
```bash
# 1. 清理并编译
make clean && make -j8

# 2. 启动服务器
./main_server -p 8081

# 3. 访问主页
http://localhost:8081
```

## 📝 常见开发任务

### 🆕 添加新页面

#### 1. 创建页面文件
```bash
# 在 static/pages/ 目录创建新页面
touch static/pages/new_feature.html
```

#### 2. 页面模板
```html
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>🆕 新功能 - 摄像头服务器</title>
    <link rel="stylesheet" href="/static/css/unified-style.css">
</head>
<body>
    <div class="app-container">
        <!-- 统一导航栏 -->
        <div id="navigation-container"></div>

        <!-- 页面标题 -->
        <header class="page-header">
            <h1 class="page-title">New Feature</h1>
            <p class="page-subtitle">Feature description</p>
        </header>

        <!-- 主要内容 -->
        <div class="card fade-in">
            <div class="card-header">
                <h3 class="card-title">🆕 功能标题</h3>
            </div>
            <div class="card-content">
                <!-- 页面内容 -->
            </div>
        </div>
    </div>

    <script src="/static/js/navigation.js"></script>
    <script>
        document.addEventListener('DOMContentLoaded', function() {
            console.log('🆕 新功能页面已加载');
        });
    </script>
</body>
</html>
```

#### 3. 添加到导航栏
```html
<!-- 编辑 static/components/navigation.html -->
<nav class="top-navigation">
    <div class="nav-tabs">
        <!-- 现有导航项... -->
        <a href="/new_feature.html" class="nav-tab" data-page="new_feature">🆕 新功能</a>
    </div>
</nav>
```

#### 4. 更新页面映射
```javascript
// 编辑 static/js/navigation.js 的 detectCurrentPage() 方法
const pageMap = {
    // 现有映射...
    '/new_feature.html': 'new_feature'
};
```

### 🔌 添加新 API

#### 1. 创建路由文件
```cpp
// src/web/new_routes.h
#pragma once
#include <crow.h>

namespace cam_server {
namespace web {

class NewRoutes {
public:
    static void setupRoutes(crow::SimpleApp& app);
private:
    static void setupNewApiRoute(crow::SimpleApp& app);
};

} // namespace web
} // namespace cam_server
```

```cpp
// src/web/new_routes.cpp
#include "web/new_routes.h"

namespace cam_server {
namespace web {

void NewRoutes::setupRoutes(crow::SimpleApp& app) {
    setupNewApiRoute(app);
}

void NewRoutes::setupNewApiRoute(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/api/new/endpoint")
    ([](const crow::request& req) {
        std::string response = "{\"success\":true,\"data\":\"Hello World\"}";
        crow::response res(200, response);
        res.set_header("Content-Type", "application/json");
        return res;
    });
}

} // namespace web
} // namespace cam_server
```

#### 2. 更新 Makefile
```makefile
# 在 WEB_SOURCES 中添加新文件
WEB_SOURCES = src/web/video_server.cpp \
              src/web/http_routes.cpp \
              src/web/new_routes.cpp \
              # 其他文件...
```

#### 3. 注册路由
```cpp
// 在 src/web/video_server.cpp 的 setupRoutes() 方法中添加
#include "web/new_routes.h"

void VideoServer::setupRoutes() {
    // 现有路由...
    NewRoutes::setupRoutes(app_);
}
```

### 🎨 修改样式

#### 全局样式修改
```css
/* 编辑 static/css/unified-style.css */
.new-component {
    background: rgba(255, 255, 255, 0.9);
    border-radius: 8px;
    padding: 16px;
}
```

#### 页面特定样式
```html
<!-- 在页面内添加 -->
<style>
.page-specific-style {
    color: rgba(44, 62, 80, 0.9);
}
</style>
```

## 🔧 调试技巧

### 编译调试
```bash
# 编译调试版本
make debug

# 检查语法
make test-compile

# 查看编译统计
make stats
```

### 运行时调试
```bash
# 启动服务器并查看详细日志
./main_server -p 8081

# 测试 API
curl -s http://localhost:8081/api/cameras | jq

# 检查页面加载
curl -I http://localhost:8081/
```

### 前端调试
```javascript
// 在浏览器控制台检查导航栏状态
console.log(window.navigationManager.getCurrentPage());
console.log(window.navigationManager.isLoaded());

// 测试导航功能
window.navigationManager.setActivePage('new_feature');
```

## 📁 文件组织规范

### 命名约定
- **页面文件**: `feature_name.html` (下划线分隔)
- **路由文件**: `feature_routes.cpp/h`
- **样式类**: `.feature-component` (连字符分隔)
- **JavaScript**: `camelCase`

### 目录结构
```
新功能开发建议目录结构:
src/web/feature_routes.cpp     # 后端 API
static/pages/feature_name.html # 前端页面
static/js/feature.js          # 页面特定 JS (可选)
```

## 🧪 测试流程

### 功能测试
1. **编译测试**: `make clean && make -j8`
2. **启动测试**: `./main_server -p 8081`
3. **页面测试**: 访问所有导航页面
4. **API 测试**: 使用 curl 或 Postman 测试接口
5. **浏览器测试**: 检查控制台是否有错误

### 导航栏测试
- 访问 `http://localhost:8081/test_navigation.html`
- 检查所有导航链接是否正常
- 验证当前页面高亮是否正确

## 🚨 常见问题

### 编译问题
```bash
# 问题: 找不到头文件
# 解决: 检查 Makefile 中的 INCLUDES 路径

# 问题: 链接错误
# 解决: 检查 Makefile 中的 LIBS 和源文件列表

# 问题: 编译警告
# 解决: 检查未使用的参数和变量
```

### 页面问题
```javascript
// 问题: 导航栏不显示
// 解决: 检查是否包含 navigation.js 和容器元素

// 问题: 样式不一致
// 解决: 确保引用了 unified-style.css

// 问题: 页面不高亮
// 解决: 检查页面映射和 data-page 属性
```

### API 问题
```bash
# 问题: 404 错误
# 解决: 检查路由注册和 URL 路径

# 问题: CORS 错误
# 解决: 检查响应头设置

# 问题: JSON 格式错误
# 解决: 验证 JSON 格式和 Content-Type
```

## 📚 参考资源

### 项目文档
- `doc/project_architecture.md` - 项目架构详解
- `doc/api_documentation.md` - API 接口文档
- `README.md` - 项目概述

### 代码参考
- `static/pages/frame_extraction.html` - 页面设计标准
- `src/web/system_routes.cpp` - API 实现参考
- `static/js/navigation.js` - 前端组件参考

### 在线资源
- [Crow Framework](https://crowcpp.org/) - Web 框架文档
- [V4L2 API](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html) - 摄像头接口文档
