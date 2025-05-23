#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <fstream>
#include <iterator>
#include <map>
#include <opencv2/opencv.hpp>
#include "vision/frame_processor.h"

// 包含Crow头文件
#define CROW_MAIN
#include "third_party/crow/crow.h"

// 包含项目头文件
#include "include/camera/camera_manager.h"
#include "include/monitor/logger.h"

using namespace cam_server;

/**
 * @brief 客户端连接信息
 */
struct ClientInfo {
    crow::websocket::connection* conn;
    std::string current_device;
};

/**
 * @brief WebSocket视频流测试服务器
 *
 * 这个程序演示如何通过WebSocket实时传输摄像头视频流
 */
class WebSocketVideoStreamServer {
public:
    WebSocketVideoStreamServer() : is_running_(false), frame_count_(0) {
        // 注意：CameraManager是单例，我们需要使用不同的方法
        // 暂时使用单个CameraManager实例，后续可以扩展
    }

    bool initialize() {
        std::cout << "🚀 初始化WebSocket视频流服务器..." << std::endl;

        // 初始化摄像头管理器（单例）
        auto& camera_manager = camera::CameraManager::getInstance();
        if (!camera_manager.initialize("config/config.json")) {
            std::cout << "❌ 摄像头管理器初始化失败" << std::endl;
            return false;
        }

        std::cout << "✅ 摄像头管理器初始化完成" << std::endl;
        return true;
    }

    bool start() {
        std::cout << "🚀 启动WebSocket视频流服务器..." << std::endl;

        try {
            // 设置基本路由
            CROW_ROUTE(app_, "/")
            ([](const crow::request& /*req*/) {
                return "WebSocket Video Stream Server";
            });

            // 设置静态文件服务
            CROW_ROUTE(app_, "/test_video_stream.html")
            ([](const crow::request& /*req*/) {
                std::ifstream file("test_video_stream.html");
                if (!file.is_open()) {
                    return crow::response(404, "File not found");
                }

                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                file.close();

                crow::response res(200, content);
                res.set_header("Content-Type", "text/html; charset=utf-8");
                return res;
            });

            // 修复版测试页面
            CROW_ROUTE(app_, "/websocket_video_test_fixed.html")
            ([](const crow::request& /*req*/) {
                std::ifstream file("websocket_video_test_fixed.html");
                if (!file.is_open()) {
                    return crow::response(404, "File not found");
                }

                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                file.close();

                crow::response res(200, content);
                res.set_header("Content-Type", "text/html; charset=utf-8");
                return res;
            });

            // 双摄像头控制页面
            CROW_ROUTE(app_, "/dual_camera_control.html")
            ([](const crow::request& /*req*/) {
                std::ifstream file("dual_camera_control.html");
                if (!file.is_open()) {
                    return crow::response(404, "File not found");
                }

                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                file.close();

                crow::response res(200, content);
                res.set_header("Content-Type", "text/html; charset=utf-8");
                return res;
            });

            // 单摄像头推流页面
            CROW_ROUTE(app_, "/single_camera_stream.html")
            ([](const crow::request& /*req*/) {
                std::ifstream file("single_camera_stream.html");
                if (!file.is_open()) {
                    return crow::response(404, "File not found");
                }

                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                file.close();

                crow::response res(200, content);
                res.set_header("Content-Type", "text/html; charset=utf-8");
                return res;
            });

            // 设置WebSocket视频流路由
            CROW_ROUTE(app_, "/ws/video")
            .websocket(&app_)
            .onopen([this](crow::websocket::connection& conn) {
                std::string client_id = generateClientId();
                std::cout << "📱 新的视频流客户端连接: " << client_id << std::endl;

                // 存储连接信息
                {
                    std::lock_guard<std::mutex> lock(clients_mutex_);
                    clients_[client_id] = {&conn, ""};  // 连接和当前设备路径
                }

                // 发送欢迎消息
                conn.send_text("{\"type\":\"welcome\",\"client_id\":\"" + client_id + "\",\"message\":\"视频流连接成功\"}");

                // 测试：发送一个小的二进制数据
                std::string test_data = "TEST_BINARY_DATA_123456789";
                conn.send_binary(test_data);
                std::cout << "🧪 发送测试二进制数据，大小: " << test_data.size() << " 字节" << std::endl;
            })
            .onclose([this](crow::websocket::connection& conn, const std::string& reason, uint16_t code) {
                std::cout << "📱 视频流客户端断开连接，原因: " << reason << ", 代码: " << code << std::endl;

                // 查找并移除客户端
                std::lock_guard<std::mutex> lock(clients_mutex_);
                for (auto it = clients_.begin(); it != clients_.end(); ++it) {
                    if (it->second.conn == &conn) {
                        std::string client_id = it->first;
                        std::cout << "🗑️ 移除客户端: " << client_id << std::endl;
                        clients_.erase(it);
                        break;
                    }
                }
            })
            .onmessage([this](crow::websocket::connection& conn, const std::string& data, bool /*is_binary*/) {
                std::cout << "📨 收到客户端消息: " << data << std::endl;

                try {
                    // 尝试解析JSON命令
                    std::string device_path = "/dev/video0"; // 默认设备

                    // 简单的JSON解析，查找device字段
                    size_t device_pos = data.find("\"device\":");
                    if (device_pos != std::string::npos) {
                        size_t start = data.find("\"", device_pos + 9);
                        if (start != std::string::npos) {
                            size_t end = data.find("\"", start + 1);
                            if (end != std::string::npos) {
                                device_path = data.substr(start + 1, end - start - 1);
                            }
                        }
                    }

                    // 处理客户端命令
                    if (data.find("start_camera") != std::string::npos) {
                        handleStartCamera(conn, device_path);
                    } else if (data.find("stop_camera") != std::string::npos) {
                        handleStopCamera(conn);
                    } else if (data.find("get_status") != std::string::npos) {
                        handleGetStatus(conn);
                    } else if (data.find("get_info") != std::string::npos) {
                        handleGetInfo(conn);
                    } else {
                        conn.send_text("{\"type\":\"error\",\"message\":\"未知命令\"}");
                    }
                } catch (const std::exception& e) {
                    std::cout << "❌ 处理消息时发生错误: " << e.what() << std::endl;
                    conn.send_text("{\"type\":\"error\",\"message\":\"命令处理失败\"}");
                }
            });

            // 启动服务器
            is_running_ = true;
            server_thread_ = std::thread([this]() {
                app_.port(8081).multithreaded().run();
            });

            std::cout << "✅ WebSocket视频流服务器启动成功，监听端口: 8081" << std::endl;
            std::cout << "📱 WebSocket视频流地址: ws://localhost:8081/ws/video" << std::endl;
            return true;

        } catch (const std::exception& e) {
            std::cout << "❌ 启动失败: " << e.what() << std::endl;
            return false;
        }
    }

