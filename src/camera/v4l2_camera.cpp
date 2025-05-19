#include "camera/v4l2_camera.h"
#include "monitor/logger.h"
#include "utils/string_utils.h"
#include "utils/file_utils.h"
#include "camera/format_utils.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <algorithm>
#include <set>
#include <chrono>
#include <iostream>  // 添加iostream头文件，用于std::cerr
#include <cstring>   // 添加cstring头文件，用于memcpy

namespace cam_server {
namespace camera {

V4L2Camera::V4L2Camera()
    : fd_(-1),
      is_open_(false),
      is_capturing_(false),
      stop_flag_(false) {
}

V4L2Camera::~V4L2Camera() {
    close();
}

std::vector<CameraDeviceInfo> V4L2Camera::scanDevices() {
    std::vector<CameraDeviceInfo> devices;

    // 打开/dev目录
    DIR* dir = opendir("/dev");
    if (!dir) {
        LOG_ERROR("无法打开/dev目录", "V4L2Camera");
        return devices;
    }

    // 遍历/dev目录，查找video设备
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;

        // 检查是否为video设备
        if (name.find("video") == 0) {
            std::string devicePath = "/dev/" + name;

            // 尝试打开设备
            int fd = ::open(devicePath.c_str(), O_RDWR);
            if (fd < 0) {
                continue;  // 无法打开，跳过
            }

            // 获取设备信息
            struct v4l2_capability cap;
            if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
                ::close(fd);
                continue;  // 无法获取设备信息，跳过
            }

            // 检查是否为视频捕获设备
            if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
                ::close(fd);
                continue;  // 不是视频捕获设备，跳过
            }

            // 创建设备信息
            CameraDeviceInfo deviceInfo;
            deviceInfo.device_path = devicePath;
            deviceInfo.device_name = reinterpret_cast<const char*>(cap.card);
            deviceInfo.description = reinterpret_cast<const char*>(cap.driver);

            // 查询设备支持的格式
            queryCapabilities(fd, deviceInfo);

            devices.push_back(deviceInfo);
            ::close(fd);
        }
    }

    closedir(dir);
    return devices;
}

