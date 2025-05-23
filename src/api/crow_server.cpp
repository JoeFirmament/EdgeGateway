#include "../../include/api/crow_server.h"
#include "../../include/monitor/logger.h"
#include "../../include/utils/file_utils.h"
#include "../../include/system/system_monitor.h"

// 包含Crow头文件
#define CROW_MAIN
#include "crow/crow.h"

#include <iostream>
#include <mutex>
#include <unordered_set>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>

namespace cam_server {
namespace api {

// 简化的WebSocket连接信息
struct SimpleWebSocketConnection {
    std::string client_id;
    crow::websocket::connection* ws;
    bool is_connected;
    std::chrono::steady_clock::time_point last_activity;
};

// CrowServer的简化实现
class CrowServer::Impl {
public:
    Impl() : is_running_(false), request_count_(0), error_count_(0) {}

    ~Impl() {
        stop();
    }

    bool initialize(const CrowServerConfig& config, std::shared_ptr<RestHandler> rest_handler) {
        if (is_initialized_) {
            LOG_WARNING("Crow服务器已经初始化", "CrowServer");
            return true;
        }

        LOG_DEBUG("开始初始化Crow服务器...", "CrowServer");

        try {
            // 验证配置
            if (config.port <= 0 || config.port > 65535) {
                LOG_ERROR("无效的端口号: " + std::to_string(config.port), "CrowServer");
                return false;
            }

            // 保存配置和处理器
            config_ = config;
            rest_handler_ = rest_handler;

            // 重置统计信息
            request_count_ = 0;
            error_count_ = 0;

            // 创建Crow应用
            app_ = std::make_unique<crow::SimpleApp>();

            // 标记为已初始化
            is_initialized_ = true;

            LOG_DEBUG("Crow服务器初始化成功，监听地址: " + config_.address +
                     ":" + std::to_string(config_.port), "CrowServer");
            return true;

        } catch (const std::exception& e) {
            LOG_ERROR("Crow服务器初始化失败: " + std::string(e.what()), "CrowServer");
            is_initialized_ = false;
            return false;
        }
    }

    bool start() {
        if (is_running_) {
            return true;
        }

        if (!is_initialized_ || !app_) {
            LOG_ERROR("Crow服务器未初始化", "CrowServer");
            return false;
        }

        try {
            // 设置路由
            LOG_INFO("正在设置HTTP路由...", "CrowServer");
            setupRoutes();
            LOG_INFO("HTTP路由设置完成", "CrowServer");

            // 设置WebSocket - 使用简化版本
            LOG_INFO("正在设置WebSocket路由...", "CrowServer");
            setupWebSocket();
            LOG_INFO("WebSocket路由设置完成", "CrowServer");

            // 设置摄像头WebSocket路由
            LOG_INFO("正在设置摄像头WebSocket路由...", "CrowServer");
            setupCameraWebSocket();
            LOG_INFO("摄像头WebSocket路由设置完成", "CrowServer");

            // 启动服务器线程
            is_running_ = true;
            server_thread_ = std::thread([this]() {
                LOG_INFO("Crow服务器线程已启动", "CrowServer");

                try {
                    app_->port(config_.port)
                        .multithreaded()
                        .run();
                } catch (const std::exception& e) {
                    LOG_ERROR("Crow服务器运行失败: " + std::string(e.what()), "CrowServer");
                    is_running_ = false;
                }

                LOG_INFO("Crow服务器线程已停止", "CrowServer");
            });

            LOG_INFO("Crow服务器启动成功，监听端口: " + std::to_string(config_.port), "CrowServer");
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("启动Crow服务器失败: " + std::string(e.what()), "CrowServer");
            is_running_ = false;
            return false;
        }
    }

