#include "monitor/logger.h"
#include "utils/string_utils.h"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <codecvt>
#include <locale>
#include <atomic>
#include <thread>
#include <algorithm>
#include <string.h>

namespace cam_server {
namespace monitor {

// =============== 静态全局变量 ================
// 全局互斥锁，用于Logger实例初始化
static std::mutex s_instance_mutex;

// 用于字符编码转换
static std::mutex s_utf8_mutex;

// 线程本地存储缓冲区，减少内存分配和锁争用
thread_local char t_format_buffer[8192];
thread_local char t_convert_buffer[4096];

// 环形缓冲区大小
constexpr size_t RING_BUFFER_SIZE = 8192;
constexpr size_t RING_ITEM_SIZE = 1024;

// =============== 静态辅助函数 ================

// 转换为UTF-8编码
std::string toUtf8(const std::string& input) {
    if (input.empty()) {
        return input;
    }

    try {
        // 检查是否已经是UTF-8
        bool is_utf8 = true;
        const unsigned char* str = reinterpret_cast<const unsigned char*>(input.c_str());
        size_t len = input.length();
        
        for (size_t i = 0; i < len; i++) {
            if (str[i] <= 0x7F) {
                // ASCII字符，继续
                continue;
            } else if (str[i] >= 0xC0 && str[i] <= 0xDF) {
                // 2字节UTF-8序列
                if (i + 1 < len && (str[i+1] & 0xC0) == 0x80) {
                    i += 1;
                } else {
                    is_utf8 = false;
                    break;
                }
            } else if (str[i] >= 0xE0 && str[i] <= 0xEF) {
                // 3字节UTF-8序列
                if (i + 2 < len && (str[i+1] & 0xC0) == 0x80 && (str[i+2] & 0xC0) == 0x80) {
                    i += 2;
                } else {
                    is_utf8 = false;
                    break;
                }
            } else if (str[i] >= 0xF0 && str[i] <= 0xF7) {
                // 4字节UTF-8序列
                if (i + 3 < len && (str[i+1] & 0xC0) == 0x80 && (str[i+2] & 0xC0) == 0x80 && (str[i+3] & 0xC0) == 0x80) {
                    i += 3;
                } else {
                    is_utf8 = false;
                    break;
                }
            } else {
                is_utf8 = false;
                break;
            }
        }
        
        if (is_utf8) {
            return input; // 已经是UTF-8
        }

        // 假设输入是GBK或其他编码，尝试转换为UTF-8
        std::lock_guard<std::mutex> lock(s_utf8_mutex);
        
        // 尝试使用iconv或系统本地化设置转换
        try {
            // 这里使用了C++11的转换函数
            // 注意：这不是最佳的中文转换方法，但在许多系统上可以工作
            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
            
            // 首先尝试转换为宽字符
            std::wstring wide;
            
            // 由于不知道确切的源编码，这里采用系统默认编码
            size_t input_size = input.size();
            size_t converted = 0;
            
            for (size_t i = 0; i < input_size;) {
                wchar_t wc;
                size_t len = mbtowc(&wc, input.c_str() + i, input_size - i);
                if (len > 0) {
                    wide.push_back(wc);
                    i += len;
                    converted++;
                } else {
                    // 转换失败，跳过当前字节
                    i++;
                }
            }
            
            // 如果成功转换了字符，则转换回UTF-8
            if (converted > 0) {
                return conv.to_bytes(wide);
            }
        } catch (...) {
            // 转换失败，使用备用方法
        }
        
        // 备用方法：简单替换无法识别的字符
        std::string result;
        result.reserve(input.size());
        
        for (unsigned char c : input) {
            if (c < 0x80) {
                result.push_back(c);
            } else {
                // 对于无法识别的字符，用?替代
                result.push_back('?');
            }
        }
        
        return result;
    } catch (const std::exception& e) {
        // 转换失败时，返回原始字符串
        return input;
    }
}

// =============== Logger类实现 ================

// 单例获取
Logger& Logger::getInstance() {
    // 使用双重检查锁定模式
    static Logger* instance = nullptr;
    static std::atomic<bool> initialized(false);
    
    if (!initialized.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock(s_instance_mutex);
        if (!initialized.load(std::memory_order_relaxed)) {
            instance = new Logger();
            initialized.store(true, std::memory_order_release);
        }
    }
    
    return *instance;
}

// 日志级别名称
std::string Logger::getLevelName(LogLevel level) const {
    switch (level) {
        case LogLevel::TRACE:    return "TRACE";
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO";
        case LogLevel::WARNING:  return "WARNING";
        case LogLevel::ERROR:    return "ERROR";
        case LogLevel::FATAL:    return "FATAL";
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
    config_.async_queue_size = RING_BUFFER_SIZE;
    config_.date_format = "%Y-%m-%d %H:%M:%S";
    
    // 初始化环形缓冲区
    ring_buffer_ = new char[RING_BUFFER_SIZE * RING_ITEM_SIZE];
    ring_head_ = 0;
    ring_tail_ = 0;
    for (size_t i = 0; i < RING_BUFFER_SIZE; i++) {
        ring_buffer_[i * RING_ITEM_SIZE] = '\0';
    }
    
    // 初始化日志计数器
    log_count_.store(0, std::memory_order_relaxed);
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
    
    // 清理环形缓冲区
    delete[] ring_buffer_;
}

// 初始化日志器
bool Logger::initialize(const LogConfig& config) {
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
            std::cerr << "创建日志目录失败: " << e.what() << std::endl;
            return false;
        }
        
        // 打开日志文件
        log_file_stream_ = std::make_unique<std::ofstream>(config_.log_file, std::ios::app);
        if (!log_file_stream_->is_open()) {
            std::cerr << "打开日志文件失败: " << config_.log_file << std::endl;
            return false;
        }
    }
    
