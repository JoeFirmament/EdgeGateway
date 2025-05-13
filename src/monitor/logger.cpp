#include "monitor/logger.h"
#include "utils/string_utils.h"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <filesystem>

namespace cam_server {
namespace monitor {

// 静态方法实现
Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

std::string Logger::getLevelName(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE:    return "TRACE";
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO";
        case LogLevel::WARNING:  return "WARNING";
        case LogLevel::ERROR:    return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default:                 return "UNKNOWN";
    }
}

// 构造函数
Logger::Logger() : is_initialized_(false), stop_flag_(false) {
    // 设置默认配置
    config_.log_file = "logs/cam_server.log";
    config_.min_level = LogLevel::INFO;
    config_.console_output = true;
    config_.file_output = true;
    config_.include_timestamp = true;
    config_.include_level = true;
    config_.include_source = true;
    config_.include_thread_id = true;
    config_.include_file_line = true;
    config_.include_function = true;
    config_.max_file_size = 10 * 1024 * 1024; // 10MB
    config_.max_file_count = 5;
    config_.async_logging = true;
    config_.async_queue_size = 1000;
}

// 析构函数
Logger::~Logger() {
    // 停止异步日志线程
    stopAsyncLogging();
    
    // 刷新日志
    flush();
    
    // 关闭日志文件
    if (log_file_stream_ && log_file_stream_->is_open()) {
        log_file_stream_->close();
    }
}

// 初始化日志器
bool Logger::initialize(const LoggerConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    // 更新配置
    config_ = config;
    
    // 创建日志目录
    if (config_.file_output) {
        std::filesystem::path log_path(config_.log_file);
        std::filesystem::path log_dir = log_path.parent_path();
        
        try {
            if (!log_dir.empty() && !std::filesystem::exists(log_dir)) {
                std::filesystem::create_directories(log_dir);
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to create log directory: " << e.what() << std::endl;
            return false;
        }
        
        // 打开日志文件
        log_file_stream_ = std::make_unique<std::ofstream>(config_.log_file, std::ios::app);
        if (!log_file_stream_->is_open()) {
            std::cerr << "Failed to open log file: " << config_.log_file << std::endl;
            return false;
        }
    }
    
    // 启动异步日志线程
    if (config_.async_logging) {
        startAsyncLogging();
    }
    
    is_initialized_ = true;
    
    // 记录初始化成功日志
    log(LogLevel::INFO, "Logger initialized", "Logger");
    
    return true;
}

// 写入日志
void Logger::log(LogLevel level, const std::string& message, const std::string& source,
                const std::string& file, int line, const std::string& function) {
    // 检查日志级别
    if (level < config_.min_level) {
        return;
    }
    
    // 创建日志条目
    LogEntry entry;
    entry.level = level;
    entry.message = message;
    entry.time = std::chrono::system_clock::now();
    entry.source = source;
    entry.thread_id = std::this_thread::get_id();
    entry.file = file;
    entry.line = line;
    entry.function = function;
    
    // 调用回调函数
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (log_callback_) {
            log_callback_(entry);
        }
    }
    
    // 异步日志
    if (config_.async_logging) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (async_queue_.size() < static_cast<size_t>(config_.async_queue_size)) {
            async_queue_.push_back(entry);
            queue_cond_.notify_one();
        }
    } else {
        // 同步日志
        std::string formatted_entry = formatLogEntry(entry);
        
        if (config_.console_output) {
            writeToConsole(formatted_entry, level);
        }
        
        if (config_.file_output) {
            writeToFile(formatted_entry);
        }
    }
}

// 写入跟踪日志
void Logger::trace(const std::string& message, const std::string& source,
                  const std::string& file, int line, const std::string& function) {
    log(LogLevel::TRACE, message, source, file, line, function);
}

// 写入调试日志
void Logger::debug(const std::string& message, const std::string& source,
                  const std::string& file, int line, const std::string& function) {
    log(LogLevel::DEBUG, message, source, file, line, function);
}

