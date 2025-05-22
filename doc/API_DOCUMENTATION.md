# 摄像头服务器 API 文档

本文档详细描述了摄像头服务器提供的所有API接口。

## 基础信息

- **基础URL**: `/api`
- **默认端口**: 8080 (HTTP) / 8443 (HTTPS)
- **内容类型**: `application/json`

## 1. 摄像头控制 API

### 1.1 获取摄像头状态

- **URL**: `/camera/status`
- **方法**: `GET`
- **描述**: 获取当前摄像头的状态信息
- **参数**: 无
- **响应**:
  ```json
  {
    "status": "success",
    "is_open": true,
    "is_capturing": true,
    "device_info": {
      "device_path": "/dev/video0",
      "name": "USB Camera",
      "bus_info": "usb-0000:01:00.0-1.2",
      "formats": ["MJPG", "YUYV"],
      "resolutions": ["1920x1080", "1280x720"],
      "fps": [30, 60]
    }
  }
  ```

### 1.2 获取所有摄像头列表

- **URL**: `/camera/list`
- **方法**: `GET`
- **描述**: 获取系统中所有可用的摄像头设备列表
- **参数**: 无
- **响应**:
  ```json
  {
    "status": "success",
    "cameras": [
      {
        "device_path": "/dev/video0",
        "name": "USB Camera",
        "bus_info": "usb-0000:01:00.0-1.2",
        "formats": ["MJPG", "YUYV"],
        "resolutions": ["1920x1080", "1280x720"],
        "fps": [30, 60]
      }
    ]
  }
  ```

### 1.3 打开摄像头

- **URL**: `/camera/open`
- **方法**: `POST`
- **描述**: 打开指定参数的摄像头
- **请求体**:
  ```json
  {
    "device_path": "/dev/video0",
    "format": "MJPG",
    "width": 1280,
    "height": 720,
    "fps": 30
  }
  ```
- **参数说明**:
  - `device_path`: 摄像头设备路径
  - `format`: 视频格式 (如: MJPG, YUYV)
  - `width`: 视频宽度
  - `height`: 视频高度
  - `fps`: 帧率
- **响应**:
  ```json
  {
    "status": "success",
    "message": "Camera opened successfully"
  }
  ```

### 1.4 关闭摄像头

- **URL**: `/camera/close`
- **方法**: `POST`
- **描述**: 关闭当前打开的摄像头
- **参数**: 无
- **响应**:
  ```json
  {
    "status": "success",
    "message": "Camera closed successfully"
  }
  ```

### 1.5 开始预览

- **URL**: `/camera/start_preview`
- **方法**: `POST`
- **描述**: 开始摄像头视频流预览
- **参数**: 无
- **响应**:
  ```json
  {
    "status": "success",
    "message": "Preview started",
    "stream_url": "/api/camera/mjpeg"
  }
  ```

### 1.6 停止预览

- **URL**: `/camera/stop_preview`
- **方法**: `POST`
- **描述**: 停止摄像头视频流预览
- **参数**: 无
- **响应**:
  ```json
  {
    "status": "success",
    "message": "Preview stopped"
  }
  ```

### 1.7 拍照

- **URL**: `/camera/capture`
- **方法**: `POST`
- **描述**: 拍摄一张照片并保存
- **请求体** (可选):
  ```json
  {
    "output_path": "/path/to/save/image.jpg",
    "quality": 90
  }
  ```
- **参数说明**:
  - `output_path`: 保存路径 (可选，默认为配置的保存目录)
  - `quality`: 图片质量 (1-100, 仅对JPEG有效)
- **响应**:
  ```json
  {
    "status": "success",
    "message": "Image captured successfully",
    "file_path": "/path/to/saved/image.jpg"
  }
  ```

### 1.8 开始录制视频

- **URL**: `/camera/start_recording`
- **方法**: `POST`
- **描述**: 开始录制视频
- **请求体**:
  ```json
  {
    "output_path": "/path/to/save/video.mp4",
    "format": "mp4",
    "encoder": "h264",
    "bitrate": 4000,
    "max_duration": 300
  }
  ```
- **参数说明**:
  - `output_path`: 保存路径 (可选)
  - `format`: 视频格式 (mp4, avi等)
  - `encoder`: 编码器 (h264, mjpeg等)
  - `bitrate`: 视频比特率 (kbps)
  - `max_duration`: 最大录制时长 (秒)
- **响应**:
  ```json
  {
    "status": "success",
    "message": "Recording started",
    "recording_id": "rec_123456"
  }
  ```

### 1.9 停止录制视频

- **URL**: `/camera/stop_recording`
- **方法**: `POST`
- **描述**: 停止当前正在录制的视频
- **参数**: 无
- **响应**:
  ```json
  {
    "status": "success",
    "message": "Recording stopped",
    "file_path": "/path/to/saved/video.mp4"
  }
  ```

### 1.10 MJPEG 视频流

- **URL**: `/camera/mjpeg`
- **方法**: `GET`
- **描述**: 获取MJPEG视频流
- **参数**:
  - `camera_id` (可选): 指定要使用的摄像头设备路径 (如: `/dev/video0`)
  - `client_id` (可选): 客户端标识符，用于多客户端管理
- **响应**: MJPEG视频流 (multipart/x-mixed-replace)

#### MJPEG 流开启的必要条件

1. **设备准备**
   - 摄像头设备必须存在且可访问
   - 设备需要支持 MJPEG 格式（通过 `format` 参数指定）

