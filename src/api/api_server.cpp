#include "api/api_server.h"
#include "api/rest_handler.h"
#include "api/web_server.h"
#include "api/mjpeg_streamer.h"
#include "api/camera_api.h"
#include "camera/camera_manager.h"
#include "monitor/logger.h"
#include "system/system_monitor.h"
#include "utils/time_utils.h"

#include <chrono>
#include <thread>
#include <mutex>
#include <sstream>
#include <random>
#include <iomanip>
#include <iostream>  // for std::cerr
#include <sys/stat.h>  // for struct stat
#include <cstdlib>  // for system
#include <fstream>  // for std::ifstream
#include <cstring>  // for strerror
#include <cerrno>   // for errno
#include <filesystem>  // for fs::directory_iterator
#include <fmt/format.h>  // for fmt::format

namespace fs = std::filesystem;  // 显式定义命名空间别名
namespace cam_server {
namespace api {

ApiServer& ApiServer::getInstance() {
    static ApiServer instance;
    return instance;
}

ApiServer::ApiServer()
    : is_initialized_(false), stop_flag_(false) {
    // 初始化状态
    status_.state = ApiServerState::STOPPED;
    status_.address = "";
    status_.port = 0;
    status_.using_https = false;
    status_.start_time = 0;
    status_.request_count = 0;
    status_.error_count = 0;
    status_.error_message = "";
}

ApiServer::~ApiServer() {
    stop();
}

bool ApiServer::initialize(const ApiServerConfig& config) {
    std::cerr << "[API][DEBUG] 初始化API服务器..." << std::endl;

    // 保存配置
    config_ = config;

    // 创建Web服务器
    web_server_ = std::make_unique<WebServer>();
    if (!web_server_) {
        std::cerr << "[API][ERROR] 创建Web服务器失败" << std::endl;
        return false;
    }

    // 创建REST处理器
    rest_handler_ = std::make_shared<RestHandler>(config_.enable_cors, config_.cors_allowed_origins);
    if (!rest_handler_) {
        std::cerr << "[API][ERROR] 创建REST处理器失败" << std::endl;
        return false;
    }

    // 初始化REST处理器
    if (!rest_handler_->initialize(config_.enable_api_key, config_.api_key)) {
        std::cerr << "[API][ERROR] 初始化REST处理器失败" << std::endl;
        return false;
    }

    // 设置API密钥
    if (config_.enable_api_key) {
        rest_handler_->enableApiKey(true);
        rest_handler_->setApiKey(config_.api_key);
    }

    // 注册API路由
    registerApiRoutes();

    // 初始化Web服务器
    WebServerConfig web_config;
    web_config.address = config_.address;
    web_config.port = config_.port;
    web_config.static_files_dir = config_.static_files_dir;
    web_config.use_https = config_.use_https;
    web_config.ssl_cert_path = config_.ssl_cert_path;
    web_config.ssl_key_path = config_.ssl_key_path;
    web_config.num_threads = 4;
    web_config.log_level = config_.log_level;

    if (!web_server_->initialize(web_config, rest_handler_)) {
        std::cerr << "[API][ERROR] 初始化Web服务器失败" << std::endl;
        return false;
    }

    // 设置初始化标志
    is_initialized_ = true;

    std::cerr << "[API][DEBUG] API服务器初始化成功" << std::endl;
    return true;
}

bool ApiServer::start() {
    LOG_INFO("正在启动API服务器...", "ApiServer");
    if (!is_initialized_) {
        LOG_ERROR("API服务器未初始化", "ApiServer");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        if (status_.state == ApiServerState::RUNNING || status_.state == ApiServerState::STARTING) {
            LOG_INFO("API服务器已经在运行中或正在启动", "ApiServer");
            return true;
        }
        LOG_INFO("更新API服务器状态为STARTING", "ApiServer");
        status_.state = ApiServerState::STARTING;
        if (status_callback_) {
            status_callback_(status_);
        }
    }

    // 检查配置
    LOG_INFO("API服务器配置: 地址=" + config_.address + ", 端口=" + std::to_string(config_.port) +
             ", HTTPS=" + (config_.use_https ? "是" : "否") +
             ", 静态文件目录=" + config_.static_files_dir, "ApiServer");

    // 检查静态文件目录是否存在
    struct stat st;
    if (stat(config_.static_files_dir.c_str(), &st) != 0) {
        std::cerr << "[API] 静态文件目录不存在: " << config_.static_files_dir << std::endl;
    } else {
        std::cerr << "[API] 静态文件目录存在: " << config_.static_files_dir << std::endl;
    }

    // 检查静态文件目录内容（替换system命令）
    LOG_DEBUG(fmt::format("静态文件目录: {}", config_.static_files_dir), "ApiServer");
    try {
        for (const auto& entry : fs::directory_iterator(config_.static_files_dir)) {
            LOG_DEBUG(fmt::format("  Found: {}", entry.path().filename().string()), "ApiServer");
        }
    } catch (const std::exception& e) {
        LOG_ERROR(fmt::format("无法列出目录内容: {}", e.what()), "ApiServer");
    }

    // 启动Web服务器
    LOG_INFO("正在启动Web服务器...", "ApiServer");
    std::cerr << "[API] 正在启动Web服务器..." << std::endl;
    if (!web_server_) {
        std::cerr << "[API] Web服务器对象为空" << std::endl;
        LOG_ERROR("Web服务器对象为空", "ApiServer");
        return false;
    }

    // 检查Web服务器是否已初始化
    std::cerr << "[API] Web服务器已创建" << std::endl;

    // 检查Web服务器配置
    std::cerr << "[API] Web服务器配置: 地址=" << config_.address << ", 端口=" << config_.port <<
        ", 静态文件目录=" << config_.static_files_dir << std::endl;

    if (!web_server_->start()) {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_.state = ApiServerState::ERROR;
        status_.error_message = "无法启动Web服务器";
        if (status_callback_) {
            status_callback_(status_);
        }
        LOG_ERROR("无法启动Web服务器", "ApiServer");
        std::cerr << "[API] 无法启动Web服务器" << std::endl;
        return false;
    }
    LOG_INFO("Web服务器启动成功", "ApiServer");
    std::cerr << "[API] Web服务器启动成功" << std::endl;

    // 检查Web服务器是否正在运行
    std::cerr << "[API] Web服务器已启动" << std::endl;

    // 更新状态
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        LOG_INFO("更新API服务器状态为RUNNING", "ApiServer");
        status_.state = ApiServerState::RUNNING;
        status_.address = config_.address;
        status_.port = config_.port;
        status_.using_https = config_.use_https;
        status_.start_time = utils::TimeUtils::getCurrentTimeMillis();
        status_.request_count = 0;
        status_.error_count = 0;
        status_.error_message = "";
        if (status_callback_) {
            status_callback_(status_);
        }
    }

