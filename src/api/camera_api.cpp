#include "api/camera_api.h"
#include "camera/camera_manager.h"
#include "monitor/logger.h"
#include "utils/string_utils.h"
#include "utils/file_utils.h"
#include "video/i_video_recorder.h"
#include "api/mjpeg_streamer.h"

// 简单的视频录制器实现
namespace cam_server {
namespace video {

class SimpleVideoRecorder : public IVideoRecorder {
public:
    SimpleVideoRecorder() : is_initialized_(false), is_recording_(false) {
        status_.state = RecordingState::IDLE;
        status_.current_file = "";
        status_.duration = 0.0;
        status_.frame_count = 0;
        status_.file_size = 0;
        status_.error_message = "";
    }

    ~SimpleVideoRecorder() override {
        stopRecording();
    }

    bool initialize(const RecordingConfig& config) override {
        config_ = config;
        is_initialized_ = true;
        return true;
    }

    bool startRecording() override {
        if (!is_initialized_) {
            return false;
        }

        // 创建输出文件
        output_file_.open(config_.output_path, std::ios::binary);
        if (!output_file_) {
            return false;
        }

        is_recording_ = true;
        status_.state = RecordingState::RECORDING;
        status_.current_file = config_.output_path;
        status_.duration = 0.0;
        status_.frame_count = 0;
        status_.file_size = 0;

        start_time_ = std::chrono::steady_clock::now();

        if (status_callback_) {
            status_callback_(status_);
        }

        return true;
    }

    bool stopRecording() override {
        if (!is_recording_) {
            return true;
        }

        output_file_.close();
        is_recording_ = false;
        status_.state = RecordingState::IDLE;

        if (status_callback_) {
            status_callback_(status_);
        }

        return true;
    }

    bool pauseRecording() override {
        if (!is_recording_) {
            return false;
        }

        status_.state = RecordingState::PAUSED;

        if (status_callback_) {
            status_callback_(status_);
        }

        return true;
    }

    bool resumeRecording() override {
        if (status_.state != RecordingState::PAUSED) {
            return false;
        }

        status_.state = RecordingState::RECORDING;

        if (status_callback_) {
            status_callback_(status_);
        }

        return true;
    }

    bool processFrame(const camera::Frame& frame) override {
        if (!is_recording_ || status_.state != RecordingState::RECORDING) {
            return false;
        }

        // 简单地将帧数据写入文件
        output_file_.write(reinterpret_cast<const char*>(frame.data.data()), frame.data.size());

        // 更新状态
        status_.frame_count++;
        auto now = std::chrono::steady_clock::now();
        status_.duration = std::chrono::duration<double>(now - start_time_).count();
        status_.file_size = output_file_.tellp();

        if (status_callback_) {
            status_callback_(status_);
        }

        return true;
    }

    RecordingStatus getStatus() const override {
        return status_;
    }

    void setStatusCallback(std::function<void(const RecordingStatus&)> callback) override {
        status_callback_ = callback;
    }

    bool setConfig(const RecordingConfig& config) override {
        if (is_recording_) {
            return false;
        }

        config_ = config;
        return true;
    }

