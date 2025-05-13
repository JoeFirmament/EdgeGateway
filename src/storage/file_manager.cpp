#include "storage/file_manager.h"
#include "utils/file_utils.h"
#include "utils/string_utils.h"
#include "monitor/logger.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

// 辅助函数：将std::filesystem::file_time_type转换为std::chrono::system_clock::time_point
std::chrono::system_clock::time_point fileTimeToSystemTime(const std::filesystem::file_time_type& file_time) {
    // 将file_time转换为time_t
    auto file_time_since_epoch = file_time.time_since_epoch();
    auto file_time_sec = std::chrono::duration_cast<std::chrono::seconds>(file_time_since_epoch);

    // 创建system_clock时间点
    std::chrono::system_clock::time_point system_time = std::chrono::system_clock::from_time_t(file_time_sec.count());

    return system_time;
}

namespace cam_server {
namespace storage {

// 单例实例
FileManager& FileManager::getInstance() {
    static FileManager instance;
    return instance;
}

FileManager::FileManager()
    : is_initialized_(false) {
}

bool FileManager::initialize(const std::string& base_dir) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 规范化基础目录路径
    base_dir_ = normalizePath(base_dir);

    // 确保基础目录存在
    if (!utils::FileUtils::directoryExists(base_dir_)) {
        if (!utils::FileUtils::createDirectory(base_dir_, true)) {
            LOG_ERROR("无法创建基础目录: " + base_dir_, "FileManager");
            return false;
        }
    }

    is_initialized_ = true;
    LOG_INFO("文件管理器初始化成功，基础目录: " + base_dir_, "FileManager");
    return true;
}

std::vector<FileInfo> FileManager::getFileList(const std::string& dir_path, bool recursive,
                                             FileType filter) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_initialized_) {
        LOG_ERROR("文件管理器未初始化", "FileManager");
        return {};
    }

    // 规范化目录路径
    std::string path = normalizePath(dir_path);

    // 检查目录是否存在
    if (!utils::FileUtils::directoryExists(path)) {
        LOG_ERROR("目录不存在: " + path, "FileManager");
        return {};
    }

    std::vector<FileInfo> files;

    try {
        // 遍历目录
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                // 获取文件信息
                FileInfo file_info;
                file_info.name = entry.path().filename().string();
                file_info.path = entry.path().string();
                file_info.size = entry.file_size();
                file_info.create_time = fileTimeToSystemTime(entry.last_write_time());
                file_info.modify_time = fileTimeToSystemTime(entry.last_write_time());
                file_info.extension = entry.path().extension().string();

                // 确定文件类型
                file_info.type = getFileType(file_info.path);

                // 获取额外信息
                file_info.extra_info = getExtraInfo(file_info.path, file_info.type);

                // 应用过滤器
                if (filter == FileType::OTHER || file_info.type == filter) {
                    files.push_back(file_info);
                }
            } else if (recursive && entry.is_directory()) {
                // 递归获取子目录中的文件
                std::vector<FileInfo> sub_files = getFileList(entry.path().string(), true, filter);
                files.insert(files.end(), sub_files.begin(), sub_files.end());
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("获取文件列表失败: " + std::string(e.what()), "FileManager");
    }

    return files;
}

