#include "utils/config_manager.h"
#include "utils/file_utils.h"
#include "utils/string_utils.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <regex>

namespace cam_server {
namespace utils {

// 获取ConfigManager单例
ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

// 构造函数
ConfigManager::ConfigManager() : is_initialized_(false) {
}

// 初始化配置管理器
bool ConfigManager::initialize(const std::string& config_file) {
    std::lock_guard<std::mutex> lock(mutex_);

    config_file_ = config_file;

    // 加载配置文件
    bool result = loadConfig(config_file);

    is_initialized_ = result;
    return result;
}

// 加载配置
bool ConfigManager::loadConfig(const std::string& config_file) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 如果指定了配置文件，更新当前配置文件路径
    if (!config_file.empty()) {
        config_file_ = config_file;
    }

    // 检查配置文件是否存在
    if (!FileUtils::fileExists(config_file_)) {
        return false;
    }

    try {
        // 读取配置文件内容
        std::string content = FileUtils::readFile(config_file_);
        if (content.empty()) {
            return false;
        }

        // 清空当前配置
        config_data_.clear();

        // 简单的JSON解析
        // 这里使用一个非常简单的方法解析JSON，仅支持基本的键值对
        std::istringstream iss(content);
        std::string line;
        std::string current_section;

        // 正则表达式匹配节
        std::regex section_regex("\\s*\\[([^\\]]+)\\]\\s*");
        // 正则表达式匹配键值对
        std::regex keyval_regex("\\s*([^=]+)\\s*=\\s*(.*)\\s*");

        while (std::getline(iss, line)) {
            // 跳过空行和注释
            if (line.empty() || line[0] == '#' || line[0] == ';') {
                continue;
            }

            std::smatch match;

            // 检查是否是节
            if (std::regex_match(line, match, section_regex)) {
                current_section = match[1].str();
            }
            // 检查是否是键值对
            else if (std::regex_match(line, match, keyval_regex)) {
                std::string key = match[1].str();
                std::string value = match[2].str();

                // 去除首尾空白
                key = StringUtils::trim(key);
                value = StringUtils::trim(value);

                // 去除引号
                if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                    value = value.substr(1, value.size() - 2);
                }

                // 构建完整键名
                std::string full_key = current_section.empty() ? key : current_section + "." + key;

                // 尝试解析值类型
                if (value == "true" || value == "false") {
                    // 布尔值
                    config_data_[full_key] = (value == "true");
                } else if (std::regex_match(value, std::regex("^-?\\d+$"))) {
                    // 整数
                    config_data_[full_key] = std::stoi(value);
                } else if (std::regex_match(value, std::regex("^-?\\d+\\.\\d+$"))) {
                    // 浮点数
                    config_data_[full_key] = std::stod(value);
                } else if (std::regex_match(value, std::regex("^\\[.*\\]$"))) {
                    // 数组
                    std::string array_str = value.substr(1, value.size() - 2);
                    std::vector<std::string> items = StringUtils::split(array_str, ',');

                    // 检查数组类型
                    if (!items.empty()) {
                        bool is_int = true;
                        bool is_double = true;

                        for (const auto& item : items) {
                            std::string trimmed = StringUtils::trim(item);
                            if (!std::regex_match(trimmed, std::regex("^-?\\d+$"))) {
                                is_int = false;
                            }
                            if (!std::regex_match(trimmed, std::regex("^-?\\d+(\\.\\d+)?$"))) {
                                is_double = false;
                            }
                        }

                        if (is_int) {
                            // 整数数组
                            std::vector<int> int_array;
                            for (const auto& item : items) {
                                int_array.push_back(std::stoi(StringUtils::trim(item)));
                            }
                            config_data_[full_key] = int_array;
                        } else if (is_double) {
                            // 浮点数数组
                            std::vector<double> double_array;
                            for (const auto& item : items) {
                                double_array.push_back(std::stod(StringUtils::trim(item)));
                            }
                            config_data_[full_key] = double_array;
                        } else {
                            // 字符串数组
                            std::vector<std::string> str_array;
                            for (const auto& item : items) {
                                std::string trimmed = StringUtils::trim(item);
                                // 去除引号
                                if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
                                    trimmed = trimmed.substr(1, trimmed.size() - 2);
                                }
                                str_array.push_back(trimmed);
                            }
                            config_data_[full_key] = str_array;
                        }
                    }
                } else {
                    // 字符串
                    config_data_[full_key] = value;
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load config file: " << e.what() << std::endl;
        return false;
    }
}

// 保存配置
bool ConfigManager::saveConfig(const std::string& config_file) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 如果指定了配置文件，更新当前配置文件路径
    if (!config_file.empty()) {
        config_file_ = config_file;
    }

