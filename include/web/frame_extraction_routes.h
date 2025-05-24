#pragma once

#include "third_party/crow/crow.h"
#include "web/video_server.h"

namespace cam_server {
namespace web {

/**
 * @brief 帧提取路由设置类
 */
class FrameExtractionRoutes {
public:
    /**
     * @brief 设置帧提取相关路由
     */
    static void setupRoutes(crow::SimpleApp& app, VideoServer* server);

private:
    /**
     * @brief 帧提取开始API
     */
    static void setupStartRoute(crow::SimpleApp& app, VideoServer* server);

    /**
     * @brief 帧提取状态API
     */
    static void setupStatusRoute(crow::SimpleApp& app, VideoServer* server);

    /**
     * @brief 帧提取停止API
     */
    static void setupStopRoute(crow::SimpleApp& app, VideoServer* server);

    /**
     * @brief 帧提取下载API
     */
    static void setupDownloadRoute(crow::SimpleApp& app, VideoServer* server);

    /**
     * @brief 帧预览API
     */
    static void setupPreviewRoute(crow::SimpleApp& app, VideoServer* server);

    /**
     * @brief 实际的帧提取实现
     */
    static void extractFramesFromMJPEG(VideoServer* server, const std::string& task_id,
                                      const std::string& input_file, const std::string& output_dir,
                                      int interval, const std::string& format);
};

} // namespace web
} // namespace cam_server