std::vector<DirectoryInfo> FileManager::getDirectoryList(const std::string& dir_path, bool recursive) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_initialized_) {
        LOG_ERROR("文件管理器未初始化", "FileManager");
        return {};
    }

    // 规范化目录路径
    std::string path = normalizePath(dir_path);

    // 检查目录是否存在
    if (!utils::FileUtils::directoryExists(path)) {
        LOG_ERROR("目录不存在: " + path, "FileManager");
        return {};
    }

    std::vector<DirectoryInfo> directories;

    try {
        // 遍历目录
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_directory()) {
                // 获取目录信息
                DirectoryInfo dir_info;
                dir_info.name = entry.path().filename().string();
                dir_info.path = entry.path().string();
                dir_info.create_time = fileTimeToSystemTime(entry.last_write_time());
                dir_info.modify_time = fileTimeToSystemTime(entry.last_write_time());

                // 统计文件和子目录数量
                int file_count = 0;
                int dir_count = 0;
                int64_t total_size = 0;

                for (const auto& sub_entry : fs::directory_iterator(entry.path())) {
                    if (sub_entry.is_regular_file()) {
                        file_count++;
                        total_size += sub_entry.file_size();
                    } else if (sub_entry.is_directory()) {
                        dir_count++;
                    }
                }

                dir_info.file_count = file_count;
                dir_info.dir_count = dir_count;
                dir_info.total_size = total_size;

                directories.push_back(dir_info);

                // 递归获取子目录
                if (recursive) {
                    std::vector<DirectoryInfo> sub_dirs = getDirectoryList(entry.path().string(), true);
                    directories.insert(directories.end(), sub_dirs.begin(), sub_dirs.end());
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("获取目录列表失败: " + std::string(e.what()), "FileManager");
    }

    return directories;
}

bool FileManager::createDirectory(const std::string& dir_path, bool recursive) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_initialized_) {
        LOG_ERROR("文件管理器未初始化", "FileManager");
        return false;
    }

    // 规范化目录路径
    std::string path = normalizePath(dir_path);

    // 创建目录
    if (!utils::FileUtils::createDirectory(path, recursive)) {
        LOG_ERROR("创建目录失败: " + path, "FileManager");
        return false;
    }

    LOG_INFO("创建目录成功: " + path, "FileManager");
    return true;
}

bool FileManager::deleteFile(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_initialized_) {
        LOG_ERROR("文件管理器未初始化", "FileManager");
        return false;
    }

    // 规范化文件路径
    std::string path = normalizePath(file_path);

    // 检查文件是否存在
    if (!utils::FileUtils::fileExists(path)) {
        LOG_ERROR("文件不存在: " + path, "FileManager");
        return false;
    }

    // 删除文件
    if (!utils::FileUtils::deleteFile(path)) {
        LOG_ERROR("删除文件失败: " + path, "FileManager");
        return false;
    }

    LOG_INFO("删除文件成功: " + path, "FileManager");
    return true;
}

bool FileManager::deleteDirectory(const std::string& dir_path, bool recursive) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_initialized_) {
        LOG_ERROR("文件管理器未初始化", "FileManager");
        return false;
    }

    // 规范化目录路径
    std::string path = normalizePath(dir_path);

    // 检查目录是否存在
    if (!utils::FileUtils::directoryExists(path)) {
        LOG_ERROR("目录不存在: " + path, "FileManager");
        return false;
    }

    // 删除目录
    if (!utils::FileUtils::deleteDirectory(path, recursive)) {
        LOG_ERROR("删除目录失败: " + path, "FileManager");
        return false;
    }

    LOG_INFO("删除目录成功: " + path, "FileManager");
    return true;
}

bool FileManager::rename(const std::string& old_path, const std::string& new_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_initialized_) {
        LOG_ERROR("文件管理器未初始化", "FileManager");
        return false;
    }

    // 规范化路径
    std::string old_normalized = normalizePath(old_path);
    std::string new_normalized = normalizePath(new_path);

    // 检查源路径是否存在
    if (!utils::FileUtils::fileExists(old_normalized) && !utils::FileUtils::directoryExists(old_normalized)) {
        LOG_ERROR("源路径不存在: " + old_normalized, "FileManager");
        return false;
    }

    // 检查目标路径是否已存在
    if (utils::FileUtils::fileExists(new_normalized) || utils::FileUtils::directoryExists(new_normalized)) {
        LOG_ERROR("目标路径已存在: " + new_normalized, "FileManager");
        return false;
    }

    // 重命名
    if (!utils::FileUtils::rename(old_normalized, new_normalized)) {
        LOG_ERROR("重命名失败: " + old_normalized + " -> " + new_normalized, "FileManager");
        return false;
    }

    LOG_INFO("重命名成功: " + old_normalized + " -> " + new_normalized, "FileManager");
    return true;
}

