#ifndef I_VIDEO_SPLITTER_H
#define I_VIDEO_SPLITTER_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>

namespace cam_server {
namespace video {

/**
 * @brief 视频分割配置结构体
 */
struct SplitConfig {
    // 输入视频路径
    std::string input_path;
    // 输出目录路径
    std::string output_dir;
    // 输出文件名前缀
    std::string output_prefix;
    // 输出文件格式（jpg, png等）
    std::string output_format;
    // 输出图像质量（1-100）
    int quality;
    // 每隔多少秒提取一帧
    double interval;
    // 是否按时间点提取
    bool extract_by_time;
    // 时间点列表（秒）
    std::vector<double> time_points;
    // 是否按帧号提取
    bool extract_by_frame;
    // 帧号列表
    std::vector<int> frame_numbers;
    // 最大提取帧数，0表示不限制
    int max_frames;
};

/**
 * @brief 任务状态枚举
 */
enum class SplitTaskState {
    PENDING,    // 等待执行
    RUNNING,    // 正在执行
    COMPLETED,  // 已完成
    CANCELLED,  // 已取消
    ERROR       // 错误
};

/**
 * @brief 任务状态结构体
 */
struct SplitTaskStatus {
    // 任务ID
    std::string task_id;
    // 任务状态
    SplitTaskState state;
    // 输入文件
    std::string input_path;
    // 输出目录
    std::string output_dir;
    // 已处理帧数
    int processed_frames;
    // 总帧数
    int total_frames;
    // 已生成图像数
    int generated_images;
    // 进度（0-1）
    double progress;
    // 开始时间（毫秒时间戳）
    int64_t start_time;
    // 结束时间（毫秒时间戳）
    int64_t end_time;
    // 错误信息
    std::string error_message;
};

/**
 * @brief 视频分割器接口
 */
class IVideoSplitter {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~IVideoSplitter() = default;

    /**
     * @brief 初始化分割器
     * @return 是否初始化成功
     */
    virtual bool initialize() = 0;

    /**
     * @brief 创建分割任务
     * @param config 分割配置
     * @return 任务ID
     */
    virtual std::string createTask(const SplitConfig& config) = 0;

    /**
     * @brief 开始执行任务
     * @param taskId 任务ID
     * @return 是否成功开始任务
     */
    virtual bool startTask(const std::string& taskId) = 0;

    /**
     * @brief 取消任务
     * @param taskId 任务ID
     * @return 是否成功取消任务
     */
    virtual bool cancelTask(const std::string& taskId) = 0;

    /**
     * @brief 获取任务状态
     * @param taskId 任务ID
     * @return 任务状态
     */
    virtual SplitTaskStatus getTaskStatus(const std::string& taskId) const = 0;

    /**
     * @brief 获取所有任务状态
     * @return 所有任务状态列表
     */
    virtual std::vector<SplitTaskStatus> getAllTaskStatus() const = 0;

    /**
     * @brief 设置状态回调函数
     * @param callback 状态回调函数
     */
    virtual void setStatusCallback(std::function<void(const SplitTaskStatus&)> callback) = 0;

    /**
     * @brief 清理已完成的任务
     * @param keepLastN 保留最近的N个任务，0表示全部清理
     * @return 清理的任务数量
     */
    virtual int cleanupCompletedTasks(int keepLastN = 0) = 0;
};

// 创建FFmpeg分割器的工厂函数
std::shared_ptr<IVideoSplitter> createFFmpegSplitter();

} // namespace video
} // namespace cam_server

#endif // I_VIDEO_SPLITTER_H
