#ifndef CAM_SERVER_UTILS_DEBUG_UTILS_H
#define CAM_SERVER_UTILS_DEBUG_UTILS_H

#include <iostream>
#include <string>
#include <sstream>
#include <mutex>
#include <map>

namespace cam_server {
namespace utils {

/**
 * @brief 日志级别枚举
 */
enum class LogLevel {
    TRACE,  // 最详细的跟踪信息
    DEBUG,  // 调试信息
    INFO,   // 一般信息
    WARN,   // 警告信息
    ERROR,  // 错误信息
    FATAL,  // 致命错误
    OFF     // 关闭日志
};

/**
 * @brief 调试输出工具类
 */
class DebugUtils {
public:
    /**
     * @brief 设置全局日志级别
     *
     * @param level 日志级别
     */
    static void setGlobalLogLevel(LogLevel level) {
        global_log_level_ = level;
    }

    /**
     * @brief 设置模块日志级别
     *
     * @param module 模块名称
     * @param level 日志级别
     */
    static void setModuleLogLevel(const std::string& module, LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        module_log_levels_[module] = level;
    }

    /**
     * @brief 获取模块日志级别
     *
     * @param module 模块名称
     * @return 日志级别
     */
    static LogLevel getModuleLogLevel(const std::string& module) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = module_log_levels_.find(module);
        if (it != module_log_levels_.end()) {
            return it->second;
        }
        return global_log_level_;
    }

    /**
     * @brief 输出日志信息
     *
     * @param level 日志级别
     * @param module 模块名称
     * @param file 文件名
     * @param function 函数名
     * @param line 行号
     * @param message 日志信息
     */
    static void log(LogLevel level, const std::string& module, const std::string& file, const std::string& function, int line, const std::string& message) {
        if (level < getModuleLogLevel(module)) {
            return;
        }

        std::string level_str;
        switch (level) {
            case LogLevel::TRACE: level_str = "TRACE"; break;
            case LogLevel::DEBUG: level_str = "DEBUG"; break;
            case LogLevel::INFO:  level_str = "INFO"; break;
            case LogLevel::WARN:  level_str = "WARN"; break;
            case LogLevel::ERROR: level_str = "ERROR"; break;
            case LogLevel::FATAL: level_str = "FATAL"; break;
            default: level_str = "UNKNOWN"; break;
        }

        std::cerr << "[" << level_str << "][" << module << "][" << file << ":" << function << ":" << line << "] " << message << std::endl;
    }

    /**
     * @brief 输出日志信息（简化版）
     *
     * @param level 日志级别
     * @param module 模块名称
     * @param message 日志信息
     */
    static void log(LogLevel level, const std::string& module, const std::string& message) {
        if (level < getModuleLogLevel(module)) {
            return;
        }

        std::string level_str;
        switch (level) {
            case LogLevel::TRACE: level_str = "TRACE"; break;
            case LogLevel::DEBUG: level_str = "DEBUG"; break;
            case LogLevel::INFO:  level_str = "INFO"; break;
            case LogLevel::WARN:  level_str = "WARN"; break;
            case LogLevel::ERROR: level_str = "ERROR"; break;
            case LogLevel::FATAL: level_str = "FATAL"; break;
            default: level_str = "UNKNOWN"; break;
        }

        std::cerr << "[" << level_str << "][" << module << "] " << message << std::endl;
    }

    /**
     * @brief 获取文件名（不包含路径）
     *
     * @param file 完整文件路径
     * @return 文件名
     */
    static std::string getFileName(const std::string& file) {
        size_t pos = file.find_last_of("/\\");
        if (pos != std::string::npos) {
            return file.substr(pos + 1);
        }
        return file;
    }

private:
    inline static LogLevel global_log_level_ = LogLevel::INFO;
    inline static std::map<std::string, LogLevel> module_log_levels_ = {};
    inline static std::mutex mutex_ = {};
};

// 静态成员变量声明
// 注意：静态成员变量的定义应该放在源文件中，而不是头文件中

// 定义日志宏，方便使用
#define LOG_TRACE_FULL(module, message) \
    cam_server::utils::DebugUtils::log(cam_server::utils::LogLevel::TRACE, module, cam_server::utils::DebugUtils::getFileName(__FILE__), __FUNCTION__, __LINE__, message)

#define LOG_DEBUG_FULL(module, message) \
    cam_server::utils::DebugUtils::log(cam_server::utils::LogLevel::DEBUG, module, cam_server::utils::DebugUtils::getFileName(__FILE__), __FUNCTION__, __LINE__, message)

#define LOG_INFO_FULL(module, message) \
    cam_server::utils::DebugUtils::log(cam_server::utils::LogLevel::INFO, module, cam_server::utils::DebugUtils::getFileName(__FILE__), __FUNCTION__, __LINE__, message)

#define LOG_WARN_FULL(module, message) \
    cam_server::utils::DebugUtils::log(cam_server::utils::LogLevel::WARN, module, cam_server::utils::DebugUtils::getFileName(__FILE__), __FUNCTION__, __LINE__, message)

#define LOG_ERROR_FULL(module, message) \
    cam_server::utils::DebugUtils::log(cam_server::utils::LogLevel::ERROR, module, cam_server::utils::DebugUtils::getFileName(__FILE__), __FUNCTION__, __LINE__, message)

#define LOG_FATAL_FULL(module, message) \
    cam_server::utils::DebugUtils::log(cam_server::utils::LogLevel::FATAL, module, cam_server::utils::DebugUtils::getFileName(__FILE__), __FUNCTION__, __LINE__, message)

#define DEBUG_TRACE(module, message) \
    cam_server::utils::DebugUtils::log(cam_server::utils::LogLevel::TRACE, module, message)

#define DEBUG_DEBUG(module, message) \
    cam_server::utils::DebugUtils::log(cam_server::utils::LogLevel::DEBUG, module, message)

#define DEBUG_INFO(module, message) \
    cam_server::utils::DebugUtils::log(cam_server::utils::LogLevel::INFO, module, message)

#define DEBUG_WARN(module, message) \
    cam_server::utils::DebugUtils::log(cam_server::utils::LogLevel::WARN, module, message)

#define DEBUG_ERROR(module, message) \
    cam_server::utils::DebugUtils::log(cam_server::utils::LogLevel::ERROR, module, message)

#define DEBUG_FATAL(module, message) \
    cam_server::utils::DebugUtils::log(cam_server::utils::LogLevel::FATAL, module, message)

} // namespace utils
} // namespace cam_server

#endif // CAM_SERVER_UTILS_DEBUG_UTILS_H
