#include "camera/camera_manager.h"
#include "camera/v4l2_camera.h"
#include "monitor/logger.h"
#include "utils/config_manager.h"
#include <sstream>
#include <typeinfo>  // for typeid
#include <atomic>    // for std::atomic
#include <chrono>    // for std::chrono
#include <thread>    // for std::thread

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
    LOG_DEBUG("开始初始化摄像头管理器...", "CameraManager");

    // 从配置文件加载配置 - 在获取互斥锁之前调用
    LOG_DEBUG("正在加载配置文件...", "CameraManager");
    auto& config = utils::ConfigManager::getInstance();
    if (!config.initialize(config_path)) {
        LOG_ERROR("无法加载配置文件: " + config_path, "CameraManager");
        LOG_ERROR("无法加载配置文件: " + config_path, "CameraManager");
        return false;
    }
    LOG_DEBUG("配置文件加载成功", "CameraManager");

    // 获取互斥锁
    LOG_DEBUG("正在获取互斥锁...", "CameraManager");
    std::lock_guard<std::mutex> lock(device_mutex_);
    LOG_DEBUG("获取互斥锁成功", "CameraManager");

    LOG_DEBUG("摄像头管理器初始化成功", "CameraManager");
    LOG_INFO("摄像头管理器初始化成功", "CameraManager");
    return true;
}

std::vector<CameraDeviceInfo> CameraManager::scanDevices() {
    // 创建一个临时的V4L2摄像头对象来扫描设备
    auto v4l2Camera = std::make_shared<V4L2Camera>();
    return v4l2Camera->scanDevices();
}

