#include <iostream>
#include <string>
#include <memory>
#include <csignal>
#include <thread>
#include <chrono>
#include <getopt.h>

// 包含各模块的头文件
#include "camera/camera_manager.h"
#include "video/video_recorder.h"
#include "video/video_splitter.h"
#include "api/api_server.h"
#include "storage/storage_manager.h"
#include "storage/file_manager.h"
#include "monitor/logger.h"
#include "system/system_monitor.h"
#include "utils/config_manager.h"

using namespace cam_server;

// 全局变量，用于信号处理
volatile sig_atomic_t g_running = 1;

// 信号处理函数
void signal_handler(int signal) {
    std::cout << "接收到信号: " << signal << std::endl;
    g_running = 0;
}

// 打印欢迎信息
void print_welcome() {
    std::cout << "=======================================" << std::endl;
    std::cout << "  RK3588 摄像头服务器 (C++ 版本)" << std::endl;
    std::cout << "  版本: 0.1.0" << std::endl;
    std::cout << "=======================================" << std::endl;
    std::cout << "正在初始化..." << std::endl;
}

// 打印使用说明
void print_usage(const char* program_name) {
    std::cout << "用法: " << program_name << " [选项]" << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  -h, --help                 显示此帮助信息" << std::endl;
    std::cout << "  -c, --config <文件>        指定配置文件路径" << std::endl;
    std::cout << "  -v, --version              显示版本信息" << std::endl;
    std::cout << "  -d, --device <设备路径>    指定摄像头设备" << std::endl;
    std::cout << "  -r, --resolution <宽x高>   指定分辨率" << std::endl;
    std::cout << "  -f, --fps <帧率>           指定帧率" << std::endl;
    std::cout << "  -o, --output <目录>        指定输出目录" << std::endl;
    std::cout << "  -l, --log-level <级别>     指定日志级别 (trace, debug, info, warning, error, critical)" << std::endl;
}

// 解析命令行参数
bool parse_args(int argc, char* argv[], std::string& config_path, std::string& device_path,
               std::string& resolution, int& fps, std::string& output_dir, std::string& log_level) {
    static struct option long_options[] = {
        {"help",       no_argument,       0, 'h'},
        {"version",    no_argument,       0, 'v'},
        {"config",     required_argument, 0, 'c'},
        {"device",     required_argument, 0, 'd'},
        {"resolution", required_argument, 0, 'r'},
        {"fps",        required_argument, 0, 'f'},
        {"output",     required_argument, 0, 'o'},
        {"log-level",  required_argument, 0, 'l'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "hvc:d:r:f:o:l:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return false;
            case 'v':
                std::cout << "RK3588 摄像头服务器 (C++ 版本) v0.1.0" << std::endl;
                return false;
            case 'c':
                config_path = optarg;
                break;
            case 'd':
                device_path = optarg;
                break;
            case 'r':
                resolution = optarg;
                break;
            case 'f':
                fps = std::atoi(optarg);
                break;
            case 'o':
                output_dir = optarg;
                break;
            case 'l':
                log_level = optarg;
                break;
            default:
                std::cerr << "错误: 未知选项" << std::endl;
                print_usage(argv[0]);
                return false;
        }
    }

    return true;
}

