#include "web/serial_routes.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <cstring>
#include <fcntl.h>
#include <errno.h>

namespace cam_server {
namespace web {

void SerialRoutes::setupRoutes(crow::SimpleApp& app) {
    // 设置串口信息相关的所有路由
    setupSerialDevicesRoute(app);
    setupSerialInfoRoute(app);
}

void SerialRoutes::setupSerialDevicesRoute(crow::SimpleApp& app) {
    // 串口设备列表API - 返回系统中所有串口设备信息
    // 为什么这样做：前端需要显示所有可用的串口设备及其状态
    // 如何使用：GET /api/serial/devices 返回JSON格式的设备列表
    CROW_ROUTE(app, "/api/serial/devices")
    ([](const crow::request& /*req*/) {
        try {
            std::string response = "{\"success\":true,\"devices\":[";

            // 扫描串口设备
            auto devices = scanSerialDevices();

            bool first = true;
            for (const auto& device : devices) {
                if (!first) response += ",";
                first = false;

                response += "{";
                response += "\"device\":\"" + device.device_path + "\",";
                response += "\"type\":\"" + device.device_type + "\",";
                response += "\"description\":\"" + device.description + "\",";
                response += "\"driver\":\"" + device.driver + "\",";
                response += "\"status\":\"" + device.status + "\",";
                response += "\"permissions\":\"" + device.permissions + "\",";
                response += "\"last_access\":\"" + device.last_access + "\",";
                response += "\"supported_bauds\":\"" + device.supported_bauds + "\"";
                response += "}";
            }

            response += "]}";

            crow::response res(200, response);
            res.set_header("Content-Type", "application/json");
            return res;

        } catch (const std::exception& e) {
            std::string error_response = "{\"success\":false,\"error\":\"" + std::string(e.what()) + "\"}";
            crow::response res(500, error_response);
            res.set_header("Content-Type", "application/json");
            return res;
        }
    });
}

void SerialRoutes::setupSerialInfoRoute(crow::SimpleApp& app) {
    // 特定串口设备信息API - 返回指定串口设备的详细信息
    // 为什么这样做：前端可能需要查看特定设备的详细配置和状态
    // 如何使用：GET /api/serial/info?device=/dev/ttyUSB0
    CROW_ROUTE(app, "/api/serial/info")
    ([](const crow::request& req) {
        try {
            // 获取设备路径参数
            std::string device_path = req.url_params.get("device") ? req.url_params.get("device") : "";

            if (device_path.empty()) {
                std::string error_response = "{\"success\":false,\"error\":\"缺少设备路径参数\"}";
                crow::response res(400, error_response);
                res.set_header("Content-Type", "application/json");
                return res;
            }

            // 获取设备详细信息
            SerialDeviceInfo device_info;
            if (!getSerialDeviceInfo(device_path, device_info)) {
                std::string error_response = "{\"success\":false,\"error\":\"无法获取设备信息\"}";
                crow::response res(404, error_response);
                res.set_header("Content-Type", "application/json");
                return res;
            }

            // 构建响应
            std::string response = "{\"success\":true,\"device\":{";
            response += "\"device\":\"" + device_info.device_path + "\",";
            response += "\"type\":\"" + device_info.device_type + "\",";
            response += "\"description\":\"" + device_info.description + "\",";
            response += "\"driver\":\"" + device_info.driver + "\",";
            response += "\"status\":\"" + device_info.status + "\",";
            response += "\"permissions\":\"" + device_info.permissions + "\",";
            response += "\"last_access\":\"" + device_info.last_access + "\",";
            response += "\"supported_bauds\":\"" + device_info.supported_bauds + "\"";
            response += "}}";

            crow::response res(200, response);
            res.set_header("Content-Type", "application/json");
            return res;

        } catch (const std::exception& e) {
            std::string error_response = "{\"success\":false,\"error\":\"" + std::string(e.what()) + "\"}";
            crow::response res(500, error_response);
            res.set_header("Content-Type", "application/json");
            return res;
        }
    });
}

// 扫描系统中的串口设备
std::vector<SerialDeviceInfo> SerialRoutes::scanSerialDevices() {
    std::vector<SerialDeviceInfo> devices;

    // 扫描常见的串口设备路径
    std::vector<std::string> serial_patterns = {
        "/dev/ttyUSB",    // USB转串口设备
        "/dev/ttyACM",    // USB CDC ACM设备
        "/dev/ttyS",      // 传统串口
        "/dev/ttyAMA",    // ARM UART设备
        "/dev/ttyO",      // OMAP UART设备
        "/dev/ttymxc",    // i.MX UART设备
        "/dev/ttyTHS"     // Tegra高速串口
    };

    // 扫描每种类型的设备
    for (const auto& pattern : serial_patterns) {
        for (int i = 0; i < 32; ++i) {  // 检查0-31编号的设备
            std::string device_path = pattern + std::to_string(i);

            if (std::filesystem::exists(device_path)) {
                SerialDeviceInfo device_info;
                if (getSerialDeviceInfo(device_path, device_info)) {
                    devices.push_back(device_info);
                }
            }
        }
    }

    return devices;
}

// 获取特定串口设备的详细信息
bool SerialRoutes::getSerialDeviceInfo(const std::string& device_path, SerialDeviceInfo& info) {
    // 检查设备文件是否存在
    if (!std::filesystem::exists(device_path)) {
        return false;
    }

    info.device_path = device_path;

    // 获取设备类型
    if (device_path.find("ttyUSB") != std::string::npos) {
        info.device_type = "USB Serial";
    } else if (device_path.find("ttyACM") != std::string::npos) {
        info.device_type = "USB CDC ACM";
    } else if (device_path.find("ttyS") != std::string::npos) {
        info.device_type = "Serial Port";
    } else if (device_path.find("ttyAMA") != std::string::npos) {
        info.device_type = "ARM UART";
    } else if (device_path.find("ttyO") != std::string::npos) {
        info.device_type = "OMAP UART";
    } else if (device_path.find("ttymxc") != std::string::npos) {
        info.device_type = "i.MX UART";
    } else if (device_path.find("ttyTHS") != std::string::npos) {
        info.device_type = "Tegra UART";
    } else {
        info.device_type = "Unknown";
    }

    // 获取设备权限信息
    struct stat file_stat;
    if (stat(device_path.c_str(), &file_stat) == 0) {
        char permissions[10];
        snprintf(permissions, sizeof(permissions), "%o", file_stat.st_mode & 0777);
        info.permissions = permissions;

        // 获取最后访问时间
        char time_str[64];
        struct tm* tm_info = localtime(&file_stat.st_atime);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        info.last_access = time_str;
    } else {
        info.permissions = "Unknown";
        info.last_access = "Unknown";
    }

    // 检查设备状态
    if (access(device_path.c_str(), R_OK | W_OK) == 0) {
        // 尝试打开设备检查是否被占用
        int fd = open(device_path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd >= 0) {
            info.status = "available";
            close(fd);
        } else {
            if (errno == EBUSY) {
                info.status = "busy";
            } else {
                info.status = "error";
            }
        }
    } else {
        info.status = "error";
    }

    // 获取驱动信息（从sysfs）
    std::string sysfs_path = "/sys/class/tty/" + std::filesystem::path(device_path).filename().string() + "/device/driver";
    if (std::filesystem::exists(sysfs_path)) {
        try {
            std::string driver_link = std::filesystem::read_symlink(sysfs_path);
            info.driver = std::filesystem::path(driver_link).filename().string();
        } catch (...) {
            info.driver = "Unknown";
        }
    } else {
        info.driver = "Unknown";
    }

    // 获取设备描述（从udev或sysfs）
    info.description = getDeviceDescription(device_path);

    // 设置支持的波特率（通用值）
    info.supported_bauds = "9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600";

    return true;
}

// 获取设备描述信息
std::string SerialRoutes::getDeviceDescription(const std::string& device_path) {
    std::string device_name = std::filesystem::path(device_path).filename().string();

    // 尝试从sysfs获取设备描述
    std::string sysfs_path = "/sys/class/tty/" + device_name + "/device/product";
    if (std::filesystem::exists(sysfs_path)) {
        std::ifstream file(sysfs_path);
        if (file.is_open()) {
            std::string description;
            std::getline(file, description);
            if (!description.empty()) {
                return description;
            }
        }
    }

    // 尝试从manufacturer获取
    sysfs_path = "/sys/class/tty/" + device_name + "/device/manufacturer";
    if (std::filesystem::exists(sysfs_path)) {
        std::ifstream file(sysfs_path);
        if (file.is_open()) {
            std::string manufacturer;
            std::getline(file, manufacturer);
            if (!manufacturer.empty()) {
                return manufacturer + " Serial Device";
            }
        }
    }

    // 根据设备类型返回默认描述
    if (device_path.find("ttyUSB") != std::string::npos) {
        return "USB Serial Converter";
    } else if (device_path.find("ttyACM") != std::string::npos) {
        return "USB CDC ACM Device";
    } else if (device_path.find("ttyS") != std::string::npos) {
        return "Serial Port";
    } else if (device_path.find("ttyAMA") != std::string::npos) {
        return "ARM UART";
    } else {
        return "Serial Device";
    }
}

} // namespace web
} // namespace cam_server