    bool stop() {
        if (!is_running_) {
            return true;
        }

        LOG_DEBUG("正在停止Crow服务器...", "CrowServer");

        try {
            // 停止Crow应用
            if (app_) {
                LOG_DEBUG("正在停止Crow应用...", "CrowServer");
                app_->stop();
            }

            // 等待服务器线程结束
            if (server_thread_.joinable()) {
                LOG_DEBUG("等待服务器线程结束...", "CrowServer");
                server_thread_.detach(); // 使用detach避免阻塞
            }

            is_running_ = false;
            LOG_INFO("Crow服务器已停止", "CrowServer");
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("停止Crow服务器失败: " + std::string(e.what()), "CrowServer");
            is_running_ = false;
            return false;
        }
    }

    bool isRunning() const {
        return is_running_;
    }

    int64_t getRequestCount() const {
        return request_count_.load();
    }

    int64_t getErrorCount() const {
        return error_count_.load();
    }

    void registerWebSocketHandler(
        [[maybe_unused]] const std::string& path,
        [[maybe_unused]] std::function<void(std::string_view, void*, bool, std::string)> message_handler,
        [[maybe_unused]] std::function<void(std::string)> open_handler,
        [[maybe_unused]] std::function<void(std::string, int, std::string_view)> close_handler) {

        LOG_DEBUG("注册WebSocket处理器，路径: " + path, "CrowServer");
        // 简化版本暂时不实现动态注册
    }

    void broadcastWebSocketMessage(const std::string& path, const std::string& message, bool is_binary) {
        std::lock_guard<std::mutex> lock(ws_connections_mutex_);

        for (auto& [client_id, conn] : ws_connections_) {
            if (conn.is_connected && conn.ws) {
                try {
                    if (is_binary) {
                        conn.ws->send_binary(message);
                    } else {
                        conn.ws->send_text(message);
                    }
                    request_count_++;
                } catch (const std::exception& e) {
                    LOG_ERROR("广播消息失败: " + std::string(e.what()), "CrowServer");
                    error_count_++;
                }
            }
        }

        LOG_DEBUG("广播消息到路径: " + path + ", 长度: " + std::to_string(message.size()), "CrowServer");
    }

    bool sendWebSocketMessage(const std::string& client_id, const std::string& message, bool is_binary) {
        std::lock_guard<std::mutex> lock(ws_connections_mutex_);

        auto it = ws_connections_.find(client_id);
        if (it == ws_connections_.end() || !it->second.is_connected || !it->second.ws) {
            LOG_WARNING("找不到客户端: " + client_id, "CrowServer");
            return false;
        }

        try {
            if (is_binary) {
                it->second.ws->send_binary(message);
            } else {
                it->second.ws->send_text(message);
            }
            request_count_++;

            LOG_DEBUG("发送消息到客户端: " + client_id + ", 长度: " + std::to_string(message.size()), "CrowServer");
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("发送消息失败: " + std::string(e.what()), "CrowServer");
            error_count_++;
            return false;
        }
    }

    bool disconnectClient(const std::string& client_id) {
        std::lock_guard<std::mutex> lock(ws_connections_mutex_);

        auto it = ws_connections_.find(client_id);
        if (it == ws_connections_.end() || !it->second.is_connected || !it->second.ws) {
            LOG_WARNING("找不到要断开的客户端: " + client_id, "CrowServer");
            return false;
        }

        try {
            it->second.ws->close();
            it->second.is_connected = false;

            LOG_DEBUG("断开客户端连接: " + client_id, "CrowServer");
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("断开客户端连接失败: " + std::string(e.what()), "CrowServer");
            return false;
        }
    }

private:
    // 设置基本路由
    void setupRoutes() {
        if (!app_ || !rest_handler_) {
            return;
        }

        try {
            // 根路径
            CROW_ROUTE((*app_), "/")
            ([this]() {
                request_count_++;
                return "Cam Server is running! WebSocket: ws://localhost:" + std::to_string(config_.port) + "/ws";
            });

            // 设置静态文件路由 - 使用简单的实现避免编译错误
            if (!config_.static_files_dir.empty()) {
                LOG_DEBUG("设置静态文件路由，目录: " + config_.static_files_dir, "CrowServer");

                // 简单的测试页面路由
                CROW_ROUTE((*app_), "/test_websocket_simple.html")
                ([this]() {
                    request_count_++;
                    std::string file_path = config_.static_files_dir + "/test_websocket_simple.html";
                    std::ifstream file(file_path);
                    if (!file.is_open()) {
                        error_count_++;
                        return crow::response(404, "File not found");
                    }
                    std::string content((std::istreambuf_iterator<char>(file)),
                                      std::istreambuf_iterator<char>());
                    crow::response res(200, content);
                    res.set_header("Content-Type", "text/html; charset=utf-8");
                    return res;
                });

                LOG_DEBUG("静态文件路由设置完成", "CrowServer");
            }

            LOG_DEBUG("基本路由设置完成", "CrowServer");
        } catch (const std::exception& e) {
            LOG_ERROR("路由设置失败: " + std::string(e.what()), "CrowServer");
            throw;
        }
    }

