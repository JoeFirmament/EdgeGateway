# 🌐 API设计指南

## 📋 概述

本文档详细介绍了RK3588双摄像头推流系统的RESTful API设计。采用**一个功能一个API**的设计理念，前后端完全分离，通过HTTP API进行通信。

## 🎯 设计理念

### 核心原则
- 🎯 **一功能一API** - 每个功能对应一个独立的API端点
- 🌐 **RESTful设计** - 遵循REST架构风格
- 📱 **前后端分离** - HTML页面通过API与后端通信
- 🔄 **状态无关** - API调用之间相互独立
- 📊 **JSON通信** - 统一使用JSON格式数据交换

### 架构优势
- ✅ **模块化** - 每个功能独立开发和测试
- ✅ **可扩展** - 轻松添加新功能和API
- ✅ **易维护** - 前后端独立维护和升级
- ✅ **多客户端** - 支持Web、移动端、第三方集成

## 🛠️ API端点设计

### 📷 摄像头管理API

#### **获取摄像头列表**
```http
GET /api/cameras
```
**响应示例:**
```json
{
    "success": true,
    "message": "获取摄像头列表成功",
    "data": {
        "cameras": [
            {
                "device": "/dev/video0",
                "name": "DECXIN Camera",
                "status": "available",
                "resolutions": ["640x480", "1280x720", "1920x1080"],
                "formats": ["MJPEG", "YUYV"]
            },
            {
                "device": "/dev/video2", 
                "name": "USB Camera",
                "status": "available",
                "resolutions": ["640x480", "1280x720"],
                "formats": ["MJPEG"]
            }
        ]
    }
}
```

#### **获取摄像头信息**
```http
GET /api/camera/{device}/info
```
**URL示例:** `/api/camera/video0/info`

**响应示例:**
```json
{
    "success": true,
    "data": {
        "device": "/dev/video0",
        "name": "DECXIN Camera",
        "driver": "uvcvideo",
        "capabilities": ["video_capture", "streaming"],
        "current_format": {
            "width": 640,
            "height": 480,
            "pixelformat": "MJPEG",
            "fps": 30
        },
        "supported_formats": [
            {"width": 640, "height": 480, "fps": [15, 30]},
            {"width": 1280, "height": 720, "fps": [15, 30]},
            {"width": 1920, "height": 1080, "fps": [15, 30]}
        ]
    }
}
```

#### **打开摄像头**
```http
POST /api/camera/{device}/open
Content-Type: application/json

{
    "width": 1280,
    "height": 720,
    "fps": 30,
    "format": "MJPEG"
}
```

#### **关闭摄像头**
```http
POST /api/camera/{device}/close
```

#### **获取摄像头状态**
```http
GET /api/camera/{device}/status
```

### 🎬 视频推流API

#### **开始推流**
```http
POST /api/streaming/{device}/start
Content-Type: application/json

{
    "width": 640,
    "height": 480,
    "fps": 30,
    "quality": 80,
    "bitrate": 2000
}
```

#### **停止推流**
```http
POST /api/streaming/{device}/stop
```

#### **获取推流状态**
```http
GET /api/streaming/{device}/status
```
**响应示例:**
```json
{
    "success": true,
    "data": {
        "device": "/dev/video0",
        "streaming": true,
        "config": {
            "width": 640,
            "height": 480,
            "fps": 30,
            "quality": 80
        },
        "stats": {
            "frame_count": 1234,
            "current_fps": 29.8,
            "bitrate_kbps": 1950,
            "duration_seconds": 41.2
        }
    }
}
```

#### **获取所有推流状态**
```http
GET /api/streaming/list
```

### 📹 录制功能API

#### **开始录制**
```http
POST /api/recording/{device}/start
Content-Type: application/json

{
    "filename": "recording_20241201_143022.mp4",
    "width": 1920,
    "height": 1080,
    "fps": 30,
    "quality": 95,
    "codec": "h264"
}
```

#### **停止录制**
```http
POST /api/recording/{device}/stop
```

#### **获取录制状态**
```http
GET /api/recording/{device}/status
```

#### **获取录制文件列表**
```http
GET /api/recording/list
```
**响应示例:**
```json
{
    "success": true,
    "data": {
        "recordings": [
            {
                "filename": "recording_20241201_143022.mp4",
                "device": "/dev/video0",
                "size_mb": 125.6,
                "duration_seconds": 180.5,
                "created_at": "2024-12-01T14:30:22Z",
                "resolution": "1920x1080",
                "fps": 30
            }
        ],
        "total_files": 1,
        "total_size_mb": 125.6
    }
}
```

