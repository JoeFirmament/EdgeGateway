#include "api/camera_api.h"
#include "camera/camera_manager.h"
#include "monitor/logger.h"
#include "utils/string_utils.h"

#include <iostream>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <cstring>
#include <algorithm>
#include <sstream>

namespace cam_server {
namespace api {

// 单例实现
CameraApi& CameraApi::getInstance() {
    static CameraApi instance;
    return instance;
}

// 构造函数
CameraApi::CameraApi() : is_initialized_(false) {
    // 初始化格式名称映射
    format_names_[V4L2_PIX_FMT_MJPEG] = "MJPG";
    format_names_[V4L2_PIX_FMT_YUYV] = "YUYV";
    format_names_[V4L2_PIX_FMT_RGB24] = "RGB24";
    format_names_[V4L2_PIX_FMT_BGR24] = "BGR24";
    format_names_[V4L2_PIX_FMT_YUV420] = "YUV420";
    format_names_[V4L2_PIX_FMT_NV12] = "NV12";
    format_names_[V4L2_PIX_FMT_NV21] = "NV21";
    format_names_[V4L2_PIX_FMT_H264] = "H264";
    format_names_[V4L2_PIX_FMT_MPEG4] = "MPEG4";
    format_names_[V4L2_PIX_FMT_JPEG] = "JPEG";
}

// 析构函数
CameraApi::~CameraApi() {
}

// 初始化API
bool CameraApi::initialize() {
    if (is_initialized_) {
        return true;
    }
    
    is_initialized_ = true;
    return true;
}

// 注册API路由
bool CameraApi::registerRoutes(RestHandler& handler) {
    // 注册获取所有摄像头的路由
    handler.registerRoute("GET", "/api/cameras", 
        [this](const HttpRequest& request) {
            return this->handleGetAllCameras(request);
        }
    );
    
    // 注册打开摄像头的路由
    handler.registerRoute("POST", "/api/cameras/open", 
        [this](const HttpRequest& request) {
            return this->handleOpenCamera(request);
        }
    );
    
    return true;
}

// 获取所有摄像头设备
std::vector<CameraDeviceInfo> CameraApi::getAllCameras() {
    std::vector<CameraDeviceInfo> devices;
    
    for (const auto& entry : std::filesystem::directory_iterator("/dev")) {
        std::string path = entry.path().string();
        if (path.find("/dev/video") == 0) {
            // 尝试提取数字部分
            std::string num_part = path.substr(10); // 跳过"/dev/video"
            if (!num_part.empty() && std::all_of(num_part.begin(), num_part.end(), ::isdigit)) {
                CameraDeviceInfo info;
                if (queryDevice(path, info) && !info.formats.empty()) {
                    devices.push_back(info);
                }
            }
        }
    }
    
    return devices;
}

// 打开摄像头
bool CameraApi::openCamera(const std::string& device_path, 
                         const std::string& format,
                         int width, int height, int fps) {
    try {
        // 获取摄像头管理器实例
        auto& camera_manager = camera::CameraManager::getInstance();
        
        // 关闭当前打开的摄像头
        camera_manager.closeDevice();
        
        // 打开新的摄像头
        if (!camera_manager.openDevice(device_path, width, height, fps)) {
            LOG_ERROR("无法打开摄像头: " + device_path, "CameraApi");
            return false;
        }
        
        LOG_INFO("成功打开摄像头: " + device_path + ", 分辨率: " + 
                std::to_string(width) + "x" + std::to_string(height) + 
                ", 格式: " + format + ", 帧率: " + std::to_string(fps), 
                "CameraApi");
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("打开摄像头异常: " + std::string(e.what()), "CameraApi");
        return false;
    }
}

// 处理获取所有摄像头的请求
HttpResponse CameraApi::handleGetAllCameras(const HttpRequest& request) {
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "application/json";
    
    try {
        auto cameras = getAllCameras();
        
        // 构建JSON响应
        std::ostringstream json;
        json << "{\"cameras\":[";
        
        for (size_t i = 0; i < cameras.size(); i++) {
            const auto& camera = cameras[i];
            
            json << "{";
            json << "\"path\":\"" << camera.path << "\",";
            json << "\"name\":\"" << camera.name << "\",";
            json << "\"bus_info\":\"" << camera.bus_info << "\",";
            json << "\"formats\":{";
            
            bool first_format = true;
            for (const auto& format : camera.formats) {
                if (!first_format) {
                    json << ",";
                }
                
                json << "\"" << format.first << "\":[";
                
                bool first_res = true;
                for (const auto& res : format.second) {
                    if (!first_res) {
                        json << ",";
                    }
                    
                    json << "{\"width\":" << res.width << ",\"height\":" << res.height << "}";
                    first_res = false;
                }
                
                json << "]";
                first_format = false;
            }
            
            json << "}";
            json << "}";
            
            if (i < cameras.size() - 1) {
                json << ",";
            }
        }
        
        json << "]}";
        
        response.body = json.str();
    } catch (const std::exception& e) {
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = "{\"error\":\"" + std::string(e.what()) + "\"}";
    }
    
    return response;
}

// 处理打开摄像头的请求
HttpResponse CameraApi::handleOpenCamera(const HttpRequest& request) {
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "application/json";
    
    try {
        // 解析请求体
        std::string device_path;
        std::string format;
        int width = 0;
        int height = 0;
        int fps = 30;
        
        // 简单的JSON解析
        std::string body = request.body;
        
        auto extract_value = [&body](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\":";
            size_t pos = body.find(search);
            if (pos == std::string::npos) {
                return "";
            }
            
            pos += search.length();
            
            // 检查是否是字符串值
            bool is_string = false;
            if (pos < body.length() && body[pos] == '"') {
                is_string = true;
                pos++;
            }
            
            size_t end_pos;
            if (is_string) {
                end_pos = body.find("\"", pos);
            } else {
                end_pos = body.find_first_of(",}", pos);
            }
            
            if (end_pos == std::string::npos) {
                return "";
            }
            
            return body.substr(pos, end_pos - pos);
        };
        
        device_path = extract_value("device_path");
        format = extract_value("format");
        
        std::string width_str = extract_value("width");
        std::string height_str = extract_value("height");
        std::string fps_str = extract_value("fps");
        
        if (!width_str.empty()) {
            width = std::stoi(width_str);
        }
        
        if (!height_str.empty()) {
            height = std::stoi(height_str);
        }
        
        if (!fps_str.empty()) {
            fps = std::stoi(fps_str);
        }
        
        // 验证参数
        if (device_path.empty() || format.empty() || width <= 0 || height <= 0 || fps <= 0) {
            response.status_code = 400;
            response.body = "{\"error\":\"缺少必要参数或参数无效\"}";
            return response;
        }
        
        // 打开摄像头
        if (openCamera(device_path, format, width, height, fps)) {
            response.body = "{\"success\":true,\"message\":\"摄像头已成功打开\"}";
        } else {
            response.status_code = 500;
            response.body = "{\"success\":false,\"error\":\"无法打开摄像头\"}";
        }
    } catch (const std::exception& e) {
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = "{\"error\":\"" + std::string(e.what()) + "\"}";
    }
    
    return response;
}

// 查询设备信息
bool CameraApi::queryDevice(const std::string& device_path, CameraDeviceInfo& info) {
    int fd = open(device_path.c_str(), O_RDWR);
    if (fd < 0) {
        LOG_ERROR("无法打开设备: " + device_path, "CameraApi");
        return false;
    }
    
    // 获取设备信息
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        LOG_ERROR("无法查询设备能力: " + device_path, "CameraApi");
        close(fd);
        return false;
    }
    
