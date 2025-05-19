#include "api/camera_api.h"
#include "camera/camera_manager.h"
#include "monitor/logger.h"
#include "utils/string_utils.h"
#include "utils/file_utils.h"
#include "video/i_video_recorder.h"
#include "api/mjpeg_streamer.h"
#include "camera/format_utils.h"

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
        const auto& data = frame.getData();
        output_file_.write(reinterpret_cast<const char*>(data.data()), data.size());

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
namespace fs = std::filesystem;
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
CameraApi::CameraApi() 
    : is_initialized_(false), 
      video_recorder_(nullptr),
      mjpeg_streamer_(MjpegStreamer::getInstance()) {
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
bool CameraApi::registerRoutes(RestHandler& rest_handler) {
    std::cerr << "[CAMERA_API][DEBUG] 开始注册摄像头API路由..." << std::endl;

    // 获取摄像头状态
    std::cerr << "[CAMERA_API][DEBUG] 注册摄像头状态API: GET /api/camera/status" << std::endl;
    rest_handler.registerRoute("GET", "/api/camera/status", [this](const HttpRequest& request) {
        return handleGetCameraStatus(request);
    });

    // 获取所有摄像头列表
    std::cerr << "[CAMERA_API][DEBUG] 注册摄像头列表API: GET /api/camera/list" << std::endl;
    rest_handler.registerRoute("GET", "/api/camera/list", [this](const HttpRequest& request) {
        return handleGetAllCameras(request);
    });

    // 打开摄像头
    std::cerr << "[CAMERA_API][DEBUG] 注册打开摄像头API: POST /api/camera/open" << std::endl;
    rest_handler.registerRoute("POST", "/api/camera/open", [this](const HttpRequest& request) {
        return handleOpenCamera(request);
    });

    // 关闭摄像头
    std::cerr << "[CAMERA_API][DEBUG] 注册关闭摄像头API: POST /api/camera/close" << std::endl;
    rest_handler.registerRoute("POST", "/api/camera/close", [this](const HttpRequest& request) {
        return handleCloseCamera(request);
    });

    // 开始预览
    std::cerr << "[CAMERA_API][DEBUG] 注册开始预览API: POST /api/camera/start_preview" << std::endl;
    rest_handler.registerRoute("POST", "/api/camera/start_preview", [this](const HttpRequest& request) {
        return handleStartPreview(request);
    });

    // 停止预览
    std::cerr << "[CAMERA_API][DEBUG] 注册停止预览API: POST /api/camera/stop_preview" << std::endl;
    rest_handler.registerRoute("POST", "/api/camera/stop_preview", [this](const HttpRequest& request) {
        return handleStopPreview(request);
    });

    // 拍照
    std::cerr << "[CAMERA_API][DEBUG] 注册拍照API: POST /api/camera/capture" << std::endl;
    rest_handler.registerRoute("POST", "/api/camera/capture", [this](const HttpRequest& request) {
        return handleCaptureImage(request);
    });

    // 开始录制
    std::cerr << "[CAMERA_API][DEBUG] 注册开始录制API: POST /api/camera/start_recording" << std::endl;
    rest_handler.registerRoute("POST", "/api/camera/start_recording", [this](const HttpRequest& request) {
        return handleStartRecording(request);
    });

    // 停止录制
    std::cerr << "[CAMERA_API][DEBUG] 注册停止录制API: POST /api/camera/stop_recording" << std::endl;
    rest_handler.registerRoute("POST", "/api/camera/stop_recording", [this](const HttpRequest& request) {
        return handleStopRecording(request);
    });

    std::cerr << "[CAMERA_API][DEBUG] 摄像头API路由注册完成" << std::endl;

    // 注册MJPEG流API
    std::cerr << "[CAMERA_API][DEBUG] 注册MJPEG流API: GET /api/camera/mjpeg" << std::endl;
    rest_handler.registerRoute("GET", "/api/camera/mjpeg", [this](const HttpRequest& request) {
        return handleMjpegStream(request);
    });

    return true;
}

// 获取所有摄像头设备
std::vector<CameraDeviceInfo> CameraApi::getAllCameras() {
    std::vector<CameraDeviceInfo> devices;

    for (const auto& entry : fs::directory_iterator("/dev")) {
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
        std::string format_name = camera::FormatUtils::getV4L2FormatName(fmt.pixelformat);

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

        // 获取一帧图像
        auto frame = camera_manager.getFrame();

        // 验证帧数据
        if (frame.getData().empty()) {
            LOG_ERROR("捕获图像失败：空数据", "CameraApi");
            return "";
        }

        // 创建输出目录
        std::filesystem::path p(output_path);
        if (!std::filesystem::exists(p.parent_path())) {
            if (!std::filesystem::create_directories(p.parent_path())) {
                LOG_ERROR("无法创建输出目录: " + p.parent_path().string(), "CameraApi");
                return "";
            }
        }

        // 打开输出文件
        std::ofstream file(output_path, std::ios::binary);
        if (!file) {
            LOG_ERROR("无法打开输出文件: " + output_path, "CameraApi");
            return "";
        }

        // 如果是MJPEG格式，直接写入
        if (frame.getFormat() == camera::PixelFormat::MJPEG) {
            const auto& data = frame.getData();
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
            file.close();
            return output_path;
        }

        // 否则需要转换为JPEG
        std::vector<uint8_t> jpeg_data;
        if (!mjpeg_streamer_.encodeToJpeg(frame, jpeg_data)) {
            LOG_ERROR("JPEG编码失败", "CameraApi");
            return "";
        }

        file.write(reinterpret_cast<const char*>(jpeg_data.data()), jpeg_data.size());
        file.close();

        return output_path;

    } catch (const std::exception& e) {
        LOG_ERROR("捕获图像时发生异常: " + std::string(e.what()), "CameraApi");
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

// 处理获取摄像头状态请求
HttpResponse CameraApi::handleGetCameraStatus(const HttpRequest& request) {
    try {
        auto& camera_manager = camera::CameraManager::getInstance();
        bool is_open = camera_manager.isDeviceOpen();
        bool is_capturing = is_open && camera_manager.isCapturing();

        std::ostringstream json;
        json << "{";
        json << "\"success\":true,";
        json << "\"status\":\"" << (is_open ? (is_capturing ? "capturing" : "opened") : "closed") << "\",";
        json << "\"is_open\":" << (is_open ? "true" : "false") << ",";
        json << "\"is_capturing\":" << (is_capturing ? "true" : "false");

        if (is_open) {
            try {
                auto device = camera_manager.getCurrentDevice();
                if (!device) {
                    throw std::runtime_error("无法获取当前设备");
                }

                const auto& params = device->getParams();
                const auto& info = device->getDeviceInfo();
                
                json << ",\"device_info\":{";
                json << "\"path\":\"" << info.device_path << "\",";
                json << "\"name\":\"" << info.device_name << "\",";
                json << "\"description\":\"" << info.description << "\"";
                json << "},";
                
                json << "\"params\":{";
                json << "\"width\":" << params.width << ",";
                json << "\"height\":" << params.height << ",";
                json << "\"fps\":" << params.fps << ",";
                json << "\"format\":\"" << camera::FormatUtils::getPixelFormatName(params.format) << "\",";
                json << "\"brightness\":" << params.brightness << ",";
                json << "\"contrast\":" << params.contrast << ",";
                json << "\"saturation\":" << params.saturation << ",";
                json << "\"exposure\":" << params.exposure;
                json << "}";
            } catch (const std::exception& e) {
                LOG_ERROR("获取设备信息失败: " + std::string(e.what()), "CameraApi");
                json << ",\"device_info_error\":\"" << e.what() << "\"";
            }
        }
        json << "}";

        HttpResponse response;
        response.status_code = 200;
        response.content_type = "application/json";
        response.body = json.str();
        return response;
    } catch (const std::exception& e) {
        LOG_ERROR("获取摄像头状态失败: " + std::string(e.what()), "CameraApi");
        HttpResponse response;
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = "{\"success\":false,\"error\":\"获取摄像头状态失败: " + std::string(e.what()) + "\"}";
        return response;
    }
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

// 处理MJPEG流请求
HttpResponse CameraApi::handleMjpegStream(const HttpRequest& request) {
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "multipart/x-mixed-replace;boundary=frame";
    response.is_streaming = true;

    // 获取客户端ID和摄像头ID
    std::string client_id;
    std::string camera_id;
    auto it = request.query_params.find("client_id");
    if (it != request.query_params.end()) {
        client_id = it->second;
    }
    it = request.query_params.find("camera_id");
    if (it != request.query_params.end()) {
        camera_id = it->second;
    }

    // 检查摄像头是否已打开
    auto& camera_manager = camera::CameraManager::getInstance();
    if (!camera_manager.isDeviceOpen() || !camera_manager.isCapturing()) {
        response.status_code = 400;
        response.content_type = "application/json";
        response.body = "{\"error\":\"摄像头未打开或未在预览状态\"}";
        return response;
    }

    // 初始化MJPEG流处理器
    MjpegStreamerConfig config;
    config.jpeg_quality = 80;
    config.max_fps = 30;
    config.max_clients = 2;
    if (!mjpeg_streamer_.initialize(config)) {
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = "{\"error\":\"MJPEG流处理器初始化失败\"}";
        return response;
    }

    // 启动MJPEG流处理器
    if (!mjpeg_streamer_.start()) {
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = "{\"error\":\"MJPEG流处理器启动失败\"}";
        return response;
    }

    // 添加流回调
    response.stream_callback = [this, client_id, camera_id](std::function<void(const std::vector<uint8_t>&)> write_callback) {
        // 添加MJPEG客户端
        mjpeg_streamer_.addClient(client_id, camera_id,
            [write_callback](const std::vector<uint8_t>& frame_data) {
                // 构建MJPEG帧
                std::string header = "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                                   std::to_string(frame_data.size()) + "\r\n\r\n";
                
                // 发送帧头
                std::vector<uint8_t> header_data(header.begin(), header.end());
                write_callback(header_data);
                
                // 发送帧数据
                write_callback(frame_data);
            },
            [](const std::string& error) {
                LOG_ERROR("MJPEG流错误: " + error, "CameraApi");
            },
            [this, client_id]() {
                LOG_INFO("MJPEG客户端断开连接: " + client_id, "CameraApi");
                mjpeg_streamer_.removeClient(client_id);
            }
        );
    };

    return response;
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
        response.body = "{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}";
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
            response.body = "{\"status\":\"error\",\"message\":\"缺少必要参数或参数无效\"}";
            return response;
        }

        // 打开摄像头
        if (openCamera(device_path, format, width, height, fps)) {
            response.body = "{\"status\":\"success\",\"message\":\"摄像头已成功打开\"}";
        } else {
            response.status_code = 500;
            response.body = "{\"status\":\"error\",\"message\":\"无法打开摄像头\"}";
        }
    } catch (const std::exception& e) {
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = "{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}";
    }

    return response;
}

// 处理启动摄像头预览的请求
HttpResponse CameraApi::handleStartPreview(const HttpRequest& request) {
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "application/json";

    try {
        if (startPreview()) {
            response.body = "{\"status\":\"success\",\"message\":\"摄像头预览已启动\"}";
        } else {
            response.status_code = 500;
            response.body = "{\"status\":\"error\",\"message\":\"无法启动摄像头预览\"}";
        }
    } catch (const std::exception& e) {
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = "{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}";
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
            response.body = "{\"status\":\"success\",\"message\":\"摄像头预览已停止\"}";
        } else {
            response.status_code = 500;
            response.body = "{\"status\":\"error\",\"message\":\"无法停止摄像头预览\"}";
        }
    } catch (const std::exception& e) {
        response.status_code = 500;
        response.content_type = "application/json";
        response.body = "{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}";
    }

    return response;
}

} // namespace api
} // namespace cam_server
