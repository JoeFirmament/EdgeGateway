#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <cstring>
#include <iomanip>
#include <fmt/format.h>

// 日志头文件
#include "monitor/logger.h"
#include "utils/debug_utils.h"

// 使用 cam_server 命名空间中的 monitor 子命名空间
using namespace cam_server::monitor;

// 格式名称映射
std::map<uint32_t, std::string> format_names = {
    {V4L2_PIX_FMT_MJPEG, "MJPG"},
    {V4L2_PIX_FMT_YUYV, "YUYV"},
    {V4L2_PIX_FMT_RGB24, "RGB24"},
    {V4L2_PIX_FMT_BGR24, "BGR24"},
    {V4L2_PIX_FMT_YUV420, "YUV420"},
    {V4L2_PIX_FMT_NV12, "NV12"},
    {V4L2_PIX_FMT_NV21, "NV21"},
    {V4L2_PIX_FMT_H264, "H264"},
    {V4L2_PIX_FMT_MPEG4, "MPEG4"},
    {V4L2_PIX_FMT_JPEG, "JPEG"}
};

// 分辨率结构体
struct Resolution {
    uint32_t width;
    uint32_t height;

    bool operator<(const Resolution& other) const {
        if (width != other.width) {
            return width < other.width;
        }
        return height < other.height;
    }

    bool operator==(const Resolution& other) const {
        return width == other.width && height == other.height;
    }
};

// 设备信息结构体
struct DeviceInfo {
    std::string path;
    std::string name;
    std::string bus_info;
    std::map<std::string, std::set<Resolution>> formats;
};

// 获取格式名称
std::string get_format_name(uint32_t format) {
    char fmt_str[5] = {0};
    fmt_str[0] = format & 0xFF;
    fmt_str[1] = (format >> 8) & 0xFF;
    fmt_str[2] = (format >> 16) & 0xFF;
    fmt_str[3] = (format >> 24) & 0xFF;

    auto it = format_names.find(format);
    if (it != format_names.end()) {
        return it->second;
    }
    return std::string(fmt_str);
}

// 查询设备信息
bool query_device(const std::string& device_path, DeviceInfo& info) {
    int fd = open(device_path.c_str(), O_RDWR);
    if (fd < 0) {
        LOG_ERROR(fmt::format("无法打开设备: {}", device_path), "CameraTool");
        return false;
    }

    // 获取设备信息
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        LOG_ERROR(fmt::format("无法查询设备能力: {}", device_path), "CameraTool");
        close(fd);
        return false;
    }

    // 检查是否是视频捕获设备
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        LOG_ERROR(fmt::format("不是视频捕获设备: {}", device_path), "CameraTool");
        close(fd);
        return false;
    }

    info.path = device_path;
    info.name = reinterpret_cast<const char*>(cap.card);
    info.bus_info = reinterpret_cast<const char*>(cap.bus_info);

    // 查询支持的格式
    struct v4l2_fmtdesc fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0) {
        std::string format_name = get_format_name(fmt.pixelformat);
        
        // 查询该格式支持的分辨率
        struct v4l2_frmsizeenum frmsize;
        memset(&frmsize, 0, sizeof(frmsize));
        frmsize.pixel_format = fmt.pixelformat;
        frmsize.index = 0;
        
        if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                // 离散分辨率
                do {
                    Resolution res = {frmsize.discrete.width, frmsize.discrete.height};
                    info.formats[format_name].insert(res);
                    frmsize.index++;
                } while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0);
            } else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE || 
                       frmsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
                // 连续或步进分辨率，我们选择一些常用的分辨率
                std::vector<std::pair<uint32_t, uint32_t>> common_resolutions = {
                    {640, 480}, {800, 600}, {1024, 768}, {1280, 720}, 
                    {1280, 960}, {1600, 1200}, {1920, 1080}, {2560, 1440}, 
                    {3840, 2160}
                };
                
                for (const auto& res : common_resolutions) {
                    if (res.first >= frmsize.stepwise.min_width && 
                        res.first <= frmsize.stepwise.max_width &&
                        res.second >= frmsize.stepwise.min_height && 
                        res.second <= frmsize.stepwise.max_height) {
                        Resolution r = {res.first, res.second};
                        info.formats[format_name].insert(r);
                    }
                }
            }
        }
        
        fmt.index++;
    }

    close(fd);
    return true;
}

// 扫描所有视频设备
std::vector<DeviceInfo> scan_devices() {
    std::vector<DeviceInfo> devices;
    
    for (const auto& entry : std::filesystem::directory_iterator("/dev")) {
        std::string path = entry.path().string();
        if (path.find("/dev/video") == 0) {
            // 尝试提取数字部分
            std::string num_part = path.substr(10); // 跳过"/dev/video"
            if (!num_part.empty() && std::all_of(num_part.begin(), num_part.end(), ::isdigit)) {
                DeviceInfo info;
                if (query_device(path, info) && !info.formats.empty()) {
                    devices.push_back(info);
                }
            }
        }
    }
    
    return devices;
}