    // 检查是否是视频捕获设备
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        LOG_ERROR("不是视频捕获设备: " + device_path, "CameraApi");
        close(fd);
        return false;
    }
    
    info.path = device_path;
    info.name = reinterpret_cast<const char*>(cap.card);
    info.bus_info = reinterpret_cast<const char*>(cap.bus_info);
    
    // 查询支持的格式
    struct v4l2_fmtdesc fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0) {
        std::string format_name = getFormatName(fmt.pixelformat);
        
        // 查询该格式支持的分辨率
        struct v4l2_frmsizeenum frmsize;
        memset(&frmsize, 0, sizeof(frmsize));
        frmsize.pixel_format = fmt.pixelformat;
        frmsize.index = 0;
        
        if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                // 离散分辨率
                do {
                    ResolutionInfo res = {frmsize.discrete.width, frmsize.discrete.height};
                    info.formats[format_name].insert(res);
                    frmsize.index++;
                } while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0);
            } else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE || 
                      frmsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
                // 连续或步进分辨率，我们选择一些常用的分辨率
                std::vector<std::pair<uint32_t, uint32_t>> common_resolutions = {
                    {640, 480}, {800, 600}, {1024, 768}, {1280, 720}, 
                    {1280, 960}, {1600, 1200}, {1920, 1080}, {2560, 1440}, 
                    {3840, 2160}
                };
                
                for (const auto& res : common_resolutions) {
                    if (res.first >= frmsize.stepwise.min_width && 
                        res.first <= frmsize.stepwise.max_width &&
                        res.second >= frmsize.stepwise.min_height && 
                        res.second <= frmsize.stepwise.max_height) {
                        ResolutionInfo r = {res.first, res.second};
                        info.formats[format_name].insert(r);
                    }
                }
            }
        }
        
        fmt.index++;
    }
    
    close(fd);
    return true;
}

// 获取格式名称
std::string CameraApi::getFormatName(uint32_t format) {
    char fmt_str[5] = {0};
    fmt_str[0] = format & 0xFF;
    fmt_str[1] = (format >> 8) & 0xFF;
    fmt_str[2] = (format >> 16) & 0xFF;
    fmt_str[3] = (format >> 24) & 0xFF;
    
    auto it = format_names_.find(format);
    if (it != format_names_.end()) {
        return it->second;
    }
    return std::string(fmt_str);
}

} // namespace api
} // namespace cam_server
