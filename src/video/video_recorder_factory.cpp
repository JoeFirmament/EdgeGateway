#include "video/video_recorder_factory.h"
#include "video/ffmpeg_recorder.h"
#include "monitor/logger.h"

namespace cam_server {
namespace video {

std::shared_ptr<IVideoRecorder> VideoRecorderFactory::createRecorder() {
    try {
        // 创建FFmpeg录制器
        auto recorder = std::make_shared<FFmpegRecorder>();
        if (!recorder) {
            LOG_ERROR("无法创建FFmpeg录制器", "VideoRecorderFactory");
            return nullptr;
        }
        
        return recorder;
    } catch (const std::exception& e) {
        LOG_ERROR("创建录制器异常: " + std::string(e.what()), "VideoRecorderFactory");
        return nullptr;
    }
}

} // namespace video
} // namespace cam_server
