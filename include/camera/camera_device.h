#ifndef CAMERA_DEVICE_H
#define CAMERA_DEVICE_H

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace cam_server {
namespace camera {

/**
 * @brief 像素格式枚举
 */
enum class PixelFormat {
    UNKNOWN,
    YUYV,
    MJPEG,
    H264,
    NV12,
    RGB24,
    BGR24
};

/**
 * @brief 帧结构体，表示一帧图像数据
 */
struct Frame {
    // 图像数据
    std::vector<uint8_t> data;
    // 图像宽度
    int width;
    // 图像高度
    int height;
    // 像素格式
    PixelFormat format;
    // 时间戳（微秒）
    int64_t timestamp;
};

/**
 * @brief 摄像头参数结构体
 */
struct CameraParams {
    // 图像宽度
    int width;
    // 图像高度
    int height;
    // 帧率
    int fps;
    // 像素格式
    PixelFormat format;
    // 亮度（0-100）
    int brightness;
    // 对比度（0-100）
    int contrast;
    // 饱和度（0-100）
    int saturation;
    // 曝光（-100-100）
    int exposure;
};

/**
 * @brief 摄像头设备信息结构体
 */
struct CameraDeviceInfo {
    // 设备路径
    std::string device_path;
    // 设备名称
    std::string device_name;
    // 设备描述
    std::string description;
    // 支持的分辨率列表
    std::vector<std::pair<int, int>> supported_resolutions;
    // 支持的帧率列表
    std::vector<int> supported_fps;
    // 支持的像素格式列表
    std::vector<PixelFormat> supported_formats;
};

/**
 * @brief 摄像头设备接口类
 */
class CameraDevice {
public:
    /**
     * @brief 析构函数
     */
    virtual ~CameraDevice() = default;

    /**
     * @brief 打开摄像头设备
     * @param device_path 设备路径
     * @param width 图像宽度
     * @param height 图像高度
     * @param fps 帧率
     * @return 是否成功打开设备
     */
    virtual bool open(const std::string& device_path, int width, int height, int fps) = 0;

    /**
     * @brief 关闭摄像头设备
     * @return 是否成功关闭设备
     */
    virtual bool close() = 0;

    /**
     * @brief 检查设备是否已打开
     * @return 设备是否已打开
     */
    virtual bool isOpen() const = 0;

    /**
     * @brief 开始捕获视频帧
     * @return 是否成功开始捕获
     */
    virtual bool startCapture() = 0;

    /**
     * @brief 停止捕获视频帧
     * @return 是否成功停止捕获
     */
    virtual bool stopCapture() = 0;

    /**
     * @brief 检查是否正在捕获
     * @return 是否正在捕获
     */
    virtual bool isCapturing() const = 0;

    /**
     * @brief 获取一帧图像
     * @param timeout_ms 超时时间（毫秒）
     * @return 帧数据
     */
    virtual Frame getFrame(int timeout_ms = 1000) = 0;

    /**
     * @brief 设置帧回调函数
     * @param callback 帧回调函数
     */
    virtual void setFrameCallback(std::function<void(const Frame&)> callback) = 0;

    /**
     * @brief 获取设备信息
     * @return 设备信息
     */
    virtual CameraDeviceInfo getDeviceInfo() const = 0;

    /**
     * @brief 获取当前参数
     * @return 当前参数
     */
    virtual CameraParams getParams() const = 0;

    /**
     * @brief 设置参数
     * @param params 参数
     * @return 是否成功设置参数
     */
    virtual bool setParams(const CameraParams& params) = 0;
};

/**
 * @brief 创建V4L2摄像头设备
 * @return V4L2摄像头设备指针
 */
std::shared_ptr<CameraDevice> createV4L2CameraDevice();

/**
 * @brief 扫描系统中可用的摄像头设备
 * @return 可用摄像头设备列表
 */
std::vector<CameraDeviceInfo> scanCameraDevices();

} // namespace camera
} // namespace cam_server

#endif // CAMERA_DEVICE_H
