#include <iostream>  // for std::cout, std::cerr, std::endl
#include <string>
#include <memory>
#include <csignal>
#include <thread>
#include <chrono>
#include <getopt.h>
#include <sys/stat.h>
#include <cstdlib>  // for system()
#include <fstream>
#include <sstream>
#include <sys/utsname.h>
#include <locale.h>  // 添加locale头文件，用于设置本地化
#include <filesystem>

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
#include "utils/debug_utils.h"
#include "utils/file_utils.h"
#include "utils/string_utils.h"

using namespace cam_server;

// 全局变量，用于信号处理
volatile sig_atomic_t g_running = 1;
volatile sig_atomic_t g_signal_received = 0;
api::ApiServer* g_api_server = nullptr;

// 信号处理函数
void signal_handler(int signal) {
    std::cout << "\n收到信号: " << signal << std::endl;
    
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "正在优雅地关闭服务器..." << std::endl;
        g_running = 0;
        
        if (g_api_server) {
            g_api_server->stop();
        }
    }
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

// 创建目录（如果不存在）
void create_directory_if_not_exists(const std::string& dir_path) {
    try {
        if (!std::filesystem::exists(dir_path)) {
            std::filesystem::create_directories(dir_path);
            std::cout << "已创建目录: " << dir_path << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "创建目录失败 '" << dir_path << "': " << e.what() << std::endl;
    }
}

// 初始化日志系统
bool initialize_logger(const std::string& log_level_str) {
    // 创建日志目录
    create_directory_if_not_exists("logs");

    // 配置日志系统
    monitor::LogConfig config;
    config.log_file = "logs/cam_server.log";
    config.console_output = true;
    config.file_output = true;
    config.include_timestamp = true;
    config.include_level = true;
    config.include_source = true;
    config.include_thread_id = true;
    config.include_file_line = true;
    config.include_function = true;
    config.date_format = "%Y-%m-%d %H:%M:%S";

    // 设置日志级别
    if (log_level_str == "trace") {
        config.min_level = monitor::LogLevel::TRACE;
    } else if (log_level_str == "debug") {
        config.min_level = monitor::LogLevel::DEBUG;
    } else if (log_level_str == "info") {
        config.min_level = monitor::LogLevel::INFO;
    } else if (log_level_str == "warning") {
        config.min_level = monitor::LogLevel::WARNING;
    } else if (log_level_str == "error") {
        config.min_level = monitor::LogLevel::ERROR;
    } else if (log_level_str == "critical") {
        config.min_level = monitor::LogLevel::FATAL;
    } else {
        // 默认为INFO级别
        config.min_level = monitor::LogLevel::INFO;
    }

    // 初始化日志系统
    return monitor::Logger::getInstance().initialize(config);
}

// 初始化配置管理器
bool initialize_config(const std::string& config_path) {
    std::cerr << "========== 开始初始化配置管理器 ==========" << std::endl;
    std::cerr << "配置文件路径: " << config_path << std::endl;

    // 检查配置文件是否存在
    struct stat st;
    if (stat(config_path.c_str(), &st) != 0) {
        std::cerr << "错误: 配置文件不存在: " << config_path << std::endl;
        std::cerr << "========== 配置管理器初始化失败 ==========" << std::endl;
        return false;
    }

    std::cerr << "配置文件检查成功，大小: " << st.st_size << " 字节" << std::endl;

    // 尝试初始化配置管理器
    auto& config_manager = utils::ConfigManager::getInstance();
    std::cerr << "正在调用ConfigManager::initialize()..." << std::endl;
    bool result = config_manager.initialize(config_path);

    if (result) {
        std::cerr << "配置管理器初始化成功!" << std::endl;

        // 打印一些关键配置项，验证配置是否正确加载
        std::cerr << "验证关键配置项:" << std::endl;
        std::cerr << "  API地址: " << config_manager.getString("api.address", "未设置") << std::endl;
        std::cerr << "  API端口: " << config_manager.getInt("api.port", -1) << std::endl;
        std::cerr << "  静态文件目录: " << config_manager.getString("api.static_files_dir", "未设置") << std::endl;
        std::cerr << "  摄像头设备: " << config_manager.getString("camera.device", "未设置") << std::endl;
    } else {
        std::cerr << "错误: 配置管理器初始化失败!" << std::endl;
    }

    std::cerr << "========== 配置管理器初始化" << (result ? "成功" : "失败") << " ==========" << std::endl;
    return result;
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
    LOG_INFO("初始化文件管理器开始", "Main");
    auto& storage_manager = storage::StorageManager::getInstance();
    LOG_INFO("获取视频目录: " + storage_manager.getVideoDir(), "Main");
    bool result = storage::FileManager::getInstance().initialize(storage_manager.getVideoDir());
    LOG_INFO("文件管理器初始化结果: " + std::string(result ? "成功" : "失败"), "Main");
    return result;
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
    // 使用主配置文件，不需要单独的摄像头配置文件
    std::string config_path = "/home/orangepi/Qworkspace/cam_server_cpp/config/config.json";
    LOG_INFO("使用配置文件: " + config_path, "Main");
    if (!camera_manager.initialize(config_path)) {
        return false;
    }

    // 打开摄像头设备
    LOG_INFO("正在打开摄像头设备: " + device + ", 分辨率: " + std::to_string(width) + "x" + std::to_string(height) + ", 帧率: " + std::to_string(frame_rate), "Main");

    bool result = camera_manager.openDevice(device, width, height, frame_rate);

    LOG_INFO("打开摄像头设备结果: " + std::string(result ? "成功" : "失败"), "Main");

    return result;
}

// 初始化API服务器
bool initialize_api_server() {
    LOG_INFO("正在初始化API服务器...", "Main");
    DEBUG_INFO("MAIN", "正在初始化API服务器...");
    auto& config = utils::ConfigManager::getInstance();

    api::ApiServerConfig api_config;
    api_config.address = config.getString("api.address", "0.0.0.0");
    api_config.port = config.getInt("api.port", 8080);
    api_config.static_files_dir = config.getString("api.static_files_dir", "static");
    api_config.use_https = config.getBool("api.use_https", false);
    api_config.ssl_cert_path = config.getString("api.ssl_cert_path", "");
    api_config.ssl_key_path = config.getString("api.ssl_key_path", "");
    api_config.enable_cors = config.getBool("api.enable_cors", true);
    api_config.cors_allowed_origins = config.getString("api.cors_allowed_origins", "*");
    api_config.enable_api_key = config.getBool("api.enable_api_key", false);
    api_config.api_key = config.getString("api.api_key", "");
    api_config.log_level = config.getString("api.log_level", "info");

    LOG_INFO("API服务器配置: 地址=" + api_config.address + ", 端口=" + std::to_string(api_config.port) +
             ", 静态文件目录=" + api_config.static_files_dir, "Main");
    DEBUG_INFO("MAIN", "API服务器配置: 地址=" + api_config.address + ", 端口=" + std::to_string(api_config.port) +
             ", 静态文件目录=" + api_config.static_files_dir);

    // 检查静态文件目录是否存在
    struct stat st;
    if (stat(api_config.static_files_dir.c_str(), &st) != 0) {
        LOG_WARNING("静态文件目录不存在: " + api_config.static_files_dir + "，尝试创建", "Main");
        try {
            // 使用系统命令创建目录
            std::string cmd = "mkdir -p " + api_config.static_files_dir;
            int result = ::system(cmd.c_str());
            if (result == 0) {
                LOG_INFO("已创建静态文件目录: " + api_config.static_files_dir, "Main");
            } else {
                LOG_ERROR("无法创建静态文件目录，错误码: " + std::to_string(result), "Main");
            }
        } catch (const std::exception& e) {
            LOG_ERROR("无法创建静态文件目录: " + std::string(e.what()), "Main");
        }
    }

    return api::ApiServer::getInstance().initialize(api_config);
}

// 初始化系统监控器
bool initialize_system_monitor() {
    auto& config = utils::ConfigManager::getInstance();

    int update_interval_ms = config.getInt("monitor.interval_ms", 1000);

    return system::SystemMonitor::getInstance().initialize(update_interval_ms);
}

// 记录系统信息到文件
void logSystemInfo() {
    std::ofstream logFile("system_info.log", std::ios::app);
    
    if (!logFile.is_open()) {
        monitor::Logger::getInstance().error("无法打开系统信息日志文件", "Main");
        return;
    }
    
    // 获取时间
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    char time_str[100];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t_now));
    
    logFile << "==================================================\n";
    logFile << "系统信息记录 - " << time_str << "\n";
    logFile << "==================================================\n";
    
    // 获取系统信息
    struct utsname sys_info;
    if (uname(&sys_info) == 0) {
        logFile << "系统名称: " << sys_info.sysname << "\n";
        logFile << "节点名称: " << sys_info.nodename << "\n";
        logFile << "发行版本: " << sys_info.release << "\n";
        logFile << "版本信息: " << sys_info.version << "\n";
        logFile << "硬件架构: " << sys_info.machine << "\n";
    }
    
    // 读取CPU信息
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        std::string modelName;
        int cpuCores = 0;
        
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") != std::string::npos) {
                modelName = line.substr(line.find(":") + 2);
            }
            if (line.find("processor") != std::string::npos) {
                cpuCores++;
            }
        }
        
        logFile << "CPU型号: " << modelName << "\n";
        logFile << "CPU核心数: " << cpuCores << "\n";
    }
    
    // 读取内存信息
    std::ifstream meminfo("/proc/meminfo");
    if (meminfo.is_open()) {
        std::string line;
        std::string totalMem;
        
        while (std::getline(meminfo, line)) {
            if (line.find("MemTotal") != std::string::npos) {
                totalMem = line.substr(line.find(":") + 1);
                break;
            }
        }
        
        logFile << "总内存: " << totalMem << "\n";
    }
    
    logFile << "--------------------------------------------------\n";
    logFile << "摄像头服务器版本: 0.1.0\n";
    logFile << "编译时间: " << __DATE__ << " " << __TIME__ << "\n";
    logFile << "==================================================\n\n";
    
    logFile.close();
    
    monitor::Logger::getInstance().info("系统信息已记录到 system_info.log", "Main");
}

