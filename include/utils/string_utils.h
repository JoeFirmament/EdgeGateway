#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <locale>
#include <iomanip>

namespace cam_server {
namespace utils {

/**
 * @brief 字符串工具类
 */
class StringUtils {
public:
    /**
     * @brief 分割字符串
     * @param str 要分割的字符串
     * @param delimiter 分隔符
     * @return 分割后的字符串数组
     */
    static std::vector<std::string> split(const std::string& str, char delimiter);

    /**
     * @brief 分割字符串
     * @param str 要分割的字符串
     * @param delimiter 分隔符
     * @return 分割后的字符串数组
     */
    static std::vector<std::string> split(const std::string& str, const std::string& delimiter);

    /**
     * @brief 连接字符串数组
     * @param strings 字符串数组
     * @param delimiter 分隔符
     * @return 连接后的字符串
     */
    static std::string join(const std::vector<std::string>& strings, const std::string& delimiter);

    /**
     * @brief 转换为小写
     * @param str 要转换的字符串
     * @return 转换后的字符串
     */
    static std::string toLower(const std::string& str);

    /**
     * @brief 转换为大写
     * @param str 要转换的字符串
     * @return 转换后的字符串
     */
    static std::string toUpper(const std::string& str);

    /**
     * @brief 去除首尾空白字符
     * @param str 要处理的字符串
     * @return 处理后的字符串
     */
    static std::string trim(const std::string& str);

    /**
     * @brief 去除左侧空白字符
     * @param str 要处理的字符串
     * @return 处理后的字符串
     */
    static std::string trimLeft(const std::string& str);

    /**
     * @brief 去除右侧空白字符
     * @param str 要处理的字符串
     * @return 处理后的字符串
     */
    static std::string trimRight(const std::string& str);

    /**
     * @brief 替换字符串中的子串
     * @param str 原字符串
     * @param from 要替换的子串
     * @param to 替换为的子串
     * @return 替换后的字符串
     */
    static std::string replace(const std::string& str, const std::string& from, const std::string& to);

    /**
     * @brief 检查字符串是否以指定前缀开始
     * @param str 要检查的字符串
     * @param prefix 前缀
     * @return 是否以指定前缀开始
     */
    static bool startsWith(const std::string& str, const std::string& prefix);

    /**
     * @brief 检查字符串是否以指定后缀结束
     * @param str 要检查的字符串
     * @param suffix 后缀
     * @return 是否以指定后缀结束
     */
    static bool endsWith(const std::string& str, const std::string& suffix);

    /**
     * @brief 检查字符串是否包含子串
     * @param str 要检查的字符串
     * @param substring 子串
     * @return 是否包含子串
     */
    static bool contains(const std::string& str, const std::string& substring);

    /**
     * @brief 将字符串转换为整数
     * @param str 要转换的字符串
     * @param default_value 默认值
     * @return 转换后的整数
     */
    static int toInt(const std::string& str, int default_value = 0);

    /**
     * @brief 将字符串转换为浮点数
     * @param str 要转换的字符串
     * @param default_value 默认值
     * @return 转换后的浮点数
     */
    static double toDouble(const std::string& str, double default_value = 0.0);

    /**
     * @brief 将字符串转换为布尔值
     * @param str 要转换的字符串
     * @param default_value 默认值
     * @return 转换后的布尔值
     */
    static bool toBool(const std::string& str, bool default_value = false);

    /**
     * @brief 将整数转换为字符串
     * @param value 要转换的整数
     * @return 转换后的字符串
     */
    static std::string toString(int value);

    /**
     * @brief 将浮点数转换为字符串
     * @param value 要转换的浮点数
     * @param precision 精度
     * @return 转换后的字符串
     */
    static std::string toString(double value, int precision = 6);

    /**
     * @brief 将布尔值转换为字符串
     * @param value 要转换的布尔值
     * @return 转换后的字符串
     */
    static std::string toString(bool value);

    /**
     * @brief 格式化字符串
     * @param format 格式字符串
     * @param ... 参数
     * @return 格式化后的字符串
     */
    template<typename... Args>
    static std::string format(const std::string& format, Args... args) {
        // 计算格式化后的字符串长度
        int size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
        if (size <= 0) {
            return "";
        }

        // 创建足够大的缓冲区
        std::vector<char> buf(size);
        std::snprintf(buf.data(), size, format.c_str(), args...);

        // 返回格式化后的字符串
        return std::string(buf.data(), buf.data() + size - 1);
    }

    /**
     * @brief 将字符串转换为十六进制表示
     * @param str 要转换的字符串
     * @return 十六进制表示
     */
    static std::string toHex(const std::string& str);

    /**
     * @brief 将十六进制字符串转换为原始字符串
     * @param hex 十六进制字符串
     * @return 原始字符串
     */
    static std::string fromHex(const std::string& hex);

    /**
     * @brief 生成随机字符串
     * @param length 字符串长度
     * @param include_digits 是否包含数字
     * @param include_lowercase 是否包含小写字母
     * @param include_uppercase 是否包含大写字母
     * @param include_special 是否包含特殊字符
     * @return 随机字符串
     */
    static std::string randomString(int length, bool include_digits = true,
                                  bool include_lowercase = true, bool include_uppercase = true,
                                  bool include_special = false);
};

} // namespace utils
} // namespace cam_server

#endif // STRING_UTILS_H
