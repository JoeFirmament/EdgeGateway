#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <string>
#include <vector>
#include <chrono>
#include <functional>

namespace cam_server {
namespace utils {

/**
 * @brief 文件工具类
 */
class FileUtils {
public:
    /**
     * @brief 检查文件是否存在
     * @param file_path 文件路径
     * @return 文件是否存在
     */
    static bool fileExists(const std::string& file_path);

    /**
     * @brief 检查目录是否存在
     * @param dir_path 目录路径
     * @return 目录是否存在
     */
    static bool directoryExists(const std::string& dir_path);

    /**
     * @brief 创建目录
     * @param dir_path 目录路径
     * @param recursive 是否递归创建父目录
     * @return 是否成功创建
     */
    static bool createDirectory(const std::string& dir_path, bool recursive = true);

    /**
     * @brief 删除文件
     * @param file_path 文件路径
     * @return 是否成功删除
     */
    static bool deleteFile(const std::string& file_path);

    /**
     * @brief 删除目录
     * @param dir_path 目录路径
     * @param recursive 是否递归删除子目录和文件
     * @return 是否成功删除
     */
    static bool deleteDirectory(const std::string& dir_path, bool recursive = false);

    /**
     * @brief 重命名文件或目录
     * @param old_path 原路径
     * @param new_path 新路径
     * @return 是否成功重命名
     */
    static bool rename(const std::string& old_path, const std::string& new_path);

    /**
     * @brief 获取文件大小
     * @param file_path 文件路径
     * @return 文件大小（字节）
     */
    static int64_t getFileSize(const std::string& file_path);

    /**
     * @brief 获取文件修改时间
     * @param file_path 文件路径
     * @return 文件修改时间
     */
    static std::chrono::system_clock::time_point getFileModifyTime(const std::string& file_path);

    /**
     * @brief 获取文件创建时间
     * @param file_path 文件路径
     * @return 文件创建时间
     */
    static std::chrono::system_clock::time_point getFileCreateTime(const std::string& file_path);

    /**
     * @brief 获取文件扩展名
     * @param file_path 文件路径
     * @return 文件扩展名
     */
    static std::string getFileExtension(const std::string& file_path);

    /**
     * @brief 获取文件名（不含路径）
     * @param file_path 文件路径
     * @return 文件名
     */
    static std::string getFileName(const std::string& file_path);

    /**
     * @brief 获取文件名（不含路径和扩展名）
     * @param file_path 文件路径
     * @return 文件名
     */
    static std::string getFileNameWithoutExtension(const std::string& file_path);

    /**
     * @brief 获取目录路径
     * @param file_path 文件路径
     * @return 目录路径
     */
    static std::string getDirectoryPath(const std::string& file_path);

    /**
     * @brief 获取绝对路径
     * @param path 路径
     * @return 绝对路径
     */
    static std::string getAbsolutePath(const std::string& path);

    /**
     * @brief 规范化路径
     * @param path 路径
     * @return 规范化后的路径
     */
    static std::string normalizePath(const std::string& path);

    /**
     * @brief 连接路径
     * @param path1 路径1
     * @param path2 路径2
     * @return 连接后的路径
     */
    static std::string joinPath(const std::string& path1, const std::string& path2);

    /**
     * @brief 获取文件列表
     * @param dir_path 目录路径
     * @param recursive 是否递归获取子目录
     * @return 文件路径列表
     */
    static std::vector<std::string> getFileList(const std::string& dir_path, bool recursive = false);

    /**
     * @brief 获取目录列表
     * @param dir_path 目录路径
     * @param recursive 是否递归获取子目录
     * @return 目录路径列表
     */
    static std::vector<std::string> getDirectoryList(const std::string& dir_path, bool recursive = false);

    /**
     * @brief 读取文件内容
     * @param file_path 文件路径
     * @return 文件内容
     */
    static std::string readFile(const std::string& file_path);

    /**
     * @brief 读取二进制文件内容
     * @param file_path 文件路径
     * @return 文件内容
     */
    static std::vector<uint8_t> readBinaryFile(const std::string& file_path);

    /**
     * @brief 写入文件内容
     * @param file_path 文件路径
     * @param content 文件内容
     * @param append 是否追加模式
     * @return 是否成功写入
     */
    static bool writeFile(const std::string& file_path, const std::string& content, bool append = false);

    /**
     * @brief 写入二进制文件内容
     * @param file_path 文件路径
     * @param content 文件内容
     * @param append 是否追加模式
     * @return 是否成功写入
     */
    static bool writeBinaryFile(const std::string& file_path, const std::vector<uint8_t>& content, bool append = false);

    /**
     * @brief 复制文件
     * @param src_path 源文件路径
     * @param dst_path 目标文件路径
     * @param overwrite 是否覆盖已存在的文件
     * @return 是否成功复制
     */
    static bool copyFile(const std::string& src_path, const std::string& dst_path, bool overwrite = true);

    /**
     * @brief 获取临时文件路径
     * @param prefix 前缀
     * @param suffix 后缀
     * @return 临时文件路径
     */
    static std::string getTempFilePath(const std::string& prefix = "", const std::string& suffix = "");

    /**
     * @brief 获取临时目录路径
     * @return 临时目录路径
     */
    static std::string getTempDirectoryPath();

    /**
     * @brief 获取当前工作目录
     * @return 当前工作目录
     */
    static std::string getCurrentWorkingDirectory();

    /**
     * @brief 设置当前工作目录
     * @param dir_path 目录路径
     * @return 是否成功设置
     */
    static bool setCurrentWorkingDirectory(const std::string& dir_path);

    /**
     * @brief 获取可用磁盘空间
     * @param path 路径
     * @return 可用空间（字节）
     */
    static int64_t getAvailableDiskSpace(const std::string& path);

    /**
     * @brief 获取总磁盘空间
     * @param path 路径
     * @return 总空间（字节）
     */
    static int64_t getTotalDiskSpace(const std::string& path);

    /**
     * @brief 遍历目录
     * @param dir_path 目录路径
     * @param callback 回调函数，参数为文件路径，返回值为是否继续遍历
     * @param recursive 是否递归遍历子目录
     */
    static void traverseDirectory(const std::string& dir_path,
                                std::function<bool(const std::string&)> callback,
                                bool recursive = false);
};

} // namespace utils
} // namespace cam_server

#endif // FILE_UTILS_H
