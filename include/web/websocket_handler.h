#pragma once

#include "third_party/crow/crow.h"
#include "web/video_server.h"

namespace cam_server {
namespace web {

/**
 * @brief WebSocket处理类
 */
class WebSocketHandler {
public:
    /**
     * @brief 设置WebSocket路由
     */
    static void setupRoutes(crow::SimpleApp& app, VideoServer* server);

private:
    /**
     * @brief 处理WebSocket连接打开
     */
    static void onOpen(crow::websocket::connection& conn, VideoServer* server);

    /**
     * @brief 处理WebSocket消息
     */
    static void onMessage(crow::websocket::connection& conn, const std::string& data,
                         bool is_binary, VideoServer* server);

    /**
     * @brief 处理WebSocket连接关闭
     */
    static void onClose(crow::websocket::connection& conn, const std::string& reason,
                       VideoServer* server);

    /**
     * @brief 处理WebSocket错误
     */
    static void onError(crow::websocket::connection& conn, VideoServer* server);

    // 私有辅助方法 - 基于原始实现
    static void handleStartCamera(crow::websocket::connection& conn, const std::string& device_path, VideoServer* server);
    static void handleStopCamera(crow::websocket::connection& conn, VideoServer* server);
    static void handleGetStatus(crow::websocket::connection& conn, VideoServer* server);
    static void handleGetInfo(crow::websocket::connection& conn, VideoServer* server);
    static void handleCapturePhoto(crow::websocket::connection& conn, const std::string& data, VideoServer* server);
    static void handleStartRecording(crow::websocket::connection& conn, const std::string& data, VideoServer* server);
    static void handleStopRecording(crow::websocket::connection& conn, VideoServer* server);
    static void handleGetRecordingStatus(crow::websocket::connection& conn, VideoServer* server);
    static void handleFrame(const camera::Frame& frame, const std::string& device_path, VideoServer* server);
};

} // namespace web
} // namespace cam_server
