#include "api/web_server.h"
#include "monitor/logger.h"
#include "utils/file_utils.h"  // 添加FileUtils头文件
#include "utils/debug_utils.h"  // 添加DebugUtils头文件

#include <cstring>
#include <sstream>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdlib>  // for memset and system()
#include <sys/stat.h>
#include <errno.h>   // for errno and strerror
#include <iostream>  // for std::cerr
#include <fstream>   // for std::ifstream
#include <sstream>   // for std::stringstream
#include <fmt/format.h>  // for string formatting

// 使用Mongoose作为HTTP服务器库
#include "mongoose.h"

namespace cam_server {
namespace api {

// Web服务器实现类
class WebServer::Impl {
public:
    Impl(WebServer* server) : server_(server), mg_mgr_{} {}
    ~Impl() {
        stop();
    }

    bool initialize(const WebServerConfig& config, std::shared_ptr<RestHandler> rest_handler) {
        LOG_DEBUG("开始初始化Web服务器实现...", "WebServer");

        LOG_DEBUG("设置配置...", "WebServer");
        config_ = config;
        rest_handler_ = rest_handler;

        LOG_DEBUG("Web服务器实现初始化成功", "WebServer");
        return true;
    }

    bool start() {
        if (is_running_) {
            LOG_INFO("Web服务器已经在运行中", "WebServer");
            return true;
        }

        LOG_INFO("正在初始化Mongoose管理器", "WebServer");
        mg_mgr_init(&mg_mgr_);

        // 创建监听地址
        std::string listen_addr = std::string("http://0.0.0.0:") + std::to_string(config_.port);
        if (config_.use_https) {
            listen_addr = std::string("https://0.0.0.0:") + std::to_string(config_.port);
        }

        LOG_INFO("Web服务器监听地址: " + listen_addr, "WebServer");
        LOG_DEBUG("尝试监听地址: " + listen_addr, "WebServer");
        
        // 创建HTTP连接
        struct mg_connection* nc = mg_http_listen(&mg_mgr_, listen_addr.c_str(), eventHandler, this);
        if (nc == nullptr) {
            LOG_ERROR("无法启动Web服务器: " + listen_addr, "WebServer");
            mg_mgr_free(&mg_mgr_);
            return false;
        }

        LOG_DEBUG("Web服务器监听成功", "WebServer");

        // 设置CORS头
        [[maybe_unused]] const char* cors_headers = "Access-Control-Allow-Origin: *\r\n"
                                 "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
                                 "Access-Control-Allow-Headers: Content-Type, Authorization, X-API-Key, Accept, Origin, DNT, X-CustomHeader, Keep-Alive, User-Agent, X-Requested-With, If-Modified-Since, Cache-Control\r\n"
                                 "Access-Control-Allow-Credentials: true\r\n"
                                 "Access-Control-Max-Age: 86400\r\n";

        // 设置连接属性
        nc->fn_data = this;

        // 启动事件循环线程
        is_running_ = true;
        server_thread_ = std::thread([this]() {
            while (is_running_) {
                mg_mgr_poll(&mg_mgr_, 1000);  // 1秒超时
            }
        });

        LOG_INFO("Web服务器启动成功", "WebServer");
        return true;
    }

    bool stop() {
        if (!is_running_) {
            return true;
        }

        is_running_ = false;

        if (server_thread_.joinable()) {
            server_thread_.join();
        }

        // 释放Mongoose管理器
        mg_mgr_free(&mg_mgr_);

        LOG_INFO("Web服务器已停止", "WebServer");
        return true;
    }

    // Mongoose事件处理函数
    static void eventHandler(struct mg_connection* nc, int ev, void* ev_data) {
        auto* impl = static_cast<Impl*>(nc->fn_data);

        switch (ev) {
            case MG_EV_HTTP_MSG: {
                auto* hm = static_cast<struct mg_http_message*>(ev_data);
                impl->handleHttpRequest(nc, hm);
                break;
            }
            case MG_EV_CLOSE: {
                // 处理连接关闭事件
                impl->handleConnectionClose(nc);
                break;
            }
        }
    }

