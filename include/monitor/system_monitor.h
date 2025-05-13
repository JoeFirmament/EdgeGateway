#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

namespace cam_server {
namespace monitor {

/**
 * @brief CPU信息结构体
 */
struct CpuInfo {
    // CPU使用率（0.0-1.0）
    double usage;
    // CPU温度（摄氏度）
    double temperature;
    // CPU频率（MHz）
    double frequency;
    // 核心数
    int cores;
    // 每个核心的使用率
    std::vector<double> core_usage;
};

/**
 * @brief 内存信息结构体
 */
struct MemoryInfo {
    // 总内存（字节）
    int64_t total;
    // 已用内存（字节）
    int64_t used;
    // 可用内存（字节）
    int64_t available;
    // 使用率（0.0-1.0）
    double usage;
    // 交换空间总量（字节）
    int64_t swap_total;
    // 已用交换空间（字节）
    int64_t swap_used;
    // 交换空间使用率（0.0-1.0）
    double swap_usage;
};

/**
 * @brief 网络信息结构体
 */
struct NetworkInfo {
    // 接口名称
    std::string interface;
    // IP地址
    std::string ip_address;
    // MAC地址
    std::string mac_address;
    // 接收字节数
    int64_t rx_bytes;
    // 发送字节数
    int64_t tx_bytes;
    // 接收速率（字节/秒）
    double rx_rate;
    // 发送速率（字节/秒）
    double tx_rate;
};

/**
 * @brief 磁盘信息结构体
 */
struct DiskInfo {
    // 设备名称
    std::string device;
    // 挂载点
    std::string mount_point;
    // 文件系统类型
    std::string filesystem_type;
    // 总空间（字节）
    int64_t total_space;
    // 可用空间（字节）
    int64_t available_space;
    // 已用空间（字节）
    int64_t used_space;
    // 使用率（0.0-1.0）
    double usage_ratio;
    // 读取字节数
    int64_t read_bytes;
    // 写入字节数
    int64_t write_bytes;
    // 读取速率（字节/秒）
    double read_rate;
    // 写入速率（字节/秒）
    double write_rate;
};

/**
 * @brief 系统信息结构体
 */
struct SystemInfo {
    // 主机名
    std::string hostname;
    // 操作系统名称
    std::string os_name;
    // 操作系统版本
    std::string os_version;
    // 内核版本
    std::string kernel_version;
    // 系统架构
    std::string architecture;
    // 系统启动时间
    std::chrono::system_clock::time_point boot_time;
    // 系统运行时间（秒）
    int64_t uptime;
    // 当前时间
    std::chrono::system_clock::time_point current_time;
    // CPU信息
    CpuInfo cpu;
    // 内存信息
    MemoryInfo memory;
    // 网络信息列表
    std::vector<NetworkInfo> network;
    // 磁盘信息列表
    std::vector<DiskInfo> disks;
};

/**
 * @brief 监控配置结构体
 */
struct MonitorConfig {
    // 监控间隔（毫秒）
    int interval_ms;
    // 是否监控CPU
    bool monitor_cpu;
    // 是否监控内存
    bool monitor_memory;
    // 是否监控网络
    bool monitor_network;
    // 是否监控磁盘
    bool monitor_disk;
    // 是否记录历史数据
    bool record_history;
    // 历史数据保留时长（秒）
    int history_duration;
    // 是否启用警报
    bool enable_alerts;
    // CPU使用率警报阈值（0.0-1.0）
    double cpu_alert_threshold;
    // 内存使用率警报阈值（0.0-1.0）
    double memory_alert_threshold;
    // 磁盘使用率警报阈值（0.0-1.0）
    double disk_alert_threshold;
};

/**
 * @brief 警报级别枚举
 */
enum class AlertLevel {
    INFO,       // 信息
    WARNING,    // 警告
    ERROR,      // 错误
    CRITICAL    // 严重
};

/**
 * @brief 警报结构体
 */
struct Alert {
    // 警报级别
    AlertLevel level;
    // 警报消息
    std::string message;
    // 警报时间
    std::chrono::system_clock::time_point time;
    // 警报源
    std::string source;
    // 警报值
    double value;
    // 警报阈值
    double threshold;
};

/**
 * @brief 系统监控器类
 */
class SystemMonitor {
public:
    /**
     * @brief 获取SystemMonitor单例
     * @return SystemMonitor单例的引用
     */
    static SystemMonitor& getInstance();

