#ifndef CAMERA_API_H
#define CAMERA_API_H

#include "api/rest_handler.h"
#include <string>
#include <vector>
#include <map>
#include <set>

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
    
    // 查询设备信息
    bool queryDevice(const std::string& device_path, CameraDeviceInfo& info);
    
    // 获取格式名称
    std::string getFormatName(uint32_t format);
    
    // 是否已初始化
    bool is_initialized_;
    
    // 格式名称映射
    std::map<uint32_t, std::string> format_names_;
};

} // namespace api
} // namespace cam_server

#endif // CAMERA_API_H