    // 处理HTTP请求
    void handleHttpRequest(struct mg_connection* nc, struct mg_http_message* hm) {
        // 增加请求计数
        server_->request_count_++;

        try {
            // 解析请求
            HttpRequest request;
            parseHttpRequest(hm, request);

            // 打印请求信息
            LOG_DEBUG("收到HTTP请求:", "WebServer");
            LOG_DEBUG("  方法: " + request.method, "WebServer");
            LOG_DEBUG("  路径: " + request.path, "WebServer");
            
            std::string query_params_str = "  查询参数: ";
            for (const auto& param : request.query_params) {
                query_params_str += fmt::format("{}={}, ", param.first, param.second);
            }
            if (!request.query_params.empty()) {
                query_params_str = query_params_str.substr(0, query_params_str.length() - 2); // 移除最后的逗号和空格
                LOG_DEBUG(query_params_str, "WebServer");
            }
            
            std::string headers_str = "  请求头: ";
            for (const auto& header : request.headers) {
                headers_str += fmt::format("{}: {}, ", header.first, header.second);
            }
            if (!request.headers.empty()) {
                headers_str = headers_str.substr(0, headers_str.length() - 2); // 移除最后的逗号和空格
                LOG_DEBUG(headers_str, "WebServer");
            }
            
            LOG_DEBUG("  请求体长度: " + std::to_string(request.body.length()), "WebServer");

            // 添加请求超时处理
            request.headers["X-Request-Timeout"] = "30000";  // 30秒超时

            // 处理OPTIONS请求（CORS预检请求）
            if (request.method == "OPTIONS") {
                handleOptionsRequest(nc);
                return;
            }

            // 检查是否是静态文件请求
            if (!(request.path.substr(0, 5) == "/api/") && handleStaticFileRequest(nc, hm, request)) {
                LOG_DEBUG("已处理静态文件请求: " + request.path, "WebServer");
                return;
            }

            // 处理REST请求
            LOG_DEBUG("转发到REST处理器: " + request.path, "WebServer");
            HttpResponse response = rest_handler_->handleRequest(request);

            // 打印响应信息
            LOG_DEBUG("REST处理器响应:", "WebServer");
            LOG_DEBUG("  状态码: " + std::to_string(response.status_code), "WebServer");
            LOG_DEBUG("  状态消息: " + response.status_message, "WebServer");
            LOG_DEBUG("  内容类型: " + response.content_type, "WebServer");
            LOG_DEBUG("  响应体长度: " + std::to_string(response.body.length()), "WebServer");

            // 处理流式响应
            if (response.is_streaming) {
                handleStreamingResponse(nc, response);
                return;
            }

            // 发送响应
            sendHttpResponse(nc, response);
        } catch (const std::exception& e) {
            // 错误计数增加
            server_->error_count_++;
            
            LOG_ERROR("处理请求时发生错误: " + std::string(e.what()), "WebServer");
            
            // 发送错误响应
            HttpResponse error_response;
            error_response.status_code = 500;
            error_response.status_message = "Internal Server Error";
            error_response.content_type = "application/json";
            error_response.body = "{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}";
            
            // 添加CORS头
            error_response.headers["Access-Control-Allow-Origin"] = "*";
            error_response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
            error_response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Key, Accept, Origin, DNT, X-CustomHeader, Keep-Alive, User-Agent, X-Requested-With, If-Modified-Since, Cache-Control";
            error_response.headers["Access-Control-Allow-Credentials"] = "true";
            error_response.headers["Access-Control-Max-Age"] = "86400";
            
            sendHttpResponse(nc, error_response);
            
            LOG_ERROR("处理HTTP请求时发生错误: " + std::string(e.what()), "WebServer");
        }
    }

    // 处理OPTIONS请求
    void handleOptionsRequest(struct mg_connection* nc) {
        const char* cors_headers = "Access-Control-Allow-Origin: *\r\n"
                                 "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
                                 "Access-Control-Allow-Headers: Content-Type, Authorization, X-API-Key, Accept, Origin, DNT, X-CustomHeader, Keep-Alive, User-Agent, X-Requested-With, If-Modified-Since, Cache-Control\r\n"
                                 "Access-Control-Allow-Credentials: true\r\n"
                                 "Access-Control-Max-Age: 86400\r\n";
        
        mg_http_reply(nc, 204, cors_headers, "");
    }

