#include "web/system_routes.h"
#include "system/system_monitor.h"
#include <filesystem>
#include <sstream>

namespace cam_server {
namespace web {

void SystemRoutes::setupRoutes(crow::SimpleApp& app) {
    // 设置系统信息相关的所有路由
    // 为什么这样做：将系统信息API集中管理，便于维护和扩展
    setupSystemInfoRoute(app);
    setupCameraInfoRoute(app);
}

void SystemRoutes::setupSystemInfoRoute(crow::SimpleApp& app) {
    // 系统信息API - 返回完整的系统状态信息
    // 为什么这样做：前端需要实时显示系统状态，包括CPU、内存、存储等
    // 如何使用：GET /api/system/info 返回JSON格式的系统信息
    CROW_ROUTE(app, "/api/system/info")
    ([](const crow::request& /*req*/) {
        try {
            // 获取系统监控实例 - 使用单例模式确保数据一致性
            auto& monitor = cam_server::system::SystemMonitor::getInstance();
            auto system_info = monitor.getSystemInfo();

            // 构建JSON响应 - 手动构建确保格式正确和性能
            // 为什么不用JSON库：减少依赖，提高性能，便于调试
            std::string response = "{"
                "\"success\":true,"
                "\"system\":{"
                "\"os_version\":\"" + system_info.os_version + "\","
                "\"kernel_version\":\"" + system_info.kernel_version + "\","
                "\"hostname\":\"" + system_info.hostname + "\","
                "\"uptime\":\"" + system_info.uptime + "\","
                "\"system_time\":\"" + system_info.system_time + "\",";

            // 添加负载平均值数组
            // 为什么这样做：负载平均值是数组，需要特殊处理
            response += "\"load_average\":[";
            for (size_t i = 0; i < system_info.load_average.size(); ++i) {
                if (i > 0) response += ",";
                response += std::to_string(system_info.load_average[i]);
            }
            response += "],";

            // CPU信息 - 包含核心数、使用率、温度、频率
            // 为什么包含这些：这些是监控CPU状态的关键指标
            response += "\"cpu\":{"
                "\"core_count\":" + std::to_string(system_info.cpu.core_count) + ","
                "\"usage_percent\":" + std::to_string(system_info.cpu.usage_percent) + ","
                "\"temperature\":" + std::to_string(system_info.cpu.temperature) + ","
                "\"frequency\":" + std::to_string(system_info.cpu.frequency) + ""
                "},";

            // 内存信息 - 总量、已用、空闲、使用率
            // 为什么这样做：内存是系统性能的关键指标
            response += "\"memory\":{"
                "\"total\":" + std::to_string(system_info.memory.total) + ","
                "\"used\":" + std::to_string(system_info.memory.used) + ","
                "\"free\":" + std::to_string(system_info.memory.free) + ","
                "\"usage_percent\":" + std::to_string(system_info.memory.usage_percent) + ""
                "},";

            // 存储信息数组 - 支持多个挂载点
            // 为什么是数组：系统可能有多个存储设备或分区
            response += "\"storage\":[";
            for (size_t i = 0; i < system_info.storage.size(); ++i) {
                if (i > 0) response += ",";
                const auto& storage = system_info.storage[i];
                response += "{"
                    "\"mount_point\":\"" + storage.mount_point + "\","
                    "\"total\":" + std::to_string(storage.total) + ","
                    "\"used\":" + std::to_string(storage.used) + ","
                    "\"free\":" + std::to_string(storage.free) + ","
                    "\"usage_percent\":" + std::to_string(storage.usage_percent) + ""
                    "}";
            }
            response += "],";

            // 网络信息数组 - 支持多个网络接口
            // 为什么是数组：系统通常有多个网络接口（以太网、WiFi等）
            response += "\"network\":[";
            for (size_t i = 0; i < system_info.network.size(); ++i) {
                if (i > 0) response += ",";
                const auto& net = system_info.network[i];
                response += "{"
                    "\"interface\":\"" + net.interface + "\","
                    "\"ip_address\":\"" + net.ip_address + "\","
                    "\"tx_bytes\":" + std::to_string(net.tx_bytes) + ","
                    "\"rx_bytes\":" + std::to_string(net.rx_bytes) + ","
                    "\"tx_rate\":" + std::to_string(net.tx_rate) + ","
                    "\"rx_rate\":" + std::to_string(net.rx_rate) + ""
                    "}";
            }

            response += "]}}";

            // 设置JSON响应头
            crow::response res(200, response);
            res.set_header("Content-Type", "application/json");
            return res;

        } catch (const std::exception& e) {
            // 错误处理 - 返回标准错误格式
            // 为什么这样做：确保前端能够正确处理错误情况
            std::string error_response = "{\"success\":false,\"error\":\"" + std::string(e.what()) + "\"}";
            crow::response res(500, error_response);
            res.set_header("Content-Type", "application/json");
            return res;
        }
    });
}

void SystemRoutes::setupCameraInfoRoute(crow::SimpleApp& app) {
    // 摄像头设备信息API - 返回系统中可用的摄像头设备
    // 为什么这样做：前端需要知道有哪些摄像头可用，以便用户选择
    // 如何使用：GET /api/system/cameras 返回摄像头设备列表
    CROW_ROUTE(app, "/api/system/cameras")
    ([](const crow::request& /*req*/) {
        try {
            std::string response = "{\"success\":true,\"cameras\":[";

            // 扫描可能的摄像头设备 - Linux下通常是/dev/videoN
            // 为什么扫描0-9：大多数系统不会有超过10个摄像头设备
            bool first = true;
            for (int i = 0; i < 10; ++i) {
                std::string device_path = "/dev/video" + std::to_string(i);

                // 检查设备文件是否存在
                // 为什么这样检查：设备文件存在通常意味着设备可用
                if (std::filesystem::exists(device_path)) {
                    if (!first) response += ",";
                    first = false;

                    // 构建设备信息 - 包含设备路径、名称、状态
                    // 为什么包含这些信息：前端需要显示给用户选择
                    response += "{"
                        "\"device\":\"" + device_path + "\","
                        "\"name\":\"摄像头 " + std::to_string(i) + "\","
                        "\"status\":\"可用\","
                        "\"index\":" + std::to_string(i) + ""
                        "}";
                }
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

} // namespace web
} // namespace cam_server
