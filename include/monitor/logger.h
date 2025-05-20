#ifndef CAM_SERVER_MONITOR_LOGGER_H
#define CAM_SERVER_MONITOR_LOGGER_H

#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <functional>
#include <chrono>
#include <sstream>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <ctime>

namespace cam_server {
namespace monitor {

/**
 * @brief 日志级别枚举
 */
enum class LogLevel {
    TRACE,      // 跟踪
    DEBUG,      // 调试
    INFO,       // 信息
    WARNING,    // 警告
    ERROR,      // 错误
    FATAL       // 致命错误
};

/**
 * @brief 日志条目结构体
 */
struct LogEntry {
    // 日志级别
    LogLevel level;
    // 日志消息
    std::string message;
    // 日志时间
    std::chrono::system_clock::time_point time;
    // 日志源
    std::string source;
    // 线程ID
    std::thread::id thread_id;
    // 文件名
    std::string file;
    // 行号
    int line;
    // 函数名
    std::string function;
    // 日志ID（用于排序和去重）
    uint64_t log_id;
};

/**
 * @brief 日志配置结构体
 */
struct LogConfig {
    // 日志文件路径
    std::string log_file;
    // 最小日志级别
    LogLevel min_level;
    // 是否输出到控制台
    bool console_output;
    // 是否输出到文件
    bool file_output;
    // 日期格式
    std::string date_format;
    // 是否包含时间戳
    bool include_timestamp;
    // 是否包含日志级别
    bool include_level;
    // 是否包含源信息
    bool include_source;
    // 是否包含线程ID
    bool include_thread_id;
    // 是否包含文件和行号信息
    bool include_file_line;
    // 是否包含函数名
    bool include_function;
    // 最大日志文件大小（字节）
    size_t max_file_size;
    // 最大日志文件数量
    int max_file_count;
    // 是否使用异步日志
    bool async_logging;
    // 异步日志队列大小
    int async_queue_size;
};

/**
 * @brief 日志回调函数类型
 */
using LogCallback = std::function<void(const LogEntry&)>;

/**
 * @brief 日志器类
 */
class Logger {
public:
    /**
     * @brief 获取Logger单例
     * @return Logger单例的引用
     */
    static Logger& getInstance();

    /**
     * @brief 初始化日志器
     * @param config 日志配置
     * @return 是否初始化成功
     */
    bool initialize(const LogConfig& config);

    /**
     * @brief 获取日志配置
     * @return 日志配置
     */
    LogConfig getConfig() const;

    /**
     * @brief 设置日志回调函数
     * @param callback 日志回调函数
     */
    void setLogCallback(LogCallback callback);

    /**
     * @brief 写入日志
     * @param level 日志级别
     * @param message 日志消息
     * @param source 日志源
     * @param file 文件名
     * @param line 行号
     * @param function 函数名
     */
    void log(LogLevel level, const std::string& message, const std::string& source = "",
            const std::string& file = "", int line = 0, const std::string& function = "");

    /**
     * @brief 写入跟踪日志
     * @param message 日志消息
     * @param source 日志源
     * @param file 文件名
     * @param line 行号
     * @param function 函数名
     */
    void trace(const std::string& message, const std::string& source = "",
              const std::string& file = "", int line = 0, const std::string& function = "");

    /**
     * @brief 写入调试日志
     * @param message 日志消息
     * @param source 日志源
     * @param file 文件名
     * @param line 行号
     * @param function 函数名
     */
    void debug(const std::string& message, const std::string& source = "",
              const std::string& file = "", int line = 0, const std::string& function = "");

    /**
     * @brief 写入信息日志
     * @param message 日志消息
     * @param source 日志源
     * @param file 文件名
     * @param line 行号
     * @param function 函数名
     */
    void info(const std::string& message, const std::string& source = "",
             const std::string& file = "", int line = 0, const std::string& function = "");

    /**
     * @brief 写入警告日志
     * @param message 日志消息
     * @param source 日志源
     * @param file 文件名
     * @param line 行号
     * @param function 函数名
     */
    void warning(const std::string& message, const std::string& source = "",
                const std::string& file = "", int line = 0, const std::string& function = "");