    /**
     * @brief 初始化系统监控器
     * @param config 监控配置
     * @return 是否初始化成功
     */
    bool initialize(const MonitorConfig& config);

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
     * @brief 获取当前系统信息
     * @return 系统信息
     */
    SystemInfo getCurrentInfo() const;

    /**
     * @brief 获取历史系统信息
     * @param start_time 开始时间
     * @param end_time 结束时间
     * @return 历史系统信息列表
     */
    std::vector<SystemInfo> getHistoryInfo(std::chrono::system_clock::time_point start_time,
                                         std::chrono::system_clock::time_point end_time) const;

    /**
     * @brief 获取警报列表
     * @param start_time 开始时间
     * @param end_time 结束时间
     * @param min_level 最小警报级别
     * @return 警报列表
     */
    std::vector<Alert> getAlerts(std::chrono::system_clock::time_point start_time,
                               std::chrono::system_clock::time_point end_time,
                               AlertLevel min_level = AlertLevel::INFO) const;

    /**
     * @brief 设置系统信息回调函数
     * @param callback 系统信息回调函数
     */
    void setInfoCallback(std::function<void(const SystemInfo&)> callback);

    /**
     * @brief 设置警报回调函数
     * @param callback 警报回调函数
     */
    void setAlertCallback(std::function<void(const Alert&)> callback);

    /**
     * @brief 获取监控配置
     * @return 监控配置
     */
    MonitorConfig getConfig() const;

    /**
     * @brief 更新监控配置
     * @param config 监控配置
     * @return 是否成功更新
     */
    bool updateConfig(const MonitorConfig& config);

private:
    // 私有构造函数，防止外部创建实例
    SystemMonitor();
    // 禁止拷贝构造和赋值操作
    SystemMonitor(const SystemMonitor&) = delete;
    SystemMonitor& operator=(const SystemMonitor&) = delete;
    // 析构函数
    ~SystemMonitor();

    // 监控线程函数
    void monitorThreadFunc();
    // 更新系统信息
    void updateSystemInfo();
    // 检查警报
    void checkAlerts();
    // 添加警报
    void addAlert(AlertLevel level, const std::string& message, const std::string& source,
                 double value, double threshold);
    // 清理过期历史数据
    void cleanupHistory();

    // 监控配置
    MonitorConfig config_;
    // 当前系统信息
    SystemInfo current_info_;
    // 系统信息互斥锁
    mutable std::mutex info_mutex_;
    // 历史系统信息
    std::vector<SystemInfo> history_info_;
    // 历史信息互斥锁
    mutable std::mutex history_mutex_;
    // 警报列表
    std::vector<Alert> alerts_;
    // 警报列表互斥锁
    mutable std::mutex alerts_mutex_;
    // 系统信息回调函数
    std::function<void(const SystemInfo&)> info_callback_;
    // 警报回调函数
    std::function<void(const Alert&)> alert_callback_;
    // 是否已初始化
    bool is_initialized_;
    // 是否正在运行
    std::atomic<bool> is_running_;
    // 停止标志
    std::atomic<bool> stop_flag_;
    // 监控线程
    std::thread monitor_thread_;
    // 上次更新时间
    std::chrono::system_clock::time_point last_update_time_;
    // 上次网络统计
    std::unordered_map<std::string, std::pair<int64_t, int64_t>> last_network_stats_;
    // 上次磁盘统计
    std::unordered_map<std::string, std::pair<int64_t, int64_t>> last_disk_stats_;
};

} // namespace monitor
} // namespace cam_server

#endif // SYSTEM_MONITOR_H
