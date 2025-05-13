#ifndef I_VIDEO_RECORDER_H
#define I_VIDEO_RECORDER_H

#include <string>
#include <memory>
#include <functional>

#include "camera/camera_device.h"
#include "video/video_recorder.h"

namespace cam_server {
namespace video {

/**
 * @brief 视频录制器接口
 */
class IVideoRecorder {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~IVideoRecorder() = default;

    /**
     * @brief 初始化录制器
     * @param config 录制配置
     * @return 是否初始化成功
     */
    virtual bool initialize(const RecordingConfig& config) = 0;

    /**
     * @brief 开始录制
     * @return 是否成功开始录制
     */
    virtual bool startRecording() = 0;

    /**
     * @brief 停止录制
     * @return 是否成功停止录制
     */
    virtual bool stopRecording() = 0;

    /**
     * @brief 暂停录制
     * @return 是否成功暂停录制
     */
    virtual bool pauseRecording() = 0;

    /**
     * @brief 恢复录制
     * @return 是否成功恢复录制
     */
    virtual bool resumeRecording() = 0;

    /**
     * @brief 处理一帧图像
     * @param frame 帧数据
     * @return 是否成功处理
     */
    virtual bool processFrame(const camera::Frame& frame) = 0;

    /**
     * @brief 获取当前录制状态
     * @return 录制状态
     */
    virtual RecordingStatus getStatus() const = 0;

    /**
     * @brief 设置状态回调函数
     * @param callback 状态回调函数
     */
    virtual void setStatusCallback(std::function<void(const RecordingStatus&)> callback) = 0;

    /**
     * @brief 设置录制配置
     * @param config 录制配置
     * @return 是否成功设置配置
     */
    virtual bool setConfig(const RecordingConfig& config) = 0;

    /**
     * @brief 获取当前录制配置
     * @return 录制配置
     */
    virtual RecordingConfig getConfig() const = 0;
};

// 创建FFmpeg录制器的工厂函数
std::shared_ptr<IVideoRecorder> createFFmpegRecorder();

} // namespace video
} // namespace cam_server

#endif // I_VIDEO_RECORDER_H