    RecordingConfig getConfig() const override {
        return config_;
    }

private:
    bool is_initialized_;
    bool is_recording_;
    RecordingConfig config_;
    RecordingStatus status_;
    std::function<void(const RecordingStatus&)> status_callback_;
    std::ofstream output_file_;
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace video
} // namespace cam_server

#include <iostream>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <chrono>
#include <iomanip>

namespace cam_server {
namespace api {

// 单例实现
CameraApi& CameraApi::getInstance() {
    static CameraApi instance;
    return instance;
}

// 构造函数
CameraApi::CameraApi() : is_initialized_(false), video_recorder_(nullptr) {
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

    // 设置图像和视频保存目录
    images_dir_ = "data/images";
    videos_dir_ = "data/videos";

    // 确保目录存在
    ensureDirectoryExists(images_dir_);
    ensureDirectoryExists(videos_dir_);
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

    // 注册启动摄像头预览的路由
    handler.registerRoute("POST", "/api/cameras/start_preview",
        [this](const HttpRequest& request) {
            return this->handleStartPreview(request);
        }
    );

    // 注册停止摄像头预览的路由
    handler.registerRoute("POST", "/api/cameras/stop_preview",
        [this](const HttpRequest& request) {
            return this->handleStopPreview(request);
        }
    );

    // 注册获取摄像头预览图像的路由
    handler.registerRoute("GET", "/api/cameras/preview",
        [this](const HttpRequest& request) {
            return this->handleGetPreview(request);
        }
    );

    // 注册拍照的路由
    handler.registerRoute("POST", "/api/cameras/capture",
        [this](const HttpRequest& request) {
            return this->handleCaptureImage(request);
        }
    );

    // 注册开始录制的路由
    handler.registerRoute("POST", "/api/cameras/start_recording",
        [this](const HttpRequest& request) {
            return this->handleStartRecording(request);
        }
    );

    // 注册停止录制的路由
    handler.registerRoute("POST", "/api/cameras/stop_recording",
        [this](const HttpRequest& request) {
            return this->handleStopRecording(request);
        }
    );

    // 注册获取录制状态的路由
    handler.registerRoute("GET", "/api/cameras/recording_status",
        [this](const HttpRequest& request) {
            return this->handleGetRecordingStatus(request);
        }
    );

    // 注册获取摄像头连接状态的路由
    handler.registerRoute("GET", "/api/cameras/status",
        [this](const HttpRequest& request) {
            return this->handleGetCameraStatus(request);
        }
    );

    // 注册关闭摄像头的路由
    handler.registerRoute("POST", "/api/cameras/close",
        [this](const HttpRequest& request) {
            return this->handleCloseCamera(request);
        }
    );

    std::cerr << "摄像头API路由注册成功" << std::endl;
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

// 启动摄像头预览
bool CameraApi::startPreview() {
    try {
        // 获取摄像头管理器实例
        auto& camera_manager = camera::CameraManager::getInstance();

        // 检查摄像头是否已打开
        if (!camera_manager.isDeviceOpen()) {
            LOG_ERROR("没有打开的摄像头设备", "CameraApi");
            return false;
        }

        // 开始捕获
        if (!camera_manager.startCapture()) {
            LOG_ERROR("无法开始捕获", "CameraApi");
            return false;
        }

        LOG_INFO("成功启动摄像头预览", "CameraApi");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("启动摄像头预览异常: " + std::string(e.what()), "CameraApi");
        return false;
    }
}

// 停止摄像头预览
bool CameraApi::stopPreview() {
    try {
        // 获取摄像头管理器实例
        auto& camera_manager = camera::CameraManager::getInstance();

        // 检查是否正在捕获
        if (!camera_manager.isCapturing()) {
            LOG_INFO("摄像头未在预览中", "CameraApi");
            return true;
        }

        // 停止捕获
        if (!camera_manager.stopCapture()) {
            LOG_ERROR("无法停止捕获", "CameraApi");
            return false;
        }

        LOG_INFO("成功停止摄像头预览", "CameraApi");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("停止摄像头预览异常: " + std::string(e.what()), "CameraApi");
        return false;
    }
}

// 关闭摄像头
bool CameraApi::closeCamera() {
    try {
        // 获取摄像头管理器实例
        auto& camera_manager = camera::CameraManager::getInstance();

        // 检查是否正在捕获，如果是，先停止捕获
        if (camera_manager.isCapturing()) {
            if (!camera_manager.stopCapture()) {
                LOG_ERROR("无法停止捕获", "CameraApi");
                return false;
            }
            LOG_INFO("成功停止摄像头预览", "CameraApi");
        }

        // 关闭设备
        if (!camera_manager.closeDevice()) {
            LOG_ERROR("无法关闭摄像头设备", "CameraApi");
            return false;
        }

        // 清理MJPEG流处理器中的客户端连接
        auto& mjpeg_streamer = api::MjpegStreamer::getInstance();
        // 这里我们无法直接清理特定摄像头的客户端，但可以停止MJPEG流处理器
        // 在实际应用中，应该添加一个方法来清理特定摄像头的客户端

        LOG_INFO("成功关闭摄像头设备", "CameraApi");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("关闭摄像头异常: " + std::string(e.what()), "CameraApi");
        return false;
    }
}

// 处理启动摄像头预览的请求
HttpResponse CameraApi::handleStartPreview(const HttpRequest& request) {
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "application/json";

    try {
        if (startPreview()) {
            response.body = "{\"success\":true,\"message\":\"摄像头预览已启动\"}";
        } else {
            response.status_code = 500;
            response.body = "{\"success\":false,\"error\":\"无法启动摄像头预览\"}";
        }
    } catch (const std::exception& e) {
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = "{\"error\":\"" + std::string(e.what()) + "\"}";
    }

    return response;
}

// 处理停止摄像头预览的请求
HttpResponse CameraApi::handleStopPreview(const HttpRequest& request) {
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "application/json";

    try {
        if (stopPreview()) {
            response.body = "{\"success\":true,\"message\":\"摄像头预览已停止\"}";
        } else {
            response.status_code = 500;
            response.body = "{\"success\":false,\"error\":\"无法停止摄像头预览\"}";
        }
    } catch (const std::exception& e) {
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = "{\"error\":\"" + std::string(e.what()) + "\"}";
    }

    return response;
}

// 处理获取摄像头预览图像的请求
HttpResponse CameraApi::handleGetPreview(const HttpRequest& request) {
    HttpResponse response;

    try {
        // 获取摄像头管理器实例
        auto& camera_manager = camera::CameraManager::getInstance();

        // 检查摄像头是否已打开并正在捕获
        if (!camera_manager.isDeviceOpen() || !camera_manager.isCapturing()) {
            response.status_code = 400;
            response.content_type = "application/json";
            response.body = "{\"error\":\"摄像头未打开或未在预览中\"}";
            return response;
        }

        // 获取当前设备
        auto device = camera_manager.getCurrentDevice();
        if (!device) {
            response.status_code = 500;
            response.content_type = "application/json";
            response.body = "{\"error\":\"无法获取摄像头设备\"}";
            return response;
        }

        // 获取一帧图像
        camera::Frame frame = device->getFrame(500); // 500毫秒超时
        if (frame.data.empty()) {
            response.status_code = 500;
            response.content_type = "application/json";
            response.body = "{\"error\":\"无法获取摄像头帧\"}";
            return response;
        }

        // 设置响应头
        response.status_code = 200;
        response.content_type = "image/jpeg";

        // 设置响应体
        response.body.assign(frame.data.begin(), frame.data.end());

        // 添加缓存控制头，防止浏览器缓存
        response.headers["Cache-Control"] = "no-store, no-cache, must-revalidate, max-age=0";
        response.headers["Pragma"] = "no-cache";
        response.headers["Expires"] = "0";

    } catch (const std::exception& e) {
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = "{\"error\":\"" + std::string(e.what()) + "\"}";
    }

    return response;
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

// 确保目录存在
bool CameraApi::ensureDirectoryExists(const std::string& path) {
    try {
        if (!std::filesystem::exists(path)) {
            std::filesystem::create_directories(path);
            LOG_INFO("创建目录: " + path, "CameraApi");
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("创建目录失败: " + path + ", 错误: " + e.what(), "CameraApi");
        return false;
    }
}

// 拍照并保存图像
std::string CameraApi::captureImage(const std::string& output_path, int quality) {
    try {
        // 获取摄像头管理器实例
        auto& camera_manager = camera::CameraManager::getInstance();

        // 检查摄像头是否已打开并正在捕获
        if (!camera_manager.isDeviceOpen() || !camera_manager.isCapturing()) {
            LOG_ERROR("摄像头未打开或未在预览中", "CameraApi");
            return "";
        }

        // 获取当前设备
        auto device = camera_manager.getCurrentDevice();
        if (!device) {
            LOG_ERROR("无法获取摄像头设备", "CameraApi");
            return "";
        }

        // 获取一帧图像
        camera::Frame frame = device->getFrame(500); // 500毫秒超时
        if (frame.data.empty()) {
            LOG_ERROR("无法获取摄像头帧", "CameraApi");
            return "";
        }

        // 生成文件名
        std::string file_path;
        if (output_path.empty()) {
            // 使用当前时间生成文件名
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << images_dir_ << "/capture_";
            ss << std::put_time(std::localtime(&now_time_t), "%Y%m%d_%H%M%S");
            ss << ".jpg";
            file_path = ss.str();
        } else {
            file_path = output_path;
        }

        // 确保目录存在
        std::filesystem::path p(file_path);
        ensureDirectoryExists(p.parent_path().string());

        // 如果已经是MJPEG格式，直接保存
        if (frame.format == camera::PixelFormat::MJPEG) {
            std::ofstream file(file_path, std::ios::binary);
            if (!file) {
                LOG_ERROR("无法创建文件: " + file_path, "CameraApi");
                return "";
            }
            file.write(reinterpret_cast<const char*>(frame.data.data()), frame.data.size());
            file.close();
        } else {
            // 需要转换为JPEG格式
            // 这里简化实现，实际项目中应该使用更高效的方法
            // 例如使用FFmpeg或OpenCV进行转换
            LOG_ERROR("不支持的图像格式，需要转换为JPEG", "CameraApi");
            return "";
        }

        LOG_INFO("成功保存图像: " + file_path, "CameraApi");
        return file_path;
    } catch (const std::exception& e) {
        LOG_ERROR("拍照异常: " + std::string(e.what()), "CameraApi");
        return "";
    }
}

// 开始录制视频
bool CameraApi::startRecording(const std::string& output_path,
                             const std::string& format,
                             const std::string& encoder,
                             int bitrate,
                             int max_duration) {
    std::lock_guard<std::mutex> lock(recording_mutex_);

    try {
        // 检查是否已经在录制
        if (video_recorder_ && video_recorder_->getStatus().state == video::RecordingState::RECORDING) {
            LOG_WARNING("已经在录制中", "CameraApi");
            return true;
        }

        // 获取摄像头管理器实例
        auto& camera_manager = camera::CameraManager::getInstance();

        // 检查摄像头是否已打开并正在捕获
        if (!camera_manager.isDeviceOpen() || !camera_manager.isCapturing()) {
            LOG_ERROR("摄像头未打开或未在预览中", "CameraApi");
            return false;
        }

        // 获取当前设备
        auto device = camera_manager.getCurrentDevice();
        if (!device) {
            LOG_ERROR("无法获取摄像头设备", "CameraApi");
            return false;
        }

        // 获取设备信息
        auto device_info = device->getDeviceInfo();
        auto params = device->getParams();

        // 生成文件名
        std::string file_path;
        if (output_path.empty()) {
            // 使用当前时间生成文件名
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << videos_dir_ << "/video_";
            ss << std::put_time(std::localtime(&now_time_t), "%Y%m%d_%H%M%S");
            ss << "." << format;
            file_path = ss.str();
        } else {
            file_path = output_path;
        }

        // 确保目录存在
        std::filesystem::path p(file_path);
        ensureDirectoryExists(p.parent_path().string());

        // 创建录制配置
        video::RecordingConfig config;
        config.output_path = file_path;
        config.encoder_name = encoder;
        config.container_format = format;
        config.width = params.width;
        config.height = params.height;
        config.fps = params.fps;
        config.bitrate = bitrate;
        config.gop = params.fps * 2; // 关键帧间隔设为2秒
        config.use_hw_accel = true;  // 使用硬件加速
        config.max_duration = max_duration;
        config.max_size = 0; // 不限制文件大小

        // 创建录制器
        video_recorder_ = std::make_shared<video::SimpleVideoRecorder>();
        if (!video_recorder_) {
            LOG_ERROR("无法创建视频录制器", "CameraApi");
            return false;
        }

        // 初始化录制器
        if (!video_recorder_->initialize(config)) {
            LOG_ERROR("无法初始化视频录制器", "CameraApi");
            video_recorder_.reset();
            return false;
        }

        // 设置状态回调
        video_recorder_->setStatusCallback([this](const video::RecordingStatus& status) {
            // 可以在这里处理状态变化，例如记录日志
            if (status.state == video::RecordingState::ERROR) {
                LOG_ERROR("录制错误: " + status.error_message, "CameraApi");
            }
        });

        // 开始录制
        if (!video_recorder_->startRecording()) {
            LOG_ERROR("无法开始录制", "CameraApi");
            video_recorder_.reset();
            return false;
        }

        // 设置帧回调
        camera_manager.setFrameCallback([this](const camera::Frame& frame) {
            if (video_recorder_ &&
                video_recorder_->getStatus().state == video::RecordingState::RECORDING) {
                video_recorder_->processFrame(frame);
            }
        });

        LOG_INFO("成功开始录制: " + file_path, "CameraApi");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("开始录制异常: " + std::string(e.what()), "CameraApi");
        return false;
    }
}

// 停止录制视频
std::string CameraApi::stopRecording() {
    std::lock_guard<std::mutex> lock(recording_mutex_);

    try {
        // 检查是否在录制
        if (!video_recorder_ ||
            video_recorder_->getStatus().state != video::RecordingState::RECORDING) {
            LOG_WARNING("没有正在进行的录制", "CameraApi");
            return "";
        }

        // 获取当前状态
        auto status = video_recorder_->getStatus();
        std::string file_path = status.current_file;

        // 停止录制
        if (!video_recorder_->stopRecording()) {
            LOG_ERROR("无法停止录制", "CameraApi");
            return "";
        }

        // 清除帧回调
        auto& camera_manager = camera::CameraManager::getInstance();
        camera_manager.setFrameCallback(nullptr);

        // 清除录制器
        video_recorder_.reset();

        LOG_INFO("成功停止录制: " + file_path, "CameraApi");
        return file_path;
    } catch (const std::exception& e) {
        LOG_ERROR("停止录制异常: " + std::string(e.what()), "CameraApi");
        return "";
    }
}

// 获取录制状态
std::string CameraApi::getRecordingStatus() {
    std::lock_guard<std::mutex> lock(recording_mutex_);

    try {
        if (!video_recorder_) {
            return "{\"state\":\"IDLE\",\"recording\":false}";
        }

        auto status = video_recorder_->getStatus();
        std::ostringstream json;
        json << "{";
        json << "\"state\":\"" << (status.state == video::RecordingState::RECORDING ? "RECORDING" :
                                  status.state == video::RecordingState::PAUSED ? "PAUSED" :
                                  status.state == video::RecordingState::ERROR ? "ERROR" : "IDLE") << "\",";
        json << "\"recording\":" << (status.state == video::RecordingState::RECORDING) << ",";
        json << "\"file\":\"" << status.current_file << "\",";
        json << "\"duration\":" << status.duration << ",";
        json << "\"frame_count\":" << status.frame_count << ",";
        json << "\"file_size\":" << status.file_size;
        if (status.state == video::RecordingState::ERROR) {
            json << ",\"error\":\"" << status.error_message << "\"";
        }
        json << "}";

        return json.str();
    } catch (const std::exception& e) {
        LOG_ERROR("获取录制状态异常: " + std::string(e.what()), "CameraApi");
        return "{\"state\":\"ERROR\",\"recording\":false,\"error\":\"" + std::string(e.what()) + "\"}";
    }
}

// 处理拍照请求
HttpResponse CameraApi::handleCaptureImage(const HttpRequest& request) {
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "application/json";

    try {
        // 解析请求参数
        int quality = 90;
        std::string output_path = "";

        // 简单的JSON解析
        std::string body = request.body;
        if (!body.empty()) {
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

            std::string quality_str = extract_value("quality");
            if (!quality_str.empty()) {
                quality = std::stoi(quality_str);
            }

            output_path = extract_value("output_path");
        }

        // 拍照
        std::string file_path = captureImage(output_path, quality);
        if (file_path.empty()) {
            response.status_code = 500;
            response.body = "{\"status\":\"error\",\"message\":\"拍照失败\"}";
            return response;
        }

        // 构建响应
        std::ostringstream json;
        json << "{";
        json << "\"status\":\"success\",";
        json << "\"filename\":\"" << file_path << "\",";
        json << "\"url\":\"/static/images/" << std::filesystem::path(file_path).filename().string() << "\"";
        json << "}";

        response.body = json.str();
    } catch (const std::exception& e) {
        response.status_code = 500;
        response.body = "{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}";
    }

    return response;
}

// 处理开始录制请求
HttpResponse CameraApi::handleStartRecording(const HttpRequest& request) {
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "application/json";

    try {
        // 解析请求参数
        std::string format = "mp4";
        std::string encoder = "h264_rkmpp";
        int bitrate = 4000000;
        int max_duration = 0;
        std::string output_path = "";

        // 简单的JSON解析
        std::string body = request.body;
        if (!body.empty()) {
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

            std::string format_str = extract_value("format");
            if (!format_str.empty()) {
                format = format_str;
            }

            std::string encoder_str = extract_value("encoder");
            if (!encoder_str.empty()) {
                encoder = encoder_str;
            }

            std::string bitrate_str = extract_value("bitrate");
            if (!bitrate_str.empty()) {
                bitrate = std::stoi(bitrate_str);
            }

            std::string duration_str = extract_value("duration");
            if (!duration_str.empty()) {
                max_duration = std::stoi(duration_str);
            }

            output_path = extract_value("output_path");
        }

        // 开始录制
        if (startRecording(output_path, format, encoder, bitrate, max_duration)) {
            response.body = "{\"success\":true,\"message\":\"录制已开始\"}";
        } else {
            response.status_code = 500;
            response.body = "{\"success\":false,\"error\":\"无法开始录制\"}";
        }
    } catch (const std::exception& e) {
        response.status_code = 500;
        response.body = "{\"success\":false,\"error\":\"" + std::string(e.what()) + "\"}";
    }

    return response;
}

// 处理停止录制请求
HttpResponse CameraApi::handleStopRecording(const HttpRequest& request) {
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "application/json";

    try {
        // 停止录制
        std::string file_path = stopRecording();
        if (file_path.empty()) {
            response.body = "{\"success\":true,\"message\":\"没有正在进行的录制\"}";
        } else {
            std::ostringstream json;
            json << "{";
            json << "\"success\":true,";
            json << "\"message\":\"录制已停止\",";
            json << "\"file_path\":\"" << file_path << "\"";
            json << "}";
            response.body = json.str();
        }
    } catch (const std::exception& e) {
        response.status_code = 500;
        response.body = "{\"success\":false,\"error\":\"" + std::string(e.what()) + "\"}";
    }

    return response;
}

// 处理获取录制状态请求
HttpResponse CameraApi::handleGetRecordingStatus(const HttpRequest& request) {
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "application/json";

    try {
        response.body = getRecordingStatus();
    } catch (const std::exception& e) {
        response.status_code = 500;
        response.body = "{\"success\":false,\"error\":\"" + std::string(e.what()) + "\"}";
    }

    return response;
}

// 处理获取摄像头连接状态请求
HttpResponse CameraApi::handleGetCameraStatus(const HttpRequest& request) {
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "application/json";

    try {
        // 获取查询参数中的摄像头ID
        std::string camera_id;
        auto it = request.query_params.find("camera_id");
        if (it != request.query_params.end()) {
            camera_id = it->second;
        }

        if (camera_id.empty()) {
            response.status_code = 400;
            response.body = "{\"success\":false,\"error\":\"缺少摄像头ID参数\"}";
            return response;
        }

        // 获取MJPEG流处理器实例
        auto& mjpeg_streamer = api::MjpegStreamer::getInstance();

        // 检查摄像头是否有客户端连接
        bool is_connected = false;
        std::string client_id;
        int width = 0;
        int height = 0;
        double fps = 0.0;
        std::string format;
        int64_t connection_time = 0;

        // 这里需要从MjpegStreamer类中获取摄像头连接状态
        // 由于我们没有直接的API，我们可以通过检查camera_clients_映射来判断
        // 但这需要修改MjpegStreamer类，添加获取摄像头连接状态的方法

        // 临时方案：从CameraManager获取当前摄像头信息
        auto& camera_manager = camera::CameraManager::getInstance();
        if (camera_manager.isDeviceOpen()) {
            // 获取当前设备
            auto current_device = camera_manager.getCurrentDevice();
            if (current_device) {
                // 获取设备信息
                auto device_info = current_device->getDeviceInfo();

                // 检查是否是请求的摄像头
                if (device_info.device_path == camera_id) {
                    is_connected = true;

                    // 获取当前客户端信息
                    // 这里简化处理，实际应该从MjpegStreamer获取
                    client_id = "current_client";

                    // 获取摄像头参数
                    auto params = current_device->getParams();
                    width = params.width;
                    height = params.height;
                    fps = mjpeg_streamer.getCurrentFps();

                    // 获取格式
                    switch (params.format) {
                        case camera::PixelFormat::MJPEG:
                            format = "MJPEG";
                            break;
                        case camera::PixelFormat::YUYV:
                            format = "YUYV";
                            break;
                        case camera::PixelFormat::H264:
                            format = "H264";
                            break;
                        case camera::PixelFormat::NV12:
                            format = "NV12";
                            break;
                        case camera::PixelFormat::RGB24:
                            format = "RGB24";
                            break;
                        case camera::PixelFormat::BGR24:
                            format = "BGR24";
                            break;
                        default:
                            format = "UNKNOWN";
                            break;
                    }
                }
            }
        }

        // 获取连接时间
        if (is_connected) {
            connection_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        }

        // 构建JSON响应
        std::ostringstream json;
        json << "{";
        json << "\"success\":true,";
        json << "\"is_connected\":" << (is_connected ? "true" : "false");

        if (is_connected) {
            json << ",\"client_id\":\"" << client_id << "\"";
            json << ",\"camera_id\":\"" << camera_id << "\"";
            json << ",\"width\":" << width;
            json << ",\"height\":" << height;
            json << ",\"fps\":" << fps;
            json << ",\"format\":\"" << format << "\"";
            json << ",\"connection_time\":" << connection_time;
        }

        json << "}";

        response.body = json.str();
    } catch (const std::exception& e) {
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = "{\"success\":false,\"error\":\"" + std::string(e.what()) + "\"}";
    }

    return response;
}

// 处理关闭摄像头的请求
HttpResponse CameraApi::handleCloseCamera(const HttpRequest& request) {
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "application/json";

    try {
        if (closeCamera()) {
            response.body = "{\"success\":true,\"message\":\"摄像头已成功关闭\"}";
        } else {
            response.status_code = 500;
            response.body = "{\"success\":false,\"error\":\"无法关闭摄像头\"}";
        }
    } catch (const std::exception& e) {
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = "{\"error\":\"" + std::string(e.what()) + "\"}";
    }

    return response;
}

} // namespace api
} // namespace cam_server