    // 解析HTTP请求
    void parseHttpRequest(struct mg_http_message* hm, HttpRequest& request) {
        // 解析方法
        request.method = std::string(hm->method.buf, hm->method.len);

        // 解析路径
        request.path = std::string(hm->uri.buf, hm->uri.len);

        // 解析查询参数
        if (hm->query.len > 0) {
            std::string query_string(hm->query.buf, hm->query.len);
            std::istringstream ss(query_string);
            std::string param;

            while (std::getline(ss, param, '&')) {
                size_t pos = param.find('=');
                if (pos != std::string::npos) {
                    std::string key = param.substr(0, pos);
                    std::string value = param.substr(pos + 1);
                    request.query_params[key] = value;
                }
            }
        }

        // 解析请求头
        for (int i = 0; i < MG_MAX_HTTP_HEADERS && hm->headers[i].name.len > 0; i++) {
            std::string name(hm->headers[i].name.buf, hm->headers[i].name.len);
            std::string value(hm->headers[i].value.buf, hm->headers[i].value.len);
            request.headers[name] = value;
        }

        // 解析请求体
        if (hm->body.len > 0) {
            request.body = std::string(hm->body.buf, hm->body.len);
        }

        // 设置默认客户端IP
        request.client_ip = "0.0.0.0";
    }

    // 处理静态文件请求
    bool handleStaticFileRequest(struct mg_connection* nc, struct mg_http_message* hm, const HttpRequest& request) {
        LOG_DEBUG_FULL("WEB", "处理静态文件请求: " + request.path);

        if (config_.static_files_dir.empty()) {
            LOG_DEBUG_FULL("WEB", "静态文件目录为空，无法处理静态文件请求");
            return false;
        }

        LOG_DEBUG_FULL("WEB", "静态文件目录: " + config_.static_files_dir);

        // 检查是否是静态文件请求
        if (request.method != "GET" && request.method != "HEAD") {
            LOG_DEBUG_FULL("WEB", "非GET或HEAD请求，不处理静态文件: " + request.method);
            return false;
        }

        // 构建文件路径
        std::string path = request.path;
        if (path == "/") {
            path = "/index.html";
            LOG_DEBUG_FULL("WEB", "请求根路径，使用默认文件: /index.html");
        }

        // 设置Mongoose选项
        struct mg_http_serve_opts opts;
        memset(&opts, 0, sizeof(opts));
        opts.root_dir = config_.static_files_dir.c_str();
        opts.extra_headers = "Access-Control-Allow-Origin: *\r\n";

        LOG_DEBUG_FULL("WEB", "正在发送文件，根目录: " + config_.static_files_dir + ", 路径: " + path);
        mg_http_serve_dir(nc, hm, &opts);
        LOG_DEBUG_FULL("WEB", "文件发送完成");

        return true;
    }

    // 发送HTTP响应
    void sendHttpResponse(struct mg_connection* nc, const HttpResponse& response) {
        // 构建响应头
        std::string headers;
        for (const auto& header : response.headers) {
            headers += header.first + ": " + header.second + "\r\n";
        }

        // 设置内容类型
        if (!response.content_type.empty()) {
            headers += "Content-Type: " + response.content_type + "\r\n";
        } else {
            headers += "Content-Type: application/json\r\n";
        }

        // 发送响应
        mg_http_reply(nc, response.status_code, headers.c_str(), "%s", response.body.c_str());
    }