    try {
        // 构建配置文件内容
        std::ostringstream oss;

        // 按节组织配置
        std::unordered_map<std::string, std::vector<std::pair<std::string, std::any>>> sections;

        // 将配置项分组到各个节
        for (const auto& pair : config_data_) {
            const std::string& key = pair.first;
            const std::any& value = pair.second;

            // 解析键路径
            size_t dot_pos = key.find('.');
            if (dot_pos != std::string::npos) {
                std::string section = key.substr(0, dot_pos);
                std::string subkey = key.substr(dot_pos + 1);
                sections[section].push_back(std::make_pair(subkey, value));
            } else {
                // 没有节的配置项放在默认节
                sections[""].push_back(std::make_pair(key, value));
            }
        }

        // 先写入没有节的配置项
        auto it = sections.find("");
        if (it != sections.end()) {
            for (const auto& item : it->second) {
                oss << item.first << " = ";

                // 根据值类型格式化
                try {
                    if (item.second.type() == typeid(std::string)) {
                        oss << "\"" << std::any_cast<std::string>(item.second) << "\"";
                    } else if (item.second.type() == typeid(int)) {
                        oss << std::any_cast<int>(item.second);
                    } else if (item.second.type() == typeid(double)) {
                        oss << std::any_cast<double>(item.second);
                    } else if (item.second.type() == typeid(bool)) {
                        oss << (std::any_cast<bool>(item.second) ? "true" : "false");
                    } else if (item.second.type() == typeid(std::vector<std::string>)) {
                        oss << "[";
                        const auto& arr = std::any_cast<std::vector<std::string>>(item.second);
                        for (size_t i = 0; i < arr.size(); ++i) {
                            oss << "\"" << arr[i] << "\"";
                            if (i < arr.size() - 1) {
                                oss << ", ";
                            }
                        }
                        oss << "]";
                    } else if (item.second.type() == typeid(std::vector<int>)) {
                        oss << "[";
                        const auto& arr = std::any_cast<std::vector<int>>(item.second);
                        for (size_t i = 0; i < arr.size(); ++i) {
                            oss << arr[i];
                            if (i < arr.size() - 1) {
                                oss << ", ";
                            }
                        }
                        oss << "]";
                    } else if (item.second.type() == typeid(std::vector<double>)) {
                        oss << "[";
                        const auto& arr = std::any_cast<std::vector<double>>(item.second);
                        for (size_t i = 0; i < arr.size(); ++i) {
                            oss << arr[i];
                            if (i < arr.size() - 1) {
                                oss << ", ";
                            }
                        }
                        oss << "]";
                    }
                } catch (const std::bad_any_cast&) {
                    // 忽略类型转换错误
                    oss << "\"\"";
                }

                oss << std::endl;
            }

            // 如果有其他节，添加一个空行
            if (sections.size() > 1) {
                oss << std::endl;
            }
        }

        // 写入其他节
        for (const auto& section : sections) {
            if (section.first.empty()) {
                continue; // 跳过默认节
            }

            oss << "[" << section.first << "]" << std::endl;

            for (const auto& item : section.second) {
                oss << item.first << " = ";

                // 根据值类型格式化
                try {
                    if (item.second.type() == typeid(std::string)) {
                        oss << "\"" << std::any_cast<std::string>(item.second) << "\"";
                    } else if (item.second.type() == typeid(int)) {
                        oss << std::any_cast<int>(item.second);
                    } else if (item.second.type() == typeid(double)) {
                        oss << std::any_cast<double>(item.second);
                    } else if (item.second.type() == typeid(bool)) {
                        oss << (std::any_cast<bool>(item.second) ? "true" : "false");
                    } else if (item.second.type() == typeid(std::vector<std::string>)) {
                        oss << "[";
                        const auto& arr = std::any_cast<std::vector<std::string>>(item.second);
                        for (size_t i = 0; i < arr.size(); ++i) {
                            oss << "\"" << arr[i] << "\"";
                            if (i < arr.size() - 1) {
                                oss << ", ";
                            }
                        }
                        oss << "]";
                    } else if (item.second.type() == typeid(std::vector<int>)) {
                        oss << "[";
                        const auto& arr = std::any_cast<std::vector<int>>(item.second);
                        for (size_t i = 0; i < arr.size(); ++i) {
                            oss << arr[i];
                            if (i < arr.size() - 1) {
                                oss << ", ";
                            }
                        }
                        oss << "]";
                    } else if (item.second.type() == typeid(std::vector<double>)) {
                        oss << "[";
                        const auto& arr = std::any_cast<std::vector<double>>(item.second);
                        for (size_t i = 0; i < arr.size(); ++i) {
                            oss << arr[i];
                            if (i < arr.size() - 1) {
                                oss << ", ";
                            }
                        }
                        oss << "]";
                    }
                } catch (const std::bad_any_cast&) {
                    // 忽略类型转换错误
                    oss << "\"\"";
                }

                oss << std::endl;
            }

            oss << std::endl;
        }

        // 写入文件
        return FileUtils::writeFile(config_file_, oss.str());
    } catch (const std::exception& e) {
        std::cerr << "Failed to save config file: " << e.what() << std::endl;
        return false;
    }
}

// 获取字符串配置
std::string ConfigManager::getString(const std::string& key, const std::string& default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = config_data_.find(key);
    if (it != config_data_.end()) {
        try {
            return std::any_cast<std::string>(it->second);
        } catch (const std::bad_any_cast&) {
            return default_value;
        }
    }

    return default_value;
}

// 获取整数配置
int ConfigManager::getInt(const std::string& key, int default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = config_data_.find(key);
    if (it != config_data_.end()) {
        try {
            return std::any_cast<int>(it->second);
        } catch (const std::bad_any_cast&) {
            return default_value;
        }
    }

    return default_value;
}

// 获取浮点数配置
double ConfigManager::getDouble(const std::string& key, double default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = config_data_.find(key);
    if (it != config_data_.end()) {
        try {
            return std::any_cast<double>(it->second);
        } catch (const std::bad_any_cast&) {
            return default_value;
        }
    }

    return default_value;
}

// 获取布尔配置
bool ConfigManager::getBool(const std::string& key, bool default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = config_data_.find(key);
    if (it != config_data_.end()) {
        try {
            return std::any_cast<bool>(it->second);
        } catch (const std::bad_any_cast&) {
            return default_value;
        }
    }

    return default_value;
}

// 获取字符串数组配置
std::vector<std::string> ConfigManager::getStringArray(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = config_data_.find(key);
    if (it != config_data_.end()) {
        try {
            return std::any_cast<std::vector<std::string>>(it->second);
        } catch (const std::bad_any_cast&) {
            return {};
        }
    }

    return {};
}

// 获取整数数组配置
std::vector<int> ConfigManager::getIntArray(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = config_data_.find(key);
    if (it != config_data_.end()) {
        try {
            return std::any_cast<std::vector<int>>(it->second);
        } catch (const std::bad_any_cast&) {
            return {};
        }
    }

    return {};
}

// 获取浮点数数组配置
std::vector<double> ConfigManager::getDoubleArray(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = config_data_.find(key);
    if (it != config_data_.end()) {
        try {
            return std::any_cast<std::vector<double>>(it->second);
        } catch (const std::bad_any_cast&) {
            return {};
        }
    }

    return {};
}

// 设置字符串配置
void ConfigManager::setString(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);

