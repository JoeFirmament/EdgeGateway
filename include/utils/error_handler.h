#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <string>
#include <exception>
#include <functional>
#include <vector>
#include <mutex>

namespace cam_server {
namespace utils {

/**
 * @brief 错误级别枚举
 */
enum class ErrorLevel {
    INFO,       // 信息
    WARNING,    // 警告
    ERROR,      // 错误
    CRITICAL    // 严重
};

/**
 * @brief 错误代码枚举
 */
enum class ErrorCode {
    NONE = 0,               // 无错误
    UNKNOWN = 1,            // 未知错误
    INITIALIZATION = 100,   // 初始化错误
    CONFIGURATION = 200,    // 配置错误
    CAMERA = 300,           // 摄像头错误
    VIDEO = 400,            // 视频处理错误
    STORAGE = 500,          // 存储错误
    NETWORK = 600,          // 网络错误
    API = 700,              // API错误
    SYSTEM = 800,           // 系统错误
    PERMISSION = 900,       // 权限错误
    RESOURCE = 1000         // 资源错误
};

/**
 * @brief 自定义异常类
 */
class CamServerException : public std::exception {
public:
    /**
     * @brief 构造函数
     * @param code 错误代码
     * @param message 错误消息
     * @param level 错误级别
     */
    CamServerException(ErrorCode code, const std::string& message, ErrorLevel level = ErrorLevel::ERROR);

    /**
     * @brief 获取错误消息
     * @return 错误消息
     */
    const char* what() const noexcept override;

    /**
     * @brief 获取错误代码
     * @return 错误代码
     */
    ErrorCode getCode() const;

    /**
     * @brief 获取错误级别
     * @return 错误级别
     */
    ErrorLevel getLevel() const;

    /**
     * @brief 获取错误消息
     * @return 错误消息
     */
    std::string getMessage() const;

private:
    // 错误代码
    ErrorCode code_;
    // 错误消息
    std::string message_;
    // 错误级别
    ErrorLevel level_;
};

/**
 * @brief 错误处理器类
 */
class ErrorHandler {
public:
    /**
     * @brief 获取ErrorHandler单例
     * @return ErrorHandler单例的引用
     */
    static ErrorHandler& getInstance();

    /**
     * @brief 初始化错误处理器
     * @return 是否初始化成功
     */
    bool initialize();

    /**
     * @brief 处理异常
     * @param e 异常
     */
    void handleException(const std::exception& e);

    /**
     * @brief 处理自定义异常
     * @param e 自定义异常
     */
    void handleException(const CamServerException& e);

    /**
     * @brief 处理错误
     * @param code 错误代码
     * @param message 错误消息
     * @param level 错误级别
     */
    void handleError(ErrorCode code, const std::string& message, ErrorLevel level = ErrorLevel::ERROR);

    /**
     * @brief 设置错误回调函数
     * @param callback 回调函数
     */
    void setErrorCallback(std::function<void(ErrorCode, const std::string&, ErrorLevel)> callback);

    /**
     * @brief 获取错误代码名称
     * @param code 错误代码
     * @return 错误代码名称
     */
    static std::string getErrorCodeName(ErrorCode code);

    /**
     * @brief 获取错误级别名称
     * @param level 错误级别
     * @return 错误级别名称
     */
    static std::string getErrorLevelName(ErrorLevel level);

private:
    // 私有构造函数，防止外部创建实例
    ErrorHandler();
    // 禁止拷贝构造和赋值操作
    ErrorHandler(const ErrorHandler&) = delete;
    ErrorHandler& operator=(const ErrorHandler&) = delete;

    // 错误回调函数
    std::function<void(ErrorCode, const std::string&, ErrorLevel)> error_callback_;
    // 互斥锁
    std::mutex mutex_;
    // 是否已初始化
    bool is_initialized_;
};

/**
 * @brief 抛出自定义异常的辅助函数
 * @param code 错误代码
 * @param message 错误消息
 * @param level 错误级别
 */
void throwException(ErrorCode code, const std::string& message, ErrorLevel level = ErrorLevel::ERROR);

} // namespace utils
} // namespace cam_server

#endif // ERROR_HANDLER_H
