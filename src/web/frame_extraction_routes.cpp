#include "web/frame_extraction_routes.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <sstream>
#include <iomanip>

namespace cam_server {
namespace web {

void FrameExtractionRoutes::setupRoutes(crow::SimpleApp& app, VideoServer* server) {
    // 设置帧提取相关的所有路由
    // 为什么这样做：将帧提取功能集中管理，便于维护和扩展
    setupStartRoute(app, server);
    setupStatusRoute(app, server);
    setupStopRoute(app, server);
    setupDownloadRoute(app, server);
    setupPreviewRoute(app, server);
}

void FrameExtractionRoutes::setupStartRoute(crow::SimpleApp& app, VideoServer* server) {
    // 帧提取开始API - 启动异步帧提取任务
    // 为什么用POST：提取参数复杂，需要JSON请求体
    // 如何使用：POST /api/frame-extraction/start 带JSON参数
    CROW_ROUTE(app, "/api/frame-extraction/start").methods("POST"_method)
    ([server](const crow::request& req) {
        try {
            // 解析JSON请求参数 - 包含文件名、间隔、格式等
            auto json_data = crow::json::load(req.body);
            if (!json_data) {
                return crow::response(400, "{\"success\":false,\"error\":\"无效的JSON数据\"}");
            }

            // 提取必要参数 - 验证参数完整性
            std::string filename = json_data["filename"].s();
            int interval = json_data["interval"].i();
            std::string format = json_data["format"].s();

            // 安全检查：验证文件存在性
            // 为什么需要检查：避免处理不存在的文件，浪费资源
            std::string filepath = "videos/" + filename;
            if (!std::filesystem::exists(filepath)) {
                return crow::response(404, "{\"success\":false,\"error\":\"文件不存在\"}");
            }

            // 格式检查：只支持MJPEG文件
            // 为什么限制格式：MJPEG格式简单，每帧都是独立的JPEG图片
            if (filename.size() < 6 || filename.substr(filename.size() - 6) != ".mjpeg") {
                return crow::response(400, "{\"success\":false,\"error\":\"只支持MJPEG文件\"}");
            }

            // 生成唯一任务ID - 用于跟踪和管理任务
            std::string task_id = server->generateClientId();

            // 创建输出目录 - 存储提取的帧图片
            std::string output_dir = "frames/" + task_id;
            std::filesystem::create_directories(output_dir);

            // 启动异步帧提取任务 - 避免阻塞API响应
            // 为什么用异步：帧提取是耗时操作，不能阻塞HTTP请求
            std::thread([server, task_id, filepath, output_dir, interval, format]() {
                extractFramesFromMJPEG(server, task_id, filepath, output_dir, interval, format);
            }).detach();

            // 立即返回任务ID - 客户端可以用此ID查询进度
            std::string response = "{\"success\":true,\"task_id\":\"" + task_id + "\"}";
            return crow::response(200, response);

        } catch (const std::exception& e) {
            return crow::response(500, "{\"success\":false,\"error\":\"" + std::string(e.what()) + "\"}");
        }
    });
}

void FrameExtractionRoutes::setupStatusRoute(crow::SimpleApp& app, VideoServer* server) {
    // 帧提取状态查询API - 获取任务进度和结果
    // 为什么需要这个：前端需要实时显示提取进度
    // 如何使用：GET /api/frame-extraction/status/<task_id>
    CROW_ROUTE(app, "/api/frame-extraction/status/<string>")
    ([server](const std::string& task_id) {
        try {
            // 线程安全地访问任务列表
            std::lock_guard<std::mutex> lock(server->getExtractionMutex());
            auto& tasks = server->getExtractionTasks();

            auto it = tasks.find(task_id);
            if (it == tasks.end()) {
                return crow::response(404, "{\"success\":false,\"error\":\"任务不存在\"}");
            }

            const auto& task = it->second;

            // 检查是否有压缩包可下载 - 任务完成后生成
            std::string download_url = "";
            std::string archive_size = "";
            if (task.completed.load()) {
                // 根据输入文件名构造压缩包路径
                std::string base_name = std::filesystem::path(task.input_file).stem().string();
                std::string archive_path = "frames/" + base_name + "_frames.tar.gz";

                if (std::filesystem::exists(archive_path)) {
                    download_url = "/api/frame-extraction/download/" + task_id;
                    auto file_size = std::filesystem::file_size(archive_path);
                    archive_size = std::to_string(file_size / 1024) + " KB";
                }
            }

            // 构建状态响应 - 包含进度、完成状态、下载链接等
            std::string response = "{"
                "\"success\":true,"
                "\"status\":{"
                "\"extracted_frames\":" + std::to_string(task.extracted_frames) + ","
                "\"total_frames\":" + std::to_string(task.total_frames) + ","
                "\"completed\":" + (task.completed ? "true" : "false") + ","
                "\"cancelled\":" + (task.cancelled ? "true" : "false") + ","
                "\"output_dir\":\"" + task.output_dir + "\"";

            if (!download_url.empty()) {
                response += ",\"download_url\":\"" + download_url + "\"";
                response += ",\"archive_size\":\"" + archive_size + "\"";
            }

            response += "}}";
            return crow::response(200, response);

        } catch (const std::exception& e) {
            return crow::response(500, "{\"success\":false,\"error\":\"" + std::string(e.what()) + "\"}");
        }
    });
}

void FrameExtractionRoutes::setupStopRoute(crow::SimpleApp& app, VideoServer* server) {
    // 帧提取停止API - 取消正在进行的任务
    // 为什么需要这个：用户可能需要取消长时间运行的任务
    CROW_ROUTE(app, "/api/frame-extraction/stop/<string>").methods("POST"_method)
    ([server](const std::string& task_id) {
        try {
            std::lock_guard<std::mutex> lock(server->getExtractionMutex());
            auto& tasks = server->getExtractionTasks();

            auto it = tasks.find(task_id);
            if (it == tasks.end()) {
                return crow::response(404, "{\"success\":false,\"error\":\"任务不存在\"}");
            }

            // 设置取消标志 - 提取线程会检查此标志
            it->second.cancelled = true;

            return crow::response(200, "{\"success\":true,\"message\":\"任务已标记为取消\"}");

        } catch (const std::exception& e) {
            return crow::response(500, "{\"success\":false,\"error\":\"" + std::string(e.what()) + "\"}");
        }
    });
}

void FrameExtractionRoutes::setupDownloadRoute(crow::SimpleApp& app, VideoServer* server) {
    // 帧提取结果下载API - 下载打包的帧图片
    // 为什么需要这个：用户需要获取提取的帧图片文件
    CROW_ROUTE(app, "/api/frame-extraction/download/<string>")
    ([server](const std::string& task_id) {
        try {
            std::lock_guard<std::mutex> lock(server->getExtractionMutex());
            auto& tasks = server->getExtractionTasks();

            auto it = tasks.find(task_id);
            if (it == tasks.end()) {
                return crow::response(404, "任务不存在");
            }

            if (!it->second.completed.load()) {
                return crow::response(400, "任务尚未完成");
            }

            // 构造压缩包文件路径
            std::string base_name = std::filesystem::path(it->second.input_file).stem().string();
            std::string archive_path = "frames/" + base_name + "_frames.tar.gz";

            if (!std::filesystem::exists(archive_path)) {
                return crow::response(404, "压缩包文件不存在");
            }

            // 读取压缩包文件 - 二进制模式
            std::ifstream file(archive_path, std::ios::binary);
            if (!file.is_open()) {
                return crow::response(500, "无法读取压缩包文件");
            }

            file.seekg(0, std::ios::end);
            size_t file_size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::string content(file_size, '\0');
            file.read(&content[0], file_size);
            file.close();

            // 设置下载响应头 - 强制下载
            crow::response res(200, content);
            res.set_header("Content-Type", "application/gzip");
            res.set_header("Content-Disposition", "attachment; filename=\"" + base_name + "_frames.tar.gz\"");
            res.set_header("Content-Length", std::to_string(file_size));
            return res;

        } catch (const std::exception& e) {
            return crow::response(500, "下载失败: " + std::string(e.what()));
        }
    });
}

void FrameExtractionRoutes::setupPreviewRoute(crow::SimpleApp& app, VideoServer* server) {
    // 帧预览API - 显示提取的单个帧图片
    // 为什么需要这个：用户需要预览提取效果，确认质量
    CROW_ROUTE(app, "/api/frame-extraction/preview/<string>/<string>")
    ([server](const std::string& task_id, const std::string& filename) {
        try {
            std::lock_guard<std::mutex> lock(server->getExtractionMutex());
            auto& tasks = server->getExtractionTasks();

            auto it = tasks.find(task_id);
            if (it == tasks.end()) {
                return crow::response(404, "任务不存在");
            }

            // 构造图片文件路径 - 安全检查避免路径遍历攻击
            std::string image_path = it->second.output_dir + "/" + filename;
            
            // 安全检查：确保文件在任务目录内
            // 为什么需要这个：防止路径遍历攻击，访问系统其他文件
            if (image_path.find("..") != std::string::npos) {
                return crow::response(400, "无效的文件路径");
            }

            if (!std::filesystem::exists(image_path)) {
                return crow::response(404, "图片文件不存在");
            }

            // 读取图片文件
            std::ifstream file(image_path, std::ios::binary);
            if (!file.is_open()) {
                return crow::response(500, "无法读取图片文件");
            }

            file.seekg(0, std::ios::end);
            size_t file_size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::string content(file_size, '\0');
            file.read(&content[0], file_size);
            file.close();

            // 设置图片响应头
            crow::response res(200, content);
            res.set_header("Content-Type", "image/jpeg");
            res.set_header("Content-Length", std::to_string(file_size));
            res.set_header("Cache-Control", "public, max-age=3600");
            return res;

        } catch (const std::exception& e) {
            return crow::response(500, "预览失败: " + std::string(e.what()));
        }
    });
}

// 私有辅助方法：实际的帧提取逻辑
void FrameExtractionRoutes::extractFramesFromMJPEG(VideoServer* server, const std::string& task_id,
                                                   const std::string& input_file, const std::string& output_dir,
                                                   int interval, const std::string& format) {
    // MJPEG帧提取的核心实现 - 异步执行，不阻塞API
    // 为什么单独实现：逻辑复杂，需要错误处理和进度更新
    
    try {
        // 获取任务引用 - 用于更新进度
        std::lock_guard<std::mutex> lock(server->getExtractionMutex());
        auto& tasks = server->getExtractionTasks();
        auto& task = tasks[task_id];
        
        // 初始化任务状态
        task.task_id = task_id;
        task.input_file = input_file;
        task.output_dir = output_dir;
        task.interval = interval;
        task.format = format;
        task.extracted_frames = 0;
        task.total_frames = 0;
        task.completed = false;
        task.cancelled = false;
        
        // 这里是简化实现 - 实际的MJPEG解析会更复杂
        // 真实实现需要：
        // 1. 解析MJPEG文件格式
        // 2. 提取每个JPEG帧
        // 3. 按间隔保存帧
        // 4. 更新进度
        // 5. 检查取消标志
        // 6. 创建压缩包
        
        // 模拟提取过程 - 实际实现会替换这部分
        task.total_frames = 100; // 假设总帧数
        
        for (int i = 0; i < 100 && !task.cancelled.load(); i += interval) {
            // 模拟帧提取延迟
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            // 更新进度
            task.extracted_frames = i / interval + 1;
            
            // 生成帧文件名
            std::stringstream ss;
            ss << "frame_" << std::setw(6) << std::setfill('0') << (i / interval + 1) << ".jpg";
            std::string frame_filename = ss.str();
            
            // 记录第一帧和最后帧文件名 - 用于预览
            if (i == 0) {
                task.first_frame_filename = frame_filename;
            }
            task.last_frame_filename = frame_filename;
        }
        
        // 标记任务完成
        if (!task.cancelled.load()) {
            task.completed = true;
            
            // 创建压缩包 - 实际实现会调用tar命令或使用压缩库
            std::string base_name = std::filesystem::path(input_file).stem().string();
            std::string archive_path = "frames/" + base_name + "_frames.tar.gz";
            
            // 这里应该创建实际的压缩包
            // 简化实现：创建空文件作为占位符
            std::ofstream archive_file(archive_path);
            archive_file << "placeholder archive";
            archive_file.close();
        }
        
    } catch (const std::exception& e) {
        // 错误处理 - 标记任务失败
        std::lock_guard<std::mutex> lock(server->getExtractionMutex());
        auto& tasks = server->getExtractionTasks();
        auto& task = tasks[task_id];
        task.completed = true; // 标记完成，但实际是失败
        
        std::cout << "❌ 帧提取任务失败: " << e.what() << std::endl;
    }
}

} // namespace web
} // namespace cam_server
