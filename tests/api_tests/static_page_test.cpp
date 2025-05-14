#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <cassert>
#include <curl/curl.h>

#include "api/api_server.h"
#include "utils/config_manager.h"
#include "monitor/logger.h"

using namespace cam_server;

// 用于存储CURL响应的结构体
struct CurlResponse {
    std::string data;
    size_t size;
};

// CURL写回调函数
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t real_size = size * nmemb;
    CurlResponse* response = static_cast<CurlResponse*>(userp);

    response->data.append(static_cast<char*>(contents), real_size);
    response->size += real_size;

    return real_size;
}

// 初始化日志系统
bool initialize_logger() {
    monitor::LoggerConfig config;
    config.log_file = "logs/test.log";
    config.min_level = monitor::LogLevel::DEBUG;
    config.console_output = true;
    config.file_output = true;
    config.include_timestamp = true;
    config.include_level = true;
    config.include_source = true;
    config.include_thread_id = true;
    config.include_file_line = true;
    config.include_function = true;
    config.max_file_size = 10 * 1024 * 1024;  // 10MB
    config.max_file_count = 5;
    config.async_logging = true;
    config.async_queue_size = 1000;

    return monitor::Logger::getInstance().initialize(config);
}

// 初始化配置管理器
bool initialize_config(const std::string& config_path) {
    return utils::ConfigManager::getInstance().initialize(config_path);
}

// 初始化API服务器
bool initialize_api_server() {
    auto& config = utils::ConfigManager::getInstance();

    api::ApiServerConfig api_config;
    api_config.address = "0.0.0.0";  // 使用所有可用地址
    api_config.port = 8082;  // 使用另一个端口
    api_config.static_files_dir = "static";
    api_config.use_https = false;
    api_config.ssl_cert_path = "";
    api_config.ssl_key_path = "";
    api_config.enable_cors = true;
    api_config.cors_allowed_origins = "*";
    api_config.enable_api_key = false;
    api_config.api_key = "";
    api_config.log_level = "debug";

    std::cout << "API服务器配置: 地址=" << api_config.address << ", 端口=" << api_config.port
              << ", 静态文件目录=" << api_config.static_files_dir << std::endl;

    return api::ApiServer::getInstance().initialize(api_config);
}

// 测试静态页面加载
bool test_static_page() {
    std::cout << "测试静态页面加载..." << std::endl;

    // 检查服务器状态
    auto status = api::ApiServer::getInstance().getStatus();
    std::cout << "测试前API服务器状态: "
              << "地址=" << status.address
              << ", 端口=" << status.port
              << ", 状态=" << static_cast<int>(status.state)
              << ", 错误信息=" << status.error_message << std::endl;

    if (status.state != api::ApiServerState::RUNNING) {
        std::cerr << "API服务器未处于运行状态，无法进行测试" << std::endl;
        return false;
    }

    // 初始化CURL
    std::cout << "初始化CURL..." << std::endl;
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "无法初始化CURL" << std::endl;
        return false;
    }

    // 设置URL
    std::string url = "http://127.0.0.1:8082/test.html";
    std::cout << "设置URL: " << url << std::endl;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // 设置写回调
    CurlResponse response = {std::string(), 0};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // 设置超时
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);  // 增加超时时间
    std::cout << "设置超时时间: 10秒" << std::endl;

    // 设置详细信息
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    // 执行请求
    std::cout << "执行HTTP请求..." << std::endl;
    CURLcode res = curl_easy_perform(curl);

    // 检查结果
    bool success = false;
    if (res == CURLE_OK) {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        std::cout << "HTTP响应码: " << http_code << std::endl;
        std::cout << "响应大小: " << response.size << " 字节" << std::endl;

        // 检查HTTP状态码和响应内容
        if (http_code == 200 && response.size > 0 &&
            response.data.find("服务器测试页面") != std::string::npos) {
            std::cout << "测试通过: 成功加载静态页面" << std::endl;
            success = true;
        } else {
            std::cerr << "测试失败: 无法正确加载静态页面" << std::endl;
            if (http_code != 200) {
                std::cerr << "HTTP状态码不是200" << std::endl;
            }
            if (response.size == 0) {
                std::cerr << "响应内容为空" << std::endl;
            }
            if (response.data.find("服务器测试页面") == std::string::npos) {
                std::cerr << "响应内容不包含预期文本" << std::endl;
            }
        }
    } else {
        std::cerr << "CURL错误: " << curl_easy_strerror(res) << std::endl;
    }

    // 清理CURL
    curl_easy_cleanup(curl);

    return success;
}

int main() {
    std::cout << "开始静态页面加载测试..." << std::endl;

    // 初始化日志系统
    if (!initialize_logger()) {
        std::cerr << "初始化日志系统失败" << std::endl;
        return 1;
    }

    // 初始化配置管理器
    if (!initialize_config("config/config.json")) {
        std::cerr << "初始化配置管理器失败" << std::endl;
        return 1;
    }

    // 初始化API服务器
    std::cout << "开始初始化API服务器..." << std::endl;
    bool init_result = initialize_api_server();
    std::cout << "API服务器初始化结果: " << (init_result ? "成功" : "失败") << std::endl;

    if (!init_result) {
        std::cerr << "初始化API服务器失败" << std::endl;
        return 1;
    }

    // 启动API服务器
    std::cout << "正在启动API服务器..." << std::endl;

    // 获取API服务器实例
    auto& api_server = api::ApiServer::getInstance();

    // 检查API服务器状态
    auto status_before = api_server.getStatus();
    std::cout << "启动前API服务器状态: "
              << "地址=" << status_before.address
              << ", 端口=" << status_before.port
              << ", 状态=" << static_cast<int>(status_before.state)
              << ", 错误信息=" << status_before.error_message << std::endl;

    // 尝试启动API服务器
    bool start_result = api_server.start();
    std::cout << "API服务器启动结果: " << (start_result ? "成功" : "失败") << std::endl;

    if (!start_result) {
        std::cerr << "启动API服务器失败" << std::endl;
        return 1;
    }

    // 再次检查API服务器状态
    auto status_after = api_server.getStatus();
    std::cout << "启动后API服务器状态: "
              << "地址=" << status_after.address
              << ", 端口=" << status_after.port
              << ", 状态=" << static_cast<int>(status_after.state)
              << ", 错误信息=" << status_after.error_message << std::endl;

    // 检查服务器状态
    auto status = api::ApiServer::getInstance().getStatus();
    std::cout << "API服务器状态: "
              << "地址=" << status.address
              << ", 端口=" << status.port
              << ", 状态=" << static_cast<int>(status.state)
              << ", 错误信息=" << status.error_message << std::endl;

    std::cout << "API服务器已启动，等待5秒钟..." << std::endl;

    // 检查静态文件是否存在
    std::cout << "检查静态文件: static/test.html" << std::endl;
    FILE* file = fopen("static/test.html", "r");
    if (file) {
        std::cout << "静态文件存在" << std::endl;
        fclose(file);
    } else {
        std::cerr << "静态文件不存在或无法访问" << std::endl;
        perror("fopen");
    }

    // 检查端口是否打开
    std::cout << "检查端口8082是否打开..." << std::endl;
    system("netstat -tuln | grep 8082");

    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 执行测试
    bool test_result = test_static_page();

    // 停止API服务器
    api::ApiServer::getInstance().stop();

    std::cout << "测试完成，结果: " << (test_result ? "通过" : "失败") << std::endl;

    return test_result ? 0 : 1;
}
