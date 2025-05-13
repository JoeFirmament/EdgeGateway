#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <chrono>

namespace cam_server {
namespace storage {

/**
 * @brief 文件类型枚举
 */
enum class FileType {
    VIDEO,      // 视频文件
    IMAGE,      // 图像文件
    ARCHIVE,    // 归档文件
    OTHER       // 其他文件
};

/**
 * @brief 文件信息结构体
 */
struct FileInfo {
    // 文件名
    std::string name;
    // 文件路径
    std::string path;
    // 文件大小（字节）
    int64_t size;
    // 创建时间
    std::chrono::system_clock::time_point create_time;
    // 修改时间
    std::chrono::system_clock::time_point modify_time;
    // 文件类型
    FileType type;
    // 文件扩展名
    std::string extension;
    // 额外信息（如视频时长、分辨率等）
    std::string extra_info;
};

/**
 * @brief 目录信息结构体
 */
struct DirectoryInfo {
    // 目录名
    std::string name;
    // 目录路径
    std::string path;
    // 创建时间
    std::chrono::system_clock::time_point create_time;
    // 修改时间
    std::chrono::system_clock::time_point modify_time;
    // 文件数量
    int file_count;
    // 子目录数量
    int dir_count;
    // 总大小（字节）
    int64_t total_size;
};

/**
 * @brief 文件管理器类
 */
class FileManager {
public:
    /**
     * @brief 获取FileManager单例
     * @return FileManager单例的引用
     */
    static FileManager& getInstance();

    /**
     * @brief 初始化文件管理器
     * @param base_dir 基础目录
     * @return 是否初始化成功
     */
    bool initialize(const std::string& base_dir);

    /**
     * @brief 获取文件列表
     * @param dir_path 目录路径
     * @param recursive 是否递归获取子目录
     * @param filter 文件类型过滤器
     * @return 文件信息列表
     */
    std::vector<FileInfo> getFileList(const std::string& dir_path, bool recursive = false,
                                     FileType filter = FileType::OTHER) const;

    /**
     * @brief 获取目录列表
     * @param dir_path 目录路径
     * @param recursive 是否递归获取子目录
     * @return 目录信息列表
     */
    std::vector<DirectoryInfo> getDirectoryList(const std::string& dir_path, bool recursive = false) const;

    /**
     * @brief 创建目录
     * @param dir_path 目录路径
     * @param recursive 是否递归创建父目录
     * @return 是否成功创建
     */
    bool createDirectory(const std::string& dir_path, bool recursive = true);

    /**
     * @brief 删除文件
     * @param file_path 文件路径
     * @return 是否成功删除
     */
    bool deleteFile(const std::string& file_path);

    /**
     * @brief 删除目录
     * @param dir_path 目录路径
     * @param recursive 是否递归删除子目录和文件
     * @return 是否成功删除
     */
    bool deleteDirectory(const std::string& dir_path, bool recursive = false);

    /**
     * @brief 重命名文件或目录
     * @param old_path 原路径
     * @param new_path 新路径
     * @return 是否成功重命名
     */
    bool rename(const std::string& old_path, const std::string& new_path);

    /**
     * @brief 获取文件信息
     * @param file_path 文件路径
     * @return 文件信息
     */
    FileInfo getFileInfo(const std::string& file_path) const;

    /**
     * @brief 获取目录信息
     * @param dir_path 目录路径
     * @return 目录信息
     */
    DirectoryInfo getDirectoryInfo(const std::string& dir_path) const;

    /**
     * @brief 检查文件是否存在
     * @param file_path 文件路径
     * @return 文件是否存在
     */
    bool fileExists(const std::string& file_path) const;

    /**
     * @brief 检查目录是否存在
     * @param dir_path 目录路径
     * @return 目录是否存在
     */
    bool directoryExists(const std::string& dir_path) const;

    /**
     * @brief 获取文件内容
     * @param file_path 文件路径
     * @return 文件内容
     */
    std::string readFile(const std::string& file_path) const;

    /**
     * @brief 写入文件内容
     * @param file_path 文件路径
     * @param content 文件内容
     * @param append 是否追加模式
     * @return 是否成功写入
     */
    bool writeFile(const std::string& file_path, const std::string& content, bool append = false);

    /**
     * @brief 获取基础目录
     * @return 基础目录
     */
    std::string getBaseDir() const;

private:
    // 私有构造函数，防止外部创建实例
    FileManager();
    // 禁止拷贝构造和赋值操作
    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;

    // 规范化路径
    std::string normalizePath(const std::string& path) const;
    // 获取文件类型
    FileType getFileType(const std::string& file_path) const;
    // 获取文件扩展名
    std::string getFileExtension(const std::string& file_path) const;
    // 获取额外信息
    std::string getExtraInfo(const std::string& file_path, FileType type) const;

    // 基础目录
    std::string base_dir_;
    // 互斥锁
    mutable std::mutex mutex_;
    // 是否已初始化
    bool is_initialized_;
};

} // namespace storage
} // namespace cam_server

#endif // FILE_MANAGER_H