bool V4L2Camera::open(const std::string& device_path, int width, int height, int fps) {
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 开始打开设备: " << device_path
              << ", 分辨率: " << width << "x" << height
              << ", 帧率: " << fps << std::endl;

    // 关闭已打开的设备
    if (is_open_) {
        std::cerr << "[V4L2][v4l2_camera.cpp:open] 关闭已打开的设备" << std::endl;
        close();
    }

    // 打开新设备
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 尝试打开设备: " << device_path << std::endl;
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 当前工作目录: " << utils::FileUtils::getCurrentWorkingDirectory() << std::endl;
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 当前用户: " << getenv("USER") << std::endl;
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 当前用户ID: " << getuid() << std::endl;
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 当前组ID: " << getgid() << std::endl;
    
    // 打印部分关键环境变量
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 关键环境变量:" << std::endl;
    const char* env_vars[] = {"PATH", "LD_LIBRARY_PATH", "USER", "HOME", "SHELL", nullptr};
    for (const char** var = env_vars; *var != nullptr; var++) {
        const char* value = getenv(*var);
        if (value) {
            std::cerr << "  " << *var << "=" << value << std::endl;
        }
    }

    // 检查设备文件是否存在
    struct stat st;
    if (stat(device_path.c_str(), &st) != 0) {
        std::cerr << "[V4L2][v4l2_camera.cpp:open] 设备文件不存在: " << device_path
                  << ", 错误: " << strerror(errno) << std::endl;
        LOG_ERROR("设备文件不存在: " + device_path, "V4L2Camera");
        return false;
    }
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 设备文件存在, 权限: " << std::oct << (st.st_mode & 0777) << std::dec << std::endl;
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 设备文件所有者: " << st.st_uid << ", 组: " << st.st_gid << std::endl;

    std::cerr << "[V4L2][v4l2_camera.cpp:open] 尝试打开设备文件: " << device_path << std::endl;
    std::cerr.flush();

    // 尝试使用不同的标志打开设备
    fd_ = ::open(device_path.c_str(), O_RDWR);
    if (fd_ < 0) {
        std::cerr << "[V4L2][v4l2_camera.cpp:open] 无法打开设备: " << device_path
                  << ", 错误: " << strerror(errno) << ", 错误码: " << errno << std::endl;
        std::cerr.flush();

        // 尝试使用只读模式打开
        std::cerr << "[V4L2][v4l2_camera.cpp:open] 尝试使用只读模式打开设备..." << std::endl;
        std::cerr.flush();
        fd_ = ::open(device_path.c_str(), O_RDONLY);
        if (fd_ < 0) {
            std::cerr << "[V4L2][v4l2_camera.cpp:open] 使用只读模式也无法打开设备: " << device_path
                      << ", 错误: " << strerror(errno) << ", 错误码: " << errno << std::endl;
            std::cerr.flush();
            LOG_ERROR("无法打开设备: " + device_path + ", 错误: " + std::string(strerror(errno)), "V4L2Camera");
            return false;
        }
        std::cerr << "[V4L2][v4l2_camera.cpp:open] 使用只读模式成功打开设备, fd: " << fd_ << std::endl;
        std::cerr.flush();
    } else {
        std::cerr << "[V4L2][v4l2_camera.cpp:open] 设备打开成功, fd: " << fd_ << std::endl;
        std::cerr.flush();
    }

    // 获取设备信息
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 尝试获取设备信息..." << std::endl;
    struct v4l2_capability cap;
    if (ioctl(fd_, VIDIOC_QUERYCAP, &cap) < 0) {
        std::cerr << "[V4L2][v4l2_camera.cpp:open] 无法获取设备信息, 错误: " << strerror(errno) << std::endl;
        LOG_ERROR("无法获取设备信息", "V4L2Camera");
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 设备信息获取成功" << std::endl;

    // 检查是否为视频捕获设备
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 检查是否为视频捕获设备..." << std::endl;
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        std::cerr << "[V4L2][v4l2_camera.cpp:open] 不是视频捕获设备" << std::endl;
        LOG_ERROR("不是视频捕获设备", "V4L2Camera");
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 是视频捕获设备" << std::endl;

    // 设置当前设备信息
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 设置当前设备信息..." << std::endl;
    device_info_.device_path = device_path;
    device_info_.device_name = reinterpret_cast<const char*>(cap.card);
    device_info_.description = reinterpret_cast<const char*>(cap.driver);
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 设备名称: " << device_info_.device_name << std::endl;
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 设备描述: " << device_info_.description << std::endl;

    // 查询设备支持的格式
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 查询设备支持的格式..." << std::endl;
    queryCapabilities(fd_, device_info_);
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 支持的分辨率数量: " << device_info_.supported_resolutions.size() << std::endl;
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 支持的格式数量: " << device_info_.supported_formats.size() << std::endl;

    // 设置视频格式，优先使用MJPEG格式
    if (!setVideoFormat(width, height, V4L2_PIX_FMT_MJPEG)) {
        std::cerr << "[V4L2][v4l2_camera.cpp:open] 无法设置MJPEG格式" << std::endl;
        LOG_ERROR("无法设置MJPEG格式，摄像头可能不支持MJPEG格式", "V4L2Camera");
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    current_params_.format = PixelFormat::MJPEG;
    LOG_INFO("成功设置MJPEG格式", "V4L2Camera");
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 视频格式设置成功" << std::endl;

    // 设置帧率
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 设置帧率: " << fps << std::endl;
    if (!setFrameRate(fps)) {
        std::cerr << "[V4L2][v4l2_camera.cpp:open] 无法设置帧率" << std::endl;
        LOG_ERROR("无法设置帧率", "V4L2Camera");
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 帧率设置成功" << std::endl;

    // 初始化设备
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 初始化设备..." << std::endl;
    if (!initDevice()) {
        std::cerr << "[V4L2][v4l2_camera.cpp:open] 无法初始化设备" << std::endl;
        LOG_ERROR("无法初始化设备", "V4L2Camera");
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 设备初始化成功" << std::endl;

    is_open_ = true;

    // 设置当前参数
    current_params_.width = width;
    current_params_.height = height;
    current_params_.fps = fps;
    current_params_.format = PixelFormat::MJPEG;

    LOG_INFO("成功打开摄像头设备: " + device_path, "V4L2Camera");
    std::cerr << "[V4L2][v4l2_camera.cpp:open] 成功打开摄像头设备: " << device_path << std::endl;
    return true;
}

bool V4L2Camera::close() {
    if (!is_open_) {
        return true;
    }

    // 停止捕获
    if (is_capturing_) {
        stopCapture();
    }

    // 释放内存映射
    freeMmap();

    // 关闭设备
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }

    is_open_ = false;
    LOG_INFO("关闭摄像头设备", "V4L2Camera");
    return true;
}

bool V4L2Camera::isOpen() const {
    return is_open_;
}

bool V4L2Camera::startCapture() {
    if (!is_open_) {
        LOG_ERROR("设备未打开", "V4L2Camera");
        return false;
    }

    if (is_capturing_) {
        return true;  // 已经在捕获中
    }

    // 启动视频流
    if (!startStreaming()) {
        LOG_ERROR("无法启动视频流", "V4L2Camera");
        return false;
    }

    // 设置标志
    stop_flag_ = false;
    is_capturing_ = true;

    // 启动捕获线程
    capture_thread_ = std::thread(&V4L2Camera::captureThreadFunc, this);

    LOG_INFO("开始捕获视频帧", "V4L2Camera");
    return true;
}

bool V4L2Camera::stopCapture() {
    if (!is_capturing_) {
        return true;
    }

    // 设置停止标志
    stop_flag_ = true;

    // 等待捕获线程结束
    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }

    // 停止视频流
    stopStreaming();

    is_capturing_ = false;
    LOG_INFO("停止捕获视频帧", "V4L2Camera");
    return true;
}

bool V4L2Camera::isCapturing() const {
    return is_capturing_;
}

Frame V4L2Camera::getFrame(int timeout_ms) {
    Frame frame;

    if (!is_capturing_) {
        LOG_ERROR("未开始捕获", "V4L2Camera");
        return frame;
    }

    // 等待帧队列中有帧可用
    std::unique_lock<std::mutex> lock(frame_queue_mutex_);
    if (frame_queue_.empty()) {
        auto status = frame_queue_cond_.wait_for(lock, std::chrono::milliseconds(timeout_ms));
        if (status == std::cv_status::timeout) {
            LOG_WARNING("等待帧超时", "V4L2Camera");
            return frame;
        }
    }

    if (frame_queue_.empty()) {
        LOG_WARNING("帧队列为空", "V4L2Camera");
        return frame;
    }

    // 获取队列中的第一帧
    frame = frame_queue_.front();
    frame_queue_.pop();

    return frame;
}

void V4L2Camera::setFrameCallback(std::function<void(const Frame&)> callback) {
    frame_callback_ = callback;
}

CameraDeviceInfo V4L2Camera::getDeviceInfo() const {
    return device_info_;
}

CameraParams V4L2Camera::getParams() const {
    return current_params_;
}

bool V4L2Camera::setParams(const CameraParams& params) {
    if (!is_open_) {
        LOG_ERROR("设备未打开", "V4L2Camera");
        return false;
    }

    // 如果正在捕获，需要先停止
    bool was_capturing = is_capturing_;
    if (was_capturing) {
        stopCapture();
    }

    // 设置视频格式
    uint32_t v4l2_format = pixelFormatToV4L2Format(params.format);
    if (!setVideoFormat(params.width, params.height, v4l2_format)) {
        LOG_ERROR("无法设置视频格式", "V4L2Camera");
        return false;
    }

    // 设置帧率
    if (!setFrameRate(params.fps)) {
        LOG_ERROR("无法设置帧率", "V4L2Camera");
        return false;
    }

    // 更新当前参数
    current_params_ = params;

    // 如果之前在捕获，重新开始捕获
    if (was_capturing) {
        startCapture();
    }

    return true;
}

bool V4L2Camera::initDevice() {
    if (fd_ < 0) {
        return false;
    }

    // 初始化内存映射
    return initMmap();
}

bool V4L2Camera::initMmap() {
    // 请求缓冲区
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 4;  // 请求4个缓冲区
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd_, VIDIOC_REQBUFS, &req) < 0) {
        LOG_ERROR("请求缓冲区失败", "V4L2Camera");
        return false;
    }

    if (req.count < 2) {
        LOG_ERROR("缓冲区数量不足", "V4L2Camera");
        return false;
    }

    // 分配缓冲区
    buffers_.resize(req.count);

    for (unsigned int i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd_, VIDIOC_QUERYBUF, &buf) < 0) {
            LOG_ERROR("查询缓冲区失败", "V4L2Camera");
            return false;
        }

        buffers_[i].length = buf.length;
        buffers_[i].start = mmap(
            NULL,
            buf.length,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            fd_,
            buf.m.offset
        );

        if (buffers_[i].start == MAP_FAILED) {
            LOG_ERROR("内存映射失败", "V4L2Camera");
            return false;
        }
    }

    return true;
}

