#pragma once

#include <crow.h>
#include <string>
#include <vector>

namespace cam_server {
namespace web {

/**
 * @brief 串口设备信息结构
 */
struct SerialDeviceInfo {
    std::string device_path;        // 设备路径，如 /dev/ttyUSB0
    std::string device_type;        // 设备类型，如 USB Serial, ARM UART
    std::string description;        // 设备描述
    std::string driver;            // 驱动名称
    std::string status;            // 设备状态：available, busy, error
    std::string permissions;       // 设备权限
    std::string last_access;       // 最后访问时间
    std::string supported_bauds;   // 支持的波特率
};

/**
 * @brief 串口路由处理类
 * 
 * 负责处理串口设备相关的HTTP路由，包括：
 * - 串口设备扫描和列表
 * - 串口设备详细信息查询
 * - 串口设备状态监控
 */
class SerialRoutes {
public:
    /**
     * @brief 设置串口相关的所有路由
     * @param app Crow应用实例
     */
    static void setupRoutes(crow::SimpleApp& app);

private:
    /**
     * @brief 设置串口设备列表路由
     * @param app Crow应用实例
     * 
     * 路由：GET /api/serial/devices
     * 功能：返回系统中所有串口设备的列表
     */
    static void setupSerialDevicesRoute(crow::SimpleApp& app);

    /**
     * @brief 设置串口设备信息路由
     * @param app Crow应用实例
     * 
     * 路由：GET /api/serial/info?device=/dev/ttyUSB0
     * 功能：返回指定串口设备的详细信息
     */
    static void setupSerialInfoRoute(crow::SimpleApp& app);

    /**
     * @brief 扫描系统中的串口设备
     * @return 串口设备信息列表
     * 
     * 扫描常见的串口设备路径：
     * - /dev/ttyUSB* (USB转串口)
     * - /dev/ttyACM* (USB CDC ACM)
     * - /dev/ttyS* (传统串口)
     * - /dev/ttyAMA* (ARM UART)
     * - /dev/ttyO* (OMAP UART)
     * - /dev/ttymxc* (i.MX UART)
     * - /dev/ttyTHS* (Tegra UART)
     */
    static std::vector<SerialDeviceInfo> scanSerialDevices();

    /**
     * @brief 获取特定串口设备的详细信息
     * @param device_path 设备路径
     * @param info 输出的设备信息
     * @return 成功返回true，失败返回false
     * 
     * 获取的信息包括：
     * - 设备类型和描述
     * - 驱动信息
     * - 权限和访问时间
     * - 设备状态（可用/占用/错误）
     * - 支持的波特率
     */
    static bool getSerialDeviceInfo(const std::string& device_path, SerialDeviceInfo& info);

    /**
     * @brief 获取设备描述信息
     * @param device_path 设备路径
     * @return 设备描述字符串
     * 
     * 尝试从以下来源获取描述：
     * 1. sysfs中的product信息
     * 2. sysfs中的manufacturer信息
     * 3. 根据设备类型的默认描述
     */
    static std::string getDeviceDescription(const std::string& device_path);
};

} // namespace web
} // namespace cam_server
