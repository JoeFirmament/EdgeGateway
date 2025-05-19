#ifndef CAMERA_FRAME_H
#define CAMERA_FRAME_H

#include <cstdint>
#include <memory>
#include <vector>

namespace cam_server {
namespace camera {

/**
 * @brief 像素格式枚举
 */
enum class PixelFormat {
    UNKNOWN,
    YUYV,
    MJPEG,
    H264,
    NV12,
    RGB24,
    BGR24,
    RGBA32,
    BGRA32,
    YUV420P
};

/**
 * @brief 帧元数据结构
 */
struct FrameMetadata {
    uint64_t timestamp;      // 时间戳（微秒）
    uint32_t sequence;       // 序列号
    uint32_t exposure_time;  // 曝光时间
    uint32_t gain;          // 增益
};

/**
 * @brief 帧类，表示一帧图像数据
 */
class Frame {
public:
    /**
     * @brief 默认构造函数
     */
    Frame() = default;

    /**
     * @brief 构造函数
     * @param width 宽度
     * @param height 高度
     * @param format 像素格式
     * @param data 图像数据
     */
    Frame(int width, int height, PixelFormat format, std::vector<uint8_t> data);

    /**
     * @brief 获取帧宽度
     * @return 帧宽度
     */
    int getWidth() const { return width_; }

    /**
     * @brief 获取帧高度
     * @return 帧高度
     */
    int getHeight() const { return height_; }

    /**
     * @brief 获取像素格式
     * @return 像素格式
     */
    PixelFormat getFormat() const { return format_; }

    /**
     * @brief 获取图像数据
     * @return 图像数据的常量引用
     */
    const std::vector<uint8_t>& getData() const { return data_; }

    /**
     * @brief 获取图像数据
     * @return 图像数据的引用
     */
    std::vector<uint8_t>& getData() { return data_; }

    /**
     * @brief 获取元数据
     * @return 元数据的常量引用
     */
    const FrameMetadata& getMetadata() const { return metadata_; }

    /**
     * @brief 设置元数据
     * @param metadata 元数据
     */
    void setMetadata(const FrameMetadata& metadata) { metadata_ = metadata; }

    /**
     * @brief 设置宽度
     * @param width 宽度
     */
    void setWidth(int width) { width_ = width; }

    /**
     * @brief 设置高度
     * @param height 高度
     */
    void setHeight(int height) { height_ = height; }

    /**
     * @brief 设置格式
     * @param format 格式
     */
    void setFormat(PixelFormat format) { format_ = format; }

    /**
     * @brief 设置时间戳
     * @param timestamp 时间戳
     */
    void setTimestamp(uint64_t timestamp) { metadata_.timestamp = timestamp; }

    /**
     * @brief 判断帧是否有效
     * @return 是否有效
     */
    bool isValid() const { return !data_.empty() && width_ > 0 && height_ > 0; }

private:
    int width_ = 0;                     // 帧宽度
    int height_ = 0;                    // 帧高度
    PixelFormat format_ = PixelFormat::UNKNOWN;  // 像素格式
    std::vector<uint8_t> data_;        // 图像数据
    FrameMetadata metadata_;           // 元数据
};

} // namespace camera
} // namespace cam_server

#endif // CAMERA_FRAME_H 