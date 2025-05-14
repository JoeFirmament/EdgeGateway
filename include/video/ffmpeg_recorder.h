#ifndef FFMPEG_RECORDER_H
#define FFMPEG_RECORDER_H

#include "video/i_video_recorder.h"
#include <mutex>
#include <atomic>
#include <memory>

// 前向声明，避免包含FFmpeg头文件
struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace cam_server {
namespace video {

/**
 * @brief 基于FFmpeg的视频录制器实现
 */
class FFmpegRecorder : public IVideoRecorder {
public:
    /**
     * @brief 构造函数
     */
    FFmpegRecorder();

    /**
     * @brief 析构函数
     */
    ~FFmpegRecorder() override;

    /**
     * @brief 初始化录制器
     * @param config 录制配置
     * @return 是否初始化成功
     */
    bool initialize(const RecordingConfig& config) override;

    /**
     * @brief 开始录制
     * @return 是否成功开始录制
     */
    bool startRecording() override;

    /**
     * @brief 停止录制
     * @return 是否成功停止录制
     */
    bool stopRecording() override;

    /**
     * @brief 暂停录制
     * @return 是否成功暂停录制
     */
    bool pauseRecording() override;

    /**
     * @brief 恢复录制
     * @return 是否成功恢复录制
     */
    bool resumeRecording() override;

    /**
     * @brief 处理一帧图像
     * @param frame 帧数据
     * @return 是否成功处理
     */
    bool processFrame(const camera::Frame& frame) override;

    /**
     * @brief 获取当前录制状态
     * @return 录制状态
     */
    RecordingStatus getStatus() const override;

    /**
     * @brief 设置状态回调函数
     * @param callback 状态回调函数
     */
    void setStatusCallback(std::function<void(const RecordingStatus&)> callback) override;

    /**
     * @brief 设置录制配置
     * @param config 录制配置
     * @return 是否成功设置配置
     */
    bool setConfig(const RecordingConfig& config) override;

    /**
     * @brief 获取当前录制配置
     * @return 录制配置
     */
    RecordingConfig getConfig() const override;

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
    // 生成文件名
    std::string generateFileName();

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

    // FFmpeg相关
    AVFormatContext* format_context_;
    AVCodecContext* codec_context_;
    AVStream* video_stream_;
    AVFrame* frame_;
    AVPacket* packet_;
    SwsContext* sws_context_;
    std::atomic<bool> is_recording_;
    std::atomic<bool> is_paused_;
};

} // namespace video
} // namespace cam_server

#endif // FFMPEG_RECORDER_H
