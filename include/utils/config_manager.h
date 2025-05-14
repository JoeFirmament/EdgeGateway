#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <any>
#include <functional>

namespace cam_server {
namespace utils {

/**
 * @brief 配置管理器类
 */
class ConfigManager {
public:
    /**
     * @brief 获取ConfigManager单例
     * @return ConfigManager单例的引用
     */
    static ConfigManager& getInstance();

    /**
     * @brief 初始化配置管理器
     * @param config_file 配置文件路径
     * @return 是否初始化成功
     */
    bool initialize(const std::string& config_file);

    /**
     * @brief 加载配置
     * @param config_file 配置文件路径
     * @return 是否成功加载
     */
    bool loadConfig(const std::string& config_file);

    /**
     * @brief 保存配置
     * @param config_file 配置文件路径（可选，默认使用当前配置文件）
     * @return 是否成功保存
     */
    bool saveConfig(const std::string& config_file = "");

    /**
     * @brief 获取字符串配置
     * @param key 配置键
     * @param default_value 默认值
     * @return 配置值
     */
    std::string getString(const std::string& key, const std::string& default_value = "") const;

    /**
     * @brief 获取整数配置
     * @param key 配置键
     * @param default_value 默认值
     * @return 配置值
     */
    int getInt(const std::string& key, int default_value = 0) const;

    /**
     * @brief 获取浮点数配置
     * @param key 配置键
     * @param default_value 默认值
     * @return 配置值
     */
    double getDouble(const std::string& key, double default_value = 0.0) const;

    /**
     * @brief 获取布尔配置
     * @param key 配置键
     * @param default_value 默认值
     * @return 配置值
     */
    bool getBool(const std::string& key, bool default_value = false) const;

    /**
     * @brief 获取字符串数组配置
     * @param key 配置键
     * @return 配置值
     */
    std::vector<std::string> getStringArray(const std::string& key) const;

    /**
     * @brief 获取整数数组配置
     * @param key 配置键
     * @return 配置值
     */
    std::vector<int> getIntArray(const std::string& key) const;

    /**
     * @brief 获取浮点数数组配置
     * @param key 配置键
     * @return 配置值
     */
    std::vector<double> getDoubleArray(const std::string& key) const;

    /**
     * @brief 设置字符串配置
     * @param key 配置键
     * @param value 配置值
     */
    void setString(const std::string& key, const std::string& value);

    /**
     * @brief 设置整数配置
     * @param key 配置键
     * @param value 配置值
     */
    void setInt(const std::string& key, int value);

    /**
     * @brief 设置浮点数配置
     * @param key 配置键
     * @param value 配置值
     */
    void setDouble(const std::string& key, double value);

    /**
     * @brief 设置布尔配置
     * @param key 配置键
     * @param value 配置值
     */
    void setBool(const std::string& key, bool value);

    /**
     * @brief 设置字符串数组配置
     * @param key 配置键
     * @param value 配置值
     */
    void setStringArray(const std::string& key, const std::vector<std::string>& value);

    /**
     * @brief 设置整数数组配置
     * @param key 配置键
     * @param value 配置值
     */
    void setIntArray(const std::string& key, const std::vector<int>& value);

    /**
     * @brief 设置浮点数数组配置
     * @param key 配置键
     * @param value 配置值
     */
    void setDoubleArray(const std::string& key, const std::vector<double>& value);

    /**
     * @brief 检查配置是否存在
     * @param key 配置键
     * @return 配置是否存在
     */
    bool hasKey(const std::string& key) const;

    /**
     * @brief 删除配置
     * @param key 配置键
     * @return 是否成功删除
     */
    bool removeKey(const std::string& key);

    /**
     * @brief 获取所有配置键
     * @return 所有配置键
     */
    std::vector<std::string> getAllKeys() const;

    /**
     * @brief 清空所有配置
     */
    void clear();

    /**
     * @brief 设置配置变更回调函数
     * @param callback 回调函数
     */
    void setChangeCallback(std::function<void(const std::string&, const std::any&)> callback);

private:
    // 私有构造函数，防止外部创建实例
    ConfigManager();
    // 禁止拷贝构造和赋值操作
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // 内部初始化方法，实际执行初始化逻辑
    bool initializeInternal(const std::string& config_file);

    // 内部加载配置方法，不获取互斥锁
    bool loadConfigInternal(const std::string& config_file);

    // 解析JSON对象
    void parseJsonObject(const std::string& prefix, const std::string& json);

    // 解析JSON数组
    void parseJsonArray(const std::string& key, const std::string& json);

    // 处理键值对
    void processKeyValue(const std::string& prefix, const std::string& key, const std::string& value);

    // 配置数据
    std::unordered_map<std::string, std::any> config_data_;
    // 配置文件路径
    std::string config_file_;
    // 互斥锁
    mutable std::mutex mutex_;
    // 是否已初始化
    bool is_initialized_;
    // 配置变更回调函数
    std::function<void(const std::string&, const std::any&)> change_callback_;
};

} // namespace utils
} // namespace cam_server

#endif // CONFIG_MANAGER_H
