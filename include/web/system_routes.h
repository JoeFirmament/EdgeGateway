#pragma once

#include "third_party/crow/crow.h"

namespace cam_server {
namespace web {

/**
 * @brief 系统信息路由设置类
 */
class SystemRoutes {
public:
    /**
     * @brief 设置系统信息相关路由
     */
    static void setupRoutes(crow::SimpleApp& app);

private:
    /**
     * @brief 系统信息API
     */
    static void setupSystemInfoRoute(crow::SimpleApp& app);
    
    /**
     * @brief 摄像头设备信息API
     */
    static void setupCameraInfoRoute(crow::SimpleApp& app);
};

} // namespace web
} // namespace cam_server