    // 启动异步日志线程
    if (config_.async_logging) {
        startAsyncLogging();
    }
    
    is_initialized_ = true;
    
    // 记录初始化成功日志
    log(LogLevel::INFO, "日志系统初始化成功", "Logger");
    
    return true;
}

// 写入日志
void Logger::log(LogLevel level, const std::string& message, const std::string& source,
                const std::string& file, int line, const std::string& function) {
    // 检查日志级别 - 这一步不需要锁，使用原子操作
    if (level < config_.min_level) {
        return;
    }
    
    // 增加日志计数
    uint64_t log_id = log_count_.fetch_add(1, std::memory_order_relaxed);
    
    // 创建日志条目
    LogEntry entry;
    entry.level = level;
    entry.message = toUtf8(message); // 确保消息是UTF-8编码
    entry.time = std::chrono::system_clock::now();
    entry.source = source;
    entry.thread_id = std::this_thread::get_id();
    entry.file = file;
    entry.line = line;
    entry.function = function;
    entry.log_id = log_id;
    
    // 调用回调函数 - 使用专门的回调锁
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (log_callback_) {
            log_callback_(entry);
        }
    }
    
    // 异步日志 - 使用无锁环形缓冲区
    if (config_.async_logging) {
        char* buffer = nullptr;
        {
            std::lock_guard<std::mutex> lock(ring_mutex_);
            // 检查环形缓冲区是否已满
            size_t next_head = (ring_head_ + 1) % RING_BUFFER_SIZE;
            if (next_head == ring_tail_) {
                // 缓冲区已满，丢弃最老的日志
                ring_tail_ = (ring_tail_ + 1) % RING_BUFFER_SIZE;
            }
            
            // 获取缓冲区位置
            buffer = &ring_buffer_[ring_head_ * RING_ITEM_SIZE];
            ring_head_ = next_head;
        }
        
        // 格式化日志到缓冲区
        formatLogEntryToBuffer(entry, buffer, RING_ITEM_SIZE);
        
        // 通知异步线程
        queue_cond_.notify_one();
    } else {
        // 同步日志 - 使用线程本地存储缓冲区
        formatLogEntryToBuffer(entry, t_format_buffer, sizeof(t_format_buffer));
        
        // 输出日志
        {
            std::lock_guard<std::mutex> lock(console_mutex_);
            if (config_.console_output) {
                writeToConsole(t_format_buffer, level);
            }
            
            if (config_.file_output) {
                writeToFile(t_format_buffer);
            }
        }
    }
}

// 格式化日志条目到缓冲区
void Logger::formatLogEntryToBuffer(const LogEntry& entry, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size <= 0) {
        return;
    }
    
    // 清空缓冲区
    buffer[0] = '\0';
    
    // 临时缓冲区，用于各部分的格式化
    char temp[256];
    
    // 当前缓冲区位置
    size_t pos = 0;
    
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
        
        // 格式化时间
        std::strftime(temp, sizeof(temp), config_.date_format.c_str(), &tm_buf);
        int bytes = snprintf(buffer + pos, buffer_size - pos, "%s.%03d ", temp, (int)ms);
        if (bytes > 0) {
            pos += bytes;
        }
    }
    
    // 添加日志级别
    if (config_.include_level && pos < buffer_size) {
        int bytes = snprintf(buffer + pos, buffer_size - pos, "[%s] ", getLevelName(entry.level).c_str());
        if (bytes > 0) {
            pos += bytes;
        }
    }
    
    // 添加源信息
    if (config_.include_source && !entry.source.empty() && pos < buffer_size) {
        int bytes = snprintf(buffer + pos, buffer_size - pos, "[%s] ", entry.source.c_str());
        if (bytes > 0) {
            pos += bytes;
        }
    }
    
    // 添加线程ID
    if (config_.include_thread_id && pos < buffer_size) {
        std::stringstream ss;
        ss << entry.thread_id;
        int bytes = snprintf(buffer + pos, buffer_size - pos, "[Thread-%s] ", ss.str().c_str());
        if (bytes > 0) {
            pos += bytes;
        }
    }
    
    // 添加文件和行号
    if (config_.include_file_line && !entry.file.empty() && pos < buffer_size) {
        int bytes = snprintf(buffer + pos, buffer_size - pos, "[%s:%d] ", entry.file.c_str(), entry.line);
        if (bytes > 0) {
            pos += bytes;
        }
    }
    
    // 添加函数名
    if (config_.include_function && !entry.function.empty() && pos < buffer_size) {
        int bytes = snprintf(buffer + pos, buffer_size - pos, "[%s] ", entry.function.c_str());
        if (bytes > 0) {
            pos += bytes;
        }
    }
    
    // 添加消息
    if (pos < buffer_size) {
        int bytes = snprintf(buffer + pos, buffer_size - pos, "%s", entry.message.c_str());
        if (bytes > 0) {
            pos += bytes;
        }
    }
    
    // 确保字符串以'\0'结尾
    buffer[buffer_size - 1] = '\0';
}

