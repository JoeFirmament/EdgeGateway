#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <csignal>

#include "api/api_server.h"
#include "utils/config_manager.h"
#include "monitor/logger.h"

using namespace cam_server;

// 全局变量，用于信号处理
volatile sig_atomic_t g_running = 1;

// 信号处理函数
void signal_handler(int sig) {
    std::cout << "接收到信号: " << sig << std::endl;
    g_running = 0;
}

// 初始化日志系统
bool initialize_logger() {
    monitor::LoggerConfig config;
    config.log_file = "logs/simple_test.log";
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
bool initialize_config() {
    // 创建一个简单的配置
    utils::ConfigManager& config_manager = utils::ConfigManager::getInstance();
    
    // 不从文件加载，直接设置必要的配置
    return true;
}

// 初始化API服务器
bool initialize_api_server() {
    api::ApiServerConfig api_config;
    api_config.address = "0.0.0.0";  // 使用所有可用地址
    api_config.port = 8082;  // 使用端口8082
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

int main() {
    // 设置信号处理
    std::signal(SIGINT, signal_handler);   // Ctrl+C
    std::signal(SIGTERM, signal_handler);  // 终止信号

    std::cout << "简单服务器测试启动..." << std::endl;
    
    // 初始化日志系统
    if (!initialize_logger()) {
        std::cerr << "初始化日志系统失败" << std::endl;
        return 1;
    }
    
    // 初始化配置管理器
    if (!initialize_config()) {
        std::cerr << "初始化配置管理器失败" << std::endl;
        return 1;
    }
    
    // 初始化API服务器
    std::cout << "初始化API服务器..." << std::endl;
    if (!initialize_api_server()) {
        std::cerr << "初始化API服务器失败" << std::endl;
        return 1;
    }
    
    // 启动API服务器
    std::cout << "启动API服务器..." << std::endl;
    if (!api::ApiServer::getInstance().start()) {
        std::cerr << "启动API服务器失败" << std::endl;
        return 1;
    }
    
    // 检查服务器状态
    auto status = api::ApiServer::getInstance().getStatus();
    std::cout << "API服务器状态: " 
              << "地址=" << status.address 
              << ", 端口=" << status.port 
              << ", 状态=" << static_cast<int>(status.state) 
              << ", 错误信息=" << status.error_message << std::endl;
    
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
    
    std::cout << "服务器已启动，按Ctrl+C停止..." << std::endl;
    
    // 主循环
    while (g_running) {
        // 每秒检查一次状态
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 检查服务器状态
        auto current_status = api::ApiServer::getInstance().getStatus();
        if (current_status.state != api::ApiServerState::RUNNING) {
            std::cerr << "API服务器状态异常: " << static_cast<int>(current_status.state) << std::endl;
            std::cerr << "错误信息: " << current_status.error_message << std::endl;
            break;
        }
    }
    
    // 停止API服务器
    std::cout << "停止API服务器..." << std::endl;
    api::ApiServer::getInstance().stop();
    
    std::cout << "测试完成" << std::endl;
    
    return 0;
}