    // 设置WebSocket - 基于工作的测试代码
    void setupWebSocket() {
        LOG_INFO("=== 开始设置WebSocket路由 ===", "CrowServer");

        if (!app_) {
            LOG_ERROR("Crow应用未初始化，无法设置WebSocket", "CrowServer");
            return;
        }

        try {
            // 创建测试WebSocket路由 - 使用与测试代码相同的模式
            LOG_INFO("正在创建测试WebSocket路由: /ws", "CrowServer");

            CROW_WEBSOCKET_ROUTE((*app_), "/ws")
              .onopen([this](crow::websocket::connection& conn) {
                  std::cout << "=== /ws WebSocket连接打开 ===" << std::endl;
                  std::cout << "远程IP: " << conn.get_remote_ip() << std::endl;

                  std::lock_guard<std::mutex> lock(ws_connections_mutex_);

                  // 生成客户端ID
                  std::string client_id = generateClientId();

                  // 存储连接信息
                  SimpleWebSocketConnection ws_conn;
                  ws_conn.client_id = client_id;
                  ws_conn.ws = &conn;
                  ws_conn.is_connected = true;
                  ws_conn.last_activity = std::chrono::steady_clock::now();
                  ws_connections_[client_id] = ws_conn;

                  std::cout << "客户端ID: " << client_id << ", 当前连接数: " << ws_connections_.size() << std::endl;
              })
              .onclose([this](crow::websocket::connection& conn, const std::string& reason, uint16_t code) {
                  std::cout << "=== /ws WebSocket连接关闭 ===" << std::endl;
                  std::cout << "原因: " << reason << ", 代码: " << code << std::endl;

                  std::lock_guard<std::mutex> lock(ws_connections_mutex_);

                  // 清理连接信息
                  for (auto it = ws_connections_.begin(); it != ws_connections_.end(); ++it) {
                      if (it->second.ws == &conn) {
                          ws_connections_.erase(it);
                          break;
                      }
                  }

                  std::cout << "当前连接数: " << ws_connections_.size() << std::endl;
              })
              .onmessage([this](crow::websocket::connection& conn, const std::string& data, [[maybe_unused]] bool is_binary) {
                  std::cout << "=== /ws 收到WebSocket消息 ===" << std::endl;
                  std::cout << "数据: " << data << std::endl;

                  // 回显消息
                  conn.send_text("Echo: " + data);
              });

            LOG_INFO("测试WebSocket路由设置完成", "CrowServer");

        } catch (const std::exception& e) {
            LOG_ERROR("设置WebSocket路由时发生异常: " + std::string(e.what()), "CrowServer");
            throw;
        }

        LOG_INFO("=== WebSocket路由设置完成 ===", "CrowServer");
    }

