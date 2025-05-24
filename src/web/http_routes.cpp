#include "web/http_routes.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <chrono>

namespace cam_server {
namespace web {

void HttpRoutes::setupStaticRoutes(crow::SimpleApp& app) {
    // 动态扫描HTML页面 - 与原始实现保持一致
    setupDynamicHtmlRoutes(app);

    // 设置根路径路由 - 默认返回主页
    // 为什么这样做：提供用户友好的入口点，避免404错误
    CROW_ROUTE(app, "/")
    ([](const crow::request& /*req*/) {
        return serveHtmlFile("static/index.html");
    });

    // 动态HTML页面路由 - 支持自动发现页面
    // 为什么这样做：避免为每个HTML页面手动添加路由，支持热添加页面
    // 如何使用：访问 /page_name.html 会自动查找对应文件
    CROW_ROUTE(app, "/<string>")
    ([](const crow::request& /*req*/, const std::string& filename) {
        // 安全检查：只允许HTML文件访问，防止任意文件读取
        if (filename.size() < 5 || filename.substr(filename.size() - 5) != ".html") {
            return crow::response(404, "Only HTML files are supported: " + filename);
        }

        // 优先级搜索：先查找pages目录，再查找static目录
        // 为什么这样做：pages目录存放功能页面，static目录存放通用页面
        std::string filepath = "static/pages/" + filename;
        if (std::filesystem::exists(filepath)) {
            return serveHtmlFile(filepath);
        }

        filepath = "static/" + filename;
        if (std::filesystem::exists(filepath)) {
            return serveHtmlFile(filepath);
        }

        return crow::response(404, "Page not found: " + filename);
    });

    // CSS文件服务 - 支持样式文件的独立管理
    // 为什么这样做：CSS文件需要正确的MIME类型和缓存策略
    CROW_ROUTE(app, "/static/css/<string>")
    ([](const crow::request& /*req*/, const std::string& filename) {
        std::string filepath = "static/css/" + filename;

        if (!std::filesystem::exists(filepath)) {
            return crow::response(404, "CSS文件不存在");
        }

        std::ifstream file(filepath);
        if (!file.is_open()) {
            return crow::response(500, "无法读取CSS文件");
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();

        // 设置正确的MIME类型和缓存策略
        // 为什么这样做：浏览器需要正确的Content-Type来解析CSS，缓存提高性能
        crow::response res(200, content);
        res.set_header("Content-Type", "text/css; charset=utf-8");
        res.set_header("Cache-Control", "public, max-age=3600"); // 1小时缓存
        return res;
    });
}

void HttpRoutes::setupPhotoRoutes(crow::SimpleApp& app) {
    // 图片文件服务API - 直接返回图片数据
    // 为什么这样做：前端需要直接显示图片，而不是下载
    // 如何使用：GET /api/photos/image.jpg
    CROW_ROUTE(app, "/api/photos/<string>")
    ([](const crow::request& /*req*/, const std::string& filename) {
        std::string filepath = "photos/" + filename;

        if (!std::filesystem::exists(filepath)) {
            return crow::response(404, "图片文件不存在");
        }

        // 以二进制模式读取图片文件
        // 为什么这样做：图片是二进制数据，文本模式会损坏数据
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return crow::response(500, "无法读取图片文件");
        }

        // 获取文件大小并读取完整内容
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string content(file_size, '\0');
        file.read(&content[0], file_size);
        file.close();

        // 设置图片响应头
        // 为什么这样做：浏览器需要正确的MIME类型来显示图片
        crow::response res(200, content);
        res.set_header("Content-Type", "image/jpeg");
        res.set_header("Content-Length", std::to_string(file_size));
        res.set_header("Cache-Control", "public, max-age=3600");
        return res;
    });

    // 图片列表API - 返回所有可用图片的元数据
    // 为什么这样做：前端需要知道有哪些图片可用，以及它们的基本信息
    // 如何使用：GET /api/photos 返回JSON格式的图片列表
    CROW_ROUTE(app, "/api/photos")
    ([](const crow::request& /*req*/) {
        try {
            std::string photos_json = "{\"photos\":[";
            bool first = true;

            // 检查photos目录是否存在
            if (std::filesystem::exists("photos") && std::filesystem::is_directory("photos")) {
                // 遍历photos目录中的所有JPG文件
                for (const auto& entry : std::filesystem::directory_iterator("photos")) {
                    if (entry.is_regular_file() && entry.path().extension() == ".jpg") {
                        if (!first) photos_json += ",";
                        first = false;

                        std::string filename = entry.path().filename().string();
                        auto file_size = std::filesystem::file_size(entry.path());
                        auto ftime = std::filesystem::last_write_time(entry.path());

                        // 转换文件时间为Unix时间戳
                        // 为什么这样做：前端JavaScript更容易处理Unix时间戳
                        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();

                        // 构建图片信息JSON对象
                        photos_json += "{"
                            "\"filename\":\"" + filename + "\","
                            "\"size\":" + std::to_string(file_size) + ","
                            "\"timestamp\":" + std::to_string(timestamp) + ","
                            "\"url\":\"/api/photos/" + filename + "\""
                            "}";
                    }
                }
            }

            photos_json += "]}";

            crow::response res(200, photos_json);
            res.set_header("Content-Type", "application/json");
            return res;

        } catch (const std::exception& e) {
            return crow::response(500, "{\"error\":\"获取图片列表失败\"}");
        }
    });

    // 图片下载API - 强制下载而不是显示
    // 为什么这样做：用户可能需要保存图片到本地
    // 如何使用：GET /api/photos/image.jpg/download
    CROW_ROUTE(app, "/api/photos/<string>/download")
    ([](const crow::request& /*req*/, const std::string& filename) {
        std::string filepath = "photos/" + filename;

        if (!std::filesystem::exists(filepath)) {
            return crow::response(404, "图片文件不存在");
        }

        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return crow::response(500, "无法读取图片文件");
        }

        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string content(file_size, '\0');
        file.read(&content[0], file_size);
        file.close();

        // 设置下载响应头 - 强制浏览器下载而不是显示
        // 为什么这样做：Content-Disposition: attachment 告诉浏览器这是下载文件
        crow::response res(200, content);
        res.set_header("Content-Type", "application/octet-stream");
        res.set_header("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        res.set_header("Content-Length", std::to_string(file_size));
        return res;
    });
}

void HttpRoutes::setupVideoRoutes(crow::SimpleApp& app) {
    // 视频文件服务API - 支持多种视频格式
    // 为什么这样做：录制的视频需要能够在浏览器中播放或下载
    // 如何使用：GET /api/videos/video.avi 或 /api/videos/video.mjpeg
    CROW_ROUTE(app, "/api/videos/<string>")
    ([](const crow::request& /*req*/, const std::string& filename) {
        std::string filepath = "videos/" + filename;

        if (!std::filesystem::exists(filepath)) {
            return crow::response(404, "视频文件不存在");
        }

        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return crow::response(500, "无法读取视频文件");
        }

        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string content(file_size, '\0');
        file.read(&content[0], file_size);
        file.close();

        // 根据文件扩展名设置正确的MIME类型
        // 为什么这样做：不同的视频格式需要不同的MIME类型才能正确播放
        crow::response res(200, content);
        if (filepath.size() >= 6 && filepath.substr(filepath.size() - 6) == ".mjpeg") {
            res.set_header("Content-Type", "video/x-motion-jpeg");
        } else {
            res.set_header("Content-Type", "video/avi");
        }
        res.set_header("Content-Length", std::to_string(file_size));
        res.set_header("Cache-Control", "public, max-age=3600");
        return res;
    });

