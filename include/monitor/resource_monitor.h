#ifndef RESOURCE_MONITOR_H
#define RESOURCE_MONITOR_H

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <unordered_map>

namespace cam_server {
namespace monitor {

/**
 * @brief 资源类型枚举
 */
enum class ResourceType {
    CPU,        // CPU资源
    MEMORY,     // 内存资源
    DISK_SPACE, // 磁盘空间
    DISK_IO,    // 磁盘IO
    NETWORK,    // 网络
    GPU,        // GPU资源
    OTHER       // 其他资源
};

/**
 * @brief 资源使用情况结构体
 */
struct ResourceUsage {
    // 资源类型
    ResourceType type;
    // 资源名称
    std::string name;
    // 使用率（0.0-1.0）
    double usage;
    // 总量
    int64_t total;
    // 已用
    int64_t used;
    // 可用
    int64_t available;
    // 单位
    std::string unit;
    // 时间戳
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief 进程资源使用情况结构体
 */
struct ProcessResourceUsage {
    // 进程ID
    int pid;
    // 进程名称
    std::string name;
    // 命令行
    std::string command_line;
    // CPU使用率（0.0-1.0）
    double cpu_usage;
    // 内存使用量（字节）
    int64_t memory_usage;
    // 内存使用率（0.0-1.0）
    double memory_usage_ratio;
    // 磁盘读取速率（字节/秒）
    double disk_read_rate;
    // 磁盘写入速率（字节/秒）
    double disk_write_rate;
    // 网络接收速率（字节/秒）
    double network_rx_rate;
    // 网络发送速率（字节/秒）
    double network_tx_rate;
    // 线程数
    int thread_count;
    // 打开文件数
    int open_files;
    // 启动时间
    std::chrono::system_clock::time_point start_time;
    // 运行时间（秒）
    int64_t run_time;
    // 时间戳
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief 资源监控配置结构体
 */
struct ResourceMonitorConfig {
    // 监控间隔（毫秒）
    int interval_ms;
    // 是否监控进程
    bool monitor_processes;
    // 要监控的进程名称列表
    std::vector<std::string> process_names;
    // 是否记录历史数据
    bool record_history;
    // 历史数据保留时长（秒）
    int history_duration;
    // 是否启用警报
    bool enable_alerts;
    // 资源使用率警报阈值（0.0-1.0）
    std::unordered_map<ResourceType, double> alert_thresholds;
};

/**
 * @brief 资源监控器类
 */
class ResourceMonitor {
public:
    /**
     * @brief 获取ResourceMonitor单例
     * @return ResourceMonitor单例的引用
     */
    static ResourceMonitor& getInstance();

    /**
     * @brief 初始化资源监控器
     * @param config 监控配置
     * @return 是否初始化成功
     */
    bool initialize(const ResourceMonitorConfig& config);

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
     * @brief 获取当前资源使用情况
     * @return 资源使用情况列表
     */
    std::vector<ResourceUsage> getCurrentResourceUsage() const;

    /**
     * @brief 获取当前进程资源使用情况
     * @return 进程资源使用情况列表
     */
    std::vector<ProcessResourceUsage> getCurrentProcessUsage() const;

    /**
     * @brief 获取历史资源使用情况
     * @param resource_type 资源类型
     * @param resource_name 资源名称
     * @param start_time 开始时间
     * @param end_time 结束时间
     * @return 历史资源使用情况列表
     */
    std::vector<ResourceUsage> getHistoryResourceUsage(
        ResourceType resource_type,
        const std::string& resource_name,
        std::chrono::system_clock::time_point start_time,
        std::chrono::system_clock::time_point end_time) const;

    /**
     * @brief 获取历史进程资源使用情况
     * @param process_name 进程名称
     * @param start_time 开始时间
     * @param end_time 结束时间
     * @return 历史进程资源使用情况列表
     */
    std::vector<ProcessResourceUsage> getHistoryProcessUsage(
        const std::string& process_name,
        std::chrono::system_clock::time_point start_time,
        std::chrono::system_clock::time_point end_time) const;

    /**
     * @brief 设置资源使用情况回调函数
     * @param callback 资源使用情况回调函数
     */
    void setResourceUsageCallback(std::function<void(const std::vector<ResourceUsage>&)> callback);

    /**
     * @brief 设置进程资源使用情况回调函数
     * @param callback 进程资源使用情况回调函数
     */
    void setProcessUsageCallback(std::function<void(const std::vector<ProcessResourceUsage>&)> callback);

    /**
     * @brief 获取监控配置
     * @return 监控配置
     */
    ResourceMonitorConfig getConfig() const;

    /**
     * @brief 更新监控配置
     * @param config 监控配置
     * @return 是否成功更新
     */
    bool updateConfig(const ResourceMonitorConfig& config);

private:
    // 私有构造函数，防止外部创建实例
    ResourceMonitor();
    // 禁止拷贝构造和赋值操作
    ResourceMonitor(const ResourceMonitor&) = delete;
    ResourceMonitor& operator=(const ResourceMonitor&) = delete;
    // 析构函数
    ~ResourceMonitor();

    // 监控线程函数
    void monitorThreadFunc();
    // 更新资源使用情况
    void updateResourceUsage();
    // 更新进程资源使用情况
    void updateProcessUsage();
    // 检查警报
    void checkAlerts();
    // 清理过期历史数据
    void cleanupHistory();

    // 监控配置
    ResourceMonitorConfig config_;
    // 当前资源使用情况
    std::vector<ResourceUsage> current_resource_usage_;
    // 资源使用情况互斥锁
    mutable std::mutex resource_mutex_;
    // 当前进程资源使用情况
    std::vector<ProcessResourceUsage> current_process_usage_;
    // 进程资源使用情况互斥锁
    mutable std::mutex process_mutex_;
    // 历史资源使用情况
    std::vector<ResourceUsage> history_resource_usage_;
    // 历史资源使用情况互斥锁
    mutable std::mutex history_resource_mutex_;
    // 历史进程资源使用情况
    std::vector<ProcessResourceUsage> history_process_usage_;
    // 历史进程资源使用情况互斥锁
    mutable std::mutex history_process_mutex_;
    // 资源使用情况回调函数
    std::function<void(const std::vector<ResourceUsage>&)> resource_usage_callback_;
    // 进程资源使用情况回调函数
    std::function<void(const std::vector<ProcessResourceUsage>&)> process_usage_callback_;
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
    // 进程CPU时间统计
    std::unordered_map<int, std::pair<int64_t, int64_t>> process_cpu_times_;
};

} // namespace monitor
} // namespace cam_server

#endif // RESOURCE_MONITOR_H
