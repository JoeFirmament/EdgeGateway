#include "web/websocket_handler.h"
#include "camera/camera_manager.h"
#include "monitor/logger.h"
#include <iostream>
#include <sstream>

namespace cam_server {
namespace web {

void WebSocketHandler::setupRoutes(crow::SimpleApp& app, VideoServer* server) {
    // 设置WebSocket视频流路由 - 直接从原始实现复制
    // 为什么使用WebSocket：支持双向通信，低延迟，适合实时视频流
    // 如何使用：前端连接 ws://server:port/ws/video
    CROW_ROUTE(app, "/ws/video")
    .websocket(&app)
    .onopen([server](crow::websocket::connection& conn) {
        std::string client_id = server->generateClientId();
        std::cout << "📱 新的视频流客户端连接: " << client_id << std::endl;

        // 存储连接信息
        {
            std::lock_guard<std::mutex> lock(server->getClientsMutex());
            auto& clients = server->getClients();
            clients[client_id] = {&conn, ""};  // 连接和当前设备路径
        }

        // 发送欢迎消息
        conn.send_text("{\"type\":\"welcome\",\"client_id\":\"" + client_id + "\",\"message\":\"视频流连接成功\"}");

        // 测试：发送一个小的二进制数据
        std::string test_data = "TEST_BINARY_DATA_123456789";
        conn.send_binary(test_data);
        std::cout << "🧪 发送测试二进制数据，大小: " << test_data.size() << " 字节" << std::endl;
    })
    .onclose([server](crow::websocket::connection& conn, const std::string& reason, uint16_t code) {
        std::cout << "📱 视频流客户端断开连接，原因: " << reason << ", 代码: " << code << std::endl;

        // 查找并移除客户端
        std::lock_guard<std::mutex> lock(server->getClientsMutex());
        auto& clients = server->getClients();
        for (auto it = clients.begin(); it != clients.end(); ++it) {
            if (it->second.conn == &conn) {
                std::string client_id = it->first;
                std::cout << "🗑️ 移除客户端: " << client_id << std::endl;
                clients.erase(it);
                break;
            }
        }
    })
    .onmessage([server](crow::websocket::connection& conn, const std::string& data, bool /*is_binary*/) {
        std::cout << "📨 收到客户端消息: " << data << std::endl;

        try {
            // 尝试解析JSON命令 - 直接从原始实现复制
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

            // 处理客户端命令 - 修正方法调用
            if (data.find("start_camera") != std::string::npos) {
                handleStartCamera(conn, device_path, server);
            } else if (data.find("stop_camera") != std::string::npos) {
                handleStopCamera(conn, server);
            } else if (data.find("capture_photo") != std::string::npos) {
                handleCapturePhoto(conn, data, server);
            } else if (data.find("start_recording") != std::string::npos) {
                handleStartRecording(conn, data, server);
            } else if (data.find("stop_recording") != std::string::npos) {
                handleStopRecording(conn, server);
            } else if (data.find("get_recording_status") != std::string::npos) {
                handleGetRecordingStatus(conn, server);
            } else if (data.find("get_status") != std::string::npos) {
                handleGetStatus(conn, server);
            } else if (data.find("get_info") != std::string::npos) {
                handleGetInfo(conn, server);
            } else {
                conn.send_text("{\"type\":\"error\",\"message\":\"未知命令\"}");
            }
        } catch (const std::exception& e) {
            std::cout << "❌ 处理消息时发生错误: " << e.what() << std::endl;
            conn.send_text("{\"type\":\"error\",\"message\":\"命令处理失败\"}");
        }
    });
}

void WebSocketHandler::onOpen(crow::websocket::connection& conn, VideoServer* server) {
    // WebSocket连接建立处理
    // 为什么需要这个：初始化客户端状态，分配唯一ID，发送欢迎消息

    // 生成唯一的客户端ID - 用于区分不同的连接
    std::string client_id = server->generateClientId();

    // 线程安全地添加客户端到管理列表
    // 为什么需要锁：多个客户端可能同时连接，避免竞态条件
    {
        std::lock_guard<std::mutex> lock(server->getClientsMutex());
        auto& clients = server->getClients();
        clients[client_id] = {&conn, ""};
    }

    // 发送欢迎消息 - 告知客户端连接成功和可用功能
    // 为什么发送JSON：结构化数据便于前端解析和处理
    std::string welcome_msg = "{"
        "\"type\":\"welcome\","
        "\"client_id\":\"" + client_id + "\","
        "\"message\":\"WebSocket连接已建立\","
        "\"available_commands\":[\"start_camera\",\"stop_camera\",\"get_status\"]"
        "}";

    conn.send_text(welcome_msg);

    // 记录连接事件 - 便于调试和监控
    std::cout << "📱 新客户端连接: " + client_id << std::endl;
}

void WebSocketHandler::onMessage(crow::websocket::connection& conn, const std::string& data,
                                bool is_binary, VideoServer* server) {
    // WebSocket消息处理 - 解析客户端命令并执行相应操作
    // 为什么需要这个：实现客户端与服务器的交互控制

    // 忽略二进制消息 - 当前只处理文本命令
    // 为什么这样做：控制命令通常是文本格式，二进制数据用于视频流
    if (is_binary) {
        std::cout << "⚠️ 忽略二进制消息" << std::endl;
        return;
    }

    try {
        // 解析JSON命令 - 使用Crow内置的JSON解析器
        // 为什么用JSON：结构化格式，支持复杂参数，易于扩展
        auto json_data = crow::json::load(data);
        if (!json_data) {
            // 发送错误响应 - 告知客户端命令格式错误
            std::string error_msg = "{\"type\":\"error\",\"message\":\"无效的JSON格式\"}";
            conn.send_text(error_msg);
            return;
        }

        // 简化的命令解析 - 与原始实现保持一致
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

        // 命令分发处理 - 与原始实现保持一致的字符串匹配
        if (data.find("start_camera") != std::string::npos) {
            handleStartCamera(conn, device_path, server);
        } else if (data.find("stop_camera") != std::string::npos) {
            handleStopCamera(conn, server);
        } else if (data.find("capture_photo") != std::string::npos) {
            handleCapturePhoto(conn, data, server);
        } else if (data.find("start_recording") != std::string::npos) {
            handleStartRecording(conn, data, server);
        } else if (data.find("stop_recording") != std::string::npos) {
            handleStopRecording(conn, server);
        } else if (data.find("get_recording_status") != std::string::npos) {
            handleGetRecordingStatus(conn, server);
        } else if (data.find("get_status") != std::string::npos) {
            handleGetStatus(conn, server);
        } else if (data.find("get_info") != std::string::npos) {
            handleGetInfo(conn, server);
        } else {
            conn.send_text("{\"type\":\"error\",\"message\":\"未知命令\"}");
        }

    } catch (const std::exception& e) {
        // 异常处理 - 确保服务器稳定性
        std::string error_msg = "{"
            "\"type\":\"error\","
            "\"message\":\"处理消息时发生错误: " + std::string(e.what()) + "\""
            "}";
        conn.send_text(error_msg);
        std::cout << "❌ WebSocket消息处理错误: " << e.what() << std::endl;
    }
}

void WebSocketHandler::onClose(crow::websocket::connection& conn, const std::string& reason,
                              VideoServer* server) {
    // WebSocket连接关闭处理 - 清理客户端状态和资源
    // 为什么需要这个：避免内存泄漏，释放摄像头资源

    // 查找并移除客户端 - 线程安全操作
    std::lock_guard<std::mutex> lock(server->getClientsMutex());
    auto& clients = server->getClients();

    // 遍历查找对应的连接 - 根据连接指针匹配
    // 为什么这样查找：连接对象是唯一标识符
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (it->second.conn == &conn) {
            std::string client_id = it->first;
            std::string device = it->second.current_device;

            // 如果客户端正在使用摄像头，则释放资源
            // 为什么需要释放：避免摄像头被占用，影响其他客户端
            if (!device.empty()) {
                try {
                    auto& camera_manager = camera::CameraManager::getInstance();
                    camera_manager.stopCapture();
                    camera_manager.closeDevice();
                    std::cout << "🔌 客户端断开时自动停止摄像头: " << device << std::endl;
                } catch (const std::exception& e) {
                    std::cout << "⚠️ 停止摄像头时出错: " << e.what() << std::endl;
                }
            }

            // 从客户端列表中移除
            clients.erase(it);
            std::cout << "👋 客户端断开: " << client_id << " (原因: " << reason << ")" << std::endl;
            break;
        }
    }
}

void WebSocketHandler::onError(crow::websocket::connection& conn, VideoServer* server) {
    // WebSocket错误处理 - 处理连接错误情况
    // 为什么需要这个：网络问题或协议错误时的清理工作
    std::cout << "❌ WebSocket连接错误" << std::endl;

    // 错误时也需要清理资源 - 调用关闭处理逻辑
    onClose(conn, "connection_error", server);
}

// 私有辅助方法实现

void WebSocketHandler::handleStartCamera(crow::websocket::connection& conn, const std::string& device_path, VideoServer* server) {
    // 启动摄像头的具体实现 - 与原始版本保持一致
    std::cout << "🎥 处理启动摄像头命令，设备: " << device_path << std::endl;

    try {
        // 获取摄像头管理器实例
        auto& camera_manager = camera::CameraManager::getInstance();

        // 检查设备路径是否支持
        if (device_path != "/dev/video0" && device_path != "/dev/video2") {
            conn.send_text("{\"type\":\"error\",\"message\":\"不支持的摄像头设备: " + device_path + "\"}");
            return;
        }

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
        {
            std::lock_guard<std::mutex> lock(server->getClientsMutex());
            auto& clients = server->getClients();
            for (auto& [client_id, client_info] : clients) {
                if (client_info.conn == &conn) {
                    client_info.current_device = device_path;
                    break;
                }
            }
        }

        // 设置帧回调，将帧数据发送给对应的WebSocket客户端
        camera_manager.setFrameCallback([server, device_path](const camera::Frame& frame) {
            handleFrame(frame, device_path, server);
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

void WebSocketHandler::handleStopCamera(crow::websocket::connection& conn, VideoServer* server) {
    // 停止摄像头的具体实现 - 与原始版本保持一致
    std::cout << "🛑 处理停止摄像头命令..." << std::endl;

    try {
        // 获取摄像头管理器实例
        auto& camera_manager = camera::CameraManager::getInstance();

        // 找到当前客户端使用的设备
        std::string device_path;
        {
            std::lock_guard<std::mutex> lock(server->getClientsMutex());
            auto& clients = server->getClients();
            for (auto& [id, client_info] : clients) {
                if (client_info.conn == &conn) {
                    device_path = client_info.current_device;
                    client_info.current_device = ""; // 清空设备路径
                    break;
                }
            }
        }

        if (!device_path.empty()) {
            camera_manager.stopCapture();
            std::cout << "✅ 摄像头停止成功，设备: " << device_path << std::endl;
        }

        conn.send_text("{\"type\":\"success\",\"message\":\"摄像头已停止\"}");

    } catch (const std::exception& e) {
        std::cout << "❌ 停止摄像头时发生错误: " << e.what() << std::endl;
        conn.send_text("{\"type\":\"error\",\"message\":\"停止摄像头失败\"}");
    }
}

void WebSocketHandler::handleGetStatus(crow::websocket::connection& conn, VideoServer* server) {
    // 获取系统状态的具体实现
    try {
        // 构建状态信息 - 包含摄像头状态、客户端数量等
        std::lock_guard<std::mutex> lock(server->getClientsMutex());
        auto& clients = server->getClients();

        std::string status_msg = "{"
            "\"type\":\"status\","
            "\"client_count\":" + std::to_string(clients.size()) + ","
            "\"frame_count\":" + std::to_string(server->getFrameCount()) + ","
            "\"is_recording\":" + (server->getIsRecording() ? "true" : "false") + ""
            "}";

        conn.send_text(status_msg);

    } catch (const std::exception& e) {
        std::string error_msg = "{"
            "\"type\":\"error\","
            "\"message\":\"获取状态时发生异常: " + std::string(e.what()) + "\""
            "}";
        conn.send_text(error_msg);
    }
}



void WebSocketHandler::handleGetInfo(crow::websocket::connection& conn, VideoServer* server) {
    // 获取摄像头信息的具体实现
    try {
        // 获取摄像头管理器实例
        auto& camera_manager = camera::CameraManager::getInstance();

        std::string info_msg = std::string("{") +
            "\"type\":\"info\"," +
            "\"device_open\":" + (camera_manager.isDeviceOpen() ? "true" : "false") + "," +
            "\"capturing\":" + (camera_manager.isCapturing() ? "true" : "false") +
            "}";

        conn.send_text(info_msg);

    } catch (const std::exception& e) {
        conn.send_text("{\"type\":\"error\",\"message\":\"获取信息失败\"}");
    }
}

void WebSocketHandler::handleFrame(const camera::Frame& frame, const std::string& device_path, VideoServer* server) {
    // 处理摄像头帧数据 - 发送给所有连接的客户端
    try {
        std::lock_guard<std::mutex> lock(server->getClientsMutex());
        auto& clients = server->getClients();

        // 增加帧计数器
        server->getFrameCount()++;

        // 遍历所有客户端，发送帧数据给使用相同设备的客户端
        for (auto& [client_id, client_info] : clients) {
            if (client_info.current_device == device_path && client_info.conn) {
                try {
                    // 发送二进制帧数据
                    const auto& frame_data = frame.getData();
                    client_info.conn->send_binary(std::string(frame_data.begin(), frame_data.end()));
                } catch (const std::exception& e) {
                    std::cout << "⚠️ 发送帧数据失败: " << e.what() << std::endl;
                }
            }
        }

    } catch (const std::exception& e) {
        std::cout << "❌ 处理帧数据时发生错误: " << e.what() << std::endl;
    }
}

// 添加缺失的方法实现
void WebSocketHandler::handleCapturePhoto(crow::websocket::connection& conn, const std::string& data, VideoServer* server) {
    // 拍照功能实现 - 简化版本
    std::cout << "📸 处理拍照命令..." << std::endl;
    conn.send_text("{\"type\":\"info\",\"message\":\"拍照功能待实现\"}");
}

void WebSocketHandler::handleStartRecording(crow::websocket::connection& conn, const std::string& data, VideoServer* server) {
    // 开始录制功能实现 - 简化版本
    std::cout << "🎬 处理开始录制命令..." << std::endl;
    conn.send_text("{\"type\":\"info\",\"message\":\"录制功能待实现\"}");
}

void WebSocketHandler::handleStopRecording(crow::websocket::connection& conn, VideoServer* server) {
    // 停止录制功能实现 - 简化版本
    std::cout << "🛑 处理停止录制命令..." << std::endl;
    conn.send_text("{\"type\":\"info\",\"message\":\"停止录制功能待实现\"}");
}

void WebSocketHandler::handleGetRecordingStatus(crow::websocket::connection& conn, VideoServer* server) {
    // 获取录制状态功能实现 - 简化版本
    std::cout << "📊 处理获取录制状态命令..." << std::endl;
    conn.send_text("{\"type\":\"info\",\"message\":\"录制状态功能待实现\"}");
}

} // namespace web
} // namespace cam_server