// 写入信息日志
void Logger::info(const std::string& message, const std::string& source,
                 const std::string& file, int line, const std::string& function) {
    log(LogLevel::INFO, message, source, file, line, function);
}

// 写入警告日志
void Logger::warning(const std::string& message, const std::string& source,
                    const std::string& file, int line, const std::string& function) {
    log(LogLevel::WARNING, message, source, file, line, function);
}

// 写入错误日志
void Logger::error(const std::string& message, const std::string& source,
                  const std::string& file, int line, const std::string& function) {
    log(LogLevel::ERROR, message, source, file, line, function);
}

// 写入严重日志
void Logger::critical(const std::string& message, const std::string& source,
                     const std::string& file, int line, const std::string& function) {
    log(LogLevel::CRITICAL, message, source, file, line, function);
}

// 设置日志回调函数
void Logger::setLogCallback(std::function<void(const LogEntry&)> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    log_callback_ = callback;
}

// 获取日志配置
LoggerConfig Logger::getConfig() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

// 更新日志配置
bool Logger::updateConfig(const LoggerConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    // 如果异步日志设置发生变化，需要重启异步日志线程
    bool restart_async = (config_.async_logging != config.async_logging);
    
    // 更新配置
    config_ = config;
    
    // 重启异步日志线程
    if (restart_async) {
        if (config_.async_logging) {
            startAsyncLogging();
        } else {
            stopAsyncLogging();
        }
    }
    
    return true;
}

// 刷新日志
void Logger::flush() {
    if (config_.file_output && log_file_stream_ && log_file_stream_->is_open()) {
        std::lock_guard<std::mutex> lock(file_mutex_);
        log_file_stream_->flush();
    }
}

// 格式化日志条目
std::string Logger::formatLogEntry(const LogEntry& entry) const {
    std::ostringstream oss;
    
    // 添加时间戳
    if (config_.include_timestamp) {
        auto time_t = std::chrono::system_clock::to_time_t(entry.time);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            entry.time.time_since_epoch()).count() % 1000;
        
        std::tm tm_buf;
        #ifdef _WIN32
        localtime_s(&tm_buf, &time_t);
        #else
        localtime_r(&time_t, &tm_buf);
        #endif
        
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "." 
            << std::setfill('0') << std::setw(3) << ms << " ";
    }
    
    // 添加日志级别
    if (config_.include_level) {
        oss << "[" << getLevelName(entry.level) << "] ";
    }
    
    // 添加源信息
    if (config_.include_source && !entry.source.empty()) {
        oss << "[" << entry.source << "] ";
    }
    
    // 添加线程ID
    if (config_.include_thread_id) {
        oss << "[Thread-" << entry.thread_id << "] ";
    }
    
    // 添加文件和行号
    if (config_.include_file_line && !entry.file.empty()) {
        oss << "[" << entry.file << ":" << entry.line << "] ";
    }
    
    // 添加函数名
    if (config_.include_function && !entry.function.empty()) {
        oss << "[" << entry.function << "] ";
    }
    
    // 添加消息
    oss << entry.message;
    
    return oss.str();
}

