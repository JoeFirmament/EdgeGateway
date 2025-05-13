#ifndef VIDEO_RECORDER_H
#define VIDEO_RECORDER_H

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>

#include "camera/camera_device.h"

// 前向声明，避免包含FFmpeg头文件
struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVFrame;
struct AVPacket;

namespace cam_server {
namespace video {

/**
 * @brief 录制配置结构体
 */
struct RecordingConfig {
    // 输出文件路径
    std::string output_path;
    // 视频编码器名称（如h264_rkmpp, libx264等）
    std::string encoder_name;
    // 视频容器格式（如mp4, mkv等）
    std::string container_format;
    // 视频宽度
    int width;
    // 视频高度
    int height;
    // 帧率
    int fps;
    // 比特率（bps）
    int bitrate;
    // 关键帧间隔（GOP）
    int gop;
    // 是否使用硬件加速
    bool use_hw_accel;
    // 最大录制时长（秒），0表示不限制
    int max_duration;
    // 最大文件大小（字节），0表示不限制
    int64_t max_size;
};

/**
 * @brief 录制状态枚举
 */
enum class RecordingState {
    IDLE,       // 空闲状态
    RECORDING,  // 正在录制
    PAUSED,     // 暂停录制
    ERROR       // 错误状态
};

/**
 * @brief 录制状态信息结构体
 */
struct RecordingStatus {
    // 当前状态
    RecordingState state;
    // 当前输出文件路径
    std::string current_file;
    // 已录制时长（秒）
    double duration;
    // 已录制帧数
    int64_t frame_count;
    // 当前文件大小（字节）
    int64_t file_size;
    // 错误信息（如果状态为ERROR）
    std::string error_message;
};

/**
 * @brief 视频录制器类
 */
class VideoRecorder {
public:
    /**
     * @brief 构造函数
     */
    VideoRecorder();

    /**
     * @brief 析构函数
     */
    ~VideoRecorder();

    /**
     * @brief 初始化录制器
     * @param config 录制配置
     * @return 是否初始化成功
     */
    bool initialize(const RecordingConfig& config);

    /**
     * @brief 开始录制
     * @return 是否成功开始录制
     */
    bool startRecording();

    /**
     * @brief 停止录制
     * @return 是否成功停止录制
     */
    bool stopRecording();

    /**
     * @brief 暂停录制
     * @return 是否成功暂停录制
     */
    bool pauseRecording();

    /**
     * @brief 恢复录制
     * @return 是否成功恢复录制
     */
    bool resumeRecording();

    /**
     * @brief 处理一帧图像
     * @param frame 帧数据
     * @return 是否成功处理
     */
    bool processFrame(const camera::Frame& frame);

    /**
     * @brief 获取当前录制状态
     * @return 录制状态
     */
    RecordingStatus getStatus() const;

    /**
     * @brief 设置状态回调函数
     * @param callback 状态回调函数
     */
    void setStatusCallback(std::function<void(const RecordingStatus&)> callback);

    /**
     * @brief 设置录制配置
     * @param config 录制配置
     * @return 是否成功设置配置
     */
    bool setConfig(const RecordingConfig& config);

    /**
     * @brief 获取当前录制配置
     * @return 录制配置
     */
    RecordingConfig getConfig() const;

private:
    // 初始化FFmpeg
    bool initFFmpeg();
    // 清理FFmpeg资源
    void cleanupFFmpeg();
    // 创建输出格式上下文
    bool createOutputFormatContext();
    // 创建视频流
    bool createVideoStream();
    // 打开编码器
    bool openEncoder();
    // 写入文件头
    bool writeHeader();
    // 写入文件尾
    bool writeTrailer();
    // 编码并写入一帧
    bool encodeAndWriteFrame(const camera::Frame& frame);
    // 更新录制状态
    void updateStatus();
    // 检查是否需要分段
    bool checkSegmentation();
    // 创建新的分段文件
    bool createNewSegment();

    // 录制配置
    RecordingConfig config_;
    // 当前录制状态
    RecordingStatus status_;
    // 状态互斥锁
    mutable std::mutex status_mutex_;
    // 状态回调函数
    std::function<void(const RecordingStatus&)> status_callback_;
    // 是否已初始化
    bool is_initialized_;
    // 录制开始时间
    int64_t start_time_;
    // 上一帧时间戳
    int64_t last_frame_timestamp_;
    // 分段索引
    int segment_index_;

    // FFmpeg相关成员
    AVFormatContext* format_context_;
    AVCodecContext* codec_context_;
    AVStream* video_stream_;
    AVFrame* frame_;
    AVPacket* packet_;
};

} // namespace video
} // namespace cam_server

#endif // VIDEO_RECORDER_H