    // 设置摄像头WebSocket路由
    void setupCameraWebSocket() {
        LOG_INFO("=== 开始设置摄像头WebSocket路由 ===", "CrowServer");

        if (!app_) {
            LOG_ERROR("Crow应用未初始化，无法设置摄像头WebSocket", "CrowServer");
            return;
        }

        try {
            // 创建摄像头WebSocket路由 - /ws/camera
            LOG_INFO("正在创建摄像头WebSocket路由: /ws/camera", "CrowServer");

            CROW_WEBSOCKET_ROUTE((*app_), "/ws/camera")
              .onopen([this](crow::websocket::connection& conn) {
                  std::cout << "=== /ws/camera WebSocket连接打开 ===" << std::endl;
                  std::cout << "远程IP: " << conn.get_remote_ip() << std::endl;

                  std::lock_guard<std::mutex> lock(ws_connections_mutex_);

                  // 生成客户端ID
                  std::string client_id = generateClientId();

                  // 存储连接信息
                  SimpleWebSocketConnection ws_conn;
                  ws_conn.client_id = client_id;
                  ws_conn.ws = &conn;
                  ws_conn.is_connected = true;
                  ws_conn.last_activity = std::chrono::steady_clock::now();
                  ws_connections_[client_id] = ws_conn;

                  std::cout << "摄像头客户端ID: " << client_id << ", 当前连接数: " << ws_connections_.size() << std::endl;

                  // 通知WebSocket摄像头流处理器
                  // TODO: 集成WebSocket摄像头流处理器
              })
              .onclose([this](crow::websocket::connection& conn, const std::string& reason, uint16_t code) {
                  std::cout << "=== /ws/camera WebSocket连接关闭 ===" << std::endl;
                  std::cout << "原因: " << reason << ", 代码: " << code << std::endl;

                  std::lock_guard<std::mutex> lock(ws_connections_mutex_);

                  // 清理连接信息
                  for (auto it = ws_connections_.begin(); it != ws_connections_.end(); ++it) {
                      if (it->second.ws == &conn) {
                          std::cout << "移除摄像头客户端: " << it->second.client_id << std::endl;
                          ws_connections_.erase(it);
                          break;
                      }
                  }

                  std::cout << "当前连接数: " << ws_connections_.size() << std::endl;
              })
              .onmessage([this](crow::websocket::connection& conn, const std::string& data, [[maybe_unused]] bool is_binary) {
                  std::cout << "=== /ws/camera 收到WebSocket消息 ===" << std::endl;
                  std::cout << "数据: " << data << std::endl;

                  // 处理摄像头控制消息
                  try {
                      std::cout << "收到摄像头命令: " << data << std::endl;

                      // 改进的JSON解析和命令处理
                      if (data.find("start_camera") != std::string::npos) {
                          std::cout << "✅ 匹配到启动摄像头命令" << std::endl;
                          conn.send_text("{\"status\":\"success\",\"message\":\"摄像头启动命令已接收\",\"action\":\"start_camera\"}");
                      } else if (data.find("stop_camera") != std::string::npos) {
                          std::cout << "✅ 匹配到停止摄像头命令" << std::endl;
                          conn.send_text("{\"status\":\"success\",\"message\":\"摄像头停止命令已接收\",\"action\":\"stop_camera\"}");
                      } else if (data.find("get_status") != std::string::npos) {
                          std::cout << "✅ 匹配到获取状态命令" << std::endl;
                          conn.send_text("{\"status\":\"success\",\"camera_status\":\"ready\",\"connected_clients\":1}");
                      } else {
                          // 默认回显
                          std::cout << "❓ 未知命令，使用回显模式" << std::endl;
                          conn.send_text("Camera Echo: " + data);
                      }
                  } catch (const std::exception& e) {
                      std::cout << "❌ 处理摄像头消息时发生错误: " << e.what() << std::endl;
                      conn.send_text("{\"status\":\"error\",\"message\":\"命令处理失败\"}");
                  }
              });

            LOG_INFO("摄像头WebSocket路由设置完成", "CrowServer");

        } catch (const std::exception& e) {
            LOG_ERROR("设置摄像头WebSocket路由时发生异常: " + std::string(e.what()), "CrowServer");
            throw;
        }

        LOG_INFO("=== 摄像头WebSocket路由设置完成 ===", "CrowServer");
    }