    // 处理流式响应
    void handleStreamingResponse(struct mg_connection* nc, const HttpResponse& response) {
        // 设置响应头
        mg_printf(nc, "HTTP/1.1 %d %s\r\n", response.status_code, response.status_message.c_str());
        mg_printf(nc, "Content-Type: %s\r\n", response.content_type.c_str());
        
        // 添加自定义响应头
        for (const auto& header : response.headers) {
            mg_printf(nc, "%s: %s\r\n", header.first.c_str(), header.second.c_str());
        }
        
        // 添加CORS头
        mg_printf(nc, "Access-Control-Allow-Origin: *\r\n");
        mg_printf(nc, "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
        mg_printf(nc, "Access-Control-Allow-Headers: Content-Type\r\n");
        mg_printf(nc, "Access-Control-Max-Age: 3600\r\n");
        
        mg_printf(nc, "\r\n");  // 结束响应头

        // 如果有流回调，设置为长连接模式
        if (response.stream_callback) {
            // 创建写回调函数
            auto write_callback = [nc](const std::vector<uint8_t>& data) {
                mg_send(nc, data.data(), data.size());
            };

            // 调用流回调
            response.stream_callback(write_callback);
        }
    }

    // 处理连接关闭
    void handleConnectionClose(struct mg_connection* nc) {
        std::string client_id = std::to_string(reinterpret_cast<uintptr_t>(nc));

        std::lock_guard<std::mutex> lock(streaming_connections_mutex_);
        streaming_connections_.erase(client_id);
    }

    // 根据扩展名获取内容类型
    std::string getContentTypeFromExtension(const std::string& ext) {
        static const std::unordered_map<std::string, std::string> content_types = {
            {".html", "text/html"},
            {".htm", "text/html"},
            {".css", "text/css"},
            {".js", "application/javascript"},
            {".json", "application/json"},
            {".png", "image/png"},
            {".jpg", "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".gif", "image/gif"},
            {".svg", "image/svg+xml"},
            {".ico", "image/x-icon"},
            {".txt", "text/plain"},
            {".pdf", "application/pdf"},
            {".xml", "application/xml"},
            {".mp4", "video/mp4"},
            {".webm", "video/webm"},
            {".mp3", "audio/mpeg"},
            {".wav", "audio/wav"},
            {".ogg", "audio/ogg"},
            {".zip", "application/zip"},
            {".ttf", "font/ttf"},
            {".woff", "font/woff"},
            {".woff2", "font/woff2"}
        };

        auto it = content_types.find(ext);
        if (it != content_types.end()) {
            return it->second;
        }

        return "application/octet-stream";
    }

private:
    WebServer* server_;
    WebServerConfig config_;
    std::shared_ptr<RestHandler> rest_handler_;
    struct mg_mgr mg_mgr_;
    std::thread server_thread_;
    std::atomic<bool> is_running_{false};

    // 流式连接管理
    std::mutex streaming_connections_mutex_;
    std::unordered_map<std::string, struct mg_connection*> streaming_connections_;
};

// WebServer类实现

WebServer::WebServer()
    : is_initialized_(false), is_running_(false), request_count_(0), error_count_(0) {
    impl_ = std::make_unique<Impl>(this);
}

WebServer::~WebServer() {
    stop();
}

bool WebServer::initialize(const WebServerConfig& config, std::shared_ptr<RestHandler> rest_handler) {
    LOG_DEBUG("开始初始化Web服务器...", "WebServer");

    if (is_initialized_) {
        LOG_DEBUG("Web服务器已经初始化", "WebServer");
        return true;
    }

    LOG_DEBUG("设置配置...", "WebServer");
    config_ = config;
    rest_handler_ = rest_handler;

    LOG_DEBUG("初始化实现...", "WebServer");
    if (!impl_->initialize(config, rest_handler)) {
        LOG_ERROR("无法初始化Web服务器实现", "WebServer");
        return false;
    }
    LOG_DEBUG("实现初始化成功", "WebServer");

    is_initialized_ = true;
    LOG_DEBUG("Web服务器初始化成功", "WebServer");
    return true;
}

bool WebServer::start() {
    if (!is_initialized_) {
        LOG_ERROR("Web服务器未初始化", "WebServer");
        return false;
    }

    if (is_running_) {
        return true;
    }

    if (!impl_->start()) {
        return false;
    }

    is_running_ = true;
    return true;
}

bool WebServer::stop() {
    if (!is_running_) {
        return true;
    }

    if (!impl_->stop()) {
        return false;
    }

    is_running_ = false;
    return true;
}

bool WebServer::isRunning() const {
    return is_running_;
}

int64_t WebServer::getRequestCount() const {
    return request_count_;
}

int64_t WebServer::getErrorCount() const {
    return error_count_;
}

} // namespace api
} // namespace cam_server
