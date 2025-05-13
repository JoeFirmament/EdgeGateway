#include "api/rest_handler.h"
#include "monitor/logger.h"
#include "utils/string_utils.h"
#include <sstream>
#include <algorithm>

namespace cam_server {
namespace api {

// 构造函数
RestHandler::RestHandler() : enable_api_key_(false), enable_cors_(false) {
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

// 注册路由处理函数
bool RestHandler::registerRoute(const std::string& method, const std::string& path, RouteHandler handler) {
    if (method.empty() || path.empty() || !handler) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(routes_mutex_);
    
    // 转换为大写方法
    std::string upper_method = utils::StringUtils::toUpper(method);
    
    // 创建路由键
    RouteKey key{upper_method, path};
    
    // 检查路由是否已存在
    if (routes_.find(key) != routes_.end()) {
        return false;
    }
    
    // 注册路由
    routes_[key] = handler;
    
    return true;
}

// 处理HTTP请求
HttpResponse RestHandler::handleRequest(const HttpRequest& request) {
    // 验证API密钥
    if (enable_api_key_ && !validateApiKey(request)) {
        HttpResponse response = createErrorResponse(401, "Unauthorized: Invalid API key");
        addCorsHeaders(response, request);
        return response;
    }
    
    // 查找路由处理函数
    std::lock_guard<std::mutex> lock(routes_mutex_);
    
    // 转换为大写方法
    std::string upper_method = utils::StringUtils::toUpper(request.method);
    
    // 创建路由键
    RouteKey key{upper_method, request.path};
    
    // 查找路由
    auto it = routes_.find(key);
    if (it == routes_.end()) {
        // 尝试查找通配符路由
        bool found = false;
        RouteHandler handler;
        
        for (const auto& route : routes_) {
            // 检查路径是否匹配通配符
            if (route.first.method == upper_method && 
                utils::StringUtils::endsWith(route.first.path, "*")) {
                
                std::string prefix = route.first.path.substr(0, route.first.path.length() - 1);
                if (utils::StringUtils::startsWith(request.path, prefix)) {
                    found = true;
                    handler = route.second;
                    break;
                }
            }
        }
        
        if (!found) {
            // 路由未找到
            HttpResponse response = createErrorResponse(404, "Not Found: Route not registered");
            addCorsHeaders(response, request);
            return response;
        }
        
        // 调用处理函数
        HttpResponse response = handler(request);
        addCorsHeaders(response, request);
        return response;
    }
    
    // 调用处理函数
    HttpResponse response = it->second(request);
    addCorsHeaders(response, request);
    return response;
}

// 设置CORS配置
void RestHandler::setCorsConfig(bool enable_cors, const std::string& allowed_origins) {
    enable_cors_ = enable_cors;
    cors_allowed_origins_ = allowed_origins;
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
    response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Key";
    response.headers["Access-Control-Allow-Credentials"] = "true";
    response.headers["Access-Control-Max-Age"] = "86400"; // 24小时
}

// 创建错误响应
HttpResponse RestHandler::createErrorResponse(int status_code, const std::string& message) {
    HttpResponse response;
    response.status_code = status_code;
    
    // 设置状态消息
    switch (status_code) {
        case 400: response.status_message = "Bad Request"; break;
        case 401: response.status_message = "Unauthorized"; break;
        case 403: response.status_message = "Forbidden"; break;
        case 404: response.status_message = "Not Found"; break;
        case 405: response.status_message = "Method Not Allowed"; break;
        case 500: response.status_message = "Internal Server Error"; break;
        default: response.status_message = "Unknown Error"; break;
    }
    
    // 设置内容类型
    response.content_type = "application/json";
    
    // 设置响应体
    response.body = "{\"error\":" + std::to_string(status_code) + ",\"message\":\"" + message + "\"}";
    
    return response;
}

} // namespace api
} // namespace cam_server