#### **删除录制文件**
```http
DELETE /api/recording/file/{filename}
```

### 📸 拍照功能API

#### **拍照**
```http
POST /api/snapshot/{device}/take
Content-Type: application/json

{
    "filename": "snapshot_20241201_143022.jpg",
    "width": 2560,
    "height": 1440,
    "quality": 95
}
```

#### **获取照片列表**
```http
GET /api/snapshot/list
```

#### **删除照片**
```http
DELETE /api/snapshot/file/{filename}
```

### 🤖 AI处理API

#### **启动AI处理**
```http
POST /api/ai/{device}/start
Content-Type: application/json

{
    "type": "yolo",
    "model": "yolov8n",
    "confidence": 0.5,
    "nms": 0.4,
    "classes": ["person", "car", "bicycle"]
}
```

#### **停止AI处理**
```http
POST /api/ai/{device}/stop
```

#### **获取AI处理状态**
```http
GET /api/ai/{device}/status
```

#### **获取AI检测结果**
```http
GET /api/ai/{device}/results
```
**响应示例:**
```json
{
    "success": true,
    "data": {
        "device": "/dev/video0",
        "ai_type": "yolo",
        "processing": true,
        "latest_results": {
            "timestamp": "2024-12-01T14:30:22Z",
            "detections": [
                {
                    "class_id": 0,
                    "class_name": "person",
                    "confidence": 0.85,
                    "bbox": {"x": 100, "y": 50, "width": 200, "height": 300}
                }
            ],
            "detection_count": 1,
            "processing_time_ms": 25.6
        },
        "stats": {
            "total_frames": 1234,
            "avg_processing_time_ms": 28.3,
            "avg_detections_per_frame": 1.2
        }
    }
}
```

### 📊 系统监控API

#### **获取系统状态**
```http
GET /api/system/status
```
**响应示例:**
```json
{
    "success": true,
    "data": {
        "system": {
            "uptime_seconds": 3600,
            "cpu_usage_percent": 25.6,
            "memory_usage_mb": 512,
            "memory_total_mb": 7680,
            "temperature_celsius": 45.2
        },
        "cameras": {
            "total": 2,
            "active": 1,
            "streaming": 1,
            "recording": 0
        },
        "hardware": {
            "mpp_available": true,
            "rga_available": true,
            "npu_available": true,
            "gpu_available": true
        }
    }
}
```

#### **获取硬件信息**
```http
GET /api/system/hardware
```

#### **获取性能统计**
```http
GET /api/system/performance
```

### ⚙️ 配置管理API

#### **获取系统配置**
```http
GET /api/config
```

#### **更新系统配置**
```http
POST /api/config
Content-Type: application/json

{
    "streaming": {
        "default_quality": 80,
        "max_fps": 30
    },
    "recording": {
        "default_codec": "h264",
        "storage_path": "/var/recordings"
    },
    "ai": {
        "default_confidence": 0.5,
        "use_npu": true
    }
}
```

#### **重置配置**
```http
POST /api/config/reset
```

### 📁 文件服务API

#### **下载文件**
```http
GET /api/files/download/{filename}
```

#### **获取文件信息**
```http
GET /api/files/info/{filename}
```

## 🌐 WebSocket API

### 视频流WebSocket
```javascript
// 连接视频流
const ws = new WebSocket('ws://192.168.124.12:8080/ws/video/video0');

ws.onmessage = function(event) {
    if (event.data instanceof ArrayBuffer) {
        // 视频帧数据
        displayVideoFrame(event.data);
    } else {
        // 状态消息
        const message = JSON.parse(event.data);
        console.log('状态:', message);
    }
};
```

### AI结果WebSocket
```javascript
// 连接AI结果流
const aiWs = new WebSocket('ws://192.168.124.12:8080/ws/ai/video0');

aiWs.onmessage = function(event) {
    const results = JSON.parse(event.data);
    displayAIResults(results.detections);
};
```

## 📱 HTML页面设计

### 页面-API映射关系

#### **摄像头管理页面** (`camera_manager.html`)
```html
<!-- URL: /camera_manager.html -->
<script>
// 获取摄像头列表
fetch('/api/cameras')
    .then(response => response.json())
    .then(data => displayCameras(data.data.cameras));

// 打开摄像头
function openCamera(device) {
    fetch(`/api/camera/${device}/open`, {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({width: 1280, height: 720, fps: 30})
    });
}
</script>
```