int main(int argc, char* argv[]) {
    // 设置本地化，解决乱码问题
    setlocale(LC_ALL, "zh_CN.UTF-8");
    
    // 设置信号处理
    std::signal(SIGINT, signal_handler);   // Ctrl+C
    std::signal(SIGTERM, signal_handler);  // 终止信号
    std::signal(SIGABRT, signal_handler);  // 异常终止信号
    std::signal(SIGQUIT, signal_handler);  // Ctrl+\

    // 记录系统信息
    logSystemInfo();

    // 打印欢迎信息
    print_welcome();

    // 解析命令行参数
    std::string config_path = "/home/orangepi/Qworkspace/cam_server_cpp/config/config.json";
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
        LOG_INFO("正在初始化配置管理器...", "Main");

        // 初始化配置管理器
        if (!initialize_config(config_path)) {
            LOG_ERROR("初始化配置管理器失败", "Main");
            return 1;
        }
        LOG_INFO("配置管理器初始化成功", "Main");

        // 初始化存储管理器
        LOG_INFO("正在初始化存储管理器...", "Main");
        if (!initialize_storage(output_dir)) {
            LOG_ERROR("初始化存储管理器失败", "Main");
            return 1;
        }
        LOG_INFO("存储管理器初始化成功", "Main");

        // 初始化文件管理器
        LOG_INFO("正在初始化文件管理器...", "Main");
        if (!initialize_file_manager()) {
            LOG_ERROR("初始化文件管理器失败", "Main");
            return 1;
        }
        LOG_INFO("文件管理器初始化成功", "Main");

        // 初始化摄像头管理器
        LOG_INFO("正在初始化摄像头管理器...", "Main");
        if (!initialize_camera_manager(device_path, resolution, fps)) {
            LOG_WARNING("初始化摄像头管理器失败，但将继续运行以测试Web界面", "Main");
            // 不返回错误，继续运行
        } else {
            LOG_INFO("摄像头管理器初始化成功，等待用户通过Web界面配置并启动摄像头", "Main");
            // 注意：此时摄像头管理器已初始化，但未自动打开摄像头设备
            // 用户需要通过Web界面调用API才会真正打开和启动摄像头
        }

        // 初始化API服务器
        LOG_INFO("正在初始化API服务器...", "Main");
        if (!initialize_api_server()) {
            LOG_ERROR("初始化API服务器失败", "Main");
            return 1;
        }
        LOG_INFO("API服务器初始化成功", "Main");

        // 初始化系统监控器
        if (!initialize_system_monitor()) {
            LOG_ERROR("初始化系统监控器失败", "Main");
            return 1;
        }

        // 启动API服务器
        LOG_INFO("正在启动API服务器...", "Main");
        if (!api::ApiServer::getInstance().start()) {
            LOG_ERROR("启动API服务器失败", "Main");
            return 1;
        }
        LOG_INFO("API服务器启动成功", "Main");

        // 检查API服务器状态
        auto status = api::ApiServer::getInstance().getStatus();
        std::string state_str =
            (status.state == api::ApiServerState::RUNNING ? "运行中" :
             status.state == api::ApiServerState::STOPPED ? "已停止" :
             status.state == api::ApiServerState::STARTING ? "正在启动" :
             status.state == api::ApiServerState::STOPPING ? "正在停止" :
             status.state == api::ApiServerState::ERROR ? "错误" : "未知");
        LOG_INFO("API服务器状态: " + state_str, "Main");
        LOG_INFO("API服务器地址: " + status.address + ":" + std::to_string(status.port), "Main");

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
            try {
                // 执行周期性任务
                // 1. 检查存储空间
                storage::StorageManager::getInstance().autoCleanup(false);

                // 2. 更新系统状态
                // 这部分由SystemMonitor自动完成

                // 使用较短的睡眠时间，以便更快地响应信号
                for (int i = 0; i < 10 && g_running; ++i) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));

                    // 检查是否收到信号
                    if (g_signal_received) {
                        std::cout << "正在处理信号: " << g_signal_received << "..." << std::endl;
                        break;
                    }
                }
            } catch (const std::exception& e) {
                LOG_ERROR("主循环异常: " + std::string(e.what()), "Main");
                std::cerr << "主循环异常: " << e.what() << std::endl;
                break;
            } catch (...) {
                LOG_ERROR("主循环未知异常", "Main");
                std::cerr << "主循环未知异常" << std::endl;
                break;
            }
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
        LOG_FATAL("未处理的异常: " + std::string(e.what()), "Main");
        return EXIT_FAILURE;
    }

    return 0;
}
