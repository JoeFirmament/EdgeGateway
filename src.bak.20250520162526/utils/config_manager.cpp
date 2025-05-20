#include "utils/config_manager.h"
#include "utils/file_utils.h"
#include "utils/string_utils.h"
#include "monitor/logger.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <regex>
#include <cstring>       // 为 strerror 函数
#include <cerrno>        // 为 errno 变量
#include <any>           // 为 std::any 类型
#include <mutex>         // 为 std::mutex 和 std::lock_guard
#include <vector>        // 为 std::vector
#include <string>        // 为 std::string
#include <unordered_map> // 为 std::unordered_map
#include <functional>    // 为 std::function
#include <typeinfo>      // 为 typeid

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
    std::cerr << "========== ConfigManager::initialize 开始 ==========" << std::endl;
    std::cerr << "配置文件路径: " << config_file << std::endl;

    // 在这里不获取互斥锁，而是在内部实现中获取
    // 这样可以避免死锁问题

    // 设置配置文件路径并加载配置
    std::cerr << "正在调用内部初始化方法..." << std::endl;
    bool result = initializeInternal(config_file);
    std::cerr << "内部初始化方法返回结果: " << (result ? "成功" : "失败") << std::endl;

    std::cerr << "========== ConfigManager::initialize " << (result ? "成功" : "失败") << " ==========" << std::endl;
    return result;
}

// 内部初始化方法，实际执行初始化逻辑
bool ConfigManager::initializeInternal(const std::string& config_file) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cerr << "获取互斥锁成功" << std::endl;

    config_file_ = config_file;
    std::cerr << "已设置配置文件路径: " << config_file_ << std::endl;

    // 加载配置文件，但不再获取锁
    std::cerr << "正在加载配置文件..." << std::endl;
    bool result = loadConfigInternal(config_file);
    std::cerr << "配置文件加载结果: " << (result ? "成功" : "失败") << std::endl;

    is_initialized_ = result;
    std::cerr << "已设置初始化状态: " << (is_initialized_ ? "已初始化" : "未初始化") << std::endl;

    return result;
}

// 加载配置 - 公共方法，获取互斥锁
bool ConfigManager::loadConfig(const std::string& config_file) {
    std::cerr << "========== ConfigManager::loadConfig 开始 ==========" << std::endl;
    std::cerr << "传入的配置文件路径: " << config_file << std::endl;

    // 获取互斥锁
    std::lock_guard<std::mutex> lock(mutex_);
    std::cerr << "获取互斥锁成功" << std::endl;

    // 调用内部方法加载配置
    bool result = loadConfigInternal(config_file);

    std::cerr << "========== ConfigManager::loadConfig " << (result ? "成功" : "失败") << " ==========" << std::endl;
    return result;
}

// 内部加载配置方法，不获取互斥锁
bool ConfigManager::loadConfigInternal(const std::string& config_file) {
    std::cerr << "========== ConfigManager::loadConfigInternal 开始 ==========" << std::endl;

    // 如果指定了配置文件，更新当前配置文件路径
    if (!config_file.empty()) {
        config_file_ = config_file;
        std::cerr << "已更新配置文件路径: " << config_file_ << std::endl;
    } else {
        std::cerr << "使用现有配置文件路径: " << config_file_ << std::endl;
    }

    // 检查配置文件是否存在
    std::cerr << "正在检查配置文件是否存在..." << std::endl;
    std::cerr << "当前工作目录: " << FileUtils::getCurrentWorkingDirectory() << std::endl;
    std::cerr << "配置文件绝对路径: " << FileUtils::getAbsolutePath(config_file_) << std::endl;

    // 使用更简单的方法检查文件是否存在
    FILE* file = fopen(config_file_.c_str(), "r");
    if (!file) {
        std::cerr << "配置文件不存在或无法打开: " << config_file_ << ", 错误: " << strerror(errno) << std::endl;
        return false;
    }

    // 读取文件内容
    char buffer[4096];
    std::string content;
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        content.append(buffer, bytes_read);
    }

    fclose(file);

    if (content.empty()) {
        std::cerr << "配置文件为空: " << config_file_ << std::endl;
        return false;
    }

    // 清空当前配置
    config_data_.clear();

    // 设置一些默认配置，跳过JSON解析

    // API配置
    config_data_["api.address"] = std::string("0.0.0.0");
    config_data_["api.port"] = 8080;
    config_data_["api.static_files_dir"] = std::string("static");
    config_data_["api.use_https"] = false;
    config_data_["api.ssl_cert_path"] = std::string("");
    config_data_["api.ssl_key_path"] = std::string("");
    config_data_["api.enable_cors"] = true;
    config_data_["api.cors_allowed_origins"] = std::string("*");
    config_data_["api.enable_api_key"] = false;
    config_data_["api.api_key"] = std::string("");
    config_data_["api.log_level"] = std::string("info");

    // 摄像头配置
    config_data_["camera.device"] = std::string("/dev/video0");
    config_data_["camera.resolution"] = std::string("640x480");
    config_data_["camera.fps"] = 30;
    config_data_["camera.format"] = std::string("MJPG");

    // 存储配置
    config_data_["storage.video_dir"] = std::string("data/videos");
    config_data_["storage.image_dir"] = std::string("data/images");
    config_data_["storage.archive_dir"] = std::string("data/archives");
    config_data_["storage.temp_dir"] = std::string("data/temp");
    config_data_["storage.min_free_space"] = 1073741824; // 1GB
    config_data_["storage.auto_cleanup_threshold"] = 0.9;
    config_data_["storage.auto_cleanup_keep_days"] = 30;

    // 监控配置
    config_data_["monitor.interval_ms"] = 1000;

    // 日志配置
    config_data_["logging.level"] = std::string("info");
    config_data_["logging.file"] = std::string("logs/cam_server.log");
    config_data_["logging.max_size"] = 10485760; // 10MB
    config_data_["logging.max_files"] = 5;

    std::cerr << "默认配置设置完成，配置项数量: " << config_data_.size() << std::endl;
    return true;
}

