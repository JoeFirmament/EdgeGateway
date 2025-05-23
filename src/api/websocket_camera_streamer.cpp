#include "../../include/api/websocket_camera_streamer.h"
#include "../../include/camera/camera_manager.h"
#include "../../include/monitor/logger.h"
#include "../../include/utils/time_utils.h"

#include <chrono>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <sstream>
#include <iomanip>

// 使用FFmpeg进行JPEG编码
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace cam_server {
namespace api {

WebSocketCameraStreamer& WebSocketCameraStreamer::getInstance() {
    static WebSocketCameraStreamer instance;
    return instance;
}

WebSocketCameraStreamer::WebSocketCameraStreamer()
    : is_initialized_(false), is_running_(false), current_fps_(0.0), 
      frame_count_(0), cleanup_running_(false) {
    last_fps_time_ = std::chrono::steady_clock::now();
}

WebSocketCameraStreamer::~WebSocketCameraStreamer() {
    stop();
}

bool WebSocketCameraStreamer::initialize(const WebSocketCameraStreamerConfig& config, 
                                       std::shared_ptr<CrowServer> crow_server) {
    LOG_DEBUG("初始化WebSocket摄像头流处理器...", "WebSocketCameraStreamer");

    if (is_initialized_) {
        LOG_WARNING("WebSocket摄像头流处理器已经初始化", "WebSocketCameraStreamer");
        return true;
    }

    if (!crow_server) {
        LOG_ERROR("Crow服务器实例为空", "WebSocketCameraStreamer");
        return false;
    }

    // 保存配置和服务器实例
    config_ = config;
    crow_server_ = crow_server;

    // 验证配置
    if (config_.jpeg_quality < 1 || config_.jpeg_quality > 100) {
        LOG_ERROR("无效的JPEG质量: " + std::to_string(config_.jpeg_quality), "WebSocketCameraStreamer");
        return false;
    }

    if (config_.max_fps <= 0) {
        LOG_ERROR("无效的最大帧率: " + std::to_string(config_.max_fps), "WebSocketCameraStreamer");
        return false;
    }

    // 重置统计信息
    current_fps_ = 0.0;
    frame_count_ = 0;
    last_fps_time_ = std::chrono::steady_clock::now();

    is_initialized_ = true;
    LOG_INFO("WebSocket摄像头流处理器初始化成功", "WebSocketCameraStreamer");
    return true;
}

bool WebSocketCameraStreamer::start() {
    LOG_DEBUG("启动WebSocket摄像头流处理器...", "WebSocketCameraStreamer");

    if (!is_initialized_) {
        LOG_ERROR("WebSocket摄像头流处理器未初始化", "WebSocketCameraStreamer");
        return false;
    }

    if (is_running_) {
        LOG_WARNING("WebSocket摄像头流处理器已经在运行", "WebSocketCameraStreamer");
        return true;
    }

    // 获取摄像头管理器并设置帧回调
    auto& camera_manager = camera::CameraManager::getInstance();
    camera_manager.setFrameCallback([this](const camera::Frame& frame) {
        handleFrame(frame);
    });

    // 启动清理线程
    cleanup_running_ = true;
    cleanup_thread_ = std::thread([this]() {
        while (cleanup_running_) {
            cleanupInactiveClients();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    });

    is_running_ = true;
    LOG_INFO("WebSocket摄像头流处理器启动成功", "WebSocketCameraStreamer");
    return true;
}

bool WebSocketCameraStreamer::stop() {
    if (!is_running_) {
        return true;
    }

    LOG_DEBUG("停止WebSocket摄像头流处理器...", "WebSocketCameraStreamer");

    // 停止清理线程
    cleanup_running_ = false;
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }

    // 清理所有客户端
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.clear();
        camera_clients_.clear();
    }

    is_running_ = false;
    LOG_INFO("WebSocket摄像头流处理器已停止", "WebSocketCameraStreamer");
    return true;
}

bool WebSocketCameraStreamer::isRunning() const {
    return is_running_;
}

bool WebSocketCameraStreamer::addClient(const std::string& client_id, const std::string& camera_id) {
    std::lock_guard<std::mutex> lock(clients_mutex_);

    // 检查客户端数量限制
    if (clients_.size() >= static_cast<size_t>(config_.max_clients)) {
        LOG_WARNING("达到最大客户端数量限制: " + std::to_string(config_.max_clients), "WebSocketCameraStreamer");
        return false;
    }

    // 创建新客户端
    auto client = std::make_shared<WebSocketCameraClient>();
    client->client_id = client_id;
    client->camera_id = camera_id;
    client->last_frame_time = std::chrono::steady_clock::now();
    client->is_active = true;
    client->frame_count = 0;
    client->fps = 0.0;

    // 添加到客户端映射
    clients_[client_id] = client;
    camera_clients_[camera_id].insert(client_id);

    LOG_INFO("添加WebSocket摄像头客户端: " + client_id + ", 摄像头: " + camera_id, "WebSocketCameraStreamer");
    return true;
}

bool WebSocketCameraStreamer::removeClient(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(clients_mutex_);

    auto it = clients_.find(client_id);
    if (it == clients_.end()) {
        LOG_WARNING("客户端不存在: " + client_id, "WebSocketCameraStreamer");
        return false;
    }

    // 从摄像头客户端映射中移除
    const std::string& camera_id = it->second->camera_id;
    auto camera_it = camera_clients_.find(camera_id);
    if (camera_it != camera_clients_.end()) {
        camera_it->second.erase(client_id);
        if (camera_it->second.empty()) {
            camera_clients_.erase(camera_it);
        }
    }

    // 移除客户端
    clients_.erase(it);

    LOG_INFO("移除WebSocket摄像头客户端: " + client_id, "WebSocketCameraStreamer");
    return true;
}

size_t WebSocketCameraStreamer::getClientCount() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    return clients_.size();
}