#### **视频推流页面** (`streaming.html`)
```html
<!-- URL: /streaming.html?device=video0 -->
<script>
const device = new URLSearchParams(window.location.search).get('device');

// 开始推流
function startStreaming() {
    fetch(`/api/streaming/${device}/start`, {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
            width: 640,
            height: 480,
            fps: 30,
            quality: 80
        })
    });
}

// 连接视频流WebSocket
const ws = new WebSocket(`ws://localhost:8080/ws/video/${device}`);
</script>
```

#### **录制管理页面** (`recording.html`)
```html
<!-- URL: /recording.html?device=video0 -->
<script>
// 开始录制
function startRecording() {
    const filename = `recording_${new Date().toISOString()}.mp4`;
    fetch(`/api/recording/${device}/start`, {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
            filename: filename,
            width: 1920,
            height: 1080,
            fps: 30
        })
    });
}

// 获取录制列表
fetch('/api/recording/list')
    .then(response => response.json())
    .then(data => displayRecordings(data.data.recordings));
</script>
```

#### **AI处理页面** (`ai_processing.html`)
```html
<!-- URL: /ai_processing.html?device=video0&type=yolo -->
<script>
const aiType = new URLSearchParams(window.location.search).get('type');

// 启动AI处理
function startAI() {
    fetch(`/api/ai/${device}/start`, {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
            type: aiType,
            confidence: 0.5,
            nms: 0.4
        })
    });
}

// 连接AI结果WebSocket
const aiWs = new WebSocket(`ws://localhost:8080/ws/ai/${device}`);
</script>
```

#### **系统监控页面** (`system_monitor.html`)
```html
<!-- URL: /system_monitor.html -->
<script>
// 定期获取系统状态
setInterval(() => {
    fetch('/api/system/status')
        .then(response => response.json())
        .then(data => updateSystemStatus(data.data));
}, 5000);

// 获取硬件信息
fetch('/api/system/hardware')
    .then(response => response.json())
    .then(data => displayHardwareInfo(data.data));
</script>
```

## 🚀 使用示例

### 完整的推流启动流程
```javascript
// 1. 获取摄像头列表
const cameras = await fetch('/api/cameras').then(r => r.json());

// 2. 打开摄像头
await fetch('/api/camera/video0/open', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({width: 1280, height: 720, fps: 30})
});

// 3. 开始推流
await fetch('/api/streaming/video0/start', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({width: 640, height: 480, fps: 30, quality: 80})
});

// 4. 连接视频流
const ws = new WebSocket('ws://localhost:8080/ws/video/video0');
ws.onmessage = (event) => {
    if (event.data instanceof ArrayBuffer) {
        displayVideoFrame(event.data);
    }
};
```

### 录制和AI处理组合
```javascript
// 同时启动录制和AI处理
Promise.all([
    // 开始录制
    fetch('/api/recording/video0/start', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
            filename: 'recording.mp4',
            width: 1920,
            height: 1080,
            fps: 30
        })
    }),
    
    // 启动AI处理
    fetch('/api/ai/video0/start', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
            type: 'yolo',
            confidence: 0.5
        })
    })
]);
```

## 📊 API响应格式

### 统一响应格式
```json
{
    "success": true|false,
    "message": "描述信息",
    "data": {
        // 具体数据
    },
    "error_code": 0,
    "timestamp": "2024-12-01T14:30:22Z"
}
```

### 错误响应示例
```json
{
    "success": false,
    "message": "摄像头设备不存在",
    "error_code": 404,
    "timestamp": "2024-12-01T14:30:22Z"
}
```

## 🎯 优势总结

### 开发优势
- 🎯 **清晰分工** - 前端专注UI，后端专注逻辑
- 🔄 **独立开发** - 前后端可以并行开发
- 🧪 **易于测试** - API可以独立测试
- 📚 **文档清晰** - API文档即开发规范

### 使用优势
- 📱 **多端支持** - Web、移动端、第三方集成
- 🔧 **灵活组合** - 可以灵活组合不同功能
- ⚡ **高性能** - 前后端分离，性能更好
- 🛠️ **易维护** - 模块化设计，易于维护和扩展

这个API设计为RK3588双摄像头系统提供了完整、灵活、易用的接口，是现代Web应用的最佳实践！ 🎉
