#include "api/web_server.h"
#include "monitor/logger.h"

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
#include <filesystem>

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
        config_ = config;
        rest_handler_ = rest_handler;
        return true;
    }

    bool start() {
        if (is_running_) {
            return true;
        }

        // 初始化Mongoose管理器
        mg_mgr_init(&mg_mgr_);

        // 创建监听地址
        std::string listen_addr;
        if (config_.use_https) {
            listen_addr = "https://" + config_.address + ":" + std::to_string(config_.port);
        } else {
            listen_addr = "http://" + config_.address + ":" + std::to_string(config_.port);
        }

        // 创建HTTP连接
        struct mg_connection* nc = mg_http_listen(&mg_mgr_, listen_addr.c_str(), eventHandler, this);
        if (nc == nullptr) {
            LOG_ERROR("无法启动Web服务器: " + listen_addr, "WebServer");
            mg_mgr_free(&mg_mgr_);
            return false;
        }

        // 配置SSL
        if (config_.use_https) {
            struct mg_tls_opts opts = {};
            opts.cert = config_.ssl_cert_path.c_str();
            opts.certkey = config_.ssl_key_path.c_str();
            mg_tls_init(nc, &opts);
        }

        // 启动服务器线程
        is_running_ = true;
        server_thread_ = std::thread([this]() {
            while (is_running_) {
                mg_mgr_poll(&mg_mgr_, 1000);
            }
        });

        LOG_INFO("Web服务器已启动: " + listen_addr, "WebServer");
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
    static void eventHandler(struct mg_connection* nc, int ev, void* ev_data, void* fn_data) {
        auto* impl = static_cast<Impl*>(fn_data);
        
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

        // 解析请求
        HttpRequest request;
        parseHttpRequest(hm, request);

        // 检查是否是静态文件请求
        if (handleStaticFileRequest(nc, hm, request)) {
            return;
        }

        // 处理REST请求
        HttpResponse response = rest_handler_->handleRequest(request);

        // 处理流式响应
        if (response.is_streaming) {
            handleStreamingResponse(nc, response);
            return;
        }

        // 发送响应
        sendHttpResponse(nc, response);
    }

    // 解析HTTP请求
    void parseHttpRequest(struct mg_http_message* hm, HttpRequest& request) {
        // 解析方法
        request.method = std::string(hm->method.ptr, hm->method.len);

        // 解析路径
        request.path = std::string(hm->uri.ptr, hm->uri.len);

        // 解析查询参数
        if (hm->query.len > 0) {
            std::string query_string(hm->query.ptr, hm->query.len);
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
            std::string name(hm->headers[i].name.ptr, hm->headers[i].name.len);
            std::string value(hm->headers[i].value.ptr, hm->headers[i].value.len);
            request.headers[name] = value;
        }

        // 解析请求体
        if (hm->body.len > 0) {
            request.body = std::string(hm->body.ptr, hm->body.len);
        }

        // 获取客户端IP
        char addr[100];
        mg_ntoa(&nc->peer, addr, sizeof(addr));
        request.client_ip = addr;
    }

    // 处理静态文件请求
    bool handleStaticFileRequest(struct mg_connection* nc, struct mg_http_message* hm, const HttpRequest& request) {
        if (config_.static_files_dir.empty()) {
            return false;
        }

        // 检查是否是静态文件请求
        if (request.method != "GET" && request.method != "HEAD") {
            return false;
        }

        // 构建文件路径
        std::string path = request.path;
        if (path == "/") {
            path = "/index.html";
        }

        std::string file_path = config_.static_files_dir + path;

        // 检查文件是否存在
        if (!std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path)) {
            return false;
        }

        // 获取文件扩展名
        std::string ext = std::filesystem::path(file_path).extension().string();
        std::string content_type = getContentTypeFromExtension(ext);

        // 发送文件
        mg_http_serve_file(nc, hm, file_path.c_str(), content_type.c_str(), "");
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
        if (!response.stream_callback) {
            // 如果没有流回调，发送普通响应
            sendHttpResponse(nc, response);
            return;
        }

        // 设置连接为流式响应
        nc->is_resp = 1;  // 标记为已响应
        
        // 构建响应头
        std::string headers;
        for (const auto& header : response.headers) {
            headers += header.first + ": " + header.second + "\r\n";
        }

        // 设置内容类型
        if (!response.content_type.empty()) {
            headers += "Content-Type: " + response.content_type + "\r\n";
        } else {
            headers += "Content-Type: application/octet-stream\r\n";
        }

        // 设置传输编码为分块
        headers += "Transfer-Encoding: chunked\r\n";

        // 发送响应头
        mg_printf(nc, "HTTP/1.1 %d %s\r\n%s\r\n", 
                 response.status_code, 
                 response.status_message.c_str(),
                 headers.c_str());

        // 保存连接信息
        std::string client_id = std::to_string(reinterpret_cast<uintptr_t>(nc));
        
        {
            std::lock_guard<std::mutex> lock(streaming_connections_mutex_);
            streaming_connections_[client_id] = nc;
        }

        // 启动流回调
        response.stream_callback([this, client_id](const std::vector<uint8_t>& data) {
            sendStreamData(client_id, data);
        });
    }

    // 发送流数据
    void sendStreamData(const std::string& client_id, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(streaming_connections_mutex_);
        
        auto it = streaming_connections_.find(client_id);
        if (it == streaming_connections_.end()) {
            return;
        }

        struct mg_connection* nc = it->second;
        
        // 发送分块数据
        mg_printf(nc, "%lx\r\n", data.size());
        mg_send(nc, data.data(), data.size());
        mg_send(nc, "\r\n", 2);
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
    if (is_initialized_) {
        return true;
    }

    config_ = config;
    rest_handler_ = rest_handler;
    
    if (!impl_->initialize(config, rest_handler)) {
        return false;
    }
    
    is_initialized_ = true;
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
