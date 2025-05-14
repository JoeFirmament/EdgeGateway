#include "system/system_monitor.h"
#include "monitor/logger.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <chrono>
#include <thread>
#include <algorithm>
#include <unordered_map>
#include <filesystem>

// Linux系统头文件
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace cam_server {
namespace system {

// 单例实现
SystemMonitor& SystemMonitor::getInstance() {
    static SystemMonitor instance;
    return instance;
}

SystemMonitor::SystemMonitor()
    : update_interval_ms_(1000), is_initialized_(false), is_running_(false) {
}

SystemMonitor::~SystemMonitor() {
    stop();
}

bool SystemMonitor::initialize(int update_interval_ms) {
    if (is_initialized_) {
        return true;
    }

    update_interval_ms_ = update_interval_ms;

    // 初始化系统信息
    updateSystemInfo();

    is_initialized_ = true;
    LOG_INFO("系统监控初始化成功", "SystemMonitor");
    return true;
}

bool SystemMonitor::start() {
    if (!is_initialized_) {
        LOG_ERROR("系统监控未初始化", "SystemMonitor");
        return false;
    }

    if (is_running_) {
        return true;
    }

    is_running_ = true;
    monitor_thread_ = std::thread(&SystemMonitor::monitorThread, this);

    LOG_INFO("系统监控已启动", "SystemMonitor");
    return true;
}

bool SystemMonitor::stop() {
    if (!is_running_) {
        return true;
    }

    is_running_ = false;

    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }

    LOG_INFO("系统监控已停止", "SystemMonitor");
    return true;
}

SystemInfo SystemMonitor::getSystemInfo() const {
    std::lock_guard<std::mutex> lock(info_mutex_);
    return system_info_;
}

CpuInfo SystemMonitor::getCpuInfo() const {
    std::lock_guard<std::mutex> lock(info_mutex_);
    return system_info_.cpu;
}

MemoryInfo SystemMonitor::getMemoryInfo() const {
    std::lock_guard<std::mutex> lock(info_mutex_);
    return system_info_.memory;
}

std::vector<StorageInfo> SystemMonitor::getStorageInfo() const {
    std::lock_guard<std::mutex> lock(info_mutex_);
    return system_info_.storage;
}

std::vector<NetworkInfo> SystemMonitor::getNetworkInfo() const {
    std::lock_guard<std::mutex> lock(info_mutex_);
    return system_info_.network;
}

GpuInfo SystemMonitor::getGpuInfo() const {
    std::lock_guard<std::mutex> lock(info_mutex_);
    return system_info_.gpu;
}

PowerInfo SystemMonitor::getPowerInfo() const {
    std::lock_guard<std::mutex> lock(info_mutex_);
    return system_info_.power;
}

std::vector<double> SystemMonitor::getLoadAverage() const {
    std::lock_guard<std::mutex> lock(info_mutex_);
    return system_info_.load_average;
}

void SystemMonitor::setUpdateCallback(std::function<void(const SystemInfo&)> callback) {
    update_callback_ = callback;
}

void SystemMonitor::monitorThread() {
    while (is_running_) {
        // 更新系统信息
        updateSystemInfo();

        // 调用回调函数
        if (update_callback_) {
            update_callback_(system_info_);
        }

        // 等待下一次更新
        std::this_thread::sleep_for(std::chrono::milliseconds(update_interval_ms_));
    }
}

void SystemMonitor::updateSystemInfo() {
    std::lock_guard<std::mutex> lock(info_mutex_);

    // 记录当前时间
    last_update_time_ = std::chrono::steady_clock::now();

    // 更新各部分信息
    updateCpuInfo();
    updateGpuInfo();
    updateMemoryInfo();
    updateStorageInfo();
    updateNetworkInfo();
    updatePowerInfo();
    updateLoadAverage();
    updateBasicInfo();
}

void SystemMonitor::updateCpuInfo() {
    // 读取/proc/stat获取CPU使用情况
    std::ifstream stat_file("/proc/stat");
    if (!stat_file.is_open()) {
        LOG_ERROR("无法打开/proc/stat文件", "SystemMonitor");
        return;
    }

    std::string line;
    std::vector<uint64_t> cpu_times;
    std::vector<std::vector<uint64_t>> core_times;

    // 读取总CPU时间
    if (std::getline(stat_file, line)) {
        std::istringstream ss(line);
        std::string cpu_label;
        ss >> cpu_label; // 跳过"cpu"标签

        uint64_t time;
        while (ss >> time) {
            cpu_times.push_back(time);
        }
    }

    // 读取每个核心的CPU时间
    int core_count = 0;
    while (std::getline(stat_file, line)) {
        if (line.find("cpu") == 0 && line.find("cpu") != std::string::npos) {
            std::istringstream ss(line);
            std::string cpu_label;
            ss >> cpu_label; // 跳过"cpuN"标签

            std::vector<uint64_t> times;
            uint64_t time;
            while (ss >> time) {
                times.push_back(time);
            }

            core_times.push_back(times);
            core_count++;
        } else {
            break; // 不再是CPU数据
        }
    }

    // 计算CPU使用率
    if (!prev_cpu_times_.empty() && cpu_times.size() >= 4) {
        uint64_t prev_idle = prev_cpu_times_[3];
        uint64_t idle = cpu_times[3];

        uint64_t prev_total = 0;
        uint64_t total = 0;

        for (size_t i = 0; i < prev_cpu_times_.size(); i++) {
            prev_total += prev_cpu_times_[i];
            total += cpu_times[i];
        }

        uint64_t total_delta = total - prev_total;
        uint64_t idle_delta = idle - prev_idle;

        if (total_delta > 0) {
            system_info_.cpu.usage_percent = 100.0 * (1.0 - static_cast<double>(idle_delta) / total_delta);
        }
    }

    // 保存当前CPU时间用于下次计算
    prev_cpu_times_ = cpu_times;

    // 设置CPU核心数
    system_info_.cpu.core_count = core_count;

    // 计算每个核心的使用率
    system_info_.cpu.core_usage.resize(core_count);

    // 读取CPU温度 - RK3588上CPU温度分布在多个thermal_zone
    // thermal_zone0: soc-thermal (整体SoC温度)
    // thermal_zone1: bigcore0-thermal (大核心0温度)
    // thermal_zone2: bigcore1-thermal (大核心1温度)
    // thermal_zone3: littlecore-thermal (小核心温度)
    // thermal_zone4: center-thermal (中心温度)

    // 首先尝试读取大核心温度（通常是最热的部分）
    std::ifstream big_core_temp_file("/sys/class/thermal/thermal_zone1/temp");
    if (big_core_temp_file.is_open()) {
        int temp;
        if (big_core_temp_file >> temp) {
            system_info_.cpu.temperature = temp / 1000.0; // 转换为摄氏度
            LOG_DEBUG("CPU大核心温度: " + std::to_string(system_info_.cpu.temperature) + "°C", "SystemMonitor");
        } else {
            LOG_DEBUG("无法读取CPU大核心温度值", "SystemMonitor");
            system_info_.cpu.temperature = 0.0;
        }
    } else {
        // 如果无法读取大核心温度，尝试读取SoC温度
        std::ifstream soc_temp_file("/sys/class/thermal/thermal_zone0/temp");
        if (soc_temp_file.is_open()) {
            int temp;
            if (soc_temp_file >> temp) {
                system_info_.cpu.temperature = temp / 1000.0; // 转换为摄氏度
                LOG_DEBUG("CPU SoC温度: " + std::to_string(system_info_.cpu.temperature) + "°C", "SystemMonitor");
            } else {
                LOG_DEBUG("无法读取CPU SoC温度值", "SystemMonitor");
                system_info_.cpu.temperature = 0.0;
            }
        } else {
            LOG_DEBUG("无法打开CPU温度文件", "SystemMonitor");
            system_info_.cpu.temperature = 0.0;
        }
    }

    // 读取CPU频率（如果可用）
    std::ifstream freq_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    if (freq_file.is_open()) {
        int freq;
        freq_file >> freq;
        system_info_.cpu.frequency = freq / 1000.0; // 转换为MHz
    } else {
        system_info_.cpu.frequency = 0.0;
    }
}

void SystemMonitor::updateGpuInfo() {
    // 初始化GPU信息
    system_info_.gpu.usage_percent = 0.0;
    system_info_.gpu.temperature = 0.0;
    system_info_.gpu.memory_usage_percent = 0.0;
    system_info_.gpu.frequency = 0.0;

    // 尝试获取GPU使用率（RK3588特定）
    FILE* fp = popen("cat /sys/devices/platform/fb000000.gpu/utilization 2>/dev/null", "r");
    if (fp) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            try {
                system_info_.gpu.usage_percent = std::stod(buffer);
            } catch (const std::exception& e) {
                LOG_ERROR("无法解析GPU使用率: " + std::string(e.what()), "SystemMonitor");
            }
        }
        pclose(fp);
    }

    // 获取GPU温度 - RK3588上GPU温度在thermal_zone5
    std::ifstream gpu_temp_file("/sys/class/thermal/thermal_zone5/temp");
    if (gpu_temp_file.is_open()) {
        int temp;
        if (gpu_temp_file >> temp) {
            system_info_.gpu.temperature = temp / 1000.0; // 转换为摄氏度
            LOG_DEBUG("GPU温度: " + std::to_string(system_info_.gpu.temperature) + "°C", "SystemMonitor");
        } else {
            LOG_DEBUG("无法读取GPU温度值", "SystemMonitor");
            // 使用CPU温度作为备选
            system_info_.gpu.temperature = system_info_.cpu.temperature;
            LOG_DEBUG("使用CPU温度作为GPU温度近似值: " + std::to_string(system_info_.gpu.temperature) + "°C", "SystemMonitor");
        }
    } else {
        LOG_DEBUG("无法打开GPU温度文件: /sys/class/thermal/thermal_zone5/temp", "SystemMonitor");

        // 尝试通过命令查找GPU温度
        FILE* fp = popen("for i in /sys/class/thermal/thermal_zone*/type; do if grep -q -i gpu \"$i\"; then zone=${i%/type}; cat \"$zone/temp\" 2>/dev/null; break; fi; done", "r");
        if (fp) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), fp) != NULL) {
                try {
                    int temp = std::stoi(buffer);
                    system_info_.gpu.temperature = temp / 1000.0; // 转换为摄氏度
                    LOG_DEBUG("通过命令获取GPU温度: " + std::to_string(system_info_.gpu.temperature) + "°C", "SystemMonitor");
                } catch (const std::exception& e) {
                    LOG_DEBUG("无法解析GPU温度: " + std::string(e.what()), "SystemMonitor");
                    // 使用CPU温度作为备选
                    system_info_.gpu.temperature = system_info_.cpu.temperature;
                    LOG_DEBUG("使用CPU温度作为GPU温度近似值: " + std::to_string(system_info_.gpu.temperature) + "°C", "SystemMonitor");
                }
            } else {
                // 使用CPU温度作为备选
                system_info_.gpu.temperature = system_info_.cpu.temperature;
                LOG_DEBUG("使用CPU温度作为GPU温度近似值: " + std::to_string(system_info_.gpu.temperature) + "°C", "SystemMonitor");
            }
            pclose(fp);
        } else {
            // 使用CPU温度作为备选
            system_info_.gpu.temperature = system_info_.cpu.temperature;
            LOG_DEBUG("使用CPU温度作为GPU温度近似值: " + std::to_string(system_info_.gpu.temperature) + "°C", "SystemMonitor");
        }
    }

    // 尝试获取GPU频率（RK3588特定）
    std::ifstream freq_file("/sys/devices/platform/fb000000.gpu/clock");
    if (freq_file.is_open()) {
        int freq;
        freq_file >> freq;
        system_info_.gpu.frequency = freq / 1000000.0; // 转换为MHz
    }

    // 尝试获取GPU内存使用率
    // 注意：这是一个近似值，因为RK3588没有直接提供GPU内存使用率
    system_info_.gpu.memory_usage_percent = system_info_.gpu.usage_percent * 0.8; // 假设GPU内存使用率与GPU使用率相关
}

