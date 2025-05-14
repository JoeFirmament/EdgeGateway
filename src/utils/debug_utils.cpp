#include "utils/debug_utils.h"

namespace cam_server {
namespace utils {

// 初始化静态成员变量
LogLevel DebugUtils::global_log_level_ = LogLevel::INFO;
std::map<std::string, LogLevel> DebugUtils::module_log_levels_;
std::mutex DebugUtils::mutex_;

} // namespace utils
} // namespace cam_server
