#ifndef CAMERA_FORMAT_UTILS_H
#define CAMERA_FORMAT_UTILS_H

#include <string>
#include <linux/videodev2.h>
#include "camera/frame.h"

namespace cam_server {
namespace camera {

/**
 * @brief 格式转换工具类
 */
class FormatUtils {
public:
    /**
     * @brief 获取V4L2格式名称
     * @param format V4L2格式
     * @return 格式名称
     */
    static std::string getV4L2FormatName(uint32_t format);

    /**
     * @brief 获取像素格式名称
     * @param format 像素格式
     * @return 格式名称
     */
    static std::string getPixelFormatName(PixelFormat format);

    /**
     * @brief 将V4L2格式转换为PixelFormat
     * @param v4l2_format V4L2格式
     * @return PixelFormat
     */
    static PixelFormat v4l2FormatToPixelFormat(uint32_t v4l2_format);

    /**
     * @brief 将PixelFormat转换为V4L2格式
     * @param format PixelFormat
     * @return V4L2格式
     */
    static uint32_t pixelFormatToV4L2Format(PixelFormat format);
};

} // namespace camera
} // namespace cam_server

#endif // CAMERA_FORMAT_UTILS_H 