void SystemMonitor::updateMemoryInfo() {
    // 读取/proc/meminfo获取内存使用情况
    std::ifstream meminfo_file("/proc/meminfo");
    if (!meminfo_file.is_open()) {
        LOG_ERROR("无法打开/proc/meminfo文件", "SystemMonitor");
        return;
    }

    std::string line;
    uint64_t total_mem = 0;
    uint64_t free_mem = 0;
    uint64_t buffers = 0;
    uint64_t cached = 0;

    while (std::getline(meminfo_file, line)) {
        if (line.find("MemTotal:") == 0) {
            std::istringstream ss(line);
            std::string label;
            ss >> label >> total_mem;
            total_mem *= 1024; // 转换为字节
        } else if (line.find("MemFree:") == 0) {
            std::istringstream ss(line);
            std::string label;
            ss >> label >> free_mem;
            free_mem *= 1024; // 转换为字节
        } else if (line.find("Buffers:") == 0) {
            std::istringstream ss(line);
            std::string label;
            ss >> label >> buffers;
            buffers *= 1024; // 转换为字节
        } else if (line.find("Cached:") == 0) {
            std::istringstream ss(line);
            std::string label;
            ss >> label >> cached;
            cached *= 1024; // 转换为字节
        }
    }

    // 计算已使用内存（减去缓存和缓冲区）
    uint64_t used_mem = total_mem - free_mem - buffers - cached;

    // 更新内存信息
    system_info_.memory.total = total_mem;
    system_info_.memory.free = free_mem + buffers + cached;
    system_info_.memory.used = used_mem;

    if (total_mem > 0) {
        system_info_.memory.usage_percent = 100.0 * static_cast<double>(used_mem) / total_mem;
    } else {
        system_info_.memory.usage_percent = 0.0;
    }
}