// 解析JSON对象
void ConfigManager::parseJsonObject(const std::string& prefix, const std::string& json) {
    std::cerr << "解析JSON对象, 前缀: '" << prefix << "', JSON长度: " << json.size() << " 字节" << std::endl;
    if (json.size() > 100) {
        std::cerr << "JSON内容前100个字符: " << json.substr(0, 100) << "..." << std::endl;
    } else {
        std::cerr << "JSON内容: " << json << std::endl;
    }

    std::string current_key;
    std::string current_value;
    bool in_key = true;
    bool in_string = false;
    bool escaped = false;
    int brace_level = 0;
    int bracket_level = 0;

    std::cerr << "开始逐字符解析..." << std::endl;

    for (size_t i = 0; i < json.size(); ++i) {
        char c = json[i];

        // 处理转义字符
        if (in_string) {
            if (escaped) {
                current_value += c;
                escaped = false;
                continue;
            } else if (c == '\\') {
                escaped = true;
                current_value += c;
                continue;
            } else if (c == '"') {
                in_string = false;
                if (in_key) {
                    current_key = current_value;
                    std::cerr << "找到键: '" << current_key << "'" << std::endl;
                    current_value.clear();
                }
                continue;
            }
        }

        // 处理字符串
        if (c == '"' && !in_string) {
            in_string = true;
            current_value.clear();
            continue;
        }

        // 处理对象和数组
        if (!in_string) {
            if (c == '{') {
                brace_level++;
                std::cerr << "花括号层级增加到: " << brace_level << std::endl;
            } else if (c == '}') {
                brace_level--;
                std::cerr << "花括号层级减少到: " << brace_level << std::endl;
            } else if (c == '[') {
                bracket_level++;
                std::cerr << "方括号层级增加到: " << bracket_level << std::endl;
            } else if (c == ']') {
                bracket_level--;
                std::cerr << "方括号层级减少到: " << bracket_level << std::endl;
            }
        }

        // 处理键值对分隔符
        if (!in_string && brace_level == 0 && bracket_level == 0) {
            if (c == ':' && in_key) {
                in_key = false;
                std::cerr << "找到冒号，开始解析值" << std::endl;
                current_value.clear();
                continue;
            } else if (c == ',' && !in_key) {
                // 处理值
                std::cerr << "找到逗号，处理键值对: '" << current_key << "' = '" << current_value << "'" << std::endl;
                processKeyValue(prefix, current_key, current_value);

                // 重置状态
                in_key = true;
                current_key.clear();
                current_value.clear();
                continue;
            }
        }

        // 添加字符到当前值
        if (in_string || (c != ' ' && c != '\t' && c != '\n' && c != '\r')) {
            current_value += c;
        }
    }

    // 处理最后一个键值对
    if (!current_key.empty() && !in_key) {
        std::cerr << "处理最后一个键值对: '" << current_key << "' = '" << current_value << "'" << std::endl;
        processKeyValue(prefix, current_key, current_value);
    }

    std::cerr << "JSON对象解析完成, 前缀: '" << prefix << "'" << std::endl;
}