FileInfo FileManager::getFileInfo(const std::string& file_path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_initialized_) {
        LOG_ERROR("文件管理器未初始化", "FileManager");
        return FileInfo();
    }

    // 规范化文件路径
    std::string path = normalizePath(file_path);

    // 检查文件是否存在
    if (!utils::FileUtils::fileExists(path)) {
        LOG_ERROR("文件不存在: " + path, "FileManager");
        return FileInfo();
    }

    try {
        // 获取文件信息
        fs::path fs_path(path);
        FileInfo file_info;
        file_info.name = fs_path.filename().string();
        file_info.path = path;
        file_info.size = fs::file_size(fs_path);
        file_info.create_time = fileTimeToSystemTime(fs::last_write_time(fs_path));
        file_info.modify_time = fileTimeToSystemTime(fs::last_write_time(fs_path));
        file_info.extension = fs_path.extension().string();

        // 确定文件类型
        file_info.type = getFileType(path);

        // 获取额外信息
        file_info.extra_info = getExtraInfo(path, file_info.type);

        return file_info;
    } catch (const std::exception& e) {
        LOG_ERROR("获取文件信息失败: " + std::string(e.what()), "FileManager");
        return FileInfo();
    }
}

DirectoryInfo FileManager::getDirectoryInfo(const std::string& dir_path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_initialized_) {
        LOG_ERROR("文件管理器未初始化", "FileManager");
        return DirectoryInfo();
    }

    // 规范化目录路径
    std::string path = normalizePath(dir_path);

    // 检查目录是否存在
    if (!utils::FileUtils::directoryExists(path)) {
        LOG_ERROR("目录不存在: " + path, "FileManager");
        return DirectoryInfo();
    }

    try {
        // 获取目录信息
        fs::path fs_path(path);
        DirectoryInfo dir_info;
        dir_info.name = fs_path.filename().string();
        dir_info.path = path;
        dir_info.create_time = fileTimeToSystemTime(fs::last_write_time(fs_path));
        dir_info.modify_time = fileTimeToSystemTime(fs::last_write_time(fs_path));

        // 统计文件和子目录数量
        int file_count = 0;
        int dir_count = 0;
        int64_t total_size = 0;

        for (const auto& entry : fs::directory_iterator(fs_path)) {
            if (entry.is_regular_file()) {
                file_count++;
                total_size += entry.file_size();
            } else if (entry.is_directory()) {
                dir_count++;
            }
        }

        dir_info.file_count = file_count;
        dir_info.dir_count = dir_count;
        dir_info.total_size = total_size;

        return dir_info;
    } catch (const std::exception& e) {
        LOG_ERROR("获取目录信息失败: " + std::string(e.what()), "FileManager");
        return DirectoryInfo();
    }
}

bool FileManager::fileExists(const std::string& file_path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_initialized_) {
        LOG_ERROR("文件管理器未初始化", "FileManager");
        return false;
    }

    // 规范化文件路径
    std::string path = normalizePath(file_path);

    return utils::FileUtils::fileExists(path);
}

bool FileManager::directoryExists(const std::string& dir_path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_initialized_) {
        LOG_ERROR("文件管理器未初始化", "FileManager");
        return false;
    }

    // 规范化目录路径
    std::string path = normalizePath(dir_path);

    return utils::FileUtils::directoryExists(path);
}

std::string FileManager::readFile(const std::string& file_path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_initialized_) {
        LOG_ERROR("文件管理器未初始化", "FileManager");
        return "";
    }

    // 规范化文件路径
    std::string path = normalizePath(file_path);

    // 检查文件是否存在
    if (!utils::FileUtils::fileExists(path)) {
        LOG_ERROR("文件不存在: " + path, "FileManager");
        return "";
    }

    return utils::FileUtils::readFile(path);
}

