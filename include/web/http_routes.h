#pragma once

#include "third_party/crow/crow.h"

namespace cam_server {
namespace web {

/**
 * @brief HTTP路由设置类
 */
class HttpRoutes {
public:
    /**
     * @brief 设置静态文件路由
     */
    static void setupStaticRoutes(crow::SimpleApp& app);
    
    /**
     * @brief 设置图片API路由
     */
    static void setupPhotoRoutes(crow::SimpleApp& app);
    
    /**
     * @brief 设置视频API路由
     */
    static void setupVideoRoutes(crow::SimpleApp& app);
    
    /**
     * @brief 设置基本页面路由
     */
    static void setupPageRoutes(crow::SimpleApp& app);

private:
    /**
     * @brief 服务HTML文件
     */
    static crow::response serveHtmlFile(const std::string& filepath);
    
    /**
     * @brief 设置动态HTML路由
     */
    static void setupDynamicHtmlRoutes(crow::SimpleApp& app);
};

} // namespace web
} // namespace cam_server
