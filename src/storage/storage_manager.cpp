#include "storage/storage_manager.h"
#include "storage/file_manager.h"
#include "utils/file_utils.h"
#include "utils/string_utils.h"
#include "monitor/logger.h"

#include <algorithm>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <sys/statvfs.h>

namespace fs = std::filesystem;

namespace cam_server {
namespace storage {

// 单例实例
StorageManager& StorageManager::getInstance() {
    static StorageManager instance;
    return instance;
}

StorageManager::StorageManager()
    : is_initialized_(false) {
}

bool StorageManager::initialize(const StorageConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    
    // 创建目录结构
    if (!createDirectories()) {
        LOG_ERROR("无法创建目录结构", "StorageManager");
        return false;
    }
    
    // 检查目录权限
    if (!checkDirectoryPermissions()) {
        LOG_ERROR("目录权限检查失败", "StorageManager");
        return false;
    }
    
    // 检查存储空间
    StorageInfo storage_info = getStorageInfo();
    if (storage_info.available_space < config_.min_free_space) {
        LOG_WARNING("可用存储空间不足: " + std::to_string(storage_info.available_space) + " 字节", "StorageManager");
    }
    
    // 记录上次清理时间
    last_cleanup_time_ = std::chrono::system_clock::now();
    
    is_initialized_ = true;
    LOG_INFO("存储管理器初始化成功", "StorageManager");
    return true;
}

StorageInfo StorageManager::getStorageInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    StorageInfo info;
    
    // 获取视频目录的存储信息
    struct statvfs stat;
    if (statvfs(config_.video_dir.c_str(), &stat) != 0) {
        LOG_ERROR("无法获取存储信息: " + config_.video_dir, "StorageManager");
        return info;
    }
    
    // 计算存储信息
    info.total_space = static_cast<int64_t>(stat.f_blocks) * stat.f_frsize;
    info.available_space = static_cast<int64_t>(stat.f_bavail) * stat.f_frsize;
    info.used_space = info.total_space - info.available_space;
    info.usage_ratio = static_cast<double>(info.used_space) / info.total_space;
    info.mount_point = config_.video_dir;
    info.filesystem_type = "unknown";  // 获取文件系统类型需要更复杂的代码
    
    return info;
}

bool StorageManager::hasEnoughSpace(int64_t required_space) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    StorageInfo info = getStorageInfo();
    return info.available_space >= required_space;
}

std::string StorageManager::getVideoDir() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.video_dir;
}

std::string StorageManager::getImageDir() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.image_dir;
}

std::string StorageManager::getArchiveDir() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.archive_dir;
}

std::string StorageManager::getTempDir() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.temp_dir;
}

std::string StorageManager::createVideoPath(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_initialized_) {
        LOG_ERROR("存储管理器未初始化", "StorageManager");
        return "";
    }
    
    if (!filename.empty()) {
        return utils::FileUtils::joinPath(config_.video_dir, filename);
    } else {
        return utils::FileUtils::joinPath(config_.video_dir, generateTimestampFilename("video", ".mp4"));
    }
}

std::string StorageManager::createImagePath(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_initialized_) {
        LOG_ERROR("存储管理器未初始化", "StorageManager");
        return "";
    }
    
    if (!filename.empty()) {
        return utils::FileUtils::joinPath(config_.image_dir, filename);
    } else {
        return utils::FileUtils::joinPath(config_.image_dir, generateTimestampFilename("image", ".jpg"));
    }
}

std::string StorageManager::createArchivePath(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_initialized_) {
        LOG_ERROR("存储管理器未初始化", "StorageManager");
        return "";
    }
    
    if (!filename.empty()) {
        return utils::FileUtils::joinPath(config_.archive_dir, filename);
    } else {
        return utils::FileUtils::joinPath(config_.archive_dir, generateTimestampFilename("archive", ".zip"));
    }
}

std::string StorageManager::createTempPath(const std::string& prefix) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_initialized_) {
        LOG_ERROR("存储管理器未初始化", "StorageManager");
        return "";
    }
    
    std::string temp_prefix = prefix.empty() ? "temp" : prefix;
    return utils::FileUtils::joinPath(config_.temp_dir, generateTimestampFilename(temp_prefix, ".tmp"));
}

int StorageManager::autoCleanup(bool force) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_initialized_) {
        LOG_ERROR("存储管理器未初始化", "StorageManager");
        return 0;
    }
    
    // 检查是否需要清理
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::hours>(now - last_cleanup_time_).count();
    
    StorageInfo info = getStorageInfo();
    bool need_cleanup = force || 
                       elapsed >= 24 || 
                       info.usage_ratio >= config_.auto_cleanup_threshold;
    
    if (!need_cleanup) {
        return 0;
    }
    
    LOG_INFO("开始自动清理存储空间", "StorageManager");
    
    // 清理视频目录
    int video_count = cleanupOldFiles(config_.video_dir, config_.auto_cleanup_keep_days);
    
    // 清理图像目录
    int image_count = cleanupOldFiles(config_.image_dir, config_.auto_cleanup_keep_days);
    
    // 清理临时目录（保留1天）
    int temp_count = cleanupOldFiles(config_.temp_dir, 1);
    
    // 更新上次清理时间
    last_cleanup_time_ = now;
    
    int total_count = video_count + image_count + temp_count;
    
    // 调用回调函数
    if (cleanup_callback_ && total_count > 0) {
        // 计算释放的空间
        StorageInfo new_info = getStorageInfo();
        int64_t freed_space = new_info.available_space - info.available_space;
        cleanup_callback_(total_count, freed_space);
    }
    
    LOG_INFO("自动清理完成，共清理 " + std::to_string(total_count) + " 个文件", "StorageManager");
    return total_count;
}