void SystemMonitor::updateStorageInfo() {
    // 清空存储信息
    system_info_.storage.clear();

    // 获取挂载点信息
    std::ifstream mounts_file("/proc/mounts");
    if (!mounts_file.is_open()) {
        LOG_ERROR("无法打开/proc/mounts文件", "SystemMonitor");
        return;
    }

    std::string line;
    while (std::getline(mounts_file, line)) {
        std::istringstream ss(line);
        std::string device, mount_point, fs_type;
        ss >> device >> mount_point >> fs_type;

        // 只处理常见的文件系统类型
        if (fs_type == "ext4" || fs_type == "ext3" || fs_type == "ext2" ||
            fs_type == "xfs" || fs_type == "btrfs" || fs_type == "f2fs" ||
            fs_type == "vfat" || fs_type == "ntfs" || fs_type == "exfat") {

            struct statvfs stat;
            if (statvfs(mount_point.c_str(), &stat) == 0) {
                StorageInfo info;
                info.mount_point = mount_point;
                info.total = stat.f_blocks * stat.f_frsize;
                info.free = stat.f_bfree * stat.f_frsize;
                info.used = info.total - info.free;

                if (info.total > 0) {
                    info.usage_percent = 100.0 * static_cast<double>(info.used) / info.total;
                } else {
                    info.usage_percent = 0.0;
                }

                system_info_.storage.push_back(info);
            }
        }
    }
}

