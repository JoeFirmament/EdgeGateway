#include "camera/camera_manager.h"
#include "camera/v4l2_camera.h"
#include "monitor/logger.h"
#include "utils/config_manager.h"

namespace cam_server {
namespace camera {

// 单例实例
CameraManager& CameraManager::getInstance() {
    static CameraManager instance;
    return instance;
}

CameraManager::CameraManager()
    : current_device_(nullptr),
      is_capturing_(false) {
}

bool CameraManager::initialize(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(device_mutex_);
    
    // 从配置文件加载配置
    auto& config = utils::ConfigManager::getInstance();
    if (!config.initialize(config_path)) {
        LOG_ERROR("无法加载配置文件: " + config_path, "CameraManager");
        return false;
    }
    
    LOG_INFO("摄像头管理器初始化成功", "CameraManager");
    return true;
}

std::vector<CameraDeviceInfo> CameraManager::scanDevices() {
    // 创建一个临时的V4L2摄像头对象来扫描设备
    auto v4l2Camera = std::make_shared<V4L2Camera>();
    return v4l2Camera->scanDevices();
}

bool CameraManager::openDevice(const std::string& device_path, int width, int height, int fps) {
    std::lock_guard<std::mutex> lock(device_mutex_);
    
    // 关闭已打开的设备
    closeDevice();
    
    // 创建新的摄像头设备
    current_device_ = createV4L2CameraDevice();
    
    // 打开设备
    if (!current_device_->open(device_path, width, height, fps)) {
        LOG_ERROR("无法打开摄像头设备: " + device_path, "CameraManager");
        current_device_ = nullptr;
        return false;
    }
    
    // 设置帧回调
    current_device_->setFrameCallback([this](const Frame& frame) {
        if (frame_callback_) {
            frame_callback_(frame);
        }
    });
    
    LOG_INFO("成功打开摄像头设备: " + device_path, "CameraManager");
    return true;
}

bool CameraManager::closeDevice() {
    std::lock_guard<std::mutex> lock(device_mutex_);
    
    // 停止捕获
    if (is_capturing_) {
        stopCapture();
    }
    
    // 关闭设备
    if (current_device_) {
        bool result = current_device_->close();
        current_device_ = nullptr;
        LOG_INFO("关闭摄像头设备", "CameraManager");
        return result;
    }
    
    return true;
}

bool CameraManager::isDeviceOpen() const {
    std::lock_guard<std::mutex> lock(device_mutex_);
    return current_device_ && current_device_->isOpen();
}

std::shared_ptr<CameraDevice> CameraManager::getCurrentDevice() {
    std::lock_guard<std::mutex> lock(device_mutex_);
    return current_device_;
}

void CameraManager::setFrameCallback(std::function<void(const Frame&)> callback) {
    std::lock_guard<std::mutex> lock(device_mutex_);
    frame_callback_ = callback;
    
    if (current_device_) {
        current_device_->setFrameCallback([this](const Frame& frame) {
            if (frame_callback_) {
                frame_callback_(frame);
            }
        });
    }
}

bool CameraManager::startCapture() {
    std::lock_guard<std::mutex> lock(device_mutex_);
    
    if (!current_device_) {
        LOG_ERROR("没有打开的摄像头设备", "CameraManager");
        return false;
    }
    
    if (is_capturing_) {
        return true;  // 已经在捕获中
    }
    
    if (!current_device_->startCapture()) {
        LOG_ERROR("无法开始捕获", "CameraManager");
        return false;
    }
    
    is_capturing_ = true;
    LOG_INFO("开始捕获视频帧", "CameraManager");
    return true;
}

bool CameraManager::stopCapture() {
    std::lock_guard<std::mutex> lock(device_mutex_);
    
    if (!is_capturing_) {
        return true;  // 没有在捕获
    }
    
    if (!current_device_) {
        LOG_ERROR("没有打开的摄像头设备", "CameraManager");
        return false;
    }
    
    if (!current_device_->stopCapture()) {
        LOG_ERROR("无法停止捕获", "CameraManager");
        return false;
    }
    
    is_capturing_ = false;
    LOG_INFO("停止捕获视频帧", "CameraManager");
    return true;
}

bool CameraManager::isCapturing() const {
    std::lock_guard<std::mutex> lock(device_mutex_);
    return is_capturing_;
}

CameraParams CameraManager::getCurrentParams() const {
    std::lock_guard<std::mutex> lock(device_mutex_);
    
    if (!current_device_) {
        LOG_ERROR("没有打开的摄像头设备", "CameraManager");
        return CameraParams();
    }
    
    return current_device_->getParams();
}

bool CameraManager::setParams(const CameraParams& params) {
    std::lock_guard<std::mutex> lock(device_mutex_);
    
    if (!current_device_) {
        LOG_ERROR("没有打开的摄像头设备", "CameraManager");
        return false;
    }
    
    return current_device_->setParams(params);
}

// 创建V4L2摄像头设备的工厂函数实现
std::shared_ptr<CameraDevice> createV4L2CameraDevice() {
    return std::make_shared<V4L2Camera>();
}

// 扫描系统中可用的摄像头设备
std::vector<CameraDeviceInfo> scanCameraDevices() {
    return CameraManager::getInstance().scanDevices();
}

} // namespace camera
} // namespace cam_server
