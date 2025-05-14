#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>

namespace cam_server {
namespace system {

/**
 * @brief CPU信息结构体
 */
struct CpuInfo {
    // CPU使用率（百分比）
    double usage_percent;
    // CPU温度（摄氏度）
    double temperature;
    // CPU核心数
    int core_count;
    // 每个核心的使用率
    std::vector<double> core_usage;
    // CPU频率（MHz）
    double frequency;
};

/**
 * @brief GPU信息结构体
 */
struct GpuInfo {
    // GPU使用率（百分比）
    double usage_percent;
    // GPU温度（摄氏度）
    double temperature;
    // GPU内存使用率（百分比）
    double memory_usage_percent;
    // GPU频率（MHz）
    double frequency;
};

/**
 * @brief 内存信息结构体
 */
struct MemoryInfo {
    // 总内存（字节）
    uint64_t total;
    // 已使用内存（字节）
    uint64_t used;
    // 空闲内存（字节）
    uint64_t free;
    // 内存使用率（百分比）
    double usage_percent;
};

/**
 * @brief 存储信息结构体
 */
struct StorageInfo {
    // 挂载点
    std::string mount_point;
    // 总容量（字节）
    uint64_t total;
    // 已使用容量（字节）
    uint64_t used;
    // 空闲容量（字节）
    uint64_t free;
    // 使用率（百分比）
    double usage_percent;
};

/**
 * @brief 网络信息结构体
 */
struct NetworkInfo {
    // 网络接口名称
    std::string interface;
    // IP地址
    std::string ip_address;
    // 接收字节数
    uint64_t rx_bytes;
    // 发送字节数
    uint64_t tx_bytes;
    // 接收速率（字节/秒）
    double rx_rate;
    // 发送速率（字节/秒）
    double tx_rate;
};

/**
 * @brief 电源状态结构体
 */
struct PowerInfo {
    // 电源类型（AC/电池）
    std::string power_source;
    // 电池电量百分比（如果适用）
    int battery_percent;
    // 电池状态（充电/放电/已充满）
    std::string battery_status;
    // 预计剩余时间（分钟）
    int remaining_time;
    // 电源管理模式
    std::string power_mode;
};

/**
 * @brief 系统信息结构体
 */
struct SystemInfo {
    // CPU信息
    CpuInfo cpu;
    // GPU信息
    GpuInfo gpu;
    // 内存信息
    MemoryInfo memory;
    // 存储信息
    std::vector<StorageInfo> storage;
    // 网络信息
    std::vector<NetworkInfo> network;
    // 电源信息
    PowerInfo power;
    // 主机名
    std::string hostname;
    // 内核版本
    std::string kernel_version;
    // 操作系统版本
    std::string os_version;
    // 运行时间
    std::string uptime;
    // 系统时间
    std::string system_time;
    // 系统负载（1分钟、5分钟、15分钟）
    std::vector<double> load_average;
};

/**
 * @brief 系统监控类
 */
class SystemMonitor {
public:
    /**
     * @brief 获取SystemMonitor单例
     * @return SystemMonitor单例的引用
     */
    static SystemMonitor& getInstance();

    /**
     * @brief 初始化系统监控
     * @param update_interval_ms 更新间隔（毫秒）
     * @return 是否初始化成功
     */
    bool initialize(int update_interval_ms = 1000);

    /**
     * @brief 启动监控
     * @return 是否成功启动
     */
    bool start();

    /**
     * @brief 停止监控
     * @return 是否成功停止
     */
    bool stop();

    /**
     * @brief 获取系统信息
     * @return 系统信息
     */
    SystemInfo getSystemInfo() const;

    /**
     * @brief 获取CPU信息
     * @return CPU信息
     */
    CpuInfo getCpuInfo() const;

    /**
     * @brief 获取GPU信息
     * @return GPU信息
     */
    GpuInfo getGpuInfo() const;

    /**
     * @brief 获取内存信息
     * @return 内存信息
     */
    MemoryInfo getMemoryInfo() const;

    /**
     * @brief 获取存储信息
     * @return 存储信息
     */
    std::vector<StorageInfo> getStorageInfo() const;

    /**
     * @brief 获取网络信息
     * @return 网络信息
     */
    std::vector<NetworkInfo> getNetworkInfo() const;

    /**
     * @brief 获取电源信息
     * @return 电源信息
     */
    PowerInfo getPowerInfo() const;

    /**
     * @brief 获取系统负载
     * @return 系统负载（1分钟、5分钟、15分钟）
     */
    std::vector<double> getLoadAverage() const;

    /**
     * @brief 设置系统信息更新回调
     * @param callback 回调函数
     */
    void setUpdateCallback(std::function<void(const SystemInfo&)> callback);

private:
    // 私有构造函数，防止外部创建实例
    SystemMonitor();
    // 禁止拷贝构造和赋值操作
    SystemMonitor(const SystemMonitor&) = delete;
    SystemMonitor& operator=(const SystemMonitor&) = delete;
    // 析构函数
    ~SystemMonitor();

    // 更新系统信息
    void updateSystemInfo();
    // 更新CPU信息
    void updateCpuInfo();
    // 更新GPU信息
    void updateGpuInfo();
    // 更新内存信息
    void updateMemoryInfo();
    // 更新存储信息
    void updateStorageInfo();
    // 更新网络信息
    void updateNetworkInfo();
    // 更新电源信息
    void updatePowerInfo();
    // 更新系统负载
    void updateLoadAverage();
    // 更新系统基本信息
    void updateBasicInfo();
    // 监控线程函数
    void monitorThread();

    // 系统信息
    SystemInfo system_info_;
    // 系统信息互斥锁
    mutable std::mutex info_mutex_;
    // 更新间隔（毫秒）
    int update_interval_ms_;
    // 是否已初始化
    bool is_initialized_;
    // 是否正在运行
    std::atomic<bool> is_running_;
    // 监控线程
    std::thread monitor_thread_;
    // 更新回调函数
    std::function<void(const SystemInfo&)> update_callback_;
    // CPU使用率计算所需的上一次数据
    std::vector<uint64_t> prev_cpu_times_;
    // 网络数据计算所需的上一次数据
    std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> prev_network_bytes_;
    // 上次更新时间
    std::chrono::steady_clock::time_point last_update_time_;
};

} // namespace system
} // namespace cam_server

#endif // SYSTEM_MONITOR_H
