#ifndef CAMERA_API_H
#define CAMERA_API_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <memory>
#include <linux/videodev2.h>

#include "api/rest_handler.h"
#include "camera/camera_device.h"
#include "video/i_video_recorder.h"
#include "api/mjpeg_streamer.h"

namespace cam_server {
namespace api {

/**
 * @brief 分辨率结构体
 */
struct ResolutionInfo {
    uint32_t width;
    uint32_t height;

    bool operator<(const ResolutionInfo& other) const {
        if (width != other.width) {
            return width < other.width;
        }
        return height < other.height;
    }

    bool operator==(const ResolutionInfo& other) const {
        return width == other.width && height == other.height;
    }
};

/**
 * @brief 摄像头设备信息结构体
 */
struct CameraDeviceInfo {
    std::string path;
    std::string name;
    std::string bus_info;
    std::map<std::string, std::set<ResolutionInfo>> formats;
};

/**
 * @brief 摄像头API类
 */
class CameraApi {
public:
    /**
     * @brief 获取单例实例
     * @return API实例
     */
    static CameraApi& getInstance();

    /**
     * @brief 初始化API
     * @return 是否成功
     */
    bool initialize();

    /**
     * @brief 注册API路由
     * @param rest_handler REST处理器
     * @return 是否成功
     */
    bool registerRoutes(RestHandler& rest_handler);

    /**
     * @brief 获取所有摄像头设备
     * @return 设备列表
     */
    std::vector<CameraDeviceInfo> getAllCameras();

    /**
     * @brief 打开摄像头
     * @param device_path 设备路径
     * @param format 格式
     * @param width 宽度
     * @param height 高度
     * @param fps 帧率
     * @return 是否成功
     */
    bool openCamera(const std::string& device_path,
                   const std::string& format,
                   int width, int height, int fps);

    /**
     * @brief 关闭摄像头
     * @return 是否成功
     */
    bool closeCamera();

    /**
     * @brief 启动预览
     * @return 是否成功
     */
    bool startPreview();

    /**
     * @brief 停止预览
     * @return 是否成功
     */
    bool stopPreview();

    /**
     * @brief 拍照
     * @param output_path 输出路径
     * @param quality 图像质量
     * @return 图像文件路径
     */
    std::string captureImage(const std::string& output_path = "", int quality = 90);

    /**
     * @brief 开始录制
     * @param output_path 输出路径
     * @param format 格式
     * @param encoder 编码器
     * @param bitrate 比特率
     * @param max_duration 最大时长
     * @return 是否成功
     */
    bool startRecording(const std::string& output_path = "",
                       const std::string& format = "mp4",
                       const std::string& encoder = "h264_rkmpp",
                       int bitrate = 4000000,
                       int max_duration = 0);

    /**
     * @brief 停止录制
     * @return 录制文件路径
     */
    std::string stopRecording();

    /**
     * @brief 获取录制状态
     * @return 状态JSON字符串
     */
    std::string getRecordingStatus();

private:
    CameraApi();
    ~CameraApi();

    // 禁止拷贝和赋值
    CameraApi(const CameraApi&) = delete;
    CameraApi& operator=(const CameraApi&) = delete;

    // 查询设备信息
    bool queryDevice(const std::string& device_path, CameraDeviceInfo& info);

    // 确保目录存在
    bool ensureDirectoryExists(const std::string& path);

    // 处理HTTP请求
    HttpResponse handleGetCameraStatus(const HttpRequest& request);
    HttpResponse handleGetAllCameras(const HttpRequest& request);
    HttpResponse handleOpenCamera(const HttpRequest& request);
    HttpResponse handleCloseCamera(const HttpRequest& request);
    HttpResponse handleStartPreview(const HttpRequest& request);
    HttpResponse handleStopPreview(const HttpRequest& request);
    HttpResponse handleCaptureImage(const HttpRequest& request);
    HttpResponse handleStartRecording(const HttpRequest& request);
    HttpResponse handleStopRecording(const HttpRequest& request);
    HttpResponse handleGetRecordingStatus(const HttpRequest& request);
    HttpResponse handleMjpegStream(const HttpRequest& request);

    // 成员变量
    bool is_initialized_;
    std::string images_dir_;
    std::string videos_dir_;
    std::shared_ptr<video::IVideoRecorder> video_recorder_;
    std::mutex recording_mutex_;
    MjpegStreamer& mjpeg_streamer_;
};

} // namespace api
} // namespace cam_server

#endif // CAMERA_API_H
