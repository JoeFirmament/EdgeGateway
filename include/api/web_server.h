#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>

#include "rest_handler.h"

namespace cam_server {
namespace api {

/**
 * @brief Web服务器配置结构体
 */
struct WebServerConfig {
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
 * @brief Web服务器类
 */
class WebServer {
public:
    /**
     * @brief 构造函数
     */
    WebServer();

    /**
     * @brief 析构函数
     */
    ~WebServer();

    /**
     * @brief 初始化服务器
     * @param config 服务器配置
     * @param rest_handler REST处理器
     * @return 是否初始化成功
     */
    bool initialize(const WebServerConfig& config, std::shared_ptr<RestHandler> rest_handler);

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

private:
    // 服务器实现类（PIMPL模式）
    class Impl;
    std::unique_ptr<Impl> impl_;

    // 服务器配置
    WebServerConfig config_;
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

#endif // WEB_SERVER_H