// 写入日志文件
void Logger::writeToFile(const std::string& formatted_entry) {
    if (!log_file_stream_ || !log_file_stream_->is_open()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(file_mutex_);
    
    // 检查日志文件大小并轮转
    checkAndRotateLogFile();
    
    // 写入日志
    (*log_file_stream_) << formatted_entry << std::endl;
}

// 写入控制台
void Logger::writeToConsole(const std::string& formatted_entry, LogLevel level) {
    std::lock_guard<std::mutex> lock(console_mutex_);
    
    // 根据日志级别设置颜色
    #ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    WORD attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // 默认白色
    
    switch (level) {
        case LogLevel::TRACE:    attr = FOREGROUND_BLUE | FOREGROUND_INTENSITY; break; // 亮蓝色
        case LogLevel::DEBUG:    attr = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break; // 亮绿色
        case LogLevel::INFO:     attr = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break; // 亮青色
        case LogLevel::WARNING:  attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break; // 亮黄色
        case LogLevel::ERROR:    attr = FOREGROUND_RED | FOREGROUND_INTENSITY; break; // 亮红色
        case LogLevel::CRITICAL: attr = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break; // 亮紫色
    }
    
    SetConsoleTextAttribute(hConsole, attr);
    std::cout << formatted_entry << std::endl;
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // 恢复白色
    #else
    const char* color_code = "\033[0m"; // 默认颜色
    
    switch (level) {
        case LogLevel::TRACE:    color_code = "\033[94m"; break; // 亮蓝色
        case LogLevel::DEBUG:    color_code = "\033[92m"; break; // 亮绿色
        case LogLevel::INFO:     color_code = "\033[96m"; break; // 亮青色
        case LogLevel::WARNING:  color_code = "\033[93m"; break; // 亮黄色
        case LogLevel::ERROR:    color_code = "\033[91m"; break; // 亮红色
        case LogLevel::CRITICAL: color_code = "\033[95m"; break; // 亮紫色
    }
    
    std::cout << color_code << formatted_entry << "\033[0m" << std::endl;
    #endif
}

// 检查日志文件大小并轮转
void Logger::checkAndRotateLogFile() {
    if (!log_file_stream_ || !log_file_stream_->is_open()) {
        return;
    }
    
    // 获取当前文件大小
    log_file_stream_->flush();
    std::streampos file_size = log_file_stream_->tellp();
    
    // 如果文件大小超过限制，进行轮转
    if (file_size > config_.max_file_size) {
        // 关闭当前日志文件
        log_file_stream_->close();
        
        // 轮转日志文件
        for (int i = config_.max_file_count - 1; i > 0; --i) {
            std::string old_file = config_.log_file + "." + std::to_string(i);
            std::string new_file = config_.log_file + "." + std::to_string(i + 1);
            
            if (std::filesystem::exists(old_file)) {
                if (i == config_.max_file_count - 1) {
                    std::filesystem::remove(old_file);
                } else {
                    std::filesystem::rename(old_file, new_file);
                }
            }
        }
        
        // 重命名当前日志文件
        if (std::filesystem::exists(config_.log_file)) {
            std::filesystem::rename(config_.log_file, config_.log_file + ".1");
        }
        
        // 重新打开日志文件
        log_file_stream_ = std::make_unique<std::ofstream>(config_.log_file, std::ios::app);
    }
}

// 启动异步日志线程
void Logger::startAsyncLogging() {
    // 如果线程已经在运行，先停止
    stopAsyncLogging();
    
    // 重置停止标志
    stop_flag_ = false;
    
    // 启动异步日志线程
    async_thread_ = std::thread(&Logger::asyncLoggingThreadFunc, this);
}

// 停止异步日志线程
void Logger::stopAsyncLogging() {
    if (async_thread_.joinable()) {
        // 设置停止标志
        stop_flag_ = true;
        
        // 通知线程
        queue_cond_.notify_one();
        
        // 等待线程结束
        async_thread_.join();
    }
}

// 异步日志线程函数
void Logger::asyncLoggingThreadFunc() {
    std::vector<LogEntry> entries_to_process;
    
    while (!stop_flag_) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            // 等待有日志条目或者停止标志
            queue_cond_.wait(lock, [this] {
                return !async_queue_.empty() || stop_flag_;
            });
            
            // 如果队列为空且停止标志已设置，退出循环
            if (async_queue_.empty() && stop_flag_) {
                break;
            }
            
            // 交换队列，减少锁的持有时间
            entries_to_process.swap(async_queue_);
        }
        
        // 处理日志条目
        for (const auto& entry : entries_to_process) {
            std::string formatted_entry = formatLogEntry(entry);
            
            if (config_.console_output) {
                writeToConsole(formatted_entry, entry.level);
            }
            
            if (config_.file_output) {
                writeToFile(formatted_entry);
            }
        }
        
        // 清空处理过的条目
        entries_to_process.clear();
    }
}

} // namespace monitor
} // namespace cam_server