// 写入日志文件
void Logger::writeToFile(const char* formatted_entry) {
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
void Logger::writeToConsole(const char* formatted_entry, LogLevel level) {
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
        case LogLevel::FATAL:    attr = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break; // 亮紫色
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
        case LogLevel::FATAL:    color_code = "\033[95m"; break; // 亮紫色
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
    if (static_cast<std::streamoff>(file_size) > static_cast<std::streamoff>(config_.max_file_size)) {
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
    char local_buffer[RING_ITEM_SIZE];
    
    while (!stop_flag_) {
        // 处理环形缓冲区中的日志
        bool has_logs = false;
        
        {
            std::unique_lock<std::mutex> lock(ring_mutex_);
            
            // 等待有日志条目或者停止标志
            queue_cond_.wait_for(lock, std::chrono::seconds(1), [this] {
                return ring_head_ != ring_tail_ || stop_flag_;
            });
            
            // 如果环形缓冲区为空且停止标志已设置，退出循环
            if (ring_head_ == ring_tail_ && stop_flag_) {
                break;
            }
            
            // 处理所有可用的日志条目
            while (ring_head_ != ring_tail_) {
                // 复制日志条目到本地缓冲区
                strncpy(local_buffer, &ring_buffer_[ring_tail_ * RING_ITEM_SIZE], RING_ITEM_SIZE);
                local_buffer[RING_ITEM_SIZE - 1] = '\0';
                
                // 移动尾指针
                ring_tail_ = (ring_tail_ + 1) % RING_BUFFER_SIZE;
                
                has_logs = true;
                
                // 释放锁，处理日志
                lock.unlock();
                
                // 处理日志条目
                if (config_.console_output) {
                    // 为简化起见，我们假设所有异步日志都是INFO级别
                    writeToConsole(local_buffer, LogLevel::INFO);
                }
                
                if (config_.file_output) {
                    writeToFile(local_buffer);
                }
                
                // 重新获取锁，继续处理下一个条目
                lock.lock();
            }
        }
        
        // 如果处理了日志，刷新文件
        if (has_logs) {
            flush();
        }
    }
}

// 设置日志回调函数
void Logger::setLogCallback(std::function<void(const LogEntry&)> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    log_callback_ = callback;
}

// 获取日志配置
LogConfig Logger::getConfig() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

// 刷新日志
void Logger::flush() {
    if (config_.file_output && log_file_stream_ && log_file_stream_->is_open()) {
        std::lock_guard<std::mutex> lock(file_mutex_);
        log_file_stream_->flush();
    }
}

// 其他辅助函数的实现
// ...

// 写入不同级别的日志
void Logger::trace(const std::string& message, const std::string& source,
                  const std::string& file, int line, const std::string& function) {
    log(LogLevel::TRACE, message, source, file, line, function);
}

void Logger::debug(const std::string& message, const std::string& source,
                  const std::string& file, int line, const std::string& function) {
    log(LogLevel::DEBUG, message, source, file, line, function);
}

void Logger::info(const std::string& message, const std::string& source,
                 const std::string& file, int line, const std::string& function) {
    log(LogLevel::INFO, message, source, file, line, function);
}

void Logger::warning(const std::string& message, const std::string& source,
                    const std::string& file, int line, const std::string& function) {
    log(LogLevel::WARNING, message, source, file, line, function);
}

void Logger::error(const std::string& message, const std::string& source,
                  const std::string& file, int line, const std::string& function) {
    log(LogLevel::ERROR, message, source, file, line, function);
}

void Logger::fatal(const std::string& message, const std::string& source,
                     const std::string& file, int line, const std::string& function) {
    log(LogLevel::FATAL, message, source, file, line, function);
}

} // namespace monitor
} // namespace cam_server
