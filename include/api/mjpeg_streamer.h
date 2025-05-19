#pragma once

#include "camera/frame.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <atomic>

namespace cam_server {
namespace api {

// MJPEG流配置
struct MjpegStreamerConfig {
    int jpeg_quality = 80;        // JPEG质量 (1-100)
    int max_fps = 30;            // 最大帧率
    int max_clients = 2;         // 最大客户端数量
    int output_width = 0;        // 输出宽度 (0表示使用原始尺寸)
    int output_height = 0;       // 输出高度 (0表示使用原始尺寸)
};

// MJPEG客户端信息
struct MjpegClient {
    std::string id;                                                    // 客户端ID
    std::string camera_id;                                            // 关联的摄像头ID
    std::function<void(const std::vector<uint8_t>&)> frame_callback;  // 帧回调函数
    std::function<void(const std::string&)> error_callback;           // 错误回调函数
    std::function<void()> close_callback;                             // 关闭回调函数
    int64_t last_frame_time;                                          // 最后一帧时间
    int64_t last_activity_time;                                       // 最后活动时间
};

class MjpegStreamer {
public:
    /**
     * @brief 获取MjpegStreamer单例
     * @return MjpegStreamer单例的引用
     */
    static MjpegStreamer& getInstance();

    /**
     * @brief 初始化MJPEG流处理器
     * @param config 配置
     * @return 是否初始化成功
     */
    bool initialize(const MjpegStreamerConfig& config);

    /**
     * @brief 启动MJPEG流处理器
     * @return 是否成功启动
     */
    bool start();

    /**
     * @brief 停止MJPEG流处理器
     * @return 是否成功停止
     */
    bool stop();

    /**
     * @brief 添加客户端
     * @param client_id 客户端ID
     * @param camera_id 摄像头ID
     * @param frame_callback 帧回调函数
     * @param error_callback 错误回调函数
     * @param close_callback 关闭回调函数
     * @return 是否成功添加
     */
    bool addClient(const std::string& client_id,
                  const std::string& camera_id,
                  std::function<void(const std::vector<uint8_t>&)> frame_callback,
                  std::function<void(const std::string&)> error_callback = nullptr,
                  std::function<void()> close_callback = nullptr);

    /**
     * @brief 移除客户端
     * @param client_id 客户端ID
     * @return 是否成功移除
     */
    bool removeClient(const std::string& client_id);

    /**
     * @brief 获取客户端数量
     * @return 客户端数量
     */
    int getClientCount() const;

    /**
     * @brief 获取当前帧率
     * @return 当前帧率
     */
    double getCurrentFps() const;

    /**
     * @brief 获取运行状态
     * @return 是否正在运行
     */
    /**
     * @brief 检查客户端是否连接
     * @param client_id 客户端ID
     * @return 是否连接
     */
    bool isClientConnected(const std::string& client_id) const;

    /**
     * @brief 获取运行状态
     * @return 是否正在运行
     */
    bool isRunning() const {
        return is_running_.load();
    }

    /**
     * @brief 获取活跃客户端列表
     * @return 活跃客户端ID列表
     */
    std::vector<std::string> getActiveClients() const {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        std::vector<std::string> active_clients;
        for (const auto& pair : clients_) {
            active_clients.push_back(pair.first);
        }
        return active_clients;
    }

    /**
     * @brief 将帧编码为JPEG
     * @param frame 输入帧
     * @param jpeg_data 输出JPEG数据
     * @return 是否成功编码
     */
    bool encodeToJpeg(const camera::Frame& frame, std::vector<uint8_t>& jpeg_data);

    // 处理帧数据
    void handleFrame(const camera::Frame& frame);

private:
    // 私有构造函数，防止外部创建实例
    MjpegStreamer();
    // 禁止拷贝构造和赋值操作
    MjpegStreamer(const MjpegStreamer&) = delete;
    MjpegStreamer& operator=(const MjpegStreamer&) = delete;
    // 析构函数
    ~MjpegStreamer();

    // 调整帧大小
    bool resizeFrame(const camera::Frame& frame, camera::Frame& resized_frame);

    // 配置
    MjpegStreamerConfig config_;
    // 是否已初始化
    bool is_initialized_;
    // 是否正在运行
    std::atomic<bool> is_running_;
    // 客户端列表
    std::unordered_map<std::string, std::shared_ptr<MjpegClient>> clients_;
    // 摄像头客户端映射（摄像头ID -> 客户端ID集合）
    std::unordered_map<std::string, std::unordered_set<std::string>> camera_clients_;
    // 客户端互斥锁
    mutable std::mutex clients_mutex_;
    // 当前帧率
    std::atomic<double> current_fps_;
    // 帧计数
    std::atomic<int> frame_count_;
    // 上次计算帧率的时间
    std::chrono::steady_clock::time_point last_fps_time_;
};

} // namespace api
} // namespace cam_server
