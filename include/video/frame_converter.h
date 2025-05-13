#ifndef FRAME_CONVERTER_H
#define FRAME_CONVERTER_H

#include <memory>
#include "camera/camera_device.h"

// 前向声明，避免包含FFmpeg头文件
struct AVFrame;
struct SwsContext;

namespace cam_server {
namespace video {

/**
 * @brief 帧转换器类，用于转换不同格式的帧
 */
class FrameConverter {
public:
    /**
     * @brief 构造函数
     */
    FrameConverter();

    /**
     * @brief 析构函数
     */
    ~FrameConverter();

    /**
     * @brief 初始化转换器
     * @param src_width 源宽度
     * @param src_height 源高度
     * @param src_format 源格式
     * @param dst_width 目标宽度
     * @param dst_height 目标高度
     * @param dst_format 目标格式
     * @return 是否初始化成功
     */
    bool initialize(int src_width, int src_height, camera::PixelFormat src_format,
                   int dst_width, int dst_height, camera::PixelFormat dst_format);

    /**
     * @brief 转换帧
     * @param src_frame 源帧
     * @return 转换后的帧
     */
    camera::Frame convert(const camera::Frame& src_frame);

    /**
     * @brief 转换为FFmpeg AVFrame
     * @param src_frame 源帧
     * @param dst_frame 目标AVFrame
     * @return 是否转换成功
     */
    bool convertToAVFrame(const camera::Frame& src_frame, AVFrame* dst_frame);

    /**
     * @brief 从FFmpeg AVFrame转换
     * @param src_frame 源AVFrame
     * @return 转换后的帧
     */
    camera::Frame convertFromAVFrame(const AVFrame* src_frame);

    /**
     * @brief 重置转换器
     */
    void reset();

private:
    // 初始化SwsContext
    bool initSwsContext();
    // 清理资源
    void cleanup();
    // 将PixelFormat转换为FFmpeg格式
    int pixelFormatToAVFormat(camera::PixelFormat format) const;
    // 将FFmpeg格式转换为PixelFormat
    camera::PixelFormat avFormatToPixelFormat(int av_format) const;

    // 源宽度
    int src_width_;
    // 源高度
    int src_height_;
    // 源格式
    camera::PixelFormat src_format_;
    // 目标宽度
    int dst_width_;
    // 目标高度
    int dst_height_;
    // 目标格式
    camera::PixelFormat dst_format_;
    // 是否已初始化
    bool is_initialized_;
    // FFmpeg SwsContext
    SwsContext* sws_context_;
};

} // namespace video
} // namespace cam_server

#endif // FRAME_CONVERTER_H