    void stop() {
        if (!is_running_) {
            return;
        }

        std::cout << "🛑 停止WebSocket视频流服务器..." << std::endl;

        // 停止摄像头
        auto& camera_manager = camera::CameraManager::getInstance();
        camera_manager.stopCapture();
        camera_manager.closeDevice();

        is_running_ = false;
        app_.stop();

        if (server_thread_.joinable()) {
            server_thread_.join();
        }

        std::cout << "✅ 服务器已停止" << std::endl;
    }

    void waitForStop() {
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

private:
    void handleStartCamera(crow::websocket::connection& conn, const std::string& device_path = "/dev/video0") {
        std::cout << "🎥 处理启动摄像头命令，设备: " << device_path << std::endl;

        try {
            // 检查设备路径是否支持
            if (device_path != "/dev/video0" && device_path != "/dev/video2") {
                conn.send_text("{\"type\":\"error\",\"message\":\"不支持的摄像头设备: " + device_path + "\"}");
                return;
            }

            auto& camera_manager = camera::CameraManager::getInstance();

            // 如果当前设备已打开，先关闭
            if (camera_manager.isDeviceOpen()) {
                camera_manager.stopCapture();
                camera_manager.closeDevice();
            }

            // 打开指定的摄像头设备
            if (!camera_manager.openDevice(device_path, 640, 480, 30)) {
                conn.send_text("{\"type\":\"error\",\"message\":\"无法打开摄像头设备: " + device_path + "\"}");
                return;
            }

            // 找到当前客户端并更新其设备路径
            std::string client_id;
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                for (auto& [id, client_info] : clients_) {
                    if (client_info.conn == &conn) {
                        client_info.current_device = device_path;
                        client_id = id;
                        break;
                    }
                }
            }

            // 设置帧回调，将帧数据发送给对应的WebSocket客户端
            camera_manager.setFrameCallback([this, device_path](const camera::Frame& frame) {
                handleFrame(frame, device_path);
            });

            // 启动捕获
            if (!camera_manager.startCapture()) {
                conn.send_text("{\"type\":\"error\",\"message\":\"无法启动摄像头捕获\"}");
                return;
            }

            conn.send_text("{\"type\":\"success\",\"message\":\"摄像头已启动，视频流开始传输\"}");
            std::cout << "✅ 摄像头启动成功，设备: " << device_path << std::endl;

        } catch (const std::exception& e) {
            std::cout << "❌ 启动摄像头时发生错误: " << e.what() << std::endl;
            conn.send_text("{\"type\":\"error\",\"message\":\"启动摄像头失败\"}");
        }
    }