    /**
     * @brief 写入错误日志
     * @param message 日志消息
     * @param source 日志源
     * @param file 文件名
     * @param line 行号
     * @param function 函数名
     */
    void error(const std::string& message, const std::string& source = "",
              const std::string& file = "", int line = 0, const std::string& function = "");

    /**
     * @brief 写入致命错误日志
     * @param message 日志消息
     * @param source 日志源
     * @param file 文件名
     * @param line 行号
     * @param function 函数名
     */
    void fatal(const std::string& message, const std::string& source = "",
               const std::string& file = "", int line = 0, const std::string& function = "");

    /**
     * @brief 获取日志级别名称
     * @param level 日志级别
     * @return 日志级别名称
     */
    std::string getLevelName(LogLevel level) const;

    /**
     * @brief 刷新日志
     */
    void flush();

private:
    // 私有构造函数，防止外部创建实例
    Logger();
    // 禁止拷贝构造和赋值操作
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    // 析构函数
    ~Logger();

    /**
     * @brief 格式化日志条目到缓冲区
     * @param entry 日志条目
     * @param buffer 输出缓冲区
     * @param buffer_size 缓冲区大小
     */
    void formatLogEntryToBuffer(const LogEntry& entry, char* buffer, size_t buffer_size);

    /**
     * @brief 写入日志文件
     * @param formatted_entry 格式化后的日志条目
     */
    void writeToFile(const char* formatted_entry);

    /**
     * @brief 写入控制台
     * @param formatted_entry 格式化后的日志条目
     * @param level 日志级别
     */
    void writeToConsole(const char* formatted_entry, LogLevel level);

    /**
     * @brief 检查日志文件大小并轮转
     */
    void checkAndRotateLogFile();

    /**
     * @brief 启动异步日志线程
     */
    void startAsyncLogging();

    /**
     * @brief 停止异步日志线程
     */
    void stopAsyncLogging();

    /**
     * @brief 异步日志线程函数
     */
    void asyncLoggingThreadFunc();

    // 日志配置
    LogConfig config_;

    // 日志文件流
    std::unique_ptr<std::ofstream> log_file_stream_;

    // 文件互斥锁
    mutable std::mutex file_mutex_;

    // 控制台互斥锁
    mutable std::mutex console_mutex_;

    // 配置互斥锁
    mutable std::mutex config_mutex_;

    // 日志回调函数
    LogCallback log_callback_;

    // 回调互斥锁
    mutable std::mutex callback_mutex_;

    // 环形缓冲区互斥锁
    mutable std::mutex ring_mutex_;

    // 环形缓冲区
    char* ring_buffer_;
    
    // 环形缓冲区头指针
    size_t ring_head_;
    
    // 环形缓冲区尾指针
    size_t ring_tail_;

    // 日志计数器
    std::atomic<uint64_t> log_count_;

    // 异步日志线程
    std::thread async_thread_;

    // 异步日志条件变量
    std::condition_variable queue_cond_;

    // 停止标志
    std::atomic<bool> stop_flag_;

    // 是否已初始化
    std::atomic<bool> is_initialized_;
};

/**
 * @brief 日志宏，用于简化日志调用
 */
#define LOG_TRACE(message, source) \
    cam_server::monitor::Logger::getInstance().trace(message, source, __FILE__, __LINE__, __FUNCTION__)

#define LOG_DEBUG(message, source) \
    cam_server::monitor::Logger::getInstance().debug(message, source, __FILE__, __LINE__, __FUNCTION__)

#define LOG_INFO(message, source) \
    cam_server::monitor::Logger::getInstance().info(message, source, __FILE__, __LINE__, __FUNCTION__)

#define LOG_WARNING(message, source) \
    cam_server::monitor::Logger::getInstance().warning(message, source, __FILE__, __LINE__, __FUNCTION__)

#define LOG_ERROR(message, source) \
    cam_server::monitor::Logger::getInstance().error(message, source, __FILE__, __LINE__, __FUNCTION__)

#define LOG_FATAL(message, source) \
    cam_server::monitor::Logger::getInstance().fatal(message, source, __FILE__, __LINE__, __FUNCTION__)

} // namespace monitor
} // namespace cam_server

#endif // CAM_SERVER_MONITOR_LOGGER_H
