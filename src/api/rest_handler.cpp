#include "api/rest_handler.h"
#include "monitor/logger.h"
#include "utils/string_utils.h"
#include <sstream>
#include <algorithm>

namespace cam_server {
namespace api {

// 构造函数
RestHandler::RestHandler(bool enable_cors, const std::string& cors_allowed_origins)
    : enable_cors_(enable_cors)
    , cors_allowed_origins_(cors_allowed_origins)
    , enable_api_key_(false)
    , api_key_("") {
}

// 析构函数
RestHandler::~RestHandler() {
}

// 初始化处理器
bool RestHandler::initialize(bool enable_api_key, const std::string& api_key) {
    enable_api_key_ = enable_api_key;
    api_key_ = api_key;
    
    return true;
}

// 注册路由
bool RestHandler::registerRoute(const std::string& method, const std::string& path, RouteHandler handler) {
    if (method.empty() || path.empty() || !handler) {
        return false;
    }

    std::lock_guard<std::mutex> lock(routes_mutex_);
    RouteKey key{method, path};
    routes_[key] = handler;
    return true;
}

// 处理HTTP请求
HttpResponse RestHandler::handleRequest(const HttpRequest& request) {
    std::stringstream ss;
    ss << "处理REST请求: 方法: " << request.method << ", 路径: " << request.path;
    LOG_DEBUG(ss.str(), "REST");

    // 检查API密钥（如果启用）
    if (enable_api_key_ && !api_key_.empty()) {
        auto it = request.headers.find("X-API-Key");
        if (it == request.headers.end() || it->second != api_key_) {
            LOG_ERROR("API密钥验证失败", "REST");
            return createErrorResponse(401, "Unauthorized", "Invalid API key");
        }
    }

    // 查找路由处理器
    RouteKey route_key{request.method, request.path};
    std::stringstream ss2;
    ss2 << "查找路由处理器: " << route_key.method << " " << route_key.path;
    LOG_DEBUG(ss2.str(), "REST");

    auto it = routes_.find(route_key);
    if (it == routes_.end()) {
        std::stringstream ss3;
        ss3 << "未找到路由处理器: " << route_key.method << " " << route_key.path;
        LOG_ERROR(ss3.str(), "REST");
        
        std::stringstream ss4;
        ss4 << "已注册的路由:";
        for (const auto& route : routes_) {
            ss4 << "\n  " << route.first.method << " " << route.first.path;
        }
        LOG_DEBUG(ss4.str(), "REST");
        return createErrorResponse(404, "Not Found", "Route not found");
    }

    LOG_DEBUG("找到路由处理器，开始处理请求", "REST");

    try {
        // 调用路由处理器
        HttpResponse response = it->second(request);

        std::stringstream ss5;
        ss5 << "请求处理完成: 状态码: " << response.status_code 
           << ", 状态消息: " << response.status_message
           << ", 内容类型: " << response.content_type
           << ", 响应体长度: " << response.body.length();
        LOG_DEBUG(ss5.str(), "REST");

        // 添加CORS头
        if (enable_cors_) {
            response.headers["Access-Control-Allow-Origin"] = cors_allowed_origins_;
            response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
            response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Key";
            response.headers["Access-Control-Allow-Credentials"] = "true";
            response.headers["Access-Control-Max-Age"] = "86400";
        }

        return response;
    } catch (const std::exception& e) {
        std::stringstream ss6;
        ss6 << "处理请求时发生错误: " << e.what();
        LOG_ERROR(ss6.str(), "REST");
        return createErrorResponse(500, "Internal Server Error", e.what());
    }
}

// 获取已注册的路由列表
std::vector<std::string> RestHandler::getRegisteredRoutes() const {
    std::vector<std::string> routes;
    for (const auto& route : routes_) {
        std::stringstream ss;
        ss << route.first.method << " " << route.first.path;
        routes.push_back(ss.str());
    }
    return routes;
}

// 验证API密钥
bool RestHandler::validateApiKey(const HttpRequest& request) const {
    if (!enable_api_key_) {
        return true;
    }
    
    // 检查查询参数中的API密钥
    auto it = request.query_params.find("api_key");
    if (it != request.query_params.end() && it->second == api_key_) {
        return true;
    }
    
    // 检查请求头中的API密钥
    it = request.headers.find("X-API-Key");
    if (it != request.headers.end() && it->second == api_key_) {
        return true;
    }
    
    // 检查Authorization头
    it = request.headers.find("Authorization");
    if (it != request.headers.end()) {
        std::string auth_header = it->second;
        if (utils::StringUtils::startsWith(auth_header, "Bearer ")) {
            std::string token = auth_header.substr(7); // 去除"Bearer "前缀
            if (token == api_key_) {
                return true;
            }
        }
    }
    
    return false;
}

// 解析查询参数
void RestHandler::parseQueryParams(const std::string& query_string, std::unordered_map<std::string, std::string>& params) {
    std::istringstream ss(query_string);
    std::string param;
    
    while (std::getline(ss, param, '&')) {
        size_t pos = param.find('=');
        if (pos != std::string::npos) {
            std::string key = param.substr(0, pos);
            std::string value = param.substr(pos + 1);
            params[key] = value;
        } else {
            params[param] = "";
        }
    }
}

// 添加CORS头
void RestHandler::addCorsHeaders(HttpResponse& response, const HttpRequest& request) const {
    if (!enable_cors_) {
        return;
    }
    
    // 获取Origin头
    auto it = request.headers.find("Origin");
    if (it == request.headers.end()) {
        return;
    }
    
    std::string origin = it->second;
    
    // 检查是否允许该源
    if (cors_allowed_origins_ == "*") {
        response.headers["Access-Control-Allow-Origin"] = origin;
    } else {
        std::vector<std::string> allowed_origins = utils::StringUtils::split(cors_allowed_origins_, ',');
        bool allowed = false;
        
        for (const auto& allowed_origin : allowed_origins) {
            if (allowed_origin == origin) {
                allowed = true;
                break;
            }
        }
        
        if (allowed) {
            response.headers["Access-Control-Allow-Origin"] = origin;
        }
    }
    
    // 添加其他CORS头
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
    response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Key, Accept, Origin, DNT, X-CustomHeader, Keep-Alive, User-Agent, X-Requested-With, If-Modified-Since, Cache-Control";
    response.headers["Access-Control-Allow-Credentials"] = "true";
    response.headers["Access-Control-Max-Age"] = "86400"; // 24小时
}

// 创建错误响应
HttpResponse RestHandler::createErrorResponse(int status_code, const std::string& status_message, const std::string& error_message) {
    HttpResponse response;
    response.status_code = status_code;
    response.status_message = status_message;
    response.content_type = "application/json";
    response.body = "{\"status\":\"error\",\"message\":\"" + error_message + "\"}";

    // 添加CORS头
    if (enable_cors_) {
        response.headers["Access-Control-Allow-Origin"] = cors_allowed_origins_;
        response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
        response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Key";
        response.headers["Access-Control-Allow-Credentials"] = "true";
        response.headers["Access-Control-Max-Age"] = "86400";
    }

    return response;
}

} // namespace api
} // namespace cam_server
