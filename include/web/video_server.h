#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>

// Crow框架
#include "third_party/crow/crow.h"

// 项目头文件
#include "camera/camera_manager.h"
#include "system/system_monitor.h"

namespace cam_server {
namespace web {

/**
 * @brief 客户端连接信息
 */
struct ClientInfo {
    crow::websocket::connection* conn;
    std::string current_device;
};

/**
 * @brief 帧提取任务信息
 */
struct ExtractionTask {
    std::string task_id;
    std::string input_file;
    std::string output_dir;
    int interval;
    std::string format;
    std::atomic<int> extracted_frames{0};
    std::atomic<int> total_frames{0};
    std::atomic<bool> completed{false};
    std::atomic<bool> cancelled{false};
    std::string first_frame_filename;
    std::string last_frame_filename;

    // 删除拷贝构造和赋值操作
    ExtractionTask(const ExtractionTask&) = delete;
    ExtractionTask& operator=(const ExtractionTask&) = delete;

    // 默认构造函数
    ExtractionTask() = default;

    // 移动构造函数
    ExtractionTask(ExtractionTask&& other) noexcept;

    // 移动赋值操作符
    ExtractionTask& operator=(ExtractionTask&& other) noexcept;
};

/**
 * @brief 视频流服务器主类
 */
class VideoServer {
public:
    VideoServer();
    ~VideoServer();

    /**
     * @brief 初始化服务器
     */
    bool initialize();

    /**
     * @brief 启动服务器
     */
    bool start();

    /**
     * @brief 停止服务器
     */
    void stop();

    /**
     * @brief 等待服务器停止
     */
    void waitForStop();

    /**
     * @brief 设置端口
     */
    void setPort(int port) { port_ = port; }

private:
    // 核心组件
    crow::SimpleApp app_;
    int port_;
    bool is_running_;
    std::thread server_thread_;

    // WebSocket客户端管理
    std::unordered_map<std::string, ClientInfo> clients_;
    std::mutex clients_mutex_;

    // 视频流相关
    std::atomic<size_t> frame_count_;
    std::atomic<bool> is_recording_;
    std::atomic<size_t> recording_frame_count_;
    std::atomic<size_t> recording_file_size_;

    // 帧提取相关
    std::unordered_map<std::string, ExtractionTask> extraction_tasks_;
    std::mutex extraction_mutex_;

    // 初始化方法
    void setupRoutes();
    void setupStaticRoutes();
    void setupApiRoutes();
    void setupWebSocketRoutes();

    // 工具方法
    crow::response serveHtmlFile(const std::string& filepath);
    void setupDynamicHtmlRoutes();

    // 帧提取相关方法
    void extractFramesFromMJPEG(const std::string& task_id, const std::string& input_file,
                               const std::string& output_dir, int interval, const std::string& format);
    bool saveFrameAsJPEG(const std::vector<uint8_t>& frame_data, const std::string& output_path);
    std::string createFrameArchive(const std::string& task_id, const std::string& frames_dir,
                                  const std::string& input_file, int frame_count);

public:
    // 访问器方法 - 供其他模块使用
    std::unordered_map<std::string, ClientInfo>& getClients();
    std::mutex& getClientsMutex();
    std::unordered_map<std::string, ExtractionTask>& getExtractionTasks();
    std::mutex& getExtractionMutex();
    std::atomic<size_t>& getFrameCount();
    std::atomic<bool>& getIsRecording();
    std::atomic<size_t>& getRecordingFrameCount();
    std::atomic<size_t>& getRecordingFileSize();

    // 工具方法 - 供其他模块使用
    std::string generateClientId();
};

} // namespace web
} // namespace cam_server
