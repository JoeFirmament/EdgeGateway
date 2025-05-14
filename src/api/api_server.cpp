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
    std::cerr << "[API][api_server.cpp:initialize] 开始初始化API服务器..." << std::endl;

    if (is_initialized_) {
        std::cerr << "[API][api_server.cpp:initialize] API服务器已经初始化" << std::endl;
        return true;
    }

    std::cerr << "[API][api_server.cpp:initialize] 设置配置..." << std::endl;
    config_ = config;

    // 创建REST处理器
    std::cerr << "[API][api_server.cpp:initialize] 创建REST处理器..." << std::endl;
    rest_handler_ = std::make_shared<RestHandler>();
    if (!rest_handler_->initialize(config.enable_api_key, config.api_key)) {
        std::cerr << "[API][api_server.cpp:initialize] 无法初始化REST处理器" << std::endl;
        LOG_ERROR("无法初始化REST处理器", "ApiServer");
        return false;
    }
    std::cerr << "[API][api_server.cpp:initialize] REST处理器初始化成功" << std::endl;

    // 设置CORS配置
    std::cerr << "[API][api_server.cpp:initialize] 设置CORS配置..." << std::endl;
    rest_handler_->setCorsConfig(config.enable_cors, config.cors_allowed_origins);

    // 创建Web服务器配置
    std::cerr << "[API][api_server.cpp:initialize] 创建Web服务器配置..." << std::endl;
    WebServerConfig web_config;
    web_config.address = config.address;
    web_config.port = config.port;
    web_config.static_files_dir = config.static_files_dir;
    web_config.use_https = config.use_https;
    web_config.ssl_cert_path = config.ssl_cert_path;
    web_config.ssl_key_path = config.ssl_key_path;
    web_config.num_threads = 4;
    web_config.log_level = config.log_level;

    // 创建Web服务器
    std::cerr << "[API][api_server.cpp:initialize] 创建Web服务器..." << std::endl;
    web_server_ = std::make_unique<WebServer>();
    if (!web_server_->initialize(web_config, rest_handler_)) {
        std::cerr << "[API][api_server.cpp:initialize] 无法初始化Web服务器" << std::endl;
        LOG_ERROR("无法初始化Web服务器", "ApiServer");
        return false;
    }
    std::cerr << "[API][api_server.cpp:initialize] Web服务器初始化成功" << std::endl;

    // 注册API路由
    std::cerr << "[API][api_server.cpp:initialize] 注册API路由..." << std::endl;
    registerApiRoutes();
    std::cerr << "[API][api_server.cpp:initialize] API路由注册成功" << std::endl;

    is_initialized_ = true;
    std::cerr << "[API][api_server.cpp:initialize] API服务器初始化成功" << std::endl;
    LOG_INFO("API服务器初始化成功", "ApiServer");
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

    // 检查静态文件目录是否存在
    struct stat st;
    if (stat(config_.static_files_dir.c_str(), &st) != 0) {
        std::cerr << "[API] 静态文件目录不存在: " << config_.static_files_dir << std::endl;
    } else {
        std::cerr << "[API] 静态文件目录存在: " << config_.static_files_dir << std::endl;
    }

    // 检查静态文件目录中的文件
    std::cerr << "[API] 静态文件目录内容:" << std::endl;
    std::string cmd = "ls -la " + config_.static_files_dir;
    ::system(cmd.c_str());

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
    if (!rest_handler_) {
        return;
    }

    // 初始化并注册摄像头API
    auto& camera_api = CameraApi::getInstance();
    if (!camera_api.initialize()) {
        LOG_ERROR("无法初始化摄像头API", "ApiServer");
    } else {
        if (!camera_api.registerRoutes(*rest_handler_)) {
            LOG_ERROR("无法注册摄像头API路由", "ApiServer");
        } else {
            LOG_INFO("摄像头API路由注册成功", "ApiServer");
        }
    }

    // 注册MJPEG流端点
    rest_handler_->registerRoute("GET", "/api/stream", [](const HttpRequest& request) -> HttpResponse {
        HttpResponse response;
        response.status_code = 200;
        response.status_message = "OK";
        response.content_type = "multipart/x-mixed-replace; boundary=frame";
        response.is_streaming = true;

        // 解析查询参数
        int width = 0;
        int height = 0;
        int quality = 80;
        int fps = 30;

        auto it = request.query_params.find("width");
        if (it != request.query_params.end()) {
            width = std::stoi(it->second);
        }

        it = request.query_params.find("height");
        if (it != request.query_params.end()) {
            height = std::stoi(it->second);
        }

        it = request.query_params.find("quality");
        if (it != request.query_params.end()) {
            quality = std::stoi(it->second);
            if (quality < 1) quality = 1;
            if (quality > 100) quality = 100;
        }

        it = request.query_params.find("fps");
        if (it != request.query_params.end()) {
            fps = std::stoi(it->second);
            if (fps < 1) fps = 1;
            if (fps > 60) fps = 60;
        }

        // 生成客户端ID
        std::string client_id = ApiServer::getInstance().generateClientId();

        // 设置流回调
        response.stream_callback = [client_id, width, height, quality, fps](
            std::function<void(const std::vector<uint8_t>&)> send_data) {

            // 初始化MJPEG流处理器
            auto& streamer = MjpegStreamer::getInstance();

            MjpegStreamerConfig config;
            config.jpeg_quality = quality;
            config.max_fps = fps;
            config.max_clients = 100;
            config.output_width = width;
            config.output_height = height;

            if (!streamer.initialize(config)) {
                LOG_ERROR("无法初始化MJPEG流处理器", "ApiServer");
                return;
            }

            if (!streamer.start()) {
                LOG_ERROR("无法启动MJPEG流处理器", "ApiServer");
                return;
            }

            // 添加客户端
            streamer.addClient(
                client_id,
                [send_data](const std::vector<uint8_t>& jpeg_data) {
                    // 构建MJPEG帧
                    std::string frame_header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                                              std::to_string(jpeg_data.size()) + "\r\n\r\n";

                    // 发送帧头
                    std::vector<uint8_t> header_data(frame_header.begin(), frame_header.end());
                    send_data(header_data);

                    // 发送JPEG数据
                    send_data(jpeg_data);

                    // 发送帧尾
                    std::string frame_footer = "\r\n";
                    std::vector<uint8_t> footer_data(frame_footer.begin(), frame_footer.end());
                    send_data(footer_data);
                },
                [](const std::string& error) {
                    LOG_ERROR("MJPEG流错误: " + error, "ApiServer");
                },
                [client_id]() {
                    // 客户端关闭时移除
                    auto& streamer = MjpegStreamer::getInstance();
                    streamer.removeClient(client_id);
                    LOG_INFO("MJPEG客户端已关闭: " + client_id, "ApiServer");
                }
            );

            LOG_INFO("MJPEG客户端已连接: " + client_id, "ApiServer");
        };

        return response;
    });

    // 注册系统信息API路由
    rest_handler_->registerRoute("GET", "/systeminfo", [](const HttpRequest& request) -> HttpResponse {
        HttpResponse response;
        response.status_code = 200;
        response.status_message = "OK";
        response.content_type = "application/json";

        // 获取系统监控实例
        auto& system_monitor = system::SystemMonitor::getInstance();

        // 获取系统信息
        auto system_info = system_monitor.getSystemInfo();
        auto cpu_info = system_monitor.getCpuInfo();
        auto memory_info = system_monitor.getMemoryInfo();
        auto storage_info = system_monitor.getStorageInfo();
        auto network_info = system_monitor.getNetworkInfo();

        // 格式化内存大小
        auto formatSize = [](uint64_t size_bytes) -> std::string {
            const char* units[] = {"B", "KB", "MB", "GB", "TB"};
            int unit_index = 0;
            double size = static_cast<double>(size_bytes);

            while (size >= 1024.0 && unit_index < 4) {
                size /= 1024.0;
                unit_index++;
            }

            std::ostringstream ss;
            ss << std::fixed << std::setprecision(1) << size << units[unit_index];
            return ss.str();
        };

        // 构建JSON响应
        response.body = "{\n";

        // 基本信息
        response.body += "  \"hostname\": \"" + system_info.hostname + "\",\n";
        response.body += "  \"kernel_version\": \"" + system_info.kernel_version + "\",\n";
        response.body += "  \"os_version\": \"" + system_info.os_version + "\",\n";
        response.body += "  \"uptime\": \"" + system_info.uptime + "\",\n";
        response.body += "  \"system_time\": \"" + system_info.system_time + "\",\n";

        // CPU信息
        response.body += "  \"cpu_info\": \"" + (cpu_info.core_count > 0 ? "RK3588 (" + std::to_string(cpu_info.core_count) + " cores)" : "RK3588") + "\",\n";
        response.body += "  \"cpu_usage\": " + std::to_string(static_cast<int>(cpu_info.usage_percent)) + ",\n";
        response.body += "  \"cpu_temperature\": " + std::to_string(static_cast<int>(cpu_info.temperature)) + ",\n";

        // 内存信息
        response.body += "  \"memory_total\": \"" + formatSize(memory_info.total) + "\",\n";
        response.body += "  \"memory_used\": \"" + formatSize(memory_info.used) + "\",\n";
        response.body += "  \"memory_free\": \"" + formatSize(memory_info.free) + "\",\n";
        response.body += "  \"memory_usage\": " + std::to_string(static_cast<int>(memory_info.usage_percent)) + ",\n";

        // 磁盘信息
        if (!storage_info.empty()) {
            response.body += "  \"disk_total\": \"" + formatSize(storage_info[0].total) + "\",\n";
            response.body += "  \"disk_used\": \"" + formatSize(storage_info[0].used) + "\",\n";
            response.body += "  \"disk_free\": \"" + formatSize(storage_info[0].free) + "\",\n";
            response.body += "  \"disk_usage\": " + std::to_string(static_cast<int>(storage_info[0].usage_percent)) + ",\n";
        } else {
            response.body += "  \"disk_total\": \"Unknown\",\n";
            response.body += "  \"disk_used\": \"Unknown\",\n";
            response.body += "  \"disk_free\": \"Unknown\",\n";
            response.body += "  \"disk_usage\": 0,\n";
        }

        // 获取WiFi SSID
        std::string wifi_ssid = "-";
        FILE* fp = popen("iwgetid -r", "r");
        if (fp) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), fp) != NULL) {
                wifi_ssid = buffer;
                // 移除换行符
                if (!wifi_ssid.empty() && wifi_ssid[wifi_ssid.length()-1] == '\n') {
                    wifi_ssid.erase(wifi_ssid.length()-1);
                }
            }
            pclose(fp);
        }

        // 添加WiFi SSID到响应
        response.body += "  \"wifi_ssid\": \"" + wifi_ssid + "\",\n";

        // 网络信息
        response.body += "  \"network\": [";
        for (size_t i = 0; i < network_info.size(); i++) {
            if (i > 0) {
                response.body += ",";
            }
            response.body += "\n    {\n";
            response.body += "      \"interface\": \"" + network_info[i].interface + "\",\n";
            response.body += "      \"ip_address\": \"" + network_info[i].ip_address + "\",\n";
            response.body += "      \"rx_rate\": " + std::to_string(static_cast<int>(network_info[i].rx_rate)) + ",\n";
            response.body += "      \"tx_rate\": " + std::to_string(static_cast<int>(network_info[i].tx_rate)) + "\n";
            response.body += "    }";
        }
        response.body += "\n  ],\n";

        // 服务器信息
        response.body += "  \"server_version\": \"0.1.0\",\n";
        response.body += "  \"server_uptime\": \"" + std::to_string(utils::TimeUtils::getCurrentTimeMillis() - ApiServer::getInstance().getStatus().start_time) + " ms\"\n";
        response.body += "}";

        return response;
    });

    // 注册摄像头捕获API路由
    rest_handler_->registerRoute("GET", "/camCapture", [](const HttpRequest& request) -> HttpResponse {
        HttpResponse response;
        response.status_code = 200;
        response.status_message = "OK";
        response.content_type = "application/json";

        // 获取摄像头管理器实例
        auto& camera_manager = camera::CameraManager::getInstance();

        // 检查摄像头是否已打开
        if (!camera_manager.isDeviceOpen()) {
            response.status_code = 400;
            response.body = "{\"error\":\"摄像头未打开\"}";
            return response;
        }

        // 获取当前设备
        auto device = camera_manager.getCurrentDevice();
        if (!device) {
            response.status_code = 500;
            response.body = "{\"error\":\"无法获取摄像头设备\"}";
            return response;
        }

        // 获取设备信息
        auto device_info = device->getDeviceInfo();
        std::string device_path = device_info.device_path;

        // 获取当前参数
        auto params = device->getParams();
        int width = params.width;
        int height = params.height;
        int fps = params.fps;

        // 构建JSON响应
        response.body = "{\n";
        response.body += "  \"device\": \"" + device_path + "\",\n";
        response.body += "  \"resolution\": \"" + std::to_string(width) + "x" + std::to_string(height) + "\",\n";
        response.body += "  \"fps\": " + std::to_string(fps) + ",\n";
        response.body += "  \"is_capturing\": " + std::string(camera_manager.isCapturing() ? "true" : "false") + ",\n";
        response.body += "  \"preview_url\": \"/api/stream\"\n";
        response.body += "}";

        return response;
    });

    // 注册静态文件路由处理程序
    rest_handler_->registerRoute("GET", "/index.html", [this](const HttpRequest& request) -> HttpResponse {
        HttpResponse response;
        response.status_code = 200;
        response.status_message = "OK";
        response.content_type = "text/html";

        // 读取静态文件
        std::string file_path = "/home/orangepi/Qworkspace/cam_server_cpp/static/index.html";
        LOG_INFO("尝试读取文件: " + file_path, "ApiServer");
        std::cerr << "[API] 尝试读取文件: " << file_path << std::endl;

        // 检查文件是否存在
        struct stat st;
        if (stat(file_path.c_str(), &st) != 0) {
            LOG_ERROR("文件不存在: " + file_path + ", 错误: " + std::string(strerror(errno)), "ApiServer");
            std::cerr << "[API] 文件不存在: " << file_path << ", 错误: " << strerror(errno) << std::endl;
        } else {
            LOG_INFO("文件存在，大小: " + std::to_string(st.st_size) + " 字节", "ApiServer");
            std::cerr << "[API] 文件存在，大小: " << st.st_size << " 字节" << std::endl;
        }

        std::ifstream file(file_path);
        if (file.is_open()) {
            LOG_INFO("文件打开成功: " + file_path, "ApiServer");
            std::cerr << "[API] 文件打开成功: " << file_path << std::endl;
            std::stringstream buffer;
            buffer << file.rdbuf();
            response.body = buffer.str();
            file.close();
            LOG_INFO("文件读取成功，大小: " + std::to_string(response.body.size()) + " 字节", "ApiServer");
            std::cerr << "[API] 文件读取成功，大小: " << response.body.size() << " 字节" << std::endl;
        } else {
            LOG_ERROR("无法打开文件: " + file_path + ", 错误: " + std::string(strerror(errno)), "ApiServer");
            std::cerr << "[API] 无法打开文件: " << file_path << ", 错误: " << strerror(errno) << std::endl;
            response.status_code = 404;
            response.status_message = "Not Found";
            response.body = "<html><body><h1>404 Not Found</h1><p>The requested file was not found.</p></body></html>";
        }

        return response;
    });

    rest_handler_->registerRoute("GET", "/cameras.html", [this](const HttpRequest& request) -> HttpResponse {
        HttpResponse response;
        response.status_code = 200;
        response.status_message = "OK";
        response.content_type = "text/html";

        // 读取静态文件
        std::string file_path = "/home/orangepi/Qworkspace/cam_server_cpp/static/cameras.html";
        std::ifstream file(file_path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            response.body = buffer.str();
            file.close();
        } else {
            response.status_code = 404;
            response.status_message = "Not Found";
            response.body = "<html><body><h1>404 Not Found</h1><p>The requested file was not found.</p></body></html>";
        }

        return response;
    });

    rest_handler_->registerRoute("GET", "/camera_select.html", [this](const HttpRequest& request) -> HttpResponse {
        HttpResponse response;
        response.status_code = 200;
        response.status_message = "OK";
        response.content_type = "text/html";

        // 读取静态文件
        std::string file_path = "/home/orangepi/Qworkspace/cam_server_cpp/static/camera_select.html";
        std::ifstream file(file_path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            response.body = buffer.str();
            file.close();
        } else {
            response.status_code = 404;
            response.status_message = "Not Found";
            response.body = "<html><body><h1>404 Not Found</h1><p>The requested file was not found.</p></body></html>";
        }

        return response;
    });

    rest_handler_->registerRoute("GET", "/system_info.html", [this](const HttpRequest& request) -> HttpResponse {
        HttpResponse response;
        response.status_code = 200;
        response.status_message = "OK";
        response.content_type = "text/html";

        // 读取静态文件
        std::string file_path = "/home/orangepi/Qworkspace/cam_server_cpp/static/system_info.html";
        std::ifstream file(file_path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            response.body = buffer.str();
            file.close();
        } else {
            response.status_code = 404;
            response.status_message = "Not Found";
            response.body = "<html><body><h1>404 Not Found</h1><p>The requested file was not found.</p></body></html>";
        }

        return response;
    });

    // 注册CSS文件路由
    rest_handler_->registerRoute("GET", "/css/style.css", [this](const HttpRequest& request) -> HttpResponse {
        HttpResponse response;
        response.status_code = 200;
        response.status_message = "OK";
        response.content_type = "text/css";

        // 读取静态文件
        std::string file_path = "/home/orangepi/Qworkspace/cam_server_cpp/static/css/style.css";
        std::ifstream file(file_path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            response.body = buffer.str();
            file.close();
        } else {
            response.status_code = 404;
            response.status_message = "Not Found";
            response.body = "/* CSS file not found */";
        }

        return response;
    });

    // 注册根路径重定向
    rest_handler_->registerRoute("GET", "/", [](const HttpRequest& request) -> HttpResponse {
        HttpResponse response;
        response.status_code = 302;  // 临时重定向
        response.status_message = "Found";
        response.headers["Location"] = "/index.html";
        response.body = "<html><body><h1>Redirecting...</h1><p>Please click <a href=\"/index.html\">here</a> if you are not redirected automatically.</p></body></html>";

        return response;
    });

    // 注册其他API路由...
}

// 生成唯一的客户端ID
std::string ApiServer::generateClientId() {
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
