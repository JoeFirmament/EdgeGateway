#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <chrono>

namespace cam_server {
namespace storage {

/**
 * @brief 存储信息结构体
 */
struct StorageInfo {
    // 总空间（字节）
    int64_t total_space;
    // 可用空间（字节）
    int64_t available_space;
    // 已用空间（字节）
    int64_t used_space;
    // 使用率（0.0-1.0）
    double usage_ratio;
    // 挂载点
    std::string mount_point;
    // 文件系统类型
    std::string filesystem_type;
};

/**
 * @brief 存储配置结构体
 */
struct StorageConfig {
    // 视频存储目录
    std::string video_dir;
    // 图像存储目录
    std::string image_dir;
    // 归档存储目录
    std::string archive_dir;
    // 临时目录
    std::string temp_dir;
    // 最小可用空间（字节）
    int64_t min_free_space;
    // 自动清理阈值（0.0-1.0）
    double auto_cleanup_threshold;
    // 自动清理保留天数
    int auto_cleanup_keep_days;
};

/**
 * @brief 存储管理器类
 */
class StorageManager {
public:
    /**
     * @brief 获取StorageManager单例
     * @return StorageManager单例的引用
     */
    static StorageManager& getInstance();

    /**
     * @brief 初始化存储管理器
     * @param config 存储配置
     * @return 是否初始化成功
     */
    bool initialize(const StorageConfig& config);

    /**
     * @brief 获取存储信息
     * @return 存储信息
     */
    StorageInfo getStorageInfo() const;

    /**
     * @brief 检查存储空间是否足够
     * @param required_space 所需空间（字节）
     * @return 空间是否足够
     */
    bool hasEnoughSpace(int64_t required_space) const;

    /**
     * @brief 获取视频存储目录
     * @return 视频存储目录
     */
    std::string getVideoDir() const;

    /**
     * @brief 获取图像存储目录
     * @return 图像存储目录
     */
    std::string getImageDir() const;

    /**
     * @brief 获取归档存储目录
     * @return 归档存储目录
     */
    std::string getArchiveDir() const;

    /**
     * @brief 获取临时目录
     * @return 临时目录
     */
    std::string getTempDir() const;

    /**
     * @brief 创建视频文件路径
     * @param filename 文件名（可选）
     * @return 完整文件路径
     */
    std::string createVideoPath(const std::string& filename = "") const;

    /**
     * @brief 创建图像文件路径
     * @param filename 文件名（可选）
     * @return 完整文件路径
     */
    std::string createImagePath(const std::string& filename = "") const;

    /**
     * @brief 创建归档文件路径
     * @param filename 文件名（可选）
     * @return 完整文件路径
     */
    std::string createArchivePath(const std::string& filename = "") const;

    /**
     * @brief 创建临时文件路径
     * @param prefix 前缀（可选）
     * @return 完整文件路径
     */
    std::string createTempPath(const std::string& prefix = "") const;

    /**
     * @brief 自动清理旧文件
     * @param force 是否强制清理
     * @return 清理的文件数量
     */
    int autoCleanup(bool force = false);

    /**
     * @brief 设置自动清理回调函数
     * @param callback 回调函数
     */
    void setCleanupCallback(std::function<void(int, int64_t)> callback);

    /**
     * @brief 获取存储配置
     * @return 存储配置
     */
    StorageConfig getConfig() const;

    /**
     * @brief 更新存储配置
     * @param config 存储配置
     * @return 是否成功更新
     */
    bool updateConfig(const StorageConfig& config);

private:
    // 私有构造函数，防止外部创建实例
    StorageManager();
    // 禁止拷贝构造和赋值操作
    StorageManager(const StorageManager&) = delete;
    StorageManager& operator=(const StorageManager&) = delete;

    // 创建目录结构
    bool createDirectories();
    // 检查目录权限
    bool checkDirectoryPermissions();
    // 生成时间戳文件名
    std::string generateTimestampFilename(const std::string& prefix, const std::string& extension) const;
    // 清理指定目录中的旧文件
    int cleanupOldFiles(const std::string& dir_path, int keep_days);

    // 存储配置
    StorageConfig config_;
    // 互斥锁
    mutable std::mutex mutex_;
    // 是否已初始化
    bool is_initialized_;
    // 自动清理回调函数
    std::function<void(int, int64_t)> cleanup_callback_;
    // 上次清理时间
    std::chrono::system_clock::time_point last_cleanup_time_;
};

} // namespace storage
} // namespace cam_server

#endif // STORAGE_MANAGER_H