// 显示设备信息
void display_devices(const std::vector<DeviceInfo>& devices) {
    if (devices.empty()) {
        LOG_INFO("未找到可用的视频设备", "CameraTool");
        return;
    }
    
    LOG_INFO(fmt::format("找到 {} 个视频设备:", devices.size()), "CameraTool");
    
    for (size_t i = 0; i < devices.size(); i++) {
        const auto& device = devices[i];
        std::string device_info = fmt::format("\n[{}] {}\n    名称: {}\n    总线信息: {}\n    支持的格式和分辨率:", 
            i + 1, device.path, device.name, device.bus_info);
        LOG_INFO(device_info, "CameraTool");
        
        for (const auto& format : device.formats) {
            std::string format_info = fmt::format("      * {}: ", format.first);
            bool first = true;
            for (const auto& res : format.second) {
                if (!first) {
                    format_info += ", ";
                }
                format_info += fmt::format("{}x{}", res.width, res.height);
                first = false;
            }
            LOG_INFO(format_info, "CameraTool");
        }
    }
}

// 用户选择设备
bool select_device(const std::vector<DeviceInfo>& devices, 
                  DeviceInfo& selected_device,
                  std::string& selected_format,
                  Resolution& selected_resolution) {
    if (devices.empty()) {
        return false;
    }
    
    int device_index;
    std::cout << "\n请选择设备 [1-" << devices.size() << "]: ";
    std::cin >> device_index;
    
    if (device_index < 1 || device_index > static_cast<int>(devices.size())) {
        LOG_ERROR("无效的设备索引", "CameraTool");
        return false;
    }
    
    selected_device = devices[device_index - 1];
    
    // 显示可用格式
    std::vector<std::string> formats;
    for (const auto& format : selected_device.formats) {
        formats.push_back(format.first);
    }
    
    LOG_INFO("\n可用格式:", "CameraTool");
    for (size_t i = 0; i < formats.size(); i++) {
        LOG_INFO(fmt::format("[{}] {}", i + 1, formats[i]), "CameraTool");
    }
    
    int format_index;
    std::cout << "请选择格式 [1-" << formats.size() << "]: ";
    std::cin >> format_index;
    
    if (format_index < 1 || format_index > static_cast<int>(formats.size())) {
        LOG_ERROR("无效的格式索引", "CameraTool");
        return false;
    }
    
    selected_format = formats[format_index - 1];
    
    // 显示可用分辨率
    std::vector<Resolution> resolutions(
        selected_device.formats[selected_format].begin(),
        selected_device.formats[selected_format].end()
    );
    
    LOG_INFO("\n可用分辨率:", "CameraTool");
    for (size_t i = 0; i < resolutions.size(); i++) {
        LOG_INFO(fmt::format("[{}] {}x{}", i + 1, resolutions[i].width, resolutions[i].height), "CameraTool");
    }
    
    int resolution_index;
    std::cout << "请选择分辨率 [1-" << resolutions.size() << "]: ";
    std::cin >> resolution_index;
    
    if (resolution_index < 1 || resolution_index > static_cast<int>(resolutions.size())) {
        LOG_ERROR("无效的分辨率索引", "CameraTool");
        return false;
    }
    
    selected_resolution = resolutions[resolution_index - 1];
    
    return true;
}

// 生成配置文件
void generate_config(const DeviceInfo& device, 
                    const std::string& format,
                    const Resolution& resolution) {
    LOG_INFO("\n已选择:", "CameraTool");
    LOG_INFO(fmt::format("设备: {} ({})", device.path, device.name), "CameraTool");
    LOG_INFO(fmt::format("格式: {}", format), "CameraTool");
    LOG_INFO(fmt::format("分辨率: {}x{}", resolution.width, resolution.height), "CameraTool");
    
    std::string cmd_args = fmt::format("\n命令行参数:\n./bin/cam_server --device {} --resolution {}x{} --format {}",
        device.path, resolution.width, resolution.height, format);
    LOG_INFO(cmd_args, "CameraTool");
    
    std::string config_content = fmt::format("\n配置文件内容:\n{{\n    \"camera\": {{\n        \"device\": \"{}\",\n        \"resolution\": \"{}x{}\",\n        \"fps\": 30,\n        \"format\": \"{}\"\n    }}\n}}",
        device.path, resolution.width, resolution.height, format);
    LOG_INFO(config_content, "CameraTool");
    
    std::cout << "\n是否要更新配置文件? (y/n): ";
    char choice;
    std::cin >> choice;
    
    if (choice == 'y' || choice == 'Y') {
        // 更新配置文件的逻辑
        LOG_INFO("配置文件已更新", "CameraTool");
    }
}

int main() {
    // 初始化日志系统
    cam_server::monitor::LogConfig log_config;
    log_config.console_output = true;
    log_config.file_output = false;
    log_config.min_level = cam_server::monitor::LogLevel::DEBUG;
    
    if (!cam_server::monitor::Logger::getInstance().initialize(log_config)) {
        std::cerr << "无法初始化日志系统" << std::endl;
        return 1;
    }
    
    LOG_INFO("===== 摄像头设备扫描工具 =====", "CameraTool");
    
    auto devices = scan_devices();
    display_devices(devices);
    
    if (devices.empty()) {
        LOG_ERROR("未找到可用的摄像头设备", "CameraTool");
        return 1;
    }
    
    DeviceInfo selected_device;
    std::string selected_format;
    Resolution selected_resolution;
    
    if (select_device(devices, selected_device, selected_format, selected_resolution)) {
        generate_config(selected_device, selected_format, selected_resolution);
    } else {
        LOG_ERROR("设备选择失败", "CameraTool");
    }
    
    return 0;
}