void SystemMonitor::updateNetworkInfo() {
    // 清空网络信息
    system_info_.network.clear();

    // 获取网络接口信息
    struct ifaddrs* ifaddr;
    if (getifaddrs(&ifaddr) == -1) {
        LOG_ERROR("无法获取网络接口信息", "SystemMonitor");
        return;
    }

    // 当前时间
    auto now = std::chrono::steady_clock::now();

    // 读取/proc/net/dev获取网络流量
    std::ifstream netdev_file("/proc/net/dev");
    if (!netdev_file.is_open()) {
        LOG_ERROR("无法打开/proc/net/dev文件", "SystemMonitor");
        freeifaddrs(ifaddr);
        return;
    }

    std::string line;
    // 跳过前两行（标题行）
    std::getline(netdev_file, line);
    std::getline(netdev_file, line);

    // 处理每个网络接口
    std::unordered_map<std::string, NetworkInfo> net_info;

    while (std::getline(netdev_file, line)) {
        std::istringstream ss(line);
        std::string if_name;
        ss >> if_name;

        // 移除接口名称后的冒号
        if (!if_name.empty() && if_name.back() == ':') {
            if_name.pop_back();
        }

        // 跳过lo接口
        if (if_name == "lo") {
            continue;
        }

        uint64_t rx_bytes, rx_packets, rx_errs, rx_drop, rx_fifo, rx_frame, rx_compressed, rx_multicast;
        uint64_t tx_bytes, tx_packets, tx_errs, tx_drop, tx_fifo, tx_colls, tx_carrier, tx_compressed;

        ss >> rx_bytes >> rx_packets >> rx_errs >> rx_drop >> rx_fifo >> rx_frame >> rx_compressed >> rx_multicast
           >> tx_bytes >> tx_packets >> tx_errs >> tx_drop >> tx_fifo >> tx_colls >> tx_carrier >> tx_compressed;

        NetworkInfo info;
        info.interface = if_name;
        info.rx_bytes = rx_bytes;
        info.tx_bytes = tx_bytes;

        // 计算速率
        auto prev_it = prev_network_bytes_.find(if_name);
        if (prev_it != prev_network_bytes_.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_time_).count();
            if (elapsed > 0) {
                info.rx_rate = 1000.0 * (rx_bytes - prev_it->second.first) / elapsed;
                info.tx_rate = 1000.0 * (tx_bytes - prev_it->second.second) / elapsed;
            }
        } else {
            info.rx_rate = 0.0;
            info.tx_rate = 0.0;
        }

        // 保存当前值用于下次计算
        prev_network_bytes_[if_name] = {rx_bytes, tx_bytes};

        net_info[if_name] = info;
    }

    // 获取IP地址
    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) {
            continue;
        }

        // 只处理IPv4地址
        if (ifa->ifa_addr->sa_family == AF_INET) {
            std::string if_name = ifa->ifa_name;

            // 跳过lo接口
            if (if_name == "lo") {
                continue;
            }

            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr, ip, INET_ADDRSTRLEN);

            auto it = net_info.find(if_name);
            if (it != net_info.end()) {
                it->second.ip_address = ip;
            }
        }
    }

    // 将网络信息添加到系统信息中
    for (const auto& pair : net_info) {
        system_info_.network.push_back(pair.second);
    }

    freeifaddrs(ifaddr);
}