void StorageManager::setCleanupCallback(std::function<void(int, int64_t)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    cleanup_callback_ = callback;
}

StorageConfig StorageManager::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

bool StorageManager::updateConfig(const StorageConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 保存旧配置
    StorageConfig old_config = config_;
    
    // 更新配置
    config_ = config;
    
    // 如果目录发生变化，需要创建新目录
    if (old_config.video_dir != config_.video_dir ||
        old_config.image_dir != config_.image_dir ||
        old_config.archive_dir != config_.archive_dir ||
        old_config.temp_dir != config_.temp_dir) {
        
        if (!createDirectories()) {
            // 恢复旧配置
            config_ = old_config;
            LOG_ERROR("无法创建目录结构，配置更新失败", "StorageManager");
            return false;
        }
    }
    
    LOG_INFO("存储配置已更新", "StorageManager");
    return true;
}

bool StorageManager::createDirectories() {
    // 创建视频目录
    if (!utils::FileUtils::directoryExists(config_.video_dir)) {
        if (!utils::FileUtils::createDirectory(config_.video_dir, true)) {
            LOG_ERROR("无法创建视频目录: " + config_.video_dir, "StorageManager");
            return false;
        }
    }
    
    // 创建图像目录
    if (!utils::FileUtils::directoryExists(config_.image_dir)) {
        if (!utils::FileUtils::createDirectory(config_.image_dir, true)) {
            LOG_ERROR("无法创建图像目录: " + config_.image_dir, "StorageManager");
            return false;
        }
    }
    
    // 创建归档目录
    if (!utils::FileUtils::directoryExists(config_.archive_dir)) {
        if (!utils::FileUtils::createDirectory(config_.archive_dir, true)) {
            LOG_ERROR("无法创建归档目录: " + config_.archive_dir, "StorageManager");
            return false;
        }
    }
    
    // 创建临时目录
    if (!utils::FileUtils::directoryExists(config_.temp_dir)) {
        if (!utils::FileUtils::createDirectory(config_.temp_dir, true)) {
            LOG_ERROR("无法创建临时目录: " + config_.temp_dir, "StorageManager");
            return false;
        }
    }
    
    return true;
}

bool StorageManager::checkDirectoryPermissions() {
    // 检查视频目录权限
    if (!utils::FileUtils::directoryExists(config_.video_dir)) {
        LOG_ERROR("视频目录不存在: " + config_.video_dir, "StorageManager");
        return false;
    }
    
    // 检查图像目录权限
    if (!utils::FileUtils::directoryExists(config_.image_dir)) {
        LOG_ERROR("图像目录不存在: " + config_.image_dir, "StorageManager");
        return false;
    }
    
    // 检查归档目录权限
    if (!utils::FileUtils::directoryExists(config_.archive_dir)) {
        LOG_ERROR("归档目录不存在: " + config_.archive_dir, "StorageManager");
        return false;
    }
    
    // 检查临时目录权限
    if (!utils::FileUtils::directoryExists(config_.temp_dir)) {
        LOG_ERROR("临时目录不存在: " + config_.temp_dir, "StorageManager");
        return false;
    }
    
    // 测试写入权限
    std::string test_file = utils::FileUtils::joinPath(config_.temp_dir, "test_write_permission.tmp");
    if (!utils::FileUtils::writeFile(test_file, "test")) {
        LOG_ERROR("无法写入临时文件，权限不足", "StorageManager");
        return false;
    }
    
    // 删除测试文件
    utils::FileUtils::deleteFile(test_file);
    
    return true;
}

std::string StorageManager::generateTimestampFilename(const std::string& prefix, const std::string& extension) const {
    // 获取当前时间
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    localtime_r(&time_t_now, &tm_now);
    
    // 格式化时间戳
    std::stringstream ss;
    ss << prefix << "_"
       << std::setfill('0') << std::setw(4) << (tm_now.tm_year + 1900)
       << std::setfill('0') << std::setw(2) << (tm_now.tm_mon + 1)
       << std::setfill('0') << std::setw(2) << tm_now.tm_mday
       << "_"
       << std::setfill('0') << std::setw(2) << tm_now.tm_hour
       << std::setfill('0') << std::setw(2) << tm_now.tm_min
       << std::setfill('0') << std::setw(2) << tm_now.tm_sec;
    
    // 添加毫秒
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    
    // 添加扩展名
    ss << extension;
    
    return ss.str();
}

int StorageManager::cleanupOldFiles(const std::string& dir_path, int keep_days) {
    if (!utils::FileUtils::directoryExists(dir_path)) {
        LOG_ERROR("目录不存在: " + dir_path, "StorageManager");
        return 0;
    }
    
    // 计算截止时间
    auto now = std::chrono::system_clock::now();
    auto cutoff_time = now - std::chrono::hours(24 * keep_days);
    
    int count = 0;
    
    try {
        // 遍历目录
        for (const auto& entry : fs::directory_iterator(dir_path)) {
            if (entry.is_regular_file()) {
                // 获取文件修改时间
                auto file_time = entry.last_write_time();
                
                // 如果文件修改时间早于截止时间，则删除
                if (file_time < cutoff_time) {
                    if (utils::FileUtils::deleteFile(entry.path().string())) {
                        count++;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("清理文件失败: " + std::string(e.what()), "StorageManager");
    }
    
    return count;
}

} // namespace storage
} // namespace cam_server