size_t WebSocketCameraStreamer::getCameraClientCount(const std::string& camera_id) const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    auto it = camera_clients_.find(camera_id);
    return (it != camera_clients_.end()) ? it->second.size() : 0;
}

double WebSocketCameraStreamer::getCurrentFPS() const {
    return current_fps_.load();
}

void WebSocketCameraStreamer::broadcastFrame(const std::string& camera_id, const std::vector<uint8_t>& frame_data) {
    if (!is_running_ || !crow_server_) {
        return;
    }

    std::lock_guard<std::mutex> lock(clients_mutex_);
    
    auto camera_it = camera_clients_.find(camera_id);
    if (camera_it == camera_clients_.end()) {
        return; // 没有该摄像头的客户端
    }

    // 将帧数据转换为字符串
    std::string frame_str(frame_data.begin(), frame_data.end());

    // 广播给该摄像头的所有客户端
    for (const auto& client_id : camera_it->second) {
        auto client_it = clients_.find(client_id);
        if (client_it != clients_.end() && client_it->second->is_active) {
            // 通过Crow服务器发送WebSocket消息
            crow_server_->sendWebSocketMessage(client_id, frame_str, true); // true表示二进制消息
            
            // 更新客户端统计
            client_it->second->frame_count++;
            client_it->second->last_frame_time = std::chrono::steady_clock::now();
        }
    }

    // 更新全局帧率统计
    updateFPS();
}

bool WebSocketCameraStreamer::sendFrameToClient(const std::string& client_id, const std::vector<uint8_t>& frame_data) {
    if (!is_running_ || !crow_server_) {
        return false;
    }

    std::lock_guard<std::mutex> lock(clients_mutex_);
    
    auto it = clients_.find(client_id);
    if (it == clients_.end() || !it->second->is_active) {
        return false;
    }

    // 将帧数据转换为字符串
    std::string frame_str(frame_data.begin(), frame_data.end());

    // 发送WebSocket消息
    bool success = crow_server_->sendWebSocketMessage(client_id, frame_str, true);
    
    if (success) {
        // 更新客户端统计
        it->second->frame_count++;
        it->second->last_frame_time = std::chrono::steady_clock::now();
    }

    return success;
}

void WebSocketCameraStreamer::handleFrame(const camera::Frame& frame) {
    if (!is_running_) {
        return;
    }

    // 编码为JPEG
    std::vector<uint8_t> jpeg_data;
    if (!encodeToJpeg(frame, jpeg_data)) {
        LOG_ERROR("编码JPEG失败", "WebSocketCameraStreamer");
        return;
    }

    // 广播到所有客户端（假设使用默认摄像头ID）
    broadcastFrame("default", jpeg_data);
}

bool WebSocketCameraStreamer::encodeToJpeg(const camera::Frame& frame, std::vector<uint8_t>& jpeg_data) {
    // 这里使用简化的JPEG编码实现
    // 实际项目中应该使用FFmpeg进行编码
    
    if (frame.getData().empty()) {
        return false;
    }

    // 如果帧已经是JPEG格式，直接使用
    if (frame.getFormat() == camera::PixelFormat::MJPEG) {
        jpeg_data = frame.getData();
        return true;
    }

    // TODO: 实现其他格式到JPEG的转换
    LOG_WARNING("暂不支持非MJPEG格式的编码", "WebSocketCameraStreamer");
    return false;
}

void WebSocketCameraStreamer::updateFPS() {
    frame_count_++;
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_time_);
    
    if (duration.count() >= 1000) { // 每秒更新一次
        double fps = static_cast<double>(frame_count_) * 1000.0 / duration.count();
        current_fps_ = fps;
        frame_count_ = 0;
        last_fps_time_ = now;
    }
}

void WebSocketCameraStreamer::cleanupInactiveClients() {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    std::vector<std::string> inactive_clients;
    
    // 查找非活跃客户端（超过30秒没有活动）
    for (const auto& [client_id, client] : clients_) {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - client->last_frame_time);
        if (duration.count() > 30) {
            inactive_clients.push_back(client_id);
        }
    }
    
    // 移除非活跃客户端
    for (const auto& client_id : inactive_clients) {
        LOG_INFO("清理非活跃客户端: " + client_id, "WebSocketCameraStreamer");
        removeClient(client_id);
    }
}

} // namespace api
} // namespace cam_server
