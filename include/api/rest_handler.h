#pragma once

#include "api/http_types.h"
#include <functional>
#include <unordered_map>
#include <mutex>
#include <string>
#include <vector>
#include <iostream>

namespace cam_server {
namespace api {

/**
 * @brief HTTP请求结构体
 */
struct HttpRequest {
    // HTTP方法
    std::string method;
    // 请求路径
    std::string path;
    // 查询参数
    std::unordered_map<std::string, std::string> query_params;
    // 请求头
    std::unordered_map<std::string, std::string> headers;
    // 请求体
    std::string body;
    // 客户端IP
    std::string client_ip;
};

/**
 * @brief HTTP响应结构体
 */
struct HttpResponse {
    // 状态码
    int status_code;
    // 状态消息
    std::string status_message;
    // 响应头
    std::unordered_map<std::string, std::string> headers;
    // 响应体
    std::string body;
    // 内容类型
    std::string content_type;
    // 是否是流式响应
    bool is_streaming = false;
    // 流式响应回调函数
    std::function<void(std::function<void(const std::vector<uint8_t>&)>)> stream_callback;
};

/**
 * @brief 路由处理函数类型
 */
using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

/**
 * @brief REST处理器类
 */
class RestHandler {
public:
    /**
     * @brief 构造函数
     */
    RestHandler(bool enable_cors = false, const std::string& cors_allowed_origins = "*");

    /**
     * @brief 析构函数
     */
    virtual ~RestHandler();

    /**
     * @brief 初始化REST处理器
     * @param enable_api_key 是否启用API密钥
     * @param api_key API密钥
     * @return 是否初始化成功
     */
    bool initialize(bool enable_api_key, const std::string& api_key);

    /**
     * @brief 注册路由处理函数
     * @param method HTTP方法
     * @param path 路径
     * @param handler 处理函数
     * @return 是否成功注册
     */
    bool registerRoute(const std::string& method, const std::string& path, RouteHandler handler);

    /**
     * @brief 处理HTTP请求
     * @param request HTTP请求
     * @return HTTP响应
     */
    HttpResponse handleRequest(const HttpRequest& request);

    /**
     * @brief 获取已注册的路由列表
     * @return 路由列表
     */
    std::vector<std::string> getRegisteredRoutes() const;

    /**
     * @brief 启用/禁用CORS
     * @param enable 是否启用CORS
     */
    void enableCors(bool enable) { enable_cors_ = enable; }
    void setCorsAllowedOrigins(const std::string& origins) { cors_allowed_origins_ = origins; }

    /**
     * @brief 启用/禁用API密钥验证
     * @param enable 是否启用API密钥验证
     */
    void enableApiKey(bool enable) { enable_api_key_ = enable; }
    void setApiKey(const std::string& key) { api_key_ = key; }

private:
    // 路由键结构体
    struct RouteKey {
        // HTTP方法
        std::string method;
        // 路径
        std::string path;

        // 相等运算符
        bool operator==(const RouteKey& other) const {
            return method == other.method && path == other.path;
        }
    };

    // 路由键哈希函数
    struct RouteKeyHash {
        std::size_t operator()(const RouteKey& key) const {
            return std::hash<std::string>()(key.method) ^ std::hash<std::string>()(key.path);
        }
    };

    // 验证API密钥
    bool validateApiKey(const HttpRequest& request) const;
    // 解析查询参数
    void parseQueryParams(const std::string& query_string, std::unordered_map<std::string, std::string>& params);
    // 添加CORS头
    void addCorsHeaders(HttpResponse& response, const HttpRequest& request) const;
    // 创建错误响应
    HttpResponse createErrorResponse(int status_code, const std::string& status_message, const std::string& error_message);

    // 路由表
    std::unordered_map<RouteKey, RouteHandler, RouteKeyHash> routes_;
    // 路由表互斥锁
    std::mutex routes_mutex_;
    // 是否启用API密钥认证
    bool enable_api_key_;
    // API密钥
    std::string api_key_;
    // 是否启用CORS
    bool enable_cors_;
    // 允许的CORS源
    std::string cors_allowed_origins_;
};

} // namespace api
} // namespace cam_server
