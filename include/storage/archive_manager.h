#ifndef ARCHIVE_MANAGER_H
#define ARCHIVE_MANAGER_H

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <future>

namespace cam_server {
namespace storage {

/**
 * @brief 归档任务状态枚举
 */
enum class ArchiveTaskState {
    PENDING,    // 等待中
    RUNNING,    // 运行中
    COMPLETED,  // 已完成
    CANCELLED,  // 已取消
    ERROR       // 错误
};

/**
 * @brief 归档任务状态结构体
 */
struct ArchiveTaskStatus {
    // 任务ID
    std::string task_id;
    // 任务状态
    ArchiveTaskState state;
    // 进度（0.0-1.0）
    double progress;
    // 已处理文件数
    int processed_files;
    // 总文件数
    int total_files;
    // 归档文件大小（字节）
    int64_t archive_size;
    // 开始时间
    int64_t start_time;
    // 结束时间（如果已完成）
    int64_t end_time;
    // 错误信息（如果状态为ERROR）
    std::string error_message;
    // 输出文件路径
    std::string output_path;
};

/**
 * @brief 归档配置结构体
 */
struct ArchiveConfig {
    // 源目录或文件路径
    std::string source_path;
    // 输出文件路径
    std::string output_path;
    // 归档格式（如zip, tar.gz等）
    std::string format;
    // 压缩级别（0-9）
    int compression_level;
    // 是否包含子目录
    bool include_subdirs;
    // 文件过滤器（正则表达式）
    std::string file_filter;
    // 是否保留目录结构
    bool preserve_dir_structure;
    // 是否在完成后删除源文件
    bool delete_source_after_archive;
};

/**
 * @brief 归档管理器类
 */
class ArchiveManager {
public:
    /**
     * @brief 获取ArchiveManager单例
     * @return ArchiveManager单例的引用
     */
    static ArchiveManager& getInstance();

    /**
     * @brief 初始化归档管理器
     * @return 是否初始化成功
     */
    bool initialize();

    /**
     * @brief 创建归档任务
     * @param config 归档配置
     * @return 任务ID，空字符串表示创建失败
     */
    std::string createTask(const ArchiveConfig& config);

    /**
     * @brief 开始归档任务
     * @param task_id 任务ID
     * @return 是否成功开始任务
     */
    bool startTask(const std::string& task_id);

    /**
     * @brief 取消归档任务
     * @param task_id 任务ID
     * @return 是否成功取消任务
     */
    bool cancelTask(const std::string& task_id);

    /**
     * @brief 获取任务状态
     * @param task_id 任务ID
     * @return 任务状态
     */
    ArchiveTaskStatus getTaskStatus(const std::string& task_id) const;

    /**
     * @brief 获取所有任务状态
     * @return 所有任务状态列表
     */
    std::vector<ArchiveTaskStatus> getAllTaskStatus() const;

    /**
     * @brief 设置任务状态回调函数
     * @param callback 状态回调函数
     */
    void setStatusCallback(std::function<void(const ArchiveTaskStatus&)> callback);

    /**
     * @brief 清理已完成的任务
     * @param keep_last_n 保留最近的n个任务，0表示清理所有已完成任务
     * @return 清理的任务数量
     */
    int cleanupCompletedTasks(int keep_last_n = 0);

    /**
     * @brief 解压归档文件
     * @param archive_path 归档文件路径
     * @param output_dir 输出目录
     * @param password 密码（如果有）
     * @return 是否成功解压
     */
    bool extractArchive(const std::string& archive_path, const std::string& output_dir,
                       const std::string& password = "");

private:
    // 私有构造函数，防止外部创建实例
    ArchiveManager();
    // 禁止拷贝构造和赋值操作
    ArchiveManager(const ArchiveManager&) = delete;
    ArchiveManager& operator=(const ArchiveManager&) = delete;

    // 归档任务结构体
    struct ArchiveTask {
        // 任务ID
        std::string task_id;
        // 归档配置
        ArchiveConfig config;
        // 任务状态
        ArchiveTaskStatus status;
        // 任务线程
        std::thread thread;
        // 取消标志
        std::atomic<bool> cancel_flag;
        // 任务promise
        std::promise<void> promise;
        // 任务future
        std::future<void> future;
    };

    // 执行归档任务
    void executeTask(std::shared_ptr<ArchiveTask> task);
    // 更新任务状态
    void updateTaskStatus(const std::string& task_id, const ArchiveTaskStatus& status);
    // 生成唯一任务ID
    std::string generateTaskId() const;
    // 创建ZIP归档
    bool createZipArchive(const ArchiveTask& task);
    // 创建TAR归档
    bool createTarArchive(const ArchiveTask& task);
    // 创建7Z归档
    bool create7zArchive(const ArchiveTask& task);

    // 任务列表
    std::vector<std::shared_ptr<ArchiveTask>> tasks_;
    // 任务列表互斥锁
    mutable std::mutex tasks_mutex_;
    // 状态回调函数
    std::function<void(const ArchiveTaskStatus&)> status_callback_;
    // 是否已初始化
    bool is_initialized_;
};

} // namespace storage
} // namespace cam_server

#endif // ARCHIVE_MANAGER_H