void V4L2Camera::freeMmap() {
    for (auto& buffer : buffers_) {
        if (buffer.start != nullptr && buffer.start != MAP_FAILED) {
            munmap(buffer.start, buffer.length);
            buffer.start = nullptr;
            buffer.length = 0;
        }
    }

    buffers_.clear();
}

void V4L2Camera::queryCapabilities(int fd, CameraDeviceInfo& deviceInfo) {
    // 查询支持的格式
    struct v4l2_fmtdesc fmtdesc;
    memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    std::set<std::pair<int, int>> resolutions;
    std::vector<PixelFormat> formats;

    // 遍历所有支持的格式
    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) >= 0) {
        // 添加像素格式
        PixelFormat format = v4l2FormatToPixelFormat(fmtdesc.pixelformat);
        if (format != PixelFormat::UNKNOWN) {
            formats.push_back(format);
        }

        // 查询该格式支持的分辨率
        struct v4l2_frmsizeenum frmsize;
        memset(&frmsize, 0, sizeof(frmsize));
        frmsize.pixel_format = fmtdesc.pixelformat;
        frmsize.index = 0;

        while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0) {
            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                // 离散分辨率
                std::pair<int, int> res(frmsize.discrete.width, frmsize.discrete.height);
                resolutions.insert(res);
            } else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
                // 步进分辨率，添加一些常见分辨率
                for (uint32_t width = frmsize.stepwise.min_width;
                     width <= frmsize.stepwise.max_width;
                     width += frmsize.stepwise.step_width) {
                    for (uint32_t height = frmsize.stepwise.min_height;
                         height <= frmsize.stepwise.max_height;
                         height += frmsize.stepwise.step_height) {
                        std::pair<int, int> res(width, height);
                        resolutions.insert(res);
                    }
                }
            }

            frmsize.index++;
        }

        fmtdesc.index++;
    }

    // 将分辨率集合转换为向量
    deviceInfo.supported_resolutions.clear();
    for (const auto& res : resolutions) {
        deviceInfo.supported_resolutions.push_back(res);
    }

    // 设置支持的格式
    deviceInfo.supported_formats = formats;
}

