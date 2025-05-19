#ifndef V4L2_CAMERA_H
#define V4L2_CAMERA_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <linux/videodev2.h>

#include "camera_device.h"
#include "camera/format_utils.h"

namespace cam_server {
namespace camera {

/**
 * @brief V4L2摄像头设备实现类
 */
class V4L2Camera : public CameraDevice {
public:
    /**
     * @brief 构造函数
     */
    V4L2Camera();

    /**
     * @brief 析构函数
     */
    ~V4L2Camera() override;

    /**
     * @brief 打开摄像头设备
     * @param device_path 设备路径
     * @param width 图像宽度
     * @param height 图像高度
     * @param fps 帧率
     * @return 是否成功打开设备
     */
    bool open(const std::string& device_path, int width, int height, int fps) override;

    /**
     * @brief 关闭摄像头设备
     * @return 是否成功关闭设备
     */
    bool close() override;

    /**
     * @brief 检查设备是否已打开
     * @return 设备是否已打开
     */
    bool isOpen() const override;

    /**
     * @brief 开始捕获视频帧
     * @return 是否成功开始捕获
     */
    bool startCapture() override;

    /**
     * @brief 停止捕获视频帧
     * @return 是否成功停止捕获
     */
    bool stopCapture() override;

    /**
     * @brief 检查是否正在捕获
     * @return 是否正在捕获
     */
    bool isCapturing() const override;

    /**
     * @brief 获取一帧图像
     * @param timeout_ms 超时时间（毫秒）
     * @return 帧数据
     */
    Frame getFrame(int timeout_ms = 1000) override;

    /**
     * @brief 设置帧回调函数
     * @param callback 帧回调函数
     */
    void setFrameCallback(std::function<void(const Frame&)> callback) override;

    /**
     * @brief 获取设备信息
     * @return 设备信息
     */
    CameraDeviceInfo getDeviceInfo() const override;

    /**
     * @brief 获取当前参数
     * @return 当前参数
     */
    CameraParams getParams() const override;

    /**
     * @brief 设置参数
     * @param params 参数
     * @return 是否成功设置参数
     */
    bool setParams(const CameraParams& params) override;

    /**
     * @brief 扫描系统中的摄像头设备
     * @return 设备信息列表
     */
    std::vector<CameraDeviceInfo> scanDevices();

private:
    // 初始化设备
    bool initDevice();
    // 初始化内存映射
    bool initMmap();
    // 释放内存映射
    void freeMmap();
    // 查询设备能力
    bool queryCapabilities();

    // 查询设备支持的格式和分辨率
    void queryCapabilities(int fd, CameraDeviceInfo& deviceInfo);
    // 设置视频格式
    bool setVideoFormat(int width, int height, uint32_t pixelformat);
    // 设置帧率
    bool setFrameRate(int fps);
    // 启动视频流
    bool startStreaming();
    // 停止视频流
    bool stopStreaming();
    // 捕获线程函数
    void captureThreadFunc();
    // 将V4L2格式转换为PixelFormat
    PixelFormat v4l2FormatToPixelFormat(uint32_t v4l2_format) const;
    // 将PixelFormat转换为V4L2格式
    uint32_t pixelFormatToV4L2Format(PixelFormat format) const;
    // 处理捕获的帧
    void processFrame(const void* data, size_t size, const struct v4l2_buffer& buf);

    // 设备文件描述符
    int fd_;
    // 设备路径
    std::string device_path_;
    // 设备信息
    CameraDeviceInfo device_info_;
    // 当前参数
    CameraParams current_params_;
    // 是否已打开
    bool is_open_;
    // 是否正在捕获
    std::atomic<bool> is_capturing_;
    // 捕获线程
    std::thread capture_thread_;
    // 停止捕获标志
    std::atomic<bool> stop_flag_;
    // 帧回调函数
    std::function<void(const Frame&)> frame_callback_;
    // 帧队列
    std::queue<Frame> frame_queue_;
    // 帧队列互斥锁
    std::mutex frame_queue_mutex_;
    // 帧队列条件变量
    std::condition_variable frame_queue_cond_;
    // 内存映射缓冲区
    struct Buffer {
        void* start;
        size_t length;
    };
    // 缓冲区数组
    std::vector<Buffer> buffers_;
};

} // namespace camera
} // namespace cam_server

#endif // V4L2_CAMERA_H
