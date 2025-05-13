#ifndef VIDEO_SPLITTER_H
#define VIDEO_SPLITTER_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <future>

namespace cam_server {
namespace video {

/**
 * @brief 拆分配置结构体
 */
struct SplitConfig {
    // 输入视频文件路径
    std::string input_path;
    // 输出目录路径
    std::string output_dir;
    // 输出图像格式（如jpg, png等）
    std::string output_format;
    // 输出图像质量（0-100）
    int quality;
    // 拆分频率（每秒提取的帧数，0表示提取所有帧）
    double frame_rate;
    // 是否按时间间隔提取（true: 按时间间隔, false: 按帧间隔）
    bool time_interval;
    // 间隔值（如果time_interval为true，表示秒；否则表示帧数）
    double interval;
    // 是否保留原始时间戳信息
    bool preserve_timestamps;
    // 是否生成缩略图
    bool generate_thumbnails;
    // 缩略图大小
    int thumbnail_size;
};

/**
 * @brief 拆分任务状态枚举
 */
enum class SplitTaskState {
    PENDING,    // 等待中
    RUNNING,    // 运行中
    COMPLETED,  // 已完成
    CANCELLED,  // 已取消
    ERROR       // 错误
};

/**
 * @brief 拆分任务状态结构体
 */
struct SplitTaskStatus {
    // 任务ID
    std::string task_id;
    // 任务状态
    SplitTaskState state;
    // 进度（0.0-1.0）
    double progress;
    // 已处理帧数
    int processed_frames;
    // 总帧数
    int total_frames;
    // 已生成的图像文件数
    int generated_images;
    // 开始时间
    int64_t start_time;
    // 结束时间（如果已完成）
    int64_t end_time;
    // 错误信息（如果状态为ERROR）
    std::string error_message;
    // 输出目录
    std::string output_dir;
};

/**
 * @brief 视频拆分器类
 */
class VideoSplitter {
public:
    /**
     * @brief 构造函数
     */
    VideoSplitter();

    /**
     * @brief 析构函数
     */
    ~VideoSplitter();

    /**
     * @brief 初始化拆分器
     * @return 是否初始化成功
     */
    bool initialize();

    /**
     * @brief 创建拆分任务
     * @param config 拆分配置
     * @return 任务ID，空字符串表示创建失败
     */
    std::string createTask(const SplitConfig& config);

    /**
     * @brief 开始拆分任务
     * @param task_id 任务ID
     * @return 是否成功开始任务
     */
    bool startTask(const std::string& task_id);

    /**
     * @brief 取消拆分任务
     * @param task_id 任务ID
     * @return 是否成功取消任务
     */
    bool cancelTask(const std::string& task_id);

    /**
     * @brief 获取任务状态
     * @param task_id 任务ID
     * @return 任务状态
     */
    SplitTaskStatus getTaskStatus(const std::string& task_id) const;

    /**
     * @brief 获取所有任务状态
     * @return 所有任务状态列表
     */
    std::vector<SplitTaskStatus> getAllTaskStatus() const;

    /**
     * @brief 设置任务状态回调函数
     * @param callback 状态回调函数
     */
    void setStatusCallback(std::function<void(const SplitTaskStatus&)> callback);

    /**
     * @brief 清理已完成的任务
     * @param keep_last_n 保留最近的n个任务，0表示清理所有已完成任务
     * @return 清理的任务数量
     */
    int cleanupCompletedTasks(int keep_last_n = 0);

private:
    // 拆分任务结构体
    struct SplitTask {
        // 任务ID
        std::string task_id;
        // 拆分配置
        SplitConfig config;
        // 任务状态
        SplitTaskStatus status;
        // 任务线程
        std::thread thread;
        // 取消标志
        std::atomic<bool> cancel_flag;
        // 任务promise
        std::promise<void> promise;
        // 任务future
        std::future<void> future;
    };

    // 执行拆分任务
    void executeTask(std::shared_ptr<SplitTask> task);
    // 更新任务状态
    void updateTaskStatus(const std::string& task_id, const SplitTaskStatus& status);
    // 生成唯一任务ID
    std::string generateTaskId() const;
    // 获取视频信息
    bool getVideoInfo(const std::string& video_path, int& width, int& height, 
                     double& duration, int& total_frames, double& frame_rate);
    // 创建输出目录
    bool createOutputDirectory(const std::string& dir_path);
    // 提取帧并保存为图像
    bool extractFrameAndSave(const std::string& video_path, const std::string& output_path, 
                           double timestamp, int width, int height, const std::string& format, int quality);
    // 生成缩略图
    bool generateThumbnail(const std::string& image_path, const std::string& thumbnail_path, int size);

    // 任务列表
    std::vector<std::shared_ptr<SplitTask>> tasks_;
    // 任务列表互斥锁
    mutable std::mutex tasks_mutex_;
    // 状态回调函数
    std::function<void(const SplitTaskStatus&)> status_callback_;
    // 是否已初始化
    bool is_initialized_;
};

} // namespace video
} // namespace cam_server

#endif // VIDEO_SPLITTER_H