bool V4L2Camera::setVideoFormat(int width, int height, uint32_t pixelformat) {
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = pixelformat;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
        LOG_ERROR("无法设置视频格式", "V4L2Camera");
        return false;
    }

    // 验证设置是否成功
    if (fmt.fmt.pix.width != width || fmt.fmt.pix.height != height) {
        LOG_WARNING("摄像头不支持请求的分辨率: " + std::to_string(width) + "x" + std::to_string(height) +
                   "，实际分辨率: " + std::to_string(fmt.fmt.pix.width) + "x" + 
                   std::to_string(fmt.fmt.pix.height), "V4L2Camera");
    }

    // 验证格式是否成功设置
    if (fmt.fmt.pix.pixelformat != pixelformat) {
        LOG_ERROR("无法设置请求的像素格式: " + FormatUtils::getV4L2FormatName(pixelformat) + 
                 "，实际格式: " + FormatUtils::getV4L2FormatName(fmt.fmt.pix.pixelformat), "V4L2Camera");
        return false;
    }

    // 更新当前参数
    current_params_.width = fmt.fmt.pix.width;
    current_params_.height = fmt.fmt.pix.height;
    current_params_.format = FormatUtils::v4l2FormatToPixelFormat(fmt.fmt.pix.pixelformat);

    // 打印格式信息
    std::cerr << "[V4L2][v4l2_camera.cpp:setVideoFormat] 视频格式设置结果:" << std::endl;
    std::cerr << "  - 请求分辨率: " << width << "x" << height << std::endl;
    std::cerr << "  - 实际分辨率: " << fmt.fmt.pix.width << "x" << fmt.fmt.pix.height << std::endl;
    std::cerr << "  - 请求格式: " << FormatUtils::getV4L2FormatName(pixelformat) << std::endl;
    std::cerr << "  - 实际格式: " << FormatUtils::getV4L2FormatName(fmt.fmt.pix.pixelformat) << std::endl;
    std::cerr << "  - 当前格式: " << FormatUtils::getPixelFormatName(current_params_.format) << std::endl;

    return true;
}

bool V4L2Camera::setFrameRate(int fps) {
    if (fd_ < 0) {
        return false;
    }

    // 设置帧率
    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd_, VIDIOC_G_PARM, &parm) < 0) {
        LOG_ERROR("无法获取流参数", "V4L2Camera");
        return false;
    }

    // 检查是否支持设置帧率
    if (!(parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)) {
        LOG_WARNING("设备不支持设置帧率", "V4L2Camera");
        return false;
    }

    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = fps;

    if (ioctl(fd_, VIDIOC_S_PARM, &parm) < 0) {
        LOG_ERROR("无法设置帧率", "V4L2Camera");
        return false;
    }

    // 检查实际设置的帧率
    int actualFps = parm.parm.capture.timeperframe.denominator /
                   parm.parm.capture.timeperframe.numerator;

    if (actualFps != fps) {
        LOG_WARNING("实际帧率与请求不符: " + std::to_string(actualFps), "V4L2Camera");
    }

    return true;
}

