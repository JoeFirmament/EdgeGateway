#ifndef API_SERVER_H
#define API_SERVER_H

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace cam_server {
namespace api {

// 前向声明
class RestHandler;
class WebServer;

/**
 * @brief API服务器配置结构体
 */
struct ApiServerConfig {
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
    // 是否启用CORS
    bool enable_cors;
    // 允许的CORS源
    std::string cors_allowed_origins;
    // 是否启用API密钥认证
    bool enable_api_key;
    // API密钥
    std::string api_key;
    // 日志级别
    std::string log_level;
};

/**
 * @brief API服务器状态枚举
 */
enum class ApiServerState {
    STOPPED,    // 已停止
    STARTING,   // 正在启动
    RUNNING,    // 运行中
    STOPPING,   // 正在停止
    ERROR       // 错误状态
};

/**
 * @brief API服务器状态结构体
 */
struct ApiServerStatus {
    // 当前状态
    ApiServerState state;
    // 监听地址
    std::string address;
    // 监听端口
    int port;
    // 是否使用HTTPS
    bool using_https;
    // 启动时间
    int64_t start_time;
    // 请求计数
    int64_t request_count;
    // 错误计数
    int64_t error_count;
    // 错误信息（如果状态为ERROR）
    std::string error_message;
};

/**
 * @brief API服务器类
 */
class ApiServer {
public:
    /**
     * @brief 获取ApiServer单例
     * @return ApiServer单例的引用
     */
    static ApiServer& getInstance();

    /**
     * @brief 初始化API服务器
     * @param config 服务器配置
     * @return 是否初始化成功
     */
    bool initialize(const ApiServerConfig& config);

    /**
     * @brief 启动API服务器
     * @return 是否成功启动
     */
    bool start();

    /**
     * @brief 停止API服务器
     * @return 是否成功停止
     */
    bool stop();

    /**
     * @brief 获取服务器状态
     * @return 服务器状态
     */
    ApiServerStatus getStatus() const;

    /**
     * @brief 获取服务器配置
     * @return 服务器配置
     */
    ApiServerConfig getConfig() const;

    /**
     * @brief 注册路由处理函数
     * @param path 路径
     * @param method HTTP方法
     * @param handler 处理函数
     * @return 是否成功注册
     */
    bool registerHandler(const std::string& path, const std::string& method,
                        std::function<void(const std::string&, std::string&)> handler);

    /**
     * @brief 设置状态回调函数
     * @param callback 状态回调函数
     */
    void setStatusCallback(std::function<void(const ApiServerStatus&)> callback);

private:
    // 私有构造函数，防止外部创建实例
    ApiServer();
    // 禁止拷贝构造和赋值操作
    ApiServer(const ApiServer&) = delete;
    ApiServer& operator=(const ApiServer&) = delete;
    // 析构函数
    ~ApiServer();

    // 注册API路由
    void registerApiRoutes();
    // 注册系统控制API路由
    void registerSystemControlRoutes();
    // 生成客户端ID
    std::string generateClientId();

    // 服务器配置
    ApiServerConfig config_;
    // 服务器状态
    ApiServerStatus status_;
    // 状态互斥锁
    mutable std::mutex status_mutex_;
    // 状态回调函数
    std::function<void(const ApiServerStatus&)> status_callback_;
    // 是否已初始化
    bool is_initialized_;
    // REST处理器
    std::shared_ptr<RestHandler> rest_handler_;
    // Web服务器
    std::unique_ptr<WebServer> web_server_;
    // 服务器线程
    std::thread server_thread_;
    // 停止标志
    std::atomic<bool> stop_flag_;
};

} // namespace api
} // namespace cam_server

#endif // API_SERVER_H