2. **自动处理逻辑**
   - 如果摄像头未打开，系统会自动尝试打开指定设备
   - 如果指定了 `camera_id` 参数，会尝试切换到该摄像头
   - 如果未指定 `camera_id`，会使用当前已打开的摄像头

3. **自动参数设置**
   - 默认使用 MJPEG 格式
   - 默认分辨率：1280x720
   - 默认帧率：30fps
   - 默认 JPEG 质量：80

#### 使用示例

```html
<!-- 在网页中显示 MJPEG 流 -->
<img src="http://your-server-ip:8080/api/camera/mjpeg">

<!-- 指定摄像头 -->
<img src="http://your-server-ip:8080/api/camera/mjpeg?camera_id=/dev/video0">
```

#### 注意事项
- 高分辨率/高帧率会占用更多带宽和 CPU 资源
- 多客户端连接时会共享同一个视频流
- 如果摄像头打开失败，会返回 500 错误
- 如果未指定摄像头且没有已打开的摄像头，会返回 400 错误

## 2. 系统信息 API

### 2.1 获取系统信息

- **URL**: `/system/info`
- **方法**: `GET`
- **描述**: 获取系统硬件和软件信息
- **参数**: 无
- **响应**:
  ```json
  {
    "status": "success",
    "system": {
      "hostname": "raspberrypi",
      "os": "Raspbian GNU/Linux 10 (buster)",
      "kernel": "5.10.63-v7l+",
      "uptime": "3 days, 4:30",
      "system_time": "2023-04-15T12:34:56Z"
    },
    "cpu": {
      "usage_percent": 23.5,
      "temperature": 45.2,
      "frequency": 1500,
      "core_count": 4
    },
    "gpu": {
      "usage_percent": 10.2,
      "temperature": 50.1,
      "memory_usage_percent": 30.5,
      "frequency": 400
    },
    "memory": {
      "total": 3965488,
      "used": 1234567,
      "free": 2730921,
      "usage_percent": 31.2
    },
    "storage": [
      {
        "mount_point": "/",
        "total": 31138512896,
        "used": 4123456789,
        "free": 27015056107,
        "usage_percent": 13.2
      }
    ],
    "network": [
      {
        "interface": "eth0",
        "ip_address": "192.168.1.100",
        "rx_bytes": 1234567890,
        "tx_bytes": 987654321,
        "rx_rate": 1024,
        "tx_rate": 512
      }
    ]
  }
  ```

## 3. 系统控制 API

### 3.1 重启服务

- **URL**: `/system/restart-service`
- **方法**: `POST`
- **描述**: 重启摄像头服务
- **参数**: 无
- **响应**:
  ```json
  {
    "status": "success",
    "message": "服务正在重启"
  }
  ```

### 3.2 重启系统

- **URL**: `/system/restart`
- **方法**: `POST`
- **描述**: 重启系统
- **参数**: 无
- **响应**:
  ```json
  {
    "status": "success",
    "message": "系统正在重启"
  }
  ```

### 3.3 关闭系统

- **URL**: `/system/shutdown`
- **方法**: `POST`
- **描述**: 关闭系统
- **参数**: 无
- **响应**:
  ```json
  {
    "status": "success",
    "message": "系统正在关闭"
  }
  ```

## 4. 错误处理

所有API在出错时会返回如下格式的错误信息：

```json
{
  "status": "error",
  "message": "错误描述信息",
  "code": "ERROR_CODE"
}
```

### 常见错误码

| 错误码 | 描述 |
|--------|------|
| 400 | 请求参数错误 |
| 401 | 未授权访问 |
| 403 | 禁止访问 |
| 404 | 资源未找到 |
| 500 | 服务器内部错误 |
| 503 | 服务不可用 |

## 5. 认证

部分API可能需要API密钥进行认证。需要在请求头中添加：

```
X-API-Key: your_api_key_here
```

## 6. CORS 支持

API 支持跨域资源共享 (CORS)，可以通过配置允许特定的源访问API。

## 7. 示例代码

### 使用 cURL 调用API

```bash
# 获取摄像头状态
curl -X GET http://localhost:8080/api/camera/status

# 拍照
curl -X POST http://localhost:8080/api/camera/capture

# 开始录制视频
curl -X POST -H "Content-Type: application/json" -d '{"format":"mp4","bitrate":4000}' http://localhost:8080/api/camera/start_recording

# 获取系统信息
curl -X GET http://localhost:8080/api/system/info
```

### 使用 Python 调用API

```python
import requests
import json

# 基础URL
BASE_URL = "http://localhost:8080/api"

# 获取摄像头状态
response = requests.get(f"{BASE_URL}/camera/status")
print("Camera Status:", response.json())

# 拍照
response = requests.post(f"{BASE_URL}/camera/capture")
print("Capture Image:", response.json())

# 开始录制
payload = {
    "format": "mp4",
    "bitrate": 4000
}
response = requests.post(f"{BASE_URL}/camera/start_recording", json=payload)
print("Start Recording:", response.json())

# 获取系统信息
response = requests.get(f"{BASE_URL}/system/info")
system_info = response.json()
print("CPU Usage (%):", system_info["cpu"]["usage_percent"])
print("Memory Usage (%):", system_info["memory"]["usage_percent"])
```

## 8. 注意事项

1. 确保摄像头设备已正确连接且具有适当的权限
2. 部分操作（如系统重启、关闭）需要root权限
3. 视频流可能会消耗大量带宽，请确保网络环境良好
4. 建议使用HTTPS进行安全通信，特别是在生产环境中
5. 定期检查系统资源使用情况，避免资源耗尽导致服务不可用