    config_data_[key] = value;

    if (change_callback_) {
        change_callback_(key, value);
    }
}

// 设置整数配置
void ConfigManager::setInt(const std::string& key, int value) {
    std::lock_guard<std::mutex> lock(mutex_);

    config_data_[key] = value;

    if (change_callback_) {
        change_callback_(key, value);
    }
}

// 设置浮点数配置
void ConfigManager::setDouble(const std::string& key, double value) {
    std::lock_guard<std::mutex> lock(mutex_);

    config_data_[key] = value;

    if (change_callback_) {
        change_callback_(key, value);
    }
}

// 设置布尔配置
void ConfigManager::setBool(const std::string& key, bool value) {
    std::lock_guard<std::mutex> lock(mutex_);

    config_data_[key] = value;

    if (change_callback_) {
        change_callback_(key, value);
    }
}

// 设置字符串数组配置
void ConfigManager::setStringArray(const std::string& key, const std::vector<std::string>& value) {
    std::lock_guard<std::mutex> lock(mutex_);

    config_data_[key] = value;

    if (change_callback_) {
        change_callback_(key, value);
    }
}

// 设置整数数组配置
void ConfigManager::setIntArray(const std::string& key, const std::vector<int>& value) {
    std::lock_guard<std::mutex> lock(mutex_);

    config_data_[key] = value;

    if (change_callback_) {
        change_callback_(key, value);
    }
}

// 设置浮点数数组配置
void ConfigManager::setDoubleArray(const std::string& key, const std::vector<double>& value) {
    std::lock_guard<std::mutex> lock(mutex_);

    config_data_[key] = value;

    if (change_callback_) {
        change_callback_(key, value);
    }
}

// 检查配置是否存在
bool ConfigManager::hasKey(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    return config_data_.find(key) != config_data_.end();
}

// 删除配置
bool ConfigManager::removeKey(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = config_data_.find(key);
    if (it != config_data_.end()) {
        config_data_.erase(it);
        return true;
    }

    return false;
}

// 获取所有配置键
std::vector<std::string> ConfigManager::getAllKeys() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> keys;
    keys.reserve(config_data_.size());

    for (const auto& pair : config_data_) {
        keys.push_back(pair.first);
    }

    return keys;
}

// 清空所有配置
void ConfigManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    config_data_.clear();
}

// 设置配置变更回调函数
void ConfigManager::setChangeCallback(std::function<void(const std::string&, const std::any&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);

    change_callback_ = callback;
}

} // namespace utils
} // namespace cam_server
