#include <iostream>
#include <csignal>
#include <memory>
#include <thread>
#include <chrono>

#include "web/video_server.h"
#include "monitor/logger.h"

using namespace cam_server;

// 全局服务器实例
std::unique_ptr<web::VideoServer> g_server;

/**
 * @brief 信号处理函数
 */
void signalHandler(int signal) {
    std::cout << "\n🛑 接收到信号 " << signal << "，正在关闭服务器..." << std::endl;

    if (g_server) {
        g_server->stop();
        g_server.reset();
    }

    std::cout << "✅ 服务器已安全关闭" << std::endl;
    exit(0);
}

/**
 * @brief 显示使用帮助
 */
void showUsage(const char* program_name) {
    std::cout << "📖 使用方法:" << std::endl;
    std::cout << "  " << program_name << " [选项]" << std::endl;
    std::cout << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  -p, --port <端口>    设置服务器端口 (默认: 8081)" << std::endl;
    std::cout << "  -h, --help          显示此帮助信息" << std::endl;
    std::cout << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << program_name << " -p 8080" << std::endl;
    std::cout << "  " << program_name << " --port 9000" << std::endl;
}

/**
 * @brief 解析命令行参数
 */
int parseArguments(int argc, char* argv[]) {
    int port = 8081; // 默认端口

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            showUsage(argv[0]);
            exit(0);
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                try {
                    port = std::stoi(argv[++i]);
                    if (port < 1 || port > 65535) {
                        std::cerr << "❌ 错误: 端口必须在 1-65535 范围内" << std::endl;
                        exit(1);
                    }
                } catch (const std::exception& e) {
                    std::cerr << "❌ 错误: 无效的端口号 '" << argv[i] << "'" << std::endl;
                    exit(1);
                }
            } else {
                std::cerr << "❌ 错误: -p/--port 需要指定端口号" << std::endl;
                exit(1);
            }
        } else {
            std::cerr << "❌ 错误: 未知参数 '" << arg << "'" << std::endl;
            showUsage(argv[0]);
            exit(1);
        }
    }

    return port;
}

/**
 * @brief 主函数
 */
int main(int argc, char* argv[]) {
    std::cout << "🎥 深视边缘视觉平台 v2.0 (DeepVision Edge Platform)" << std::endl;
    std::cout << "================================" << std::endl;

    // 解析命令行参数
    int port = parseArguments(argc, argv);

    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    try {
        // 创建服务器实例
        g_server = std::make_unique<web::VideoServer>();
        g_server->setPort(port);

        // 初始化服务器
        if (!g_server->initialize()) {
            std::cerr << "❌ 服务器初始化失败" << std::endl;
            return 1;
        }

        std::cout << "✅ 服务器初始化完成" << std::endl;

        // 启动服务器
        if (!g_server->start()) {
            std::cerr << "❌ 服务器启动失败" << std::endl;
            return 1;
        }

        std::cout << "🚀 服务器已启动，端口: " << port << std::endl;
        std::cout << "🌐 访问地址: http://localhost:" << port << std::endl;
        std::cout << "📋 主要功能页面:" << std::endl;
        std::cout << "  - 🏠 主页: http://localhost:" << port << "/" << std::endl;
        std::cout << "  - 🎬 视频录制: http://localhost:" << port << "/video_recording.html" << std::endl;
        std::cout << "  - 🖼️ 帧提取: http://localhost:" << port << "/frame_extraction.html" << std::endl;
        std::cout << "  - 📸 拍照功能: http://localhost:" << port << "/photo_capture.html" << std::endl;
        std::cout << "  - 🖥️ 系统信息: http://localhost:" << port << "/system_info.html" << std::endl;
        std::cout << "  - 🔌 串口信息: http://localhost:" << port << "/serial_info.html" << std::endl;
        std::cout << std::endl;
        std::cout << "💡 按 Ctrl+C 停止服务器" << std::endl;

        // 等待服务器停止
        g_server->waitForStop();

    } catch (const std::exception& e) {
        std::cerr << "❌ 服务器运行异常: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
