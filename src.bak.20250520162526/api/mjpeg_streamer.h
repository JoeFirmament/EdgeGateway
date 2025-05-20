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
    static MjpegStreamer& getInstance();

    bool initialize(const MjpegStreamerConfig& config);
    bool start();
    bool stop();

    bool addClient(const std::string& client_id,
                  const std::string& camera_id,
                  std::function<void(const std::vector<uint8_t>&)> frame_callback,
                  std::function<void(const std::string&)> error_callback = nullptr,
                  std::function<void()> close_callback = nullptr);

    bool removeClient(const std::string& client_id);
    int getClientCount() const;
    double getCurrentFps() const;
    bool isClientConnected(const std::string& client_id) const;

private:
    MjpegStreamer();
    ~MjpegStreamer();

    void handleFrame(const camera::Frame& frame);
    bool encodeToJpeg(const camera::Frame& frame, std::vector<uint8_t>& jpeg_data);
    bool resizeFrame(const camera::Frame& frame, camera::Frame& resized_frame);

    bool is_initialized_;
    bool is_running_;
    MjpegStreamerConfig config_;
    std::atomic<double> current_fps_;
    int frame_count_;
    std::chrono::steady_clock::time_point last_fps_time_;

    mutable std::mutex clients_mutex_;
    std::unordered_map<std::string, std::shared_ptr<MjpegClient>> clients_;
    std::unordered_map<std::string, std::unordered_set<std::string>> camera_clients_;
};
} // namespace api
} // namespace cam_server 