#ifndef VIDEO_RECORDER_FACTORY_H
#define VIDEO_RECORDER_FACTORY_H

#include "video/i_video_recorder.h"
#include <memory>

namespace cam_server {
namespace video {

/**
 * @brief 视频录制器工厂类
 */
class VideoRecorderFactory {
public:
    /**
     * @brief 创建视频录制器
     * @return 视频录制器指针
     */
    static std::shared_ptr<IVideoRecorder> createRecorder();
};

} // namespace video
} // namespace cam_server

#endif // VIDEO_RECORDER_FACTORY_H