    // 生成唯一的客户端ID
    std::string generateClientId() {
        static std::atomic<uint64_t> next_id{1};
        return "ws-" + std::to_string(next_id++);
    }



    // 成员变量
    std::unique_ptr<crow::SimpleApp> app_;
    CrowServerConfig config_;
    std::shared_ptr<RestHandler> rest_handler_;
    std::thread server_thread_;
    std::atomic<bool> is_running_;
    bool is_initialized_ = false;
    std::atomic<int64_t> request_count_;
    std::atomic<int64_t> error_count_;

    // WebSocket连接管理
    std::unordered_map<std::string, SimpleWebSocketConnection> ws_connections_;
    std::mutex ws_connections_mutex_;
};

// CrowServer类实现

CrowServer::CrowServer()
    : is_initialized_(false), is_running_(false), request_count_(0), error_count_(0) {
    impl_ = std::make_unique<Impl>();
}

CrowServer::~CrowServer() {
    stop();
}

bool CrowServer::initialize(const CrowServerConfig& config, std::shared_ptr<RestHandler> rest_handler) {
    if (is_initialized_) {
        LOG_WARNING("Crow服务器已经初始化", "CrowServer");
        return true;
    }

    LOG_DEBUG("正在初始化Crow服务器...", "CrowServer");

    config_ = config;
    rest_handler_ = rest_handler;
    is_initialized_ = true;
    is_running_ = false;
    request_count_ = 0;
    error_count_ = 0;

    LOG_DEBUG("Crow服务器初始化完成", "CrowServer");
    return impl_->initialize(config, rest_handler);
}

bool CrowServer::start() {
    if (!is_initialized_) {
        LOG_ERROR("Crow服务器未初始化", "CrowServer");
        return false;
    }

    if (is_running_) {
        LOG_WARNING("Crow服务器已经在运行中", "CrowServer");
        return true;
    }

    LOG_DEBUG("正在启动Crow服务器...", "CrowServer");

    is_running_ = true;

    bool result = impl_->start();
    if (!result) {
        is_running_ = false;
        LOG_ERROR("Crow服务器启动失败", "CrowServer");
        return false;
    }

    LOG_INFO("Crow服务器启动成功", "CrowServer");
    return true;
}

bool CrowServer::stop() {
    if (!is_running_) {
        return true;
    }

    if (!impl_->stop()) {
        LOG_ERROR("停止Crow服务器实现失败", "CrowServer");
        return false;
    }

    is_running_ = false;
    LOG_INFO("Crow服务器已停止", "CrowServer");
    return true;
}

bool CrowServer::isRunning() const {
    return is_running_;
}

int64_t CrowServer::getRequestCount() const {
    return request_count_.load(std::memory_order_relaxed);
}

int64_t CrowServer::getErrorCount() const {
    return error_count_.load(std::memory_order_relaxed);
}

void CrowServer::registerWebSocketHandler(
    const std::string& path,
    std::function<void(std::string_view, void*, bool, std::string)> message_handler,
    std::function<void(std::string)> open_handler,
    std::function<void(std::string, int, std::string_view)> close_handler
) {
    impl_->registerWebSocketHandler(path, message_handler, open_handler, close_handler);
}

void CrowServer::broadcastWebSocketMessage(const std::string& path, const std::string& message, bool is_binary) {
    impl_->broadcastWebSocketMessage(path, message, is_binary);
}

bool CrowServer::sendWebSocketMessage(const std::string& client_id, const std::string& message, bool is_binary) {
    return impl_ ? impl_->sendWebSocketMessage(client_id, message, is_binary) : false;
}

bool CrowServer::disconnectClient(const std::string& client_id) {
    return impl_ ? impl_->disconnectClient(client_id) : false;
}

} // namespace api
} // namespace cam_server