bool FileManager::writeFile(const std::string& file_path, const std::string& content, bool append) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_initialized_) {
        LOG_ERROR("文件管理器未初始化", "FileManager");
        return false;
    }

    // 规范化文件路径
    std::string path = normalizePath(file_path);

    // 确保目录存在
    std::string dir_path = utils::FileUtils::getDirectoryPath(path);
    if (!utils::FileUtils::directoryExists(dir_path)) {
        if (!utils::FileUtils::createDirectory(dir_path, true)) {
            LOG_ERROR("创建目录失败: " + dir_path, "FileManager");
            return false;
        }
    }

    return utils::FileUtils::writeFile(path, content, append);
}

std::string FileManager::getBaseDir() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return base_dir_;
}

std::string FileManager::normalizePath(const std::string& path) const {
    // 如果路径是相对路径，则相对于基础目录
    if (!path.empty() && path[0] != '/') {
        return utils::FileUtils::joinPath(base_dir_, path);
    }

    return utils::FileUtils::normalizePath(path);
}

FileType FileManager::getFileType(const std::string& file_path) const {
    // 获取文件扩展名
    std::string ext = getFileExtension(file_path);

    // 转换为小写
    ext = utils::StringUtils::toLower(ext);

    // 根据扩展名判断文件类型
    if (ext == ".mp4" || ext == ".avi" || ext == ".mkv" || ext == ".mov" || ext == ".webm") {
        return FileType::VIDEO;
    } else if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".gif" || ext == ".bmp") {
        return FileType::IMAGE;
    } else if (ext == ".zip" || ext == ".tar" || ext == ".gz" || ext == ".7z" || ext == ".rar") {
        return FileType::ARCHIVE;
    } else {
        return FileType::OTHER;
    }
}

std::string FileManager::getFileExtension(const std::string& file_path) const {
    return utils::FileUtils::getFileExtension(file_path);
}

std::string FileManager::getExtraInfo(const std::string& file_path, FileType type) const {
    // 根据文件类型获取额外信息
    if (type == FileType::VIDEO) {
        // 使用FFmpeg获取视频信息
        std::string command = "ffprobe -v quiet -print_format json -show_format -show_streams \"" + file_path + "\" 2>/dev/null";
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            return "";
        }

        char buffer[128];
        std::string result = "";
        while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != nullptr) {
                result += buffer;
            }
        }
        pclose(pipe);

        // 解析JSON结果（简化处理，实际项目中应使用JSON库）
        std::string info = "";

        // 提取时长
        size_t duration_pos = result.find("\"duration\":");
        if (duration_pos != std::string::npos) {
            size_t start = result.find("\"", duration_pos + 11) + 1;
            size_t end = result.find("\"", start);
            if (start != std::string::npos && end != std::string::npos) {
                std::string duration = result.substr(start, end - start);
                info += "时长: " + duration + "秒, ";
            }
        }

        // 提取分辨率
        size_t width_pos = result.find("\"width\":");
        size_t height_pos = result.find("\"height\":");
        if (width_pos != std::string::npos && height_pos != std::string::npos) {
            size_t width_end = result.find(",", width_pos);
            size_t height_end = result.find(",", height_pos);
            if (width_end != std::string::npos && height_end != std::string::npos) {
                std::string width = result.substr(width_pos + 8, width_end - width_pos - 8);
                std::string height = result.substr(height_pos + 9, height_end - height_pos - 9);
                info += "分辨率: " + width + "x" + height;
            }
        }

        return info;
    } else if (type == FileType::IMAGE) {
        // 使用系统命令获取图像信息
        std::string command = "identify -format \"%wx%h\" \"" + file_path + "\" 2>/dev/null";
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            return "";
        }

        char buffer[128];
        std::string result = "";
        while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != nullptr) {
                result += buffer;
            }
        }
        pclose(pipe);

        if (!result.empty()) {
            return "分辨率: " + result;
        }
    }

    return "";
}

} // namespace storage
} // namespace cam_server
