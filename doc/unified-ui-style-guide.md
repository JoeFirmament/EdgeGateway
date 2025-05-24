# 🎨 统一UI样式指南

## 概述

基于现代极简主义设计理念，我们创建了一套统一的CSS样式系统，适用于所有摄像头管理系统页面。设计灵感来源于专业级界面设计，强调功能性和视觉舒适度。

## 🎯 设计理念

### 核心设计原则
- **极简主义** - 去除不必要的装饰，专注于内容
- **大量留白** - 给内容充分的呼吸空间
- **虚线边框** - 轻量级的视觉分隔，不会喧宾夺主
- **粗体标题** - 强烈的视觉层次和现代感
- **网格系统** - 整齐有序的内容组织
- **克制的色彩** - 低饱和度配色，舒适护眼

### 视觉特点
- **柔和的背景色** - #fafbfc 温和舒适的视觉体验
- **微圆角设计** - 2px 极小圆角，现代而不过分
- **虚线卡片边框** - 轻量级视觉分隔
- **半透明效果** - 营造层次感和深度
- **无外框设计** - 去除深色边框，更加简洁

### 用户体验
- **响应式设计** - 适配不同屏幕尺寸
- **微妙的动画效果** - 提升交互体验但不干扰
- **清晰的视觉层次** - 便于信息获取
- **一致的交互模式** - 降低学习成本

## 📁 文件结构

```
static/css/
└── unified-style.css    # 统一样式文件
```

## 🏗️ 基础架构

### 1. 页面容器
```html
<div class="app-container">
    <!-- 页面内容 -->
</div>
```

### 2. 顶部导航
```html
<nav class="top-navigation">
    <div class="nav-tabs">
        <a href="/" class="nav-tab">🎥 视频流</a>
        <a href="/recording" class="nav-tab active">🎬 录制</a>
        <!-- 更多导航项 -->
    </div>
</nav>
```

### 3. 页面标题
```html
<header class="page-header">
    <h1 class="page-title">页面标题</h1>
    <p class="page-subtitle">页面副标题</p>
</header>
```

## 🃏 卡片系统

### 基础卡片
```html
<div class="card">
    <div class="card-header">
        <h3 class="card-title">📹 卡片标题</h3>
        <div class="card-actions">
            <button class="btn btn-outline btn-small">操作</button>
        </div>
    </div>
    <!-- 卡片内容 -->
</div>
```

### 控制面板卡片
```html
<div class="card">
    <div class="control-panel">
        <div class="control-section">
            <div class="control-row">
                <!-- 控制元素 -->
            </div>
        </div>
    </div>
</div>
```

## 📐 网格布局

### 两列布局
```html
<div class="grid grid-2">
    <div>左侧内容</div>
    <div>右侧内容</div>
</div>
```

### 三列布局
```html
<div class="grid grid-3">
    <div>列1</div>
    <div>列2</div>
    <div>列3</div>
</div>
```

### 四列布局
```html
<div class="grid grid-4">
    <div>列1</div>
    <div>列2</div>
    <div>列3</div>
    <div>列4</div>
</div>
```

## 🎛️ 表单控件

### 表单组
```html
<div class="form-group">
    <label class="form-label">标签</label>
    <input type="text" class="form-input" placeholder="输入内容">
</div>

<div class="form-group">
    <label class="form-label">选择</label>
    <select class="form-select">
        <option>选项1</option>
        <option>选项2</option>
    </select>
</div>
```

### 控制行
```html
<div class="control-row">
    <div class="form-group">
        <!-- 表单控件 -->
    </div>
    <button class="btn btn-primary">提交</button>
</div>
```

## 🔘 按钮系统

### 按钮类型
```html
<!-- 主要按钮 -->
<button class="btn btn-primary">主要操作</button>

<!-- 成功按钮 -->
<button class="btn btn-success">成功操作</button>

<!-- 危险按钮 -->
<button class="btn btn-danger">危险操作</button>

<!-- 警告按钮 -->
<button class="btn btn-warning">警告操作</button>

<!-- 次要按钮 -->
<button class="btn btn-secondary">次要操作</button>

<!-- 轮廓按钮 -->
<button class="btn btn-outline">轮廓按钮</button>
```

### 按钮尺寸
```html
<button class="btn btn-primary btn-small">小按钮</button>
<button class="btn btn-primary">标准按钮</button>
<button class="btn btn-primary btn-large">大按钮</button>
```

## 📊 状态显示

### 状态面板
```html
<div class="status-panel">
    <div class="status-item">
        <span class="status-label">状态:</span>
        <span class="status-value success">正常</span>
    </div>
    <div class="status-item">
        <span class="status-label">连接:</span>
        <span class="status-value error">断开</span>
    </div>
</div>
```

### 状态值类型
- `.status-value` - 默认状态 (低饱和度深灰)
- `.status-value.success` - 成功状态 (低饱和度绿色)
- `.status-value.error` - 错误状态 (低饱和度红色)
- `.status-value.warning` - 警告状态 (低饱和度黄色)

## 🎬 视频显示

### 视频容器
```html
<div class="video-container">
    <canvas class="video-canvas" width="640" height="480"></canvas>
    <div class="video-overlay recording-indicator">
        🔴 录制中
    </div>
</div>
```