    LOG_INFO("API服务器已启动: " + config_.address + ":" + std::to_string(config_.port), "ApiServer");
    return true;
}

bool ApiServer::stop() {
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        if (status_.state == ApiServerState::STOPPED || status_.state == ApiServerState::STOPPING) {
            return true;
        }
        status_.state = ApiServerState::STOPPING;
        if (status_callback_) {
            status_callback_(status_);
        }
    }

    // 停止Web服务器
    if (web_server_) {
        web_server_->stop();
    }

    // 更新状态
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_.state = ApiServerState::STOPPED;
        if (status_callback_) {
            status_callback_(status_);
        }
    }

    LOG_INFO("API服务器已停止", "ApiServer");
    return true;
}

ApiServerStatus ApiServer::getStatus() const {
    std::lock_guard<std::mutex> lock(status_mutex_);

    // 创建一个临时副本，以便在const方法中更新
    ApiServerStatus status_copy = status_;

    // 更新请求计数和错误计数
    if (web_server_ && status_copy.state == ApiServerState::RUNNING) {
        status_copy.request_count = web_server_->getRequestCount();
        status_copy.error_count = web_server_->getErrorCount();
    }

    return status_copy;
}

ApiServerConfig ApiServer::getConfig() const {
    // 返回当前配置的副本
    return config_;
}

bool ApiServer::registerHandler(const std::string& path, const std::string& method,
                              std::function<void(const std::string&, std::string&)> handler) {
    if (!is_initialized_ || !rest_handler_) {
        return false;
    }

    // 创建路由处理函数
    RouteHandler route_handler = [handler](const HttpRequest& request) -> HttpResponse {
        HttpResponse response;
        response.status_code = 200;
        response.status_message = "OK";
        response.content_type = "application/json";

        try {
            handler(request.body, response.body);
        } catch (const std::exception& e) {
            response.status_code = 500;
            response.status_message = "Internal Server Error";
            response.body = "{\"error\":\"" + std::string(e.what()) + "\"}";
        }

        return response;
    };

    // 注册路由
    return rest_handler_->registerRoute(method, path, route_handler);
}

