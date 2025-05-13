#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <functional>
#include <chrono>
#include <sstream>

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
    CRITICAL    // 严重
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
};

/**
 * @brief 日志配置结构体
 */
struct LoggerConfig {
    // 日志文件路径
    std::string log_file;
    // 最小日志级别
    LogLevel min_level;
    // 是否输出到控制台
    bool console_output;
    // 是否输出到文件
    bool file_output;
    // 是否包含时间戳
    bool include_timestamp;
    // 是否包含日志级别
    bool include_level;
    // 是否包含源信息
    bool include_source;
    // 是否包含线程ID
    bool include_thread_id;
    // 是否包含文件和行号
    bool include_file_line;
    // 是否包含函数名
    bool include_function;
    // 最大日志文件大小（字节）
    int64_t max_file_size;
    // 最大日志文件数量
    int max_file_count;
    // 是否异步日志
    bool async_logging;
    // 异步日志队列大小
    int async_queue_size;
};

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
    bool initialize(const LoggerConfig& config);

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
     * @brief 写入严重日志
     * @param message 日志消息
     * @param source 日志源
     * @param file 文件名
     * @param line 行号
     * @param function 函数名
     */
    void critical(const std::string& message, const std::string& source = "",
                 const std::string& file = "", int line = 0, const std::string& function = "");

    /**
     * @brief 设置日志回调函数
     * @param callback 日志回调函数
     */
    void setLogCallback(std::function<void(const LogEntry&)> callback);

    /**
     * @brief 获取日志配置
     * @return 日志配置
     */
    LoggerConfig getConfig() const;

    /**
     * @brief 更新日志配置
     * @param config 日志配置
     * @return 是否成功更新
     */
    bool updateConfig(const LoggerConfig& config);

    /**
     * @brief 刷新日志
     */
    void flush();

    /**
     * @brief 获取日志级别名称
     * @param level 日志级别
     * @return 日志级别名称
     */
    static std::string getLevelName(LogLevel level);

private:
    // 私有构造函数，防止外部创建实例
    Logger();
    // 禁止拷贝构造和赋值操作
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    // 析构函数
    ~Logger();

    // 格式化日志条目
    std::string formatLogEntry(const LogEntry& entry) const;
    // 写入日志文件
    void writeToFile(const std::string& formatted_entry);
    // 写入控制台
    void writeToConsole(const std::string& formatted_entry, LogLevel level);
    // 检查日志文件大小并轮转
    void checkAndRotateLogFile();
    // 启动异步日志线程
    void startAsyncLogging();
    // 停止异步日志线程
    void stopAsyncLogging();
    // 异步日志线程函数
    void asyncLoggingThreadFunc();

    // 日志配置
    LoggerConfig config_;
    // 配置互斥锁
    mutable std::mutex config_mutex_;
    // 日志文件流
    std::unique_ptr<std::ofstream> log_file_stream_;
    // 文件互斥锁
    std::mutex file_mutex_;
    // 控制台互斥锁
    std::mutex console_mutex_;
    // 日志回调函数
    std::function<void(const LogEntry&)> log_callback_;
    // 回调互斥锁
    std::mutex callback_mutex_;
    // 是否已初始化
    bool is_initialized_;
    // 异步日志队列
    std::vector<LogEntry> async_queue_;
    // 队列互斥锁
    std::mutex queue_mutex_;
    // 队列条件变量
    std::condition_variable queue_cond_;
    // 异步日志线程
    std::thread async_thread_;
    // 停止标志
    std::atomic<bool> stop_flag_;
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

#define LOG_CRITICAL(message, source) \
    cam_server::monitor::Logger::getInstance().critical(message, source, __FILE__, __LINE__, __FUNCTION__)

} // namespace monitor
} // namespace cam_server

#endif // LOGGER_H