## 📋 日志系统

### 日志容器
```html
<div class="log-container">
    <div class="log-header">
        <h3 style="color: white; margin: 0;">📋 系统日志</h3>
        <button class="btn btn-warning btn-small">清空日志</button>
    </div>
    <div class="log-content"></div>
</div>
```

## 🛠️ 工具类

### 间距
```html
<div class="mb-0">无下边距</div>
<div class="mb-1">小下边距</div>
<div class="mb-2">中下边距</div>
<div class="mb-3">大下边距</div>
```

### 布局
```html
<div class="flex">弹性布局</div>
<div class="flex flex-column">垂直弹性布局</div>
<div class="text-center">居中文本</div>
<div class="w-full">全宽</div>
```

### 透明度
```html
<div class="opacity-50">50%透明度</div>
<div class="opacity-75">75%透明度</div>
```

## 🎭 动画效果

### 淡入动画
```html
<div class="fade-in">淡入效果</div>
```

### 上滑动画
```html
<div class="slide-up">上滑效果</div>
```

## 📱 响应式设计

### 断点
- **桌面端**: > 1024px - 完整布局
- **平板端**: 768px - 1024px - 简化布局
- **移动端**: < 768px - 单列布局

### 自适应网格
- `grid-2` 在平板端变为单列
- `grid-3` 在平板端变为两列，移动端变为单列
- `grid-4` 在平板端变为两列，移动端变为单列

## 🎨 色彩方案

### 设计哲学
- **低饱和度配色** - 所有颜色都经过去饱和处理，护眼舒适
- **半透明系统** - 使用 rgba() 营造层次感
- **无渐变设计** - 采用纯色，更加简洁现代

### 主要颜色 (低饱和度版本)
- **主色调**: rgba(73, 80, 87, 0.7) - 低饱和度深灰
- **成功色**: rgba(120, 150, 130, 0.4) - 低饱和度绿色
- **危险色**: rgba(180, 120, 120, 0.4) - 低饱和度红色
- **警告色**: rgba(200, 180, 120, 0.4) - 低饱和度黄色
- **次要色**: rgba(108, 117, 125, 0.6) - 低饱和度灰色

### 文本颜色
- **主文本**: rgba(73, 80, 87, 0.8) - 半透明深灰
- **次要文本**: rgba(127, 140, 141, 0.7) - 半透明中灰
- **标签文本**: rgba(73, 80, 87, 0.7) - 半透明深灰
- **页面标题**: rgba(73, 80, 87, 0.7) - 粗体大标题
- **页面副标题**: rgba(108, 117, 125, 0.6) - 柔和副标题

### 背景颜色
- **页面背景**: #fafbfc - 柔和的浅灰白色
- **卡片背景**: rgba(255, 255, 255, 0.8) - 半透明白色
- **导航背景**: rgba(255, 255, 255, 0.7) - 半透明白色
- **状态面板**: rgba(248, 249, 250, 0.7) - 半透明浅灰

### 边框颜色
- **卡片边框**: rgba(233, 236, 239, 0.6) - 虚线边框
- **表单边框**: rgba(233, 236, 239, 0.6) - 输入框边框
- **焦点边框**: rgba(73, 80, 87, 0.5) - 聚焦状态

## ✨ 最新设计改进 (v2.0)

### 🎨 视觉优化
1. **极小圆角设计** - 所有组件统一使用 2px 圆角，更加现代简洁
2. **去除深色外框** - 移除所有组件的深色边框，界面更加轻盈
3. **降低颜色饱和度** - 所有彩色元素都采用低饱和度版本，护眼舒适
4. **半透明系统** - 广泛使用 rgba() 颜色，营造层次感

### 🔄 具体改进
- **卡片系统**: 使用虚线边框 + 半透明背景，去除深色外框
- **按钮系统**: 采用低饱和度配色，取消渐变效果
- **表单控件**: 简化边框设计，聚焦时使用柔和的边框色
- **状态显示**: 使用低饱和度的状态色，更加舒适
- **视频容器**: 去除深色边框，使用半透明黑色背景
- **日志系统**: 采用半透明背景，文字使用低饱和度绿色

### 🎯 设计目标达成
- ✅ **极简主义** - 去除不必要的装饰元素
- ✅ **视觉舒适** - 低饱和度配色减少视觉疲劳
- ✅ **现代感** - 微圆角 + 无外框设计
- ✅ **层次感** - 半透明效果营造深度
- ✅ **一致性** - 统一的设计语言和色彩系统

## 📖 使用示例

查看 `video_recording_new.html` 文件，了解完整的实现示例。

## 🔄 迁移指南

### 从旧样式迁移
1. 引入统一CSS文件：`<link rel="stylesheet" href="/static/css/unified-style.css">`
2. 替换页面结构为新的卡片布局
3. 更新按钮和表单控件类名
4. 调整网格布局结构
5. 测试响应式效果

### 注意事项
- 保持HTML语义化结构
- 使用提供的工具类而非自定义样式
- 遵循设计系统的色彩和间距规范
- 确保在不同设备上测试效果
