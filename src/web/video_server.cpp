#include "web/video_server.h"
#include "web/http_routes.h"
#include "web/websocket_handler.h"
#include "web/frame_extraction_routes.h"
#include "web/system_routes.h"
#include "web/serial_routes.h"
#include "monitor/logger.h"
#include "camera/camera_manager.h"
#include "system/system_monitor.h"

#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <thread>
#include <filesystem>

namespace cam_server {
namespace web {

// ExtractionTask 移动构造函数实现
ExtractionTask::ExtractionTask(ExtractionTask&& other) noexcept
    : task_id(std::move(other.task_id))
    , input_file(std::move(other.input_file))
    , output_dir(std::move(other.output_dir))
    , interval(other.interval)
    , format(std::move(other.format))
    , extracted_frames(other.extracted_frames.load())
    , total_frames(other.total_frames.load())
    , completed(other.completed.load())
    , cancelled(other.cancelled.load())
    , first_frame_filename(std::move(other.first_frame_filename))
    , last_frame_filename(std::move(other.last_frame_filename)) {
}

// ExtractionTask 移动赋值操作符实现
ExtractionTask& ExtractionTask::operator=(ExtractionTask&& other) noexcept {
    if (this != &other) {
        task_id = std::move(other.task_id);
        input_file = std::move(other.input_file);
        output_dir = std::move(other.output_dir);
        interval = other.interval;
        format = std::move(other.format);
        extracted_frames = other.extracted_frames.load();
        total_frames = other.total_frames.load();
        completed = other.completed.load();
        cancelled = other.cancelled.load();
        first_frame_filename = std::move(other.first_frame_filename);
        last_frame_filename = std::move(other.last_frame_filename);
    }
    return *this;
}

VideoServer::VideoServer()
    : port_(8081)
    , is_running_(false)
    , frame_count_(0)
    , is_recording_(false)
    , recording_frame_count_(0)
    , recording_file_size_(0) {
}

VideoServer::~VideoServer() {
    stop();
}

bool VideoServer::initialize() {
    std::cout << "🚀 初始化视频流服务器..." << std::endl;

    // 初始化摄像头管理器
    auto& camera_manager = camera::CameraManager::getInstance();
    if (!camera_manager.initialize("config/config.json")) {
        std::cout << "❌ 摄像头管理器初始化失败" << std::endl;
        return false;
    }
    std::cout << "✅ 摄像头管理器初始化完成" << std::endl;

    // 初始化系统监控
    auto& system_monitor = system::SystemMonitor::getInstance();
    if (!system_monitor.initialize(1000)) {  // 1秒更新间隔
        std::cout << "❌ 系统监控初始化失败" << std::endl;
        return false;
    }

    if (!system_monitor.start()) {
        std::cout << "❌ 系统监控启动失败" << std::endl;
        return false;
    }
    std::cout << "✅ 系统监控初始化完成" << std::endl;

    // 设置路由
    setupRoutes();
    std::cout << "✅ 路由设置完成" << std::endl;

    return true;
}

bool VideoServer::start() {
    if (is_running_) {
        return true;
    }

    try {
        std::cout << "🚀 启动WebSocket视频流服务器，端口: " << port_ << std::endl;

        // 启动服务器线程（路由已在initialize中设置）
        is_running_ = true;
        server_thread_ = std::thread([this]() {
            app_.port(port_).multithreaded().run();
        });

        std::cout << "✅ WebSocket视频流服务器启动成功，监听端口: " << port_ << std::endl;
        std::cout << "📱 WebSocket视频流地址: ws://localhost:" << port_ << "/ws/video" << std::endl;

        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ 服务器启动失败: " << e.what() << std::endl;
        return false;
    }
}

void VideoServer::stop() {
    if (!is_running_) {
        return;
    }

    std::cout << "🛑 正在停止服务器..." << std::endl;

    // 停止摄像头
    auto& camera_manager = camera::CameraManager::getInstance();
    camera_manager.stopCapture();
    camera_manager.closeDevice();

    // 停止系统监控
    auto& system_monitor = system::SystemMonitor::getInstance();
    system_monitor.stop();

    is_running_ = false;
    app_.stop();

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    std::cout << "✅ 服务器已停止" << std::endl;
}

void VideoServer::waitForStop() {
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void VideoServer::setupRoutes() {
    // 设置各种路由
    HttpRoutes::setupStaticRoutes(app_);
    HttpRoutes::setupPhotoRoutes(app_);
    HttpRoutes::setupVideoRoutes(app_);
    HttpRoutes::setupPageRoutes(app_);

    SystemRoutes::setupRoutes(app_);
    SerialRoutes::setupRoutes(app_);
    FrameExtractionRoutes::setupRoutes(app_, this);
    WebSocketHandler::setupRoutes(app_, this);
}

std::string VideoServer::generateClientId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

crow::response VideoServer::serveHtmlFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return crow::response(404, "页面不存在");
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();

    crow::response res(200, content);
    res.set_header("Content-Type", "text/html; charset=utf-8");
    res.set_header("Cache-Control", "no-cache, no-store, must-revalidate");
    res.set_header("Pragma", "no-cache");
    res.set_header("Expires", "0");

    return res;
}

void VideoServer::setupDynamicHtmlRoutes() {
    // 这个方法现在由 HttpRoutes 处理
    // 保留接口以保持兼容性
}

// 帧提取相关方法的简化实现
void VideoServer::extractFramesFromMJPEG(const std::string& task_id, const std::string& input_file,
                                        const std::string& output_dir, int interval, const std::string& format) {
    // 这个方法将在 FrameExtractionRoutes 中实现
    // 这里只是保持接口兼容性
    std::lock_guard<std::mutex> lock(extraction_mutex_);

    auto& task = extraction_tasks_[task_id];
    task.task_id = task_id;
    task.input_file = input_file;
    task.output_dir = output_dir;
    task.interval = interval;
    task.format = format;

    // 实际的帧提取逻辑将在专门的模块中实现
    // 这里只是标记任务完成
    task.completed = true;
}

bool VideoServer::saveFrameAsJPEG(const std::vector<uint8_t>& frame_data, const std::string& output_path) {
    // 简化实现，实际逻辑在 FrameExtractionRoutes 中
    return !frame_data.empty() && !output_path.empty();
}

std::string VideoServer::createFrameArchive(const std::string& task_id, const std::string& frames_dir,
                                           const std::string& input_file, int frame_count) {
    // 简化实现，实际逻辑在 FrameExtractionRoutes 中
    return "archive_" + task_id + ".tar.gz";
}

// 获取客户端管理器的访问方法
std::unordered_map<std::string, ClientInfo>& VideoServer::getClients() {
    return clients_;
}

std::mutex& VideoServer::getClientsMutex() {
    return clients_mutex_;
}

// 获取帧提取任务管理器的访问方法
std::unordered_map<std::string, ExtractionTask>& VideoServer::getExtractionTasks() {
    return extraction_tasks_;
}

std::mutex& VideoServer::getExtractionMutex() {
    return extraction_mutex_;
}

// 获取统计信息的访问方法
std::atomic<size_t>& VideoServer::getFrameCount() {
    return frame_count_;
}

std::atomic<bool>& VideoServer::getIsRecording() {
    return is_recording_;
}

std::atomic<size_t>& VideoServer::getRecordingFrameCount() {
    return recording_frame_count_;
}

std::atomic<size_t>& VideoServer::getRecordingFileSize() {
    return recording_file_size_;
}

} // namespace web
} // namespace cam_server
