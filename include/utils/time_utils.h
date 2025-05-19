#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <string>
#include <chrono>
#include <ctime>
#include <thread>

namespace cam_server {
namespace utils {

/**
 * @brief 时间工具类
 */
class TimeUtils {
public:
    /**
     * @brief 获取当前时间戳（毫秒）
     * @return 当前时间戳（毫秒）
     */
    static int64_t getCurrentTimeMillis() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    /**
     * @brief 获取当前时间戳（微秒）
     * @return 当前时间戳（微秒）
     */
    static int64_t getCurrentTimeMicros() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    /**
     * @brief 获取当前时间戳（纳秒）
     * @return 当前时间戳（纳秒）
     */
    static int64_t getCurrentTimeNanos() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    /**
     * @brief 获取当前时间戳
     * @return 当前时间戳
     */
    static int64_t getCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    /**
     * @brief 获取当前时间字符串
     * @param format 时间格式
     * @return 当前时间字符串
     */
    static std::string getCurrentTimeString(const std::string& format = "%Y-%m-%d %H:%M:%S") {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        char buffer[100];
        std::strftime(buffer, sizeof(buffer), format.c_str(), std::localtime(&time_t_now));
        return std::string(buffer);
    }

    /**
     * @brief 获取当前日期字符串
     * @param format 日期格式
     * @return 当前日期字符串
     */
    static std::string getCurrentDateString(const std::string& format = "%Y-%m-%d") {
        return getCurrentTimeString(format);
    }

    /**
     * @brief 获取当前时间字符串（用于文件名）
     * @return 当前时间字符串（用于文件名）
     */
    static std::string getCurrentTimeStringForFilename() {
        return getCurrentTimeString("%Y%m%d_%H%M%S");
    }

    /**
     * @brief 获取当前日期字符串（用于文件名）
     * @return 当前日期字符串（用于文件名）
     */
    static std::string getCurrentDateStringForFilename() {
        return getCurrentTimeString("%Y%m%d");
    }

    /**
     * @brief 获取当前时间戳字符串
     * @return 当前时间戳字符串
     */
    static std::string getCurrentTimestampString() {
        return std::to_string(getCurrentTimeMillis());
    }

    /**
     * @brief 睡眠指定毫秒数
     * @param millis 毫秒数
     */
    static void sleepMillis(int64_t millis) {
        std::this_thread::sleep_for(std::chrono::milliseconds(millis));
    }

    /**
     * @brief 睡眠指定微秒数
     * @param micros 微秒数
     */
    static void sleepMicros(int64_t micros) {
        std::this_thread::sleep_for(std::chrono::microseconds(micros));
    }

    /**
     * @brief 睡眠指定纳秒数
     * @param nanos 纳秒数
     */
    static void sleepNanos(int64_t nanos) {
        std::this_thread::sleep_for(std::chrono::nanoseconds(nanos));
    }
};

} // namespace utils
} // namespace cam_server

#endif // TIME_UTILS_H
