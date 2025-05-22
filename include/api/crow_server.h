#pragma once

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <mutex>

#include "rest_handler.h"

namespace cam_server {
namespace api {

/**
 * @brief Crow服务器配置结构体
 */
struct CrowServerConfig {
    // 监听地址
    std::string address;
    // 监听端口
    int port;
    // 静态文件目录
    std::string static_files_dir;
    // 是否启用HTTPS
    bool use_https;
    // SSL证书路径
    std::string ssl_cert_path;
    // SSL密钥路径
    std::string ssl_key_path;
    // 线程数
    int num_threads;
    // 日志级别
    std::string log_level;
};

/**
 * @brief Crow服务器类
 */
class CrowServer {
public:
    /**
     * @brief 构造函数
     */
    CrowServer();

    /**
     * @brief 析构函数
     */
    ~CrowServer();

    /**
     * @brief 初始化服务器
     * @param config 服务器配置
     * @param rest_handler REST处理器
     * @return 是否初始化成功
     */
    bool initialize(const CrowServerConfig& config, std::shared_ptr<RestHandler> rest_handler);

    /**
     * @brief 启动服务器
     * @return 是否成功启动
     */
    bool start();

    /**
     * @brief 停止服务器
     * @return 是否成功停止
     */
    bool stop();

    /**
     * @brief 检查服务器是否正在运行
     * @return 是否正在运行
     */
    bool isRunning() const;

    /**
     * @brief 获取请求计数
     * @return 请求计数
     */
    int64_t getRequestCount() const;

    /**
     * @brief 获取错误计数
     * @return 错误计数
     */
    int64_t getErrorCount() const;

    /**
     * @brief 注册WebSocket处理器
     * @param path WebSocket路径
     * @param message_handler 消息处理函数
     * @param open_handler 连接打开处理函数
     * @param close_handler 连接关闭处理函数
     */
    void registerWebSocketHandler(
        const std::string& path,
        std::function<void(std::string_view, void*, bool, std::string)> message_handler,
        std::function<void(std::string)> open_handler = nullptr,
        std::function<void(std::string, int, std::string_view)> close_handler = nullptr
    );

    /**
     * @brief 广播WebSocket消息
     * @param path WebSocket路径
     * @param message 消息内容
     * @param is_binary 是否为二进制消息
     */
    void broadcastWebSocketMessage(const std::string& path, const std::string& message, bool is_binary = false);

    /**
     * @brief 发送WebSocket消息给特定客户端
     * @param client_id 客户端ID
     * @param message 消息内容
     * @param is_binary 是否为二进制消息
     * @return 是否发送成功
     */
    bool sendWebSocketMessage(const std::string& client_id, const std::string& message, bool is_binary = false);

    /**
     * @brief 断开指定客户端的WebSocket连接
     * @param client_id 客户端ID
     * @return 是否成功断开连接
     */
    bool disconnectClient(const std::string& client_id);

private:
    // 服务器实现类（PIMPL模式）
    class Impl;
    std::unique_ptr<Impl> impl_;

    // 服务器配置
    CrowServerConfig config_;
    // REST处理器
    std::shared_ptr<RestHandler> rest_handler_;
    // 是否已初始化
    bool is_initialized_;
    // 是否正在运行
    std::atomic<bool> is_running_;
    // 请求计数
    std::atomic<int64_t> request_count_;
    // 错误计数
    std::atomic<int64_t> error_count_;
};

} // namespace api
} // namespace cam_server
