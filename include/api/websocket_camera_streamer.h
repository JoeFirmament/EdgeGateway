#pragma once

#include "camera/frame.h"
#include "api/crow_server.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <thread>
#include <chrono>

namespace cam_server {
namespace api {

// WebSocket摄像头流配置
struct WebSocketCameraStreamerConfig {
    int jpeg_quality = 80;        // JPEG质量 (1-100)
    int max_fps = 30;            // 最大帧率
    int max_clients = 10;        // 最大客户端数量
    int output_width = 0;        // 输出宽度 (0表示使用原始尺寸)
    int output_height = 0;       // 输出高度 (0表示使用原始尺寸)
    bool enable_frame_skip = true; // 启用帧跳过以保持实时性
};

// WebSocket客户端信息
struct WebSocketCameraClient {
    std::string client_id;
    std::string camera_id;
    std::chrono::steady_clock::time_point last_frame_time;
    std::atomic<bool> is_active;
    std::atomic<int> frame_count;
    std::atomic<double> fps;
    
    WebSocketCameraClient() : is_active(false), frame_count(0), fps(0.0) {}
};

/**
 * @brief WebSocket摄像头流处理器
 * 
 * 负责将摄像头视频流通过WebSocket发送给客户端
 */
class WebSocketCameraStreamer {
public:
    /**
     * @brief 获取单例实例
     */
    static WebSocketCameraStreamer& getInstance();

    /**
     * @brief 初始化流处理器
     * @param config 配置参数
     * @param crow_server Crow服务器实例
     * @return 是否初始化成功
     */
    bool initialize(const WebSocketCameraStreamerConfig& config, 
                   std::shared_ptr<CrowServer> crow_server);

    /**
     * @brief 启动流处理器
     * @return 是否启动成功
     */
    bool start();

    /**
     * @brief 停止流处理器
     * @return 是否停止成功
     */
    bool stop();

    /**
     * @brief 检查是否正在运行
     */
    bool isRunning() const;

    /**
     * @brief 添加摄像头客户端
     * @param client_id 客户端ID
     * @param camera_id 摄像头ID
     * @return 是否添加成功
     */
    bool addClient(const std::string& client_id, const std::string& camera_id);

    /**
     * @brief 移除摄像头客户端
     * @param client_id 客户端ID
     * @return 是否移除成功
     */
    bool removeClient(const std::string& client_id);

    /**
     * @brief 获取客户端数量
     */
    size_t getClientCount() const;

    /**
     * @brief 获取指定摄像头的客户端数量
     */
    size_t getCameraClientCount(const std::string& camera_id) const;

    /**
     * @brief 获取当前帧率
     */
    double getCurrentFPS() const;

    /**
     * @brief 广播帧数据到所有客户端
     * @param camera_id 摄像头ID
     * @param frame_data JPEG帧数据
     */
    void broadcastFrame(const std::string& camera_id, const std::vector<uint8_t>& frame_data);

    /**
     * @brief 发送帧数据到指定客户端
     * @param client_id 客户端ID
     * @param frame_data JPEG帧数据
     * @return 是否发送成功
     */
    bool sendFrameToClient(const std::string& client_id, const std::vector<uint8_t>& frame_data);

private:
    // 私有构造函数，单例模式
    WebSocketCameraStreamer();
    ~WebSocketCameraStreamer();

    // 禁止拷贝和赋值
    WebSocketCameraStreamer(const WebSocketCameraStreamer&) = delete;
    WebSocketCameraStreamer& operator=(const WebSocketCameraStreamer&) = delete;

    /**
     * @brief 处理摄像头帧
     * @param frame 摄像头帧
     */
    void handleFrame(const camera::Frame& frame);

    /**
     * @brief 编码帧为JPEG
     * @param frame 原始帧
     * @param jpeg_data 输出的JPEG数据
     * @return 是否编码成功
     */
    bool encodeToJpeg(const camera::Frame& frame, std::vector<uint8_t>& jpeg_data);

    /**
     * @brief 更新FPS统计
     */
    void updateFPS();

    /**
     * @brief 清理非活跃客户端
     */
    void cleanupInactiveClients();

    // 配置
    WebSocketCameraStreamerConfig config_;
    // 是否已初始化
    bool is_initialized_;
    // 是否正在运行
    std::atomic<bool> is_running_;
    // Crow服务器实例
    std::shared_ptr<CrowServer> crow_server_;
    
    // 客户端管理
    mutable std::mutex clients_mutex_;
    std::unordered_map<std::string, std::shared_ptr<WebSocketCameraClient>> clients_;
    std::unordered_map<std::string, std::unordered_set<std::string>> camera_clients_;
    
    // 帧率统计
    std::atomic<double> current_fps_;
    std::atomic<int> frame_count_;
    std::chrono::steady_clock::time_point last_fps_time_;
    
    // 清理线程
    std::thread cleanup_thread_;
    std::atomic<bool> cleanup_running_;
};

} // namespace api
} // namespace cam_server