void ApiServer::setStatusCallback(std::function<void(const ApiServerStatus&)> callback) {
    status_callback_ = callback;
}

// 注册API路由
void ApiServer::registerApiRoutes() {
    LOG_DEBUG("开始注册API路由...", "ApiServer");

    // 注册摄像头API路由
    LOG_DEBUG("注册摄像头状态API: GET /api/camera/status", "ApiServer");
    auto& camera_api = api::CameraApi::getInstance();
    camera_api.registerRoutes(*rest_handler_);

    // 注册系统控制API路由
    LOG_DEBUG("注册系统控制API路由...", "ApiServer");
    registerSystemControlRoutes();

    // 注册根路径重定向
    LOG_DEBUG("注册根路径重定向...", "ApiServer");
    rest_handler_->registerRoute("GET", "/", [](const HttpRequest&) -> HttpResponse {
        HttpResponse response;
        response.status_code = 302;  // 临时重定向
        response.headers["Location"] = "/index.html";
        return response;
    });

    LOG_INFO("API路由注册成功", "ApiServer");
}

// 注册系统控制API路由
void ApiServer::registerSystemControlRoutes() {
    if (!rest_handler_) {
        std::cerr << "[API][ERROR] REST处理器为空，无法注册系统控制API路由" << std::endl;
        return;
    }

    std::cerr << "[API][DEBUG] 开始注册系统控制API路由..." << std::endl;

    // 注册系统信息API
    std::cerr << "[API][DEBUG] 注册系统信息API: GET /api/system/info" << std::endl;
    rest_handler_->registerRoute("GET", "/api/system/info", [](const HttpRequest&) -> HttpResponse {
        std::cerr << "[API][DEBUG] 收到系统信息请求" << std::endl;
        HttpResponse response;
        response.status_code = 200;
        response.status_message = "OK";
        response.content_type = "application/json";

        try {
            auto& system_monitor = system::SystemMonitor::getInstance();
            auto system_info = system_monitor.getSystemInfo();

            // 构建JSON响应
            std::stringstream json;
            json << "{";
            json << "\"status\":\"success\",";
            
            // 系统基本信息
            json << "\"system\":{";
            json << "\"hostname\":\"" << system_info.hostname << "\",";
            json << "\"os\":\"" << system_info.os_version << "\",";
            json << "\"kernel\":\"" << system_info.kernel_version << "\",";
            json << "\"uptime\":\"" << system_info.uptime << "\",";
            json << "\"system_time\":\"" << system_info.system_time << "\"";
            json << "},";

            // CPU信息
            json << "\"cpu\":{";
            json << "\"usage_percent\":" << system_info.cpu.usage_percent << ",";
            json << "\"temperature\":" << system_info.cpu.temperature << ",";
            json << "\"frequency\":" << system_info.cpu.frequency << ",";
            json << "\"core_count\":" << system_info.cpu.core_count;
            json << "},";

            // GPU信息
            json << "\"gpu\":{";
            json << "\"usage_percent\":" << system_info.gpu.usage_percent << ",";
            json << "\"temperature\":" << system_info.gpu.temperature << ",";
            json << "\"memory_usage_percent\":" << system_info.gpu.memory_usage_percent << ",";
            json << "\"frequency\":" << system_info.gpu.frequency;
            json << "},";

            // 内存信息
            json << "\"memory\":{";
            json << "\"total\":" << system_info.memory.total << ",";
            json << "\"used\":" << system_info.memory.used << ",";
            json << "\"free\":" << system_info.memory.free << ",";
            json << "\"usage_percent\":" << system_info.memory.usage_percent;
            json << "},";

            // 存储信息
            json << "\"storage\":[";
            for (size_t i = 0; i < system_info.storage.size(); ++i) {
                const auto& storage = system_info.storage[i];
                if (i > 0) json << ",";
                json << "{";
                json << "\"mount_point\":\"" << storage.mount_point << "\",";
                json << "\"total\":" << storage.total << ",";
                json << "\"used\":" << storage.used << ",";
                json << "\"free\":" << storage.free << ",";
                json << "\"usage_percent\":" << storage.usage_percent;
                json << "}";
            }
            json << "],";

            // 网络信息
            json << "\"network\":[";
            for (size_t i = 0; i < system_info.network.size(); ++i) {
                const auto& network = system_info.network[i];
                if (i > 0) json << ",";
                json << "{";
                json << "\"interface\":\"" << network.interface << "\",";
                json << "\"ip_address\":\"" << network.ip_address << "\",";
                json << "\"rx_bytes\":" << network.rx_bytes << ",";
                json << "\"tx_bytes\":" << network.tx_bytes << ",";
                json << "\"rx_rate\":" << network.rx_rate << ",";
                json << "\"tx_rate\":" << network.tx_rate;
                json << "}";
            }
            json << "]";

            json << "}";
            response.body = json.str();

            std::cerr << "[API][DEBUG] 系统信息API响应: " << response.body << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[API][ERROR] 获取系统信息时发生错误: " << e.what() << std::endl;
            response.status_code = 500;
            response.status_message = "Internal Server Error";
            response.body = "{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}";
        }

        return response;
    });

    // 注册重启服务API
    rest_handler_->registerRoute("POST", "/api/system/restart-service", [](const HttpRequest&) -> HttpResponse {
        HttpResponse response;
        response.status_code = 200;
        response.status_message = "OK";
        response.content_type = "application/json";

        // 创建一个新线程来重启服务，这样可以先返回响应
        std::thread([]{
            // 等待2秒，确保响应已经发送
            std::this_thread::sleep_for(std::chrono::seconds(2));

            // 重启服务
            LOG_INFO("正在重启服务...", "SystemControl");

            // 停止API服务器
            ApiServer::getInstance().stop();

            // 等待1秒
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // 重新启动API服务器
            auto& api_server = ApiServer::getInstance();
            // 使用当前配置重新初始化
            api_server.initialize(api_server.getConfig());
            api_server.start();

            LOG_INFO("服务重启完成", "SystemControl");
        }).detach();

        response.body = "{\"status\":\"success\",\"message\":\"服务正在重启\"}";
        return response;
    });

    // 注册重启系统API
    rest_handler_->registerRoute("POST", "/api/system/restart", [](const HttpRequest&) -> HttpResponse {
        HttpResponse response;
        response.status_code = 200;
        response.status_message = "OK";
        response.content_type = "application/json";

        // 创建一个新线程来重启系统，这样可以先返回响应
        std::thread([]{
            // 等待2秒，确保响应已经发送
            std::this_thread::sleep_for(std::chrono::seconds(2));

            // 重启系统
            LOG_INFO("正在重启系统...", "SystemControl");
            ::system("sudo reboot");
        }).detach();

        response.body = "{\"status\":\"success\",\"message\":\"系统正在重启\"}";
        return response;
    });

    // 注册关闭系统API
    rest_handler_->registerRoute("POST", "/api/system/shutdown", [](const HttpRequest&) -> HttpResponse {
        HttpResponse response;
        response.status_code = 200;
        response.status_message = "OK";
        response.content_type = "application/json";

        // 创建一个新线程来关闭系统，这样可以先返回响应
        std::thread([]{
            // 等待2秒，确保响应已经发送
            std::this_thread::sleep_for(std::chrono::seconds(2));

            // 关闭系统
            LOG_INFO("正在关闭系统...", "SystemControl");
            ::system("sudo shutdown -h now");
        }).detach();

        response.body = "{\"status\":\"success\",\"message\":\"系统正在关闭\"}";
        return response;
    });

    std::cerr << "[API][DEBUG] 系统控制API路由注册完成" << std::endl;
    LOG_INFO("系统控制API路由注册成功", "ApiServer");
}

// 生成唯一的客户端ID
std::string ApiServer::generateClientId(const std::string& client_id_hint) {
    // 如果提供了客户端ID提示，则使用它
    if (!client_id_hint.empty()) {
        return client_id_hint;
    }

    // 否则生成一个新的客户端ID
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";

    std::stringstream ss;
    ss << "client-";
    for (int i = 0; i < 16; i++) {
        ss << hex[dis(gen)];
    }
    ss << "-" << utils::TimeUtils::getCurrentTimeMillis();

    return ss.str();
}

} // namespace api
} // namespace cam_server