void SystemMonitor::updatePowerInfo() {
    // 初始化电源信息
    system_info_.power.power_source = "AC";
    system_info_.power.battery_percent = 100;
    system_info_.power.battery_status = "N/A";
    system_info_.power.remaining_time = -1;
    system_info_.power.power_mode = "Performance";

    // 尝试获取电源信息（如果有电池）
    FILE* fp = popen("cat /sys/class/power_supply/*/type 2>/dev/null | grep -q Battery && echo 'Battery' || echo 'AC'", "r");
    if (fp) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            std::string power_source = buffer;
            // 移除换行符
            if (!power_source.empty() && power_source[power_source.length()-1] == '\n') {
                power_source.erase(power_source.length()-1);
            }
            system_info_.power.power_source = power_source;
        }
        pclose(fp);
    }

    // 如果是电池供电，获取电池信息
    if (system_info_.power.power_source == "Battery") {
        // 获取电池电量
        FILE* fp_capacity = popen("cat /sys/class/power_supply/*/capacity 2>/dev/null", "r");
        if (fp_capacity) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), fp_capacity) != NULL) {
                try {
                    system_info_.power.battery_percent = std::stoi(buffer);
                } catch (const std::exception& e) {
                    LOG_ERROR("无法解析电池电量: " + std::string(e.what()), "SystemMonitor");
                }
            }
            pclose(fp_capacity);
        }

        // 获取电池状态
        FILE* fp_status = popen("cat /sys/class/power_supply/*/status 2>/dev/null", "r");
        if (fp_status) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), fp_status) != NULL) {
                std::string status = buffer;
                // 移除换行符
                if (!status.empty() && status[status.length()-1] == '\n') {
                    status.erase(status.length()-1);
                }
                system_info_.power.battery_status = status;
            }
            pclose(fp_status);
        }
    }

    // 获取CPU调频模式
    FILE* fp_governor = popen("cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null", "r");
    if (fp_governor) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), fp_governor) != NULL) {
            std::string governor = buffer;
            // 移除换行符
            if (!governor.empty() && governor[governor.length()-1] == '\n') {
                governor.erase(governor.length()-1);
            }
            system_info_.power.power_mode = governor;
        }
        pclose(fp_governor);
    }
}

