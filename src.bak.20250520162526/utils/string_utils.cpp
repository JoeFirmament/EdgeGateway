#include "utils/string_utils.h"
#include <random>
#include <ctime>
#include <cstdlib>
#include <regex>

namespace cam_server {
namespace utils {

std::vector<std::string> StringUtils::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream token_stream(str);
    
    while (std::getline(token_stream, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::vector<std::string> StringUtils::split(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = 0;
    
    while ((end = str.find(delimiter, start)) != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
    }
    
    tokens.push_back(str.substr(start));
    return tokens;
}

std::string StringUtils::join(const std::vector<std::string>& strings, const std::string& delimiter) {
    std::string result;
    
    for (size_t i = 0; i < strings.size(); ++i) {
        result += strings[i];
        if (i < strings.size() - 1) {
            result += delimiter;
        }
    }
    
    return result;
}

std::string StringUtils::toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string StringUtils::toUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                  [](unsigned char c) { return std::toupper(c); });
    return result;
}

std::string StringUtils::trim(const std::string& str) {
    return trimRight(trimLeft(str));
}

std::string StringUtils::trimLeft(const std::string& str) {
    std::string result = str;
    result.erase(result.begin(), std::find_if(result.begin(), result.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    return result;
}

std::string StringUtils::trimRight(const std::string& str) {
    std::string result = str;
    result.erase(std::find_if(result.rbegin(), result.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), result.end());
    return result;
}

std::string StringUtils::replace(const std::string& str, const std::string& from, const std::string& to) {
    std::string result = str;
    size_t start_pos = 0;
    
    while ((start_pos = result.find(from, start_pos)) != std::string::npos) {
        result.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    
    return result;
}

bool StringUtils::startsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

bool StringUtils::endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool StringUtils::contains(const std::string& str, const std::string& substring) {
    return str.find(substring) != std::string::npos;
}

int StringUtils::toInt(const std::string& str, int default_value) {
    try {
        return std::stoi(str);
    } catch (const std::exception&) {
        return default_value;
    }
}

double StringUtils::toDouble(const std::string& str, double default_value) {
    try {
        return std::stod(str);
    } catch (const std::exception&) {
        return default_value;
    }
}

bool StringUtils::toBool(const std::string& str, bool default_value) {
    std::string lower = toLower(str);
    
    if (lower == "true" || lower == "yes" || lower == "1" || lower == "on") {
        return true;
    } else if (lower == "false" || lower == "no" || lower == "0" || lower == "off") {
        return false;
    }
    
    return default_value;
}

std::string StringUtils::toString(int value) {
    return std::to_string(value);
}

std::string StringUtils::toString(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

std::string StringUtils::toString(bool value) {
    return value ? "true" : "false";
}

std::string StringUtils::toHex(const std::string& str) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    for (unsigned char c : str) {
        ss << std::setw(2) << static_cast<int>(c);
    }
    
    return ss.str();
}

std::string StringUtils::fromHex(const std::string& hex) {
    if (hex.length() % 2 != 0) {
        return "";
    }
    
    std::string result;
    result.reserve(hex.length() / 2);
    
    for (size_t i = 0; i < hex.length(); i += 2) {
        try {
            std::string byte = hex.substr(i, 2);
            char c = static_cast<char>(std::stoi(byte, nullptr, 16));
            result.push_back(c);
        } catch (const std::exception&) {
            return "";
        }
    }
    
    return result;
}

std::string StringUtils::randomString(int length, bool include_digits, bool include_lowercase,
                                    bool include_uppercase, bool include_special) {
    static const std::string digits = "0123456789";
    static const std::string lowercase = "abcdefghijklmnopqrstuvwxyz";
    static const std::string uppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const std::string special = "!@#$%^&*()-_=+[]{}|;:,.<>?";
    
    std::string charset;
    if (include_digits) charset += digits;
    if (include_lowercase) charset += lowercase;
    if (include_uppercase) charset += uppercase;
    if (include_special) charset += special;
    
    if (charset.empty() || length <= 0) {
        return "";
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, charset.length() - 1);
    
    std::string result;
    result.reserve(length);
    
    for (int i = 0; i < length; ++i) {
        result += charset[dis(gen)];
    }
    
    return result;
}

} // namespace utils
} // namespace cam_server