    void handleStopCamera(crow::websocket::connection& conn) {
        std::cout << "🛑 处理停止摄像头命令..." << std::endl;

        try {
            // 找到当前客户端使用的设备
            std::string device_path;
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                for (auto& [id, client_info] : clients_) {
                    if (client_info.conn == &conn) {
                        device_path = client_info.current_device;
                        client_info.current_device = ""; // 清空设备路径
                        break;
                    }
                }
            }

            if (!device_path.empty()) {
                auto& camera_manager = camera::CameraManager::getInstance();
                camera_manager.stopCapture();
                std::cout << "✅ 摄像头停止成功，设备: " << device_path << std::endl;
            }

            conn.send_text("{\"type\":\"success\",\"message\":\"摄像头已停止\"}");

        } catch (const std::exception& e) {
            std::cout << "❌ 停止摄像头时发生错误: " << e.what() << std::endl;
            conn.send_text("{\"type\":\"error\",\"message\":\"停止摄像头失败\"}");
        }
    }

    void handleGetStatus(crow::websocket::connection& conn) {
        try {
            // 找到当前客户端使用的设备
            std::string device_path;
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                for (auto& [id, client_info] : clients_) {
                    if (client_info.conn == &conn) {
                        device_path = client_info.current_device;
                        break;
                    }
                }
            }

            bool is_open = false;
            bool is_capturing = false;

            auto& camera_manager = camera::CameraManager::getInstance();
            is_open = camera_manager.isDeviceOpen();
            is_capturing = camera_manager.isCapturing();

            size_t client_count = 0;
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                client_count = clients_.size();
            }

            std::string status_json =
                "{\"type\":\"status\","
                "\"camera_open\":" + std::string(is_open ? "true" : "false") + ","
                "\"camera_capturing\":" + std::string(is_capturing ? "true" : "false") + ","
                "\"device_path\":\"" + device_path + "\","
                "\"client_count\":" + std::to_string(client_count) + ","
                "\"frame_count\":" + std::to_string(frame_count_.load()) + "}";

            conn.send_text(status_json);

        } catch (const std::exception& e) {
            std::cout << "❌ 获取状态时发生错误: " << e.what() << std::endl;
            conn.send_text("{\"type\":\"error\",\"message\":\"获取状态失败\"}");
        }
    }

    void handleGetInfo(crow::websocket::connection& conn) {
        try {
            std::cout << "📊 处理获取系统信息命令..." << std::endl;

            // 获取系统信息
            std::string info_json = "{"
                "\"type\":\"info\","
                "\"server_version\":\"1.0.0\","
                "\"supported_devices\":["
                    "{\"path\":\"/dev/video0\",\"name\":\"DECXIN CAMERA\",\"type\":\"stream\"},"
                    "{\"path\":\"/dev/video1\",\"name\":\"DECXIN CAMERA\",\"type\":\"metadata\"},"
                    "{\"path\":\"/dev/video2\",\"name\":\"USB Camera\",\"type\":\"stream\"},"
                    "{\"path\":\"/dev/video3\",\"name\":\"USB Camera\",\"type\":\"metadata\"}"
                "],"
                "\"active_devices\":[";

            // 添加活动设备信息
            auto& camera_manager = camera::CameraManager::getInstance();
            if (camera_manager.isDeviceOpen()) {
                // 获取当前打开的设备路径（从客户端信息中推断）
                std::string current_device = "/dev/video0"; // 默认值
                {
                    std::lock_guard<std::mutex> lock(clients_mutex_);
                    for (const auto& [id, client_info] : clients_) {
                        if (!client_info.current_device.empty()) {
                            current_device = client_info.current_device;
                            break;
                        }
                    }
                }
                info_json += "{\"path\":\"" + current_device + "\",\"capturing\":" +
                            (camera_manager.isCapturing() ? "true" : "false") + "}";
            }

            info_json += "],"
                "\"total_clients\":" + std::to_string(clients_.size()) + ","
                "\"total_frames\":" + std::to_string(frame_count_.load()) + ","
                "\"supported_formats\":[\"MJPEG\",\"YUYV\"],"
                "\"max_resolution\":\"1920x1200\","
                "\"recommended_resolution\":\"640x480\""
                "}";

            conn.send_text(info_json);
            std::cout << "✅ 系统信息发送成功" << std::endl;

        } catch (const std::exception& e) {
            std::cout << "❌ 获取系统信息时发生错误: " << e.what() << std::endl;
            conn.send_text("{\"type\":\"error\",\"message\":\"获取系统信息失败\"}");
        }
    }

    void handleFrame(const camera::Frame& frame, const std::string& device_path) {
        if (!is_running_) {
            return;
        }

        frame_count_++;

        // 调试信息：确认帧回调被调用
        if (frame_count_ % 50 == 1) {
            std::cout << "🎬 handleFrame被调用，帧号: " << frame_count_ << ", 设备: " << device_path << std::endl;
        }

        // 检查帧格式
        if (frame.getFormat() != camera::PixelFormat::MJPEG) {
            std::cout << "⚠️ 收到非MJPEG格式的帧，设备: " << device_path << ", 格式: " << static_cast<int>(frame.getFormat()) << std::endl;
            return;
        }

        // 获取MJPEG数据
        const auto& frame_data = frame.getData();
        if (frame_data.empty()) {
            std::cout << "⚠️ 收到空帧数据，设备: " << device_path << std::endl;
            return;
        }

        // 调试信息：确认帧数据大小
        if (frame_count_ % 50 == 1) {
            std::cout << "📊 帧数据大小: " << frame_data.size() << " 字节，设备: " << device_path << std::endl;
        }

        // 直接发送MJPEG帧数据给所有匹配的客户端
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (const auto& [client_id, client_info] : clients_) {
            // 调试信息：显示设备匹配情况
            if (frame_count_ % 50 == 1) {  // 每50帧输出一次调试信息
                std::cout << "🔍 客户端 " << client_id << " 设备匹配检查: "
                         << "client_device='" << client_info.current_device
                         << "', frame_device='" << device_path << "'" << std::endl;
            }

            if (client_info.current_device == device_path) {
                try {
                    // 尝试不同的发送方式
                    // 方式1：直接从vector构造string
                    std::string binary_data(reinterpret_cast<const char*>(frame_data.data()), frame_data.size());
                    client_info.conn->send_binary(binary_data);

                    // 调试信息：确认发送的数据大小和前几个字节
                    if (frame_count_ % 50 == 1) {
                        std::cout << "📤 发送帧数据到客户端 " << client_id
                                 << "，大小: " << frame_data.size() << " 字节";
                        if (frame_data.size() >= 4) {
                            std::cout << "，前4字节: "
                                     << std::hex << (int)frame_data[0] << " "
                                     << (int)frame_data[1] << " "
                                     << (int)frame_data[2] << " "
                                     << (int)frame_data[3] << std::dec;
                        }
                        std::cout << std::endl;
                    }

                } catch (const std::exception& e) {
                    std::cout << "❌ 发送帧数据到客户端失败: " << client_id
                             << ", 设备: " << device_path << ", 错误: " << e.what() << std::endl;
                }
            }
        }

        // 每100帧输出一次统计信息
        if (frame_count_ % 100 == 0) {
            std::cout << "📊 已处理 " << frame_count_ << " 帧，设备: " << device_path
                     << "，原始帧大小: " << frame_data.size() << " 字节" << std::endl;
        }
    }

    std::string generateClientId() {
        static std::atomic<uint64_t> next_id{1};
        return "video-client-" + std::to_string(next_id++);
    }