void SystemMonitor::updateLoadAverage() {
    // 初始化系统负载
    system_info_.load_average.clear();
    system_info_.load_average.resize(3, 0.0);

    // 读取/proc/loadavg获取系统负载
    std::ifstream loadavg_file("/proc/loadavg");
    if (!loadavg_file.is_open()) {
        LOG_ERROR("无法打开/proc/loadavg文件", "SystemMonitor");
        return;
    }

    double load1, load5, load15;
    loadavg_file >> load1 >> load5 >> load15;

    system_info_.load_average[0] = load1;
    system_info_.load_average[1] = load5;
    system_info_.load_average[2] = load15;
}

void SystemMonitor::updateBasicInfo() {
    // 获取主机名
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        system_info_.hostname = hostname;
    } else {
        system_info_.hostname = "unknown";
    }

    // 获取内核版本和操作系统版本
    struct utsname uts_info;
    if (uname(&uts_info) == 0) {
        system_info_.kernel_version = uts_info.release;
        system_info_.os_version = uts_info.sysname;
        system_info_.os_version += " ";
        system_info_.os_version += uts_info.version;
    } else {
        system_info_.kernel_version = "unknown";
        system_info_.os_version = "unknown";
    }

    // 获取运行时间
    std::ifstream uptime_file("/proc/uptime");
    if (uptime_file.is_open()) {
        double uptime_seconds;
        uptime_file >> uptime_seconds;

        int days = static_cast<int>(uptime_seconds) / (60 * 60 * 24);
        int hours = (static_cast<int>(uptime_seconds) % (60 * 60 * 24)) / (60 * 60);
        int minutes = (static_cast<int>(uptime_seconds) % (60 * 60)) / 60;
        int seconds = static_cast<int>(uptime_seconds) % 60;

        std::ostringstream ss;
        if (days > 0) {
            ss << days << "天 ";
        }
        ss << std::setfill('0') << std::setw(2) << hours << ":"
           << std::setfill('0') << std::setw(2) << minutes << ":"
           << std::setfill('0') << std::setw(2) << seconds;

        system_info_.uptime = ss.str();
    } else {
        system_info_.uptime = "unknown";
    }

    // 获取系统时间
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    struct tm now_tm;
    localtime_r(&now_time_t, &now_tm);

    char time_str[100];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &now_tm);
    system_info_.system_time = time_str;
}

} // namespace system
} // namespace cam_server