    // 视频列表API - 返回所有可用视频的元数据
    // 为什么这样做：前端需要显示视频库，包括文件大小、创建时间等信息
    CROW_ROUTE(app, "/api/videos")
    ([](const crow::request& /*req*/) {
        try {
            std::string videos_json = "{\"videos\":[";
            bool first = true;

            if (std::filesystem::exists("videos") && std::filesystem::is_directory("videos")) {
                // 支持多种视频格式
                for (const auto& entry : std::filesystem::directory_iterator("videos")) {
                    if (entry.is_regular_file() &&
                        (entry.path().extension() == ".avi" || entry.path().extension() == ".mjpeg")) {

                        if (!first) videos_json += ",";
                        first = false;

                        std::string filename = entry.path().filename().string();
                        auto file_size = std::filesystem::file_size(entry.path());
                        auto ftime = std::filesystem::last_write_time(entry.path());

                        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();

                        // 提供播放和下载两种URL
                        // 为什么这样做：用户可能需要在线播放或下载到本地
                        videos_json += "{"
                            "\"filename\":\"" + filename + "\","
                            "\"size\":" + std::to_string(file_size) + ","
                            "\"timestamp\":" + std::to_string(timestamp) + ","
                            "\"url\":\"/api/videos/" + filename + "\","
                            "\"download_url\":\"/api/videos/" + filename + "/download\""
                            "}";
                    }
                }
            }

            videos_json += "]}";

            crow::response res(200, videos_json);
            res.set_header("Content-Type", "application/json");
            return res;

        } catch (const std::exception& e) {
            return crow::response(500, "{\"error\":\"获取视频列表失败\"}");
        }
    });