private:
    crow::SimpleApp app_;
    std::thread server_thread_;
    std::atomic<bool> is_running_;
    std::atomic<uint64_t> frame_count_;

    // 客户端连接管理
    std::mutex clients_mutex_;
    std::unordered_map<std::string, ClientInfo> clients_;
};

int main() {
    std::cout << "🎥 WebSocket视频流测试程序" << std::endl;
    std::cout << "=============================" << std::endl;

    WebSocketVideoStreamServer server;

    // 初始化服务器
    if (!server.initialize()) {
        std::cout << "❌ 服务器初始化失败" << std::endl;
        return 1;
    }

    // 启动服务器
    if (!server.start()) {
        std::cout << "❌ 服务器启动失败" << std::endl;
        return 1;
    }

    std::cout << std::endl;
    std::cout << "📋 使用说明:" << std::endl;
    std::cout << "1. 在浏览器中打开 test_video_stream.html" << std::endl;
    std::cout << "2. 或者连接WebSocket: ws://localhost:8081/ws/video" << std::endl;
    std::cout << "3. 发送命令测试视频流功能" << std::endl;
    std::cout << std::endl;
    std::cout << "💡 测试命令:" << std::endl;
    std::cout << "   启动摄像头: {\"action\":\"start_camera\"}" << std::endl;
    std::cout << "   停止摄像头: {\"action\":\"stop_camera\"}" << std::endl;
    std::cout << "   获取状态:   {\"action\":\"get_status\"}" << std::endl;
    std::cout << std::endl;
    std::cout << "按 Ctrl+C 停止服务器..." << std::endl;

    // 等待服务器停止
    server.waitForStop();

    return 0;
}