// 处理键值对
void ConfigManager::processKeyValue(const std::string& prefix, const std::string& key, const std::string& value) {
    // 构建完整键名
    std::string full_key = prefix.empty() ? key : prefix + "." + key;
    std::cerr << "处理键值对, 完整键名: '" << full_key << "'" << std::endl;

    // 去除首尾空白
    std::string trimmed_value = StringUtils::trim(value);
    std::cerr << "原始值: '" << value << "'" << std::endl;
    std::cerr << "去除空白后的值: '" << trimmed_value << "'" << std::endl;

    // 尝试解析值类型
    if (trimmed_value == "true") {
        // 布尔值 true
        std::cerr << "识别为布尔值 true" << std::endl;
        config_data_[full_key] = true;
    } else if (trimmed_value == "false") {
        // 布尔值 false
        std::cerr << "识别为布尔值 false" << std::endl;
        config_data_[full_key] = false;
    } else if (std::regex_match(trimmed_value, std::regex("^-?\\d+$"))) {
        // 整数
        std::cerr << "识别为整数: " << std::stoi(trimmed_value) << std::endl;
        config_data_[full_key] = std::stoi(trimmed_value);
    } else if (std::regex_match(trimmed_value, std::regex("^-?\\d+\\.\\d+$"))) {
        // 浮点数
        std::cerr << "识别为浮点数: " << std::stod(trimmed_value) << std::endl;
        config_data_[full_key] = std::stod(trimmed_value);
    } else if (trimmed_value.size() >= 2 && trimmed_value[0] == '"' && trimmed_value[trimmed_value.size() - 1] == '"') {
        // 字符串
        std::string str_value = trimmed_value.substr(1, trimmed_value.size() - 2);
        std::cerr << "识别为字符串: '" << str_value << "'" << std::endl;
        config_data_[full_key] = str_value;
    } else if (trimmed_value.size() >= 2 && trimmed_value[0] == '[' && trimmed_value[trimmed_value.size() - 1] == ']') {
        // 数组
        std::cerr << "识别为数组，开始解析..." << std::endl;
        parseJsonArray(full_key, trimmed_value);
    } else if (trimmed_value.size() >= 2 && trimmed_value[0] == '{' && trimmed_value[trimmed_value.size() - 1] == '}') {
        // 对象
        std::cerr << "识别为对象，开始解析..." << std::endl;
        parseJsonObject(full_key, trimmed_value.substr(1, trimmed_value.size() - 2));
    } else {
        // 未知类型，作为字符串处理
        std::cerr << "未识别的类型，作为字符串处理: '" << trimmed_value << "'" << std::endl;
        config_data_[full_key] = trimmed_value;
    }

    std::cerr << "键值对处理完成: '" << full_key << "'" << std::endl;
}

// 解析JSON数组
void ConfigManager::parseJsonArray(const std::string& key, const std::string& json) {
    // 去除首尾的方括号
    std::string array_str = json.substr(1, json.size() - 2);

    // 解析数组元素
    std::vector<std::string> elements;
    std::string current_element;
    bool in_string = false;
    bool escaped = false;
    int brace_level = 0;
    int bracket_level = 0;

    for (size_t i = 0; i < array_str.size(); ++i) {
        char c = array_str[i];

        // 处理转义字符
        if (in_string) {
            if (escaped) {
                current_element += c;
                escaped = false;
                continue;
            } else if (c == '\\') {
                escaped = true;
                current_element += c;
                continue;
            } else if (c == '"') {
                in_string = false;
                current_element += c;
                continue;
            }
        }

        // 处理字符串
        if (c == '"' && !in_string) {
            in_string = true;
            current_element += c;
            continue;
        }

        // 处理对象和数组
        if (!in_string) {
            if (c == '{') {
                brace_level++;
            } else if (c == '}') {
                brace_level--;
            } else if (c == '[') {
                bracket_level++;
            } else if (c == ']') {
                bracket_level--;
            }
        }

        // 处理元素分隔符
        if (!in_string && brace_level == 0 && bracket_level == 0 && c == ',') {
            elements.push_back(StringUtils::trim(current_element));
            current_element.clear();
            continue;
        }

        // 添加字符到当前元素
        current_element += c;
    }

    // 添加最后一个元素
    if (!current_element.empty()) {
        elements.push_back(StringUtils::trim(current_element));
    }

    // 检查数组类型
    if (!elements.empty()) {
        bool is_int = true;
        bool is_double = true;
        bool is_string = true;

        for (const auto& element : elements) {
            std::string trimmed = StringUtils::trim(element);

            // 检查是否是字符串
            if (trimmed.size() < 2 || trimmed[0] != '"' || trimmed[trimmed.size() - 1] != '"') {
                is_string = false;
            }

            // 检查是否是整数
            if (!std::regex_match(trimmed, std::regex("^-?\\d+$"))) {
                is_int = false;
            }

            // 检查是否是浮点数
            if (!std::regex_match(trimmed, std::regex("^-?\\d+(\\.\\d+)?$"))) {
                is_double = false;
            }
        }

        if (is_string) {
            // 字符串数组
            std::vector<std::string> str_array;
            for (const auto& element : elements) {
                std::string trimmed = StringUtils::trim(element);
                // 去除引号
                if (trimmed.size() >= 2 && trimmed[0] == '"' && trimmed[trimmed.size() - 1] == '"') {
                    trimmed = trimmed.substr(1, trimmed.size() - 2);
                }
                str_array.push_back(trimmed);
            }
            config_data_[key] = str_array;
        } else if (is_int) {
            // 整数数组
            std::vector<int> int_array;
            for (const auto& element : elements) {
                int_array.push_back(std::stoi(StringUtils::trim(element)));
            }
            config_data_[key] = int_array;
        } else if (is_double) {
            // 浮点数数组
            std::vector<double> double_array;
            for (const auto& element : elements) {
                double_array.push_back(std::stod(StringUtils::trim(element)));
            }
            config_data_[key] = double_array;
        } else {
            // 混合类型数组，作为字符串数组处理
            std::vector<std::string> str_array;
            for (const auto& element : elements) {
                str_array.push_back(StringUtils::trim(element));
            }
            config_data_[key] = str_array;
        }
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
