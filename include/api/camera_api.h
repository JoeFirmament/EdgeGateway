#ifndef CAMERA_API_H
#define CAMERA_API_H

#include "api/rest_handler.h"
#include "video/i_video_recorder.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <memory>

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
 * @brief 摄像头API处理类
 */
class CameraApi {
public:
    /**
     * @brief 获取CameraApi单例
     * @return CameraApi单例的引用
     */
    static CameraApi& getInstance();

    /**
     * @brief 初始化API
     * @return 是否初始化成功
     */
    bool initialize();

    /**
     * @brief 注册API路由
     * @param handler REST处理器
     * @return 是否注册成功
     */
    bool registerRoutes(RestHandler& handler);

    /**
     * @brief 获取所有摄像头设备
     * @return 摄像头设备列表
     */
    std::vector<CameraDeviceInfo> getAllCameras();

    /**
     * @brief 打开摄像头
     * @param device_path 设备路径
     * @param format 格式
     * @param width 宽度
     * @param height 高度
     * @param fps 帧率
     * @return 是否成功打开
     */
    bool openCamera(const std::string& device_path,
                   const std::string& format,
                   int width, int height, int fps);

    /**
     * @brief 启动摄像头预览
     * @return 是否成功启动
     */
    bool startPreview();

    /**
     * @brief 停止摄像头预览
     * @return 是否成功停止
     */
    bool stopPreview();

    /**
     * @brief 关闭摄像头设备
     * @return 是否成功关闭
     */
    bool closeCamera();

    /**
     * @brief 拍照并保存图像
     * @param output_path 输出路径，如果为空则使用默认路径
     * @param quality JPEG质量（1-100）
     * @return 保存的文件路径，失败则返回空字符串
     */
    std::string captureImage(const std::string& output_path = "", int quality = 90);

    /**
     * @brief 开始录制视频
     * @param output_path 输出路径，如果为空则使用默认路径
     * @param format 视频格式（如mp4, mkv等）
     * @param encoder 编码器名称（如h264_rkmpp, libx264等）
     * @param bitrate 比特率（bps）
     * @param max_duration 最大录制时长（秒），0表示不限制
     * @return 是否成功开始录制
     */
    bool startRecording(const std::string& output_path = "",
                       const std::string& format = "mp4",
                       const std::string& encoder = "h264_rkmpp",
                       int bitrate = 4000000,
                       int max_duration = 0);

    /**
     * @brief 停止录制视频
     * @return 录制的文件路径，失败则返回空字符串
     */
    std::string stopRecording();

    /**
     * @brief 获取录制状态
     * @return 录制状态信息的JSON字符串
     */
    std::string getRecordingStatus();

private:
    // 私有构造函数，防止外部创建实例
    CameraApi();
    // 禁止拷贝构造和赋值操作
    CameraApi(const CameraApi&) = delete;
    CameraApi& operator=(const CameraApi&) = delete;
    // 析构函数
    ~CameraApi();

    // 处理获取所有摄像头的请求
    HttpResponse handleGetAllCameras(const HttpRequest& request);

    // 处理打开摄像头的请求
    HttpResponse handleOpenCamera(const HttpRequest& request);

    // 处理启动摄像头预览的请求
    HttpResponse handleStartPreview(const HttpRequest& request);

    // 处理停止摄像头预览的请求
    HttpResponse handleStopPreview(const HttpRequest& request);

    // 处理获取摄像头预览图像的请求
    HttpResponse handleGetPreview(const HttpRequest& request);

    // 处理拍照请求
    HttpResponse handleCaptureImage(const HttpRequest& request);

    // 处理开始录制请求
    HttpResponse handleStartRecording(const HttpRequest& request);

    // 处理停止录制请求
    HttpResponse handleStopRecording(const HttpRequest& request);

    // 处理获取录制状态请求
    HttpResponse handleGetRecordingStatus(const HttpRequest& request);

    // 处理获取摄像头连接状态请求
    HttpResponse handleGetCameraStatus(const HttpRequest& request);

    // 处理关闭摄像头的请求
    HttpResponse handleCloseCamera(const HttpRequest& request);

    // 查询设备信息
    bool queryDevice(const std::string& device_path, CameraDeviceInfo& info);

    // 获取格式名称
    std::string getFormatName(uint32_t format);

    // 是否已初始化
    bool is_initialized_;

    // 格式名称映射
    std::map<uint32_t, std::string> format_names_;

    // 视频录制器
    std::shared_ptr<video::IVideoRecorder> video_recorder_;

    // 录制互斥锁
    std::mutex recording_mutex_;

    // 图像保存目录
    std::string images_dir_;

    // 视频保存目录
    std::string videos_dir_;

    // 创建目录（如果不存在）
    bool ensureDirectoryExists(const std::string& path);
};

} // namespace api
} // namespace cam_server

#endif // CAMERA_API_H