bool V4L2Camera::startStreaming() {
    if (fd_ < 0) {
        return false;
    }

    // 将缓冲区加入队列
    for (unsigned int i = 0; i < buffers_.size(); ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
            LOG_ERROR("无法将缓冲区加入队列", "V4L2Camera");
            return false;
        }
    }

    // 开始流
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
        LOG_ERROR("无法启动视频流", "V4L2Camera");
        return false;
    }

    return true;
}

bool V4L2Camera::stopStreaming() {
    if (fd_ < 0) {
        return false;
    }

    // 停止流
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd_, VIDIOC_STREAMOFF, &type) < 0) {
        LOG_ERROR("无法停止视频流", "V4L2Camera");
        return false;
    }

    return true;
}

void V4L2Camera::captureThreadFunc() {
    while (!stop_flag_) {
        // 从队列中取出缓冲区
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        // 非阻塞模式下等待帧
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd_, &fds);

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int r = select(fd_ + 1, &fds, NULL, NULL, &tv);

        if (r == -1) {
            if (errno == EINTR) {
                continue;  // 被信号中断，重试
            }
            LOG_ERROR("select错误: " + std::string(strerror(errno)), "V4L2Camera");
            break;
        }

        if (r == 0) {
            continue;  // 超时，重试
        }

        if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
            if (errno == EAGAIN) {
                continue;  // 资源暂时不可用，重试
            }
            LOG_ERROR("无法从队列中取出缓冲区: " + std::string(strerror(errno)), "V4L2Camera");
            break;
        }

        // 处理帧
        processFrame(buffers_[buf.index].start, buf.bytesused, buf);

        // 将缓冲区放回队列
        if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
            LOG_ERROR("无法将缓冲区放回队列", "V4L2Camera");
            break;
        }
    }
}

void V4L2Camera::processFrame(const void* data, size_t size, const v4l2_buffer& buf) {
    std::cerr << "\\n[V4L2][v4l2_camera.cpp:processFrame] 处理新帧:" << std::endl;
    std::cerr << "  - 缓冲区大小: " << size << " 字节" << std::endl;
    std::cerr << "  - 当前格式: " << static_cast<int>(current_params_.format) << std::endl;
    std::cerr << "  - 分辨率: " << current_params_.width << "x" << current_params_.height << std::endl;
    std::cerr << "  - 时间戳: " << buf.timestamp.tv_sec << "." << buf.timestamp.tv_usec << std::endl;

    // 创建帧对象
    Frame frame;
    frame.setWidth(current_params_.width);
    frame.setHeight(current_params_.height);
    frame.setFormat(current_params_.format);  // 使用当前设置的格式
    frame.setTimestamp(buf.timestamp.tv_sec * 1000000LL + buf.timestamp.tv_usec);

    // 复制数据
    frame.getData().resize(size);
    std::memcpy(frame.getData().data(), data, size);

    std::cerr << "[V4L2][v4l2_camera.cpp:processFrame] 帧对象创建完成:" << std::endl;
    std::cerr << "  - 帧大小: " << frame.getData().size() << " 字节" << std::endl;
    std::cerr << "  - 帧格式: " << static_cast<int>(frame.getFormat()) << std::endl;
    std::cerr << "  - 帧分辨率: " << frame.getWidth() << "x" << frame.getHeight() << std::endl;

    // 调用回调函数
    if (frame_callback_) {
        std::cerr << "[V4L2][v4l2_camera.cpp:processFrame] 调用帧回调函数" << std::endl;
        frame_callback_(frame);
    }

    // 将帧添加到队列
    {
        std::lock_guard<std::mutex> lock(frame_queue_mutex_);
        frame_queue_.push(std::move(frame));
        frame_queue_cond_.notify_one();
    }
    std::cerr << "[V4L2][v4l2_camera.cpp:processFrame] 帧已添加到队列" << std::endl;
}

PixelFormat V4L2Camera::v4l2FormatToPixelFormat(uint32_t v4l2_format) const {
    return FormatUtils::v4l2FormatToPixelFormat(v4l2_format);
}

uint32_t V4L2Camera::pixelFormatToV4L2Format(PixelFormat format) const {
    return FormatUtils::pixelFormatToV4L2Format(format);
}

} // namespace camera
} // namespace cam_server