// 初始化日志系统
bool initialize_logger(const std::string& log_level) {
    monitor::LoggerConfig config;
    config.log_file = "logs/cam_server.log";

    // 设置日志级别
    if (log_level == "trace") {
        config.min_level = monitor::LogLevel::TRACE;
    } else if (log_level == "debug") {
        config.min_level = monitor::LogLevel::DEBUG;
    } else if (log_level == "info") {
        config.min_level = monitor::LogLevel::INFO;
    } else if (log_level == "warning") {
        config.min_level = monitor::LogLevel::WARNING;
    } else if (log_level == "error") {
        config.min_level = monitor::LogLevel::ERROR;
    } else if (log_level == "critical") {
        config.min_level = monitor::LogLevel::CRITICAL;
    } else {
        config.min_level = monitor::LogLevel::INFO;
    }

    config.console_output = true;
    config.file_output = true;
    config.include_timestamp = true;
    config.include_level = true;
    config.include_source = true;
    config.include_thread_id = true;
    config.include_file_line = (config.min_level <= monitor::LogLevel::DEBUG);
    config.include_function = (config.min_level <= monitor::LogLevel::DEBUG);
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

// 初始化存储管理器
bool initialize_storage(const std::string& output_dir) {
    auto& config = utils::ConfigManager::getInstance();

    storage::StorageConfig storage_config;

    // 如果命令行参数指定了输出目录，则使用命令行参数
    if (!output_dir.empty()) {
        storage_config.video_dir = output_dir + "/videos";
        storage_config.image_dir = output_dir + "/images";
        storage_config.archive_dir = output_dir + "/archives";
        storage_config.temp_dir = output_dir + "/temp";
    } else {
        // 否则使用配置文件中的值
        storage_config.video_dir = config.getString("storage.video_dir", "data/videos");
        storage_config.image_dir = config.getString("storage.image_dir", "data/images");
        storage_config.archive_dir = config.getString("storage.archive_dir", "data/archives");
        storage_config.temp_dir = config.getString("storage.temp_dir", "data/temp");
    }

    storage_config.min_free_space = config.getInt("storage.min_free_space", 1024 * 1024 * 1024);  // 1GB
    storage_config.auto_cleanup_threshold = config.getDouble("storage.auto_cleanup_threshold", 0.9);  // 90%
    storage_config.auto_cleanup_keep_days = config.getInt("storage.auto_cleanup_keep_days", 30);  // 30天

    return storage::StorageManager::getInstance().initialize(storage_config);
}

// 初始化文件管理器
bool initialize_file_manager() {
    auto& storage_manager = storage::StorageManager::getInstance();
    return storage::FileManager::getInstance().initialize(storage_manager.getVideoDir());
}

// 初始化摄像头管理器
bool initialize_camera_manager(const std::string& device_path, const std::string& resolution, int fps) {
    auto& config = utils::ConfigManager::getInstance();

    // 如果命令行参数未指定，则使用配置文件中的值
    std::string device = device_path.empty() ? config.getString("camera.device", "/dev/video0") : device_path;
    std::string res = resolution.empty() ? config.getString("camera.resolution", "640x480") : resolution;
    int frame_rate = fps <= 0 ? config.getInt("camera.fps", 30) : fps;

    // 解析分辨率
    int width = 640, height = 480;
    size_t pos = res.find('x');
    if (pos != std::string::npos) {
        width = std::atoi(res.substr(0, pos).c_str());
        height = std::atoi(res.substr(pos + 1).c_str());
    }

    // 初始化摄像头管理器
    auto& camera_manager = camera::CameraManager::getInstance();
    if (!camera_manager.initialize("config/camera.json")) {
        return false;
    }

    // 打开摄像头设备
    return camera_manager.openDevice(device, width, height, frame_rate);
}

// 初始化API服务器
bool initialize_api_server() {
    auto& config = utils::ConfigManager::getInstance();

    api::ApiServerConfig api_config;
    api_config.address = config.getString("api.address", "0.0.0.0");
    api_config.port = config.getInt("api.port", 8080);
    api_config.static_files_dir = config.getString("api.static_files_dir", "web");
    api_config.use_https = config.getBool("api.use_https", false);
    api_config.ssl_cert_path = config.getString("api.ssl_cert_path", "");
    api_config.ssl_key_path = config.getString("api.ssl_key_path", "");
    api_config.enable_cors = config.getBool("api.enable_cors", true);
    api_config.cors_allowed_origins = config.getString("api.cors_allowed_origins", "*");
    api_config.enable_api_key = config.getBool("api.enable_api_key", false);
    api_config.api_key = config.getString("api.api_key", "");
    api_config.log_level = config.getString("api.log_level", "info");

    return api::ApiServer::getInstance().initialize(api_config);
}

// 初始化系统监控器
bool initialize_system_monitor() {
    auto& config = utils::ConfigManager::getInstance();

    int update_interval_ms = config.getInt("monitor.interval_ms", 1000);

    return system::SystemMonitor::getInstance().initialize(update_interval_ms);
}

int main(int argc, char* argv[]) {
    // 设置信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // 打印欢迎信息
    print_welcome();

    // 解析命令行参数
    std::string config_path = "config/config.json";
    std::string device_path;
    std::string resolution;
    int fps = 0;
    std::string output_dir;
    std::string log_level = "info";

    if (!parse_args(argc, argv, config_path, device_path, resolution, fps, output_dir, log_level)) {
        return 1;
    }

    try {
        // 初始化日志系统
        if (!initialize_logger(log_level)) {
            std::cerr << "初始化日志系统失败" << std::endl;
            return 1;
        }

        LOG_INFO("摄像头服务器启动中...", "Main");

        // 初始化配置管理器
        if (!initialize_config(config_path)) {
            LOG_ERROR("初始化配置管理器失败", "Main");
            return 1;
        }

        // 初始化存储管理器
        if (!initialize_storage(output_dir)) {
            LOG_ERROR("初始化存储管理器失败", "Main");
            return 1;
        }

        // 初始化文件管理器
        if (!initialize_file_manager()) {
            LOG_ERROR("初始化文件管理器失败", "Main");
            return 1;
        }

        // 初始化摄像头管理器
        if (!initialize_camera_manager(device_path, resolution, fps)) {
            LOG_ERROR("初始化摄像头管理器失败", "Main");
            return 1;
        }

        // 初始化API服务器
        if (!initialize_api_server()) {
            LOG_ERROR("初始化API服务器失败", "Main");
            return 1;
        }

        // 初始化系统监控器
        if (!initialize_system_monitor()) {
            LOG_ERROR("初始化系统监控器失败", "Main");
            return 1;
        }

        // 启动API服务器
        if (!api::ApiServer::getInstance().start()) {
            LOG_ERROR("启动API服务器失败", "Main");
            return 1;
        }

        // 启动系统监控器
        if (!system::SystemMonitor::getInstance().start()) {
            LOG_ERROR("启动系统监控器失败", "Main");
            return 1;
        }

        LOG_INFO("摄像头服务器启动成功", "Main");
        std::cout << "所有模块初始化完成，服务器已启动" << std::endl;
        std::cout << "按 Ctrl+C 停止服务器" << std::endl;

        // 主循环
        while (g_running) {
            // 执行周期性任务
            // 1. 检查存储空间
            storage::StorageManager::getInstance().autoCleanup(false);

            // 2. 更新系统状态
            // 这部分由SystemMonitor自动完成

            std::this_thread::sleep_for(std::chrono::seconds(10));
        }

        LOG_INFO("摄像头服务器正在关闭...", "Main");
        std::cout << "正在关闭服务器..." << std::endl;

        // 停止系统监控器
        system::SystemMonitor::getInstance().stop();

        // 停止API服务器
        api::ApiServer::getInstance().stop();

        // 关闭摄像头
        camera::CameraManager::getInstance().closeDevice();

        LOG_INFO("摄像头服务器已关闭", "Main");
        std::cout << "服务器已关闭" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        LOG_CRITICAL("未处理的异常: " + std::string(e.what()), "Main");
        return 1;
    }

    return 0;
}
