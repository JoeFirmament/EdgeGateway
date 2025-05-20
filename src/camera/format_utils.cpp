#include "camera/format_utils.h"
#include "monitor/logger.h"

namespace cam_server {
namespace camera {

std::string FormatUtils::getV4L2FormatName(uint32_t format) {
    switch (format) {
        case V4L2_PIX_FMT_YUYV:
            return "YUYV";
        case V4L2_PIX_FMT_MJPEG:
            return "MJPG";
        case V4L2_PIX_FMT_H264:
            return "H264";
        case V4L2_PIX_FMT_NV12:
            return "NV12";
        case V4L2_PIX_FMT_RGB24:
            return "RGB24";
        case V4L2_PIX_FMT_BGR24:
            return "BGR24";
        default: {
            char fmt_str[5] = {0};
            fmt_str[0] = format & 0xFF;
            fmt_str[1] = (format >> 8) & 0xFF;
            fmt_str[2] = (format >> 16) & 0xFF;
            fmt_str[3] = (format >> 24) & 0xFF;
            return std::string(fmt_str);
        }
    }
}

std::string FormatUtils::getPixelFormatName(PixelFormat format) {
    switch (format) {
        case PixelFormat::YUYV:
            return "YUYV";
        case PixelFormat::MJPEG:
            return "MJPG";
        case PixelFormat::H264:
            return "H264";
        case PixelFormat::NV12:
            return "NV12";
        case PixelFormat::RGB24:
            return "RGB24";
        case PixelFormat::BGR24:
            return "BGR24";
        default:
            return "UNKNOWN";
    }
}

PixelFormat FormatUtils::v4l2FormatToPixelFormat(uint32_t v4l2_format) {
    LOG_DEBUG("转换V4L2格式: 0x" + 
             [](uint32_t format) -> std::string {
                 char buf[9];
                 snprintf(buf, sizeof(buf), "%08x", format);
                 return std::string(buf);
             }(v4l2_format), 
             "FormatUtils");
    
    switch (v4l2_format) {
        case V4L2_PIX_FMT_YUYV:
            return PixelFormat::YUYV;
        case V4L2_PIX_FMT_MJPEG:
            return PixelFormat::MJPEG;
        case V4L2_PIX_FMT_H264:
            return PixelFormat::H264;
        case V4L2_PIX_FMT_NV12:
            return PixelFormat::NV12;
        case V4L2_PIX_FMT_RGB24:
            return PixelFormat::RGB24;
        case V4L2_PIX_FMT_BGR24:
            return PixelFormat::BGR24;
        default:
            LOG_WARNING("未知的V4L2格式: 0x" + 
                       [](uint32_t format) -> std::string {
                           char buf[9];
                           snprintf(buf, sizeof(buf), "%08x", format);
                           return std::string(buf);
                       }(v4l2_format), 
                       "FormatUtils");
            return PixelFormat::UNKNOWN;
    }
}

uint32_t FormatUtils::pixelFormatToV4L2Format(PixelFormat format) {
    switch (format) {
        case PixelFormat::YUYV:
            return V4L2_PIX_FMT_YUYV;
        case PixelFormat::MJPEG:
            return V4L2_PIX_FMT_MJPEG;
        case PixelFormat::H264:
            return V4L2_PIX_FMT_H264;
        case PixelFormat::NV12:
            return V4L2_PIX_FMT_NV12;
        case PixelFormat::RGB24:
            return V4L2_PIX_FMT_RGB24;
        case PixelFormat::BGR24:
            return V4L2_PIX_FMT_BGR24;
        default:
            return V4L2_PIX_FMT_YUYV;  // 默认使用YUYV
    }
}

} // namespace camera
} // namespace cam_server 