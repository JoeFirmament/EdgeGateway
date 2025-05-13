#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>

#include "camera_device.h"

namespace cam_server {
namespace camera {

/**
 * @brief 摄像头管理器类，负责管理摄像头设备
 */
class CameraManager {
public:
    /**
     * @brief 获取CameraManager单例
     * @return CameraManager单例的引用
     */
    static CameraManager& getInstance();

    /**
     * @brief 初始化摄像头管理器
     * @param config_path 配置文件路径
     * @return 是否初始化成功
     */
    bool initialize(const std::string& config_path);

    /**
     * @brief 扫描可用的摄像头设备
     * @return 可用摄像头设备列表
     */
    std::vector<CameraDeviceInfo> scanDevices();

    /**
     * @brief 打开摄像头设备
     * @param device_path 设备路径
     * @param width 图像宽度
     * @param height 图像高度
     * @param fps 帧率
     * @return 是否成功打开设备
     */
    bool openDevice(const std::string& device_path, int width, int height, int fps);

    /**
     * @brief 关闭当前打开的摄像头设备
     * @return 是否成功关闭设备
     */
    bool closeDevice();

    /**
     * @brief 检查摄像头是否已打开
     * @return 摄像头是否已打开
     */
    bool isDeviceOpen() const;

    /**
     * @brief 获取当前打开的摄像头设备
     * @return 摄像头设备指针
     */
    std::shared_ptr<CameraDevice> getCurrentDevice();

    /**
     * @brief 设置帧回调函数
     * @param callback 帧回调函数
     */
    void setFrameCallback(std::function<void(const Frame&)> callback);

    /**
     * @brief 开始捕获视频帧
     * @return 是否成功开始捕获
     */
    bool startCapture();

    /**
     * @brief 停止捕获视频帧
     * @return 是否成功停止捕获
     */
    bool stopCapture();

    /**
     * @brief 检查是否正在捕获
     * @return 是否正在捕获
     */
    bool isCapturing() const;

    /**
     * @brief 获取当前摄像头的参数
     * @return 摄像头参数
     */
    CameraParams getCurrentParams() const;

    /**
     * @brief 设置摄像头参数
     * @param params 摄像头参数
     * @return 是否成功设置参数
     */
    bool setParams(const CameraParams& params);

private:
    // 私有构造函数，防止外部创建实例
    CameraManager();
    // 禁止拷贝构造和赋值操作
    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;

    // 当前打开的摄像头设备
    std::shared_ptr<CameraDevice> current_device_;
    // 互斥锁，保护对摄像头设备的访问
    mutable std::mutex device_mutex_;
    // 是否正在捕获
    bool is_capturing_;
    // 帧回调函数
    std::function<void(const Frame&)> frame_callback_;
};

} // namespace camera
} // namespace cam_server

#endif // CAMERA_MANAGER_H