bool CameraManager::openDevice(const std::string& device_path, int width, int height, int fps) {
    std::stringstream ss;
    ss << "开始打开摄像头设备: " << device_path
       << ", 分辨率: " << width << "x" << height
       << ", 帧率: " << fps;
    LOG_DEBUG(ss.str(), "CameraManager");

    std::lock_guard<std::mutex> lock(device_mutex_);
    LOG_DEBUG("获取互斥锁成功", "CameraManager");

    // 检查是否有已打开的设备
    if (current_device_ && current_device_->isOpen()) {
        LOG_DEBUG("已有打开的设备，尝试关闭...", "CameraManager");

        // 为了避免可能的死锁，我们先将当前设备置为nullptr，然后在后台关闭它
        std::shared_ptr<CameraDevice> old_device = current_device_;
        current_device_ = nullptr;

        // 在后台线程中关闭设备
        std::thread close_thread([old_device]() {
            LOG_DEBUG("在后台线程中关闭设备...", "CameraManager");
            old_device->close();
            LOG_DEBUG("设备关闭完成", "CameraManager");
        });
        close_thread.detach();  // 分离线程，让它在后台运行

        LOG_DEBUG("已在后台启动设备关闭线程", "CameraManager");
    } else {
        LOG_DEBUG("没有已打开的设备", "CameraManager");
    }

    // 创建新的摄像头设备
    LOG_DEBUG("正在创建V4L2摄像头设备...", "CameraManager");
    current_device_ = createV4L2CameraDevice();
    LOG_DEBUG(std::string("V4L2摄像头设备创建") + (current_device_ ? "成功" : "失败"), "CameraManager");

    // 打开设备
    LOG_DEBUG("正在打开设备...", "CameraManager");
    LOG_DEBUG(std::string("设备类型: ") + typeid(*current_device_).name(), "CameraManager");

    // 尝试将current_device_转换为V4L2Camera
    auto v4l2_camera = std::dynamic_pointer_cast<V4L2Camera>(current_device_);
    if (v4l2_camera) {
        LOG_DEBUG("成功将设备转换为V4L2Camera", "CameraManager");
    } else {
        LOG_WARNING("无法将设备转换为V4L2Camera", "CameraManager");
    }

    // 添加超时机制
    LOG_DEBUG("尝试打开摄像头设备，添加超时机制...", "CameraManager");

    // 创建一个线程来执行打开设备的操作
    std::atomic<bool> open_success(false);
    std::atomic<bool> open_completed(false);

    std::thread open_thread([&]() {
        LOG_DEBUG("打开设备线程开始执行...", "CameraManager");
        bool result = current_device_->open(device_path, width, height, fps);
        open_success.store(result);
        open_completed.store(true);
        LOG_DEBUG(std::string("打开设备线程执行完成，结果: ") + (result ? "成功" : "失败"), "CameraManager");
    });

    // 等待打开设备操作完成或超时
    const int timeout_seconds = 5;
    std::stringstream timeout_ss;
    timeout_ss << "等待打开设备操作完成，超时时间: " << timeout_seconds << "秒...";
    LOG_DEBUG(timeout_ss.str(), "CameraManager");

    auto start_time = std::chrono::steady_clock::now();
    while (!open_completed.load()) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

        if (elapsed_time >= timeout_seconds) {
            LOG_ERROR("打开设备操作超时!", "CameraManager");
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 如果线程仍在运行，分离它
    if (!open_completed.load()) {
        LOG_DEBUG("打开设备操作未完成，分离线程...", "CameraManager");
        open_thread.detach();
        LOG_ERROR("打开摄像头设备超时: " + device_path, "CameraManager");
        current_device_ = nullptr;
        return false;
    } else {
        open_thread.join();

        if (!open_success.load()) {
            LOG_ERROR("无法打开摄像头设备: " + device_path, "CameraManager");
            current_device_ = nullptr;
            return false;
        }
    }

    // 设置帧回调
    LOG_DEBUG("正在设置帧回调...", "CameraManager");
    current_device_->setFrameCallback([this](const Frame& frame) {
        if (frame_callback_) {
            frame_callback_(frame);
        }
    });
    LOG_DEBUG("帧回调设置成功", "CameraManager");

    LOG_INFO("成功打开摄像头设备: " + device_path, "CameraManager");
    return true;
}

bool CameraManager::closeDevice() {
    LOG_DEBUG("开始关闭设备...", "CameraManager");

    std::lock_guard<std::mutex> lock(device_mutex_);
    LOG_DEBUG("获取互斥锁成功", "CameraManager");

    // 停止捕获
    if (is_capturing_) {
        LOG_DEBUG("设备正在捕获中，尝试停止捕获...", "CameraManager");

        // 添加超时机制
        std::atomic<bool> stop_completed(false);

        std::thread stop_thread([&]() {
            LOG_DEBUG("停止捕获线程开始执行...", "CameraManager");
            bool result = stopCapture();
            LOG_DEBUG(std::string("停止捕获结果: ") + (result ? "成功" : "失败"), "CameraManager");
            stop_completed.store(true);
        });

        // 等待停止捕获操作完成或超时
        const int timeout_seconds = 5; // 增加超时时间到5秒
        std::stringstream stop_timeout_ss;
        stop_timeout_ss << "等待停止捕获操作完成，超时时间: " << timeout_seconds << "秒...";
        LOG_DEBUG(stop_timeout_ss.str(), "CameraManager");

        auto start_time = std::chrono::steady_clock::now();
        while (!stop_completed.load()) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

            if (elapsed_time >= timeout_seconds) {
                LOG_ERROR("停止捕获操作超时!", "CameraManager");
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 如果线程仍在运行，分离它
        if (!stop_completed.load()) {
            LOG_DEBUG("停止捕获操作未完成，分离线程...", "CameraManager");
            stop_thread.detach();
            LOG_DEBUG("强制设置捕获状态为false", "CameraManager");
            is_capturing_ = false;
        } else {
            stop_thread.join();
        }
    } else {
        LOG_DEBUG("设备未在捕获中", "CameraManager");
    }

    // 关闭设备
    if (current_device_) {
        LOG_DEBUG("尝试关闭设备...", "CameraManager");

        // 添加超时机制
        std::atomic<bool> close_completed(false);
        std::atomic<bool> close_result(false);

        std::thread close_thread([&]() {
            LOG_DEBUG("关闭设备线程开始执行...", "CameraManager");
            bool result = current_device_->close();
            close_result.store(result);
            close_completed.store(true);
            LOG_DEBUG(std::string("关闭设备结果: ") + (result ? "成功" : "失败"), "CameraManager");
        });

        // 等待关闭设备操作完成或超时
        const int timeout_seconds = 5; // 增加超时时间到5秒
        std::stringstream close_timeout_ss;
        close_timeout_ss << "等待关闭设备操作完成，超时时间: " << timeout_seconds << "秒...";
        LOG_DEBUG(close_timeout_ss.str(), "CameraManager");

        auto start_time = std::chrono::steady_clock::now();
        while (!close_completed.load()) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

            if (elapsed_time >= timeout_seconds) {
                LOG_ERROR("关闭设备操作超时!", "CameraManager");
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 如果线程仍在运行，分离它
        if (!close_completed.load()) {
            LOG_DEBUG("关闭设备操作未完成，分离线程...", "CameraManager");
            close_thread.detach();
            current_device_ = nullptr;
            LOG_INFO("关闭摄像头设备（超时）", "CameraManager");
            LOG_DEBUG("设备关闭完成（超时）", "CameraManager");
            return false;
        } else {
            close_thread.join();
            current_device_ = nullptr;
            LOG_INFO("关闭摄像头设备", "CameraManager");
            LOG_DEBUG("设备关闭完成", "CameraManager");
            return close_result.load();
        }
    } else {
        LOG_DEBUG("没有打开的设备", "CameraManager");
    }

    LOG_DEBUG("设备关闭完成", "CameraManager");
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