    // 视频下载API - 强制下载视频文件
    CROW_ROUTE(app, "/api/videos/<string>/download")
    ([](const crow::request& /*req*/, const std::string& filename) {
        std::string filepath = "videos/" + filename;

        if (!std::filesystem::exists(filepath)) {
            return crow::response(404, "视频文件不存在");
        }

        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return crow::response(500, "无法读取视频文件");
        }

        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string content(file_size, '\0');
        file.read(&content[0], file_size);
        file.close();

        crow::response res(200, content);
        res.set_header("Content-Type", "application/octet-stream");
        res.set_header("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        res.set_header("Content-Length", std::to_string(file_size));
        return res;
    });
}

void HttpRoutes::setupPageRoutes(crow::SimpleApp& app) {
    // 页面路由在setupStaticRoutes中已经处理
    // 这个方法保留用于未来可能的页面特定逻辑
    // 为什么这样做：保持接口的完整性和未来的扩展性
}

crow::response HttpRoutes::serveHtmlFile(const std::string& filepath) {
    // HTML文件服务的通用方法
    // 为什么这样做：统一HTML文件的处理逻辑，确保一致的缓存策略
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return crow::response(404, "页面不存在");
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();

    // 设置HTML响应头 - 禁用缓存确保开发时能看到最新内容
    // 为什么这样做：HTML页面经常更新，缓存会导致用户看到旧版本
    crow::response res(200, content);
    res.set_header("Content-Type", "text/html; charset=utf-8");
    res.set_header("Cache-Control", "no-cache, no-store, must-revalidate");
    res.set_header("Pragma", "no-cache");
    res.set_header("Expires", "0");
    return res;
}

void HttpRoutes::setupDynamicHtmlRoutes(crow::SimpleApp& app) {
    // 动态扫描HTML页面 - 直接从原始实现复制
    std::cout << "📄 动态扫描HTML页面..." << std::endl;

    int page_count = 0;

    // 扫描static/pages目录
    if (std::filesystem::exists("static/pages")) {
        for (const auto& entry : std::filesystem::directory_iterator("static/pages")) {
            if (entry.is_regular_file() && entry.path().extension() == ".html") {
                std::string filename = entry.path().filename().string();
                std::cout << "  ✅ 发现页面: " << filename << std::endl;
                page_count++;
            }
        }
    }

    // 扫描static目录
    if (std::filesystem::exists("static")) {
        for (const auto& entry : std::filesystem::directory_iterator("static")) {
            if (entry.is_regular_file() && entry.path().extension() == ".html") {
                std::string filename = entry.path().filename().string();
                std::cout << "  ✅ 发现页面: " << filename << std::endl;
                page_count++;
            }
        }
    }

    std::cout << "📊 总共发现 " << page_count << " 个HTML页面" << std::endl;
    std::cout << "🔗 页面访问方式:" << std::endl;
    std::cout << "  - 主页: http://localhost:8081/" << std::endl;
    std::cout << "  - 功能页面: http://localhost:8081/页面名.html" << std::endl;
    std::cout << "  - 统一导航: 所有页面都有顶部导航栏" << std::endl;
}

} // namespace web
} // namespace cam_server
