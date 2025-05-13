#ifndef MJPEG_STREAMER_H
#define MJPEG_STREAMER_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <queue>
#include <condition_variable>

#include "camera/camera_device.h"

namespace cam_server {
namespace api {

/**
 * @brief MJPEG流配置结构体
 */
struct MjpegStreamerConfig {
    // JPEG质量（1-100）
    int jpeg_quality;
    // 最大帧率
    int max_fps;
    // 最大客户端数量
    int max_clients;
    // 输出宽度（0表示使用原始宽度）
    int output_width;
    // 输出高度（0表示使用原始高度）
    int output_height;
};

/**
 * @brief MJPEG流客户端结构体
 */
struct MjpegClient {
    // 客户端ID
    std::string id;
    // 帧回调函数
    std::function<void(const std::vector<uint8_t>&)> frame_callback;
    // 错误回调函数
    std::function<void(const std::string&)> error_callback;
    // 关闭回调函数
    std::function<void()> close_callback;
    // 上次发送帧的时间戳
    int64_t last_frame_time;
};

/**
 * @brief MJPEG流处理器类
 */
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
     * @param frame_callback 帧回调函数
     * @param error_callback 错误回调函数
     * @param close_callback 关闭回调函数
     * @return 是否成功添加
     */
    bool addClient(const std::string& client_id,
                  std::function<void(const std::vector<uint8_t>&)> frame_callback,
                  std::function<void(const std::string&)> error_callback,
                  std::function<void()> close_callback);

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

private:
    // 私有构造函数，防止外部创建实例
    MjpegStreamer();
    // 禁止拷贝构造和赋值操作
    MjpegStreamer(const MjpegStreamer&) = delete;
    MjpegStreamer& operator=(const MjpegStreamer&) = delete;
    // 析构函数
    ~MjpegStreamer();

    // 处理摄像头帧
    void handleFrame(const camera::Frame& frame);
    // 将帧编码为JPEG
    bool encodeToJpeg(const camera::Frame& frame, std::vector<uint8_t>& jpeg_data);
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

#endif // MJPEG_STREAMER_H
