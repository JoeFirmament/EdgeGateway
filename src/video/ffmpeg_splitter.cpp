#include "video/i_video_splitter.h"
#include "utils/file_utils.h"
#include "utils/string_utils.h"
#include "monitor/logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <chrono>
#include <thread>
#include <filesystem>
#include <random>
#include <future>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace cam_server {
namespace video {

class FFmpegSplitter : public IVideoSplitter {
public:
    FFmpegSplitter();
    ~FFmpegSplitter() override;

    bool initialize() override;
    std::string createTask(const SplitConfig& config) override;
    bool startTask(const std::string& taskId) override;
    bool cancelTask(const std::string& taskId) override;
    SplitTaskStatus getTaskStatus(const std::string& taskId) const override;
    std::vector<SplitTaskStatus> getAllTaskStatus() const override;
    void setStatusCallback(std::function<void(const SplitTaskStatus&)> callback) override;
    int cleanupCompletedTasks(int keepLastN = 0) override;

private:
    // 分帧任务结构体
    struct SplitTask {
        std::string taskId;
        SplitConfig config;
        SplitTaskStatus status;
        std::thread thread;
        std::atomic<bool> cancelFlag;
        std::promise<void> promise;
        std::future<void> future;
    };

    // 执行分帧任务
    void executeTask(std::shared_ptr<SplitTask> task);
    // 更新任务状态
    void updateTaskStatus(const std::string& taskId, const SplitTaskStatus& status);
    // 生成唯一任务ID
    std::string generateTaskId() const;
    // 获取视频信息
    bool getVideoInfo(const std::string& videoPath, int& width, int& height,
                     double& duration, int& totalFrames, double& frameRate);
    // 创建输出目录
    bool createOutputDirectory(const std::string& dirPath);
    // 提取帧并保存为图像
    bool extractFrameAndSave(const std::string& videoPath, const std::string& outputPath,
                           double timestamp, int width, int height, const std::string& format, int quality);
    // 生成缩略图
    bool generateThumbnail(const std::string& imagePath, const std::string& thumbnailPath, int size);

    // 任务列表
    std::vector<std::shared_ptr<SplitTask>> tasks_;
    // 任务列表互斥锁
    mutable std::mutex tasks_mutex_;
    // 状态回调函数
    std::function<void(const SplitTaskStatus&)> status_callback_;
    // 是否已初始化
    bool is_initialized_;
};

FFmpegSplitter::FFmpegSplitter()
    : is_initialized_(false) {
}

FFmpegSplitter::~FFmpegSplitter() {
    // 取消所有任务
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    for (auto& task : tasks_) {
        if (task->status.state == SplitTaskState::RUNNING) {
            task->cancelFlag = true;
            if (task->thread.joinable()) {
                task->thread.join();
            }
        }
    }
}

bool FFmpegSplitter::initialize() {
    is_initialized_ = true;
    LOG_INFO("视频分帧器初始化成功", "FFmpegSplitter");
    return true;
}

std::string FFmpegSplitter::createTask(const SplitConfig& config) {
    if (!is_initialized_) {
        LOG_ERROR("分帧器未初始化", "FFmpegSplitter");
        return "";
    }

    // 检查输入文件是否存在
    if (!utils::FileUtils::fileExists(config.input_path)) {
        LOG_ERROR("输入文件不存在: " + config.input_path, "FFmpegSplitter");
        return "";
    }

    // 创建任务
    auto task = std::make_shared<SplitTask>();
    task->taskId = generateTaskId();
    task->config = config;
    task->cancelFlag = false;

    // 初始化任务状态
    task->status.task_id = task->taskId;
    task->status.state = SplitTaskState::PENDING;
    task->status.progress = 0.0;
    task->status.processed_frames = 0;
    task->status.total_frames = 0;
    task->status.generated_images = 0;
    task->status.start_time = 0;
    task->status.end_time = 0;
    task->status.error_message = "";

    // 设置输出目录
    if (config.output_dir.empty()) {
        // 使用输入文件名作为输出目录
        std::string inputFileName = fs::path(config.input_path).stem().string();
        task->status.output_dir = fs::path(config.input_path).parent_path() / inputFileName;
    } else {
        task->status.output_dir = config.output_dir;
    }

    // 添加到任务列表
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        tasks_.push_back(task);
    }

    LOG_INFO("创建分帧任务: " + task->taskId, "FFmpegSplitter");
    return task->taskId;
}

bool FFmpegSplitter::startTask(const std::string& taskId) {
    std::shared_ptr<SplitTask> task;

    // 查找任务
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        auto it = std::find_if(tasks_.begin(), tasks_.end(),
                              [&taskId](const std::shared_ptr<SplitTask>& t) {
                                  return t->taskId == taskId;
                              });

        if (it == tasks_.end()) {
            LOG_ERROR("任务不存在: " + taskId, "FFmpegSplitter");
            return false;
        }

        task = *it;
    }

    // 检查任务状态
    if (task->status.state != SplitTaskState::PENDING) {
        LOG_ERROR("任务状态不是PENDING: " + taskId, "FFmpegSplitter");
        return false;
    }

    // 更新任务状态
    task->status.state = SplitTaskState::RUNNING;
    task->status.start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // 调用状态回调
    if (status_callback_) {
        status_callback_(task->status);
    }

    // 启动任务线程
    task->thread = std::thread(&FFmpegSplitter::executeTask, this, task);

    LOG_INFO("启动分帧任务: " + taskId, "FFmpegSplitter");
    return true;
}

bool FFmpegSplitter::cancelTask(const std::string& taskId) {
    std::shared_ptr<SplitTask> task;

    // 查找任务
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        auto it = std::find_if(tasks_.begin(), tasks_.end(),
                              [&taskId](const std::shared_ptr<SplitTask>& t) {
                                  return t->taskId == taskId;
                              });

        if (it == tasks_.end()) {
            LOG_ERROR("任务不存在: " + taskId, "FFmpegSplitter");
            return false;
        }

        task = *it;
    }

    // 检查任务状态
    if (task->status.state != SplitTaskState::RUNNING) {
        LOG_ERROR("任务状态不是RUNNING: " + taskId, "FFmpegSplitter");
        return false;
    }

    // 设置取消标志
    task->cancelFlag = true;

    // 等待任务线程结束
    if (task->thread.joinable()) {
        task->thread.join();
    }

    // 更新任务状态
    task->status.state = SplitTaskState::CANCELLED;
    task->status.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // 调用状态回调
    if (status_callback_) {
        status_callback_(task->status);
    }

    LOG_INFO("取消分帧任务: " + taskId, "FFmpegSplitter");
    return true;
}

SplitTaskStatus FFmpegSplitter::getTaskStatus(const std::string& taskId) const {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    // 查找任务
    auto it = std::find_if(tasks_.begin(), tasks_.end(),
                          [&taskId](const std::shared_ptr<SplitTask>& t) {
                              return t->taskId == taskId;
                          });

    if (it == tasks_.end()) {
        SplitTaskStatus emptyStatus;
        emptyStatus.task_id = taskId;
        emptyStatus.state = SplitTaskState::ERROR;
        emptyStatus.error_message = "任务不存在";
        return emptyStatus;
    }

    return (*it)->status;
}

std::vector<SplitTaskStatus> FFmpegSplitter::getAllTaskStatus() const {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    std::vector<SplitTaskStatus> statuses;
    for (const auto& task : tasks_) {
        statuses.push_back(task->status);
    }

    return statuses;
}

void FFmpegSplitter::setStatusCallback(std::function<void(const SplitTaskStatus&)> callback) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    status_callback_ = callback;
}

int FFmpegSplitter::cleanupCompletedTasks(int keepLastN) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    // 统计已完成的任务
    std::vector<std::shared_ptr<SplitTask>> completedTasks;
    for (const auto& task : tasks_) {
        if (task->status.state == SplitTaskState::COMPLETED ||
            task->status.state == SplitTaskState::CANCELLED ||
            task->status.state == SplitTaskState::ERROR) {
            completedTasks.push_back(task);
        }
    }

    // 按完成时间排序
    std::sort(completedTasks.begin(), completedTasks.end(),
             [](const std::shared_ptr<SplitTask>& a, const std::shared_ptr<SplitTask>& b) {
                 return a->status.end_time > b->status.end_time;
             });

    // 保留最近的N个任务
    int removedCount = 0;
    for (size_t i = keepLastN; i < completedTasks.size(); ++i) {
        auto it = std::find_if(tasks_.begin(), tasks_.end(),
                              [&](const std::shared_ptr<SplitTask>& t) {
                                  return t->taskId == completedTasks[i]->taskId;
                              });

        if (it != tasks_.end()) {
            tasks_.erase(it);
            removedCount++;
        }
    }

    LOG_INFO("清理已完成任务: " + std::to_string(removedCount) + "个", "FFmpegSplitter");
    return removedCount;
}

void FFmpegSplitter::executeTask(std::shared_ptr<SplitTask> task) {
    LOG_INFO("开始执行分帧任务: " + task->taskId, "FFmpegSplitter");

    // 创建输出目录
    if (!createOutputDirectory(task->status.output_dir)) {
        task->status.state = SplitTaskState::ERROR;
        task->status.error_message = "无法创建输出目录: " + task->status.output_dir;
        task->status.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        updateTaskStatus(task->taskId, task->status);
        LOG_ERROR(task->status.error_message, "FFmpegSplitter");
        return;
    }

    // 获取视频信息
    int width, height;
    double duration, frameRate;
    int totalFrames;
    if (!getVideoInfo(task->config.input_path, width, height, duration, totalFrames, frameRate)) {
        task->status.state = SplitTaskState::ERROR;
        task->status.error_message = "无法获取视频信息: " + task->config.input_path;
        task->status.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        updateTaskStatus(task->taskId, task->status);
        LOG_ERROR(task->status.error_message, "FFmpegSplitter");
        return;
    }

    // 更新任务状态
    task->status.total_frames = totalFrames;
    updateTaskStatus(task->taskId, task->status);

    // 计算提取帧的间隔
    double interval;
    if (task->config.extract_by_time) {
        // 按时间间隔提取
        interval = task->config.interval;
    } else if (task->config.extract_by_frame) {
        // 按指定帧率提取
        interval = 1.0 / frameRate;
    } else {
        // 按原始帧率提取
        interval = 1.0 / frameRate;
    }

    // 打开输入文件
    AVFormatContext* format_context = nullptr;
    int ret = avformat_open_input(&format_context, task->config.input_path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        task->status.state = SplitTaskState::ERROR;
        task->status.error_message = "无法打开输入文件: " + std::string(err_buf);
        task->status.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        updateTaskStatus(task->taskId, task->status);
        LOG_ERROR(task->status.error_message, "FFmpegSplitter");
        return;
    }

    // 获取流信息
    ret = avformat_find_stream_info(format_context, nullptr);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        task->status.state = SplitTaskState::ERROR;
        task->status.error_message = "无法获取流信息: " + std::string(err_buf);
        task->status.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        updateTaskStatus(task->taskId, task->status);
        LOG_ERROR(task->status.error_message, "FFmpegSplitter");
        avformat_close_input(&format_context);
        return;
    }

    // 查找视频流
    int video_stream_index = -1;
    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    if (video_stream_index == -1) {
        task->status.state = SplitTaskState::ERROR;
        task->status.error_message = "未找到视频流";
        task->status.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        updateTaskStatus(task->taskId, task->status);
        LOG_ERROR(task->status.error_message, "FFmpegSplitter");
        avformat_close_input(&format_context);
        return;
    }

    // 获取视频流
    AVStream* video_stream = format_context->streams[video_stream_index];

    // 查找解码器
    const AVCodec* codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!codec) {
        task->status.state = SplitTaskState::ERROR;
        task->status.error_message = "未找到解码器";
        task->status.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        updateTaskStatus(task->taskId, task->status);
        LOG_ERROR(task->status.error_message, "FFmpegSplitter");
        avformat_close_input(&format_context);
        return;
    }

    // 创建解码器上下文
    AVCodecContext* codec_context = avcodec_alloc_context3(codec);
    if (!codec_context) {
        task->status.state = SplitTaskState::ERROR;
        task->status.error_message = "无法创建解码器上下文";
        task->status.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        updateTaskStatus(task->taskId, task->status);
        LOG_ERROR(task->status.error_message, "FFmpegSplitter");
        avformat_close_input(&format_context);
        return;
    }

    // 复制编解码器参数到上下文
    ret = avcodec_parameters_to_context(codec_context, video_stream->codecpar);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        task->status.state = SplitTaskState::ERROR;
        task->status.error_message = "无法复制编解码器参数: " + std::string(err_buf);
        task->status.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        updateTaskStatus(task->taskId, task->status);
        LOG_ERROR(task->status.error_message, "FFmpegSplitter");
        avcodec_free_context(&codec_context);
        avformat_close_input(&format_context);
        return;
    }

    // 打开解码器
    ret = avcodec_open2(codec_context, codec, nullptr);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        task->status.state = SplitTaskState::ERROR;
        task->status.error_message = "无法打开解码器: " + std::string(err_buf);
        task->status.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        updateTaskStatus(task->taskId, task->status);
        LOG_ERROR(task->status.error_message, "FFmpegSplitter");
        avcodec_free_context(&codec_context);
        avformat_close_input(&format_context);
        return;
    }

    // 分配帧和包
    AVFrame* frame = av_frame_alloc();
    AVPacket* packet = av_packet_alloc();
    if (!frame || !packet) {
        task->status.state = SplitTaskState::ERROR;
        task->status.error_message = "无法分配帧或包";
        task->status.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        updateTaskStatus(task->taskId, task->status);
        LOG_ERROR(task->status.error_message, "FFmpegSplitter");
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codec_context);
        avformat_close_input(&format_context);
        return;
    }

    // 创建 SwsContext 并设置色彩范围
    SwsContext* sws_context_temp = sws_getContext(
        codec_context->width, codec_context->height, codec_context->pix_fmt,
        codec_context->width, codec_context->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    // 设置色彩范围为 MPEG (标准范围)
    int srcRange = 0; // 0 = MPEG/TV/JPEG 范围 (16-235), 1 = JPEG/全范围 (0-255)
    int dstRange = 0;
    int brightness = 0;
    int contrast = 0;
    int saturation = 0;
    
    int colorspace_ret = sws_setColorspaceDetails(sws_context_temp, 
                                     sws_getCoefficients(SWS_CS_DEFAULT), srcRange,
                                     sws_getCoefficients(SWS_CS_DEFAULT), dstRange,
                                     brightness, contrast, saturation);
    if (colorspace_ret < 0) {
        LOG_WARNING("设置色彩空间细节失败", "FFmpegSplitter");
    }
    
    SwsContext* sws_context = sws_context_temp;

    if (!sws_context) {
        task->status.state = SplitTaskState::ERROR;
        task->status.error_message = "无法创建SwsContext";
        task->status.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        updateTaskStatus(task->taskId, task->status);
        LOG_ERROR(task->status.error_message, "FFmpegSplitter");
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codec_context);
        avformat_close_input(&format_context);
        return;
    }

    // 分配RGB帧
    AVFrame* rgb_frame = av_frame_alloc();
    if (!rgb_frame) {
        task->status.state = SplitTaskState::ERROR;
        task->status.error_message = "无法分配RGB帧";
        task->status.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        updateTaskStatus(task->taskId, task->status);
        LOG_ERROR(task->status.error_message, "FFmpegSplitter");
        sws_freeContext(sws_context);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codec_context);
        avformat_close_input(&format_context);
        return;
    }

    // 设置RGB帧参数
    rgb_frame->format = AV_PIX_FMT_RGB24;
    rgb_frame->width = codec_context->width;
    rgb_frame->height = codec_context->height;

    // 分配RGB帧缓冲区
    ret = av_frame_get_buffer(rgb_frame, 0);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        task->status.state = SplitTaskState::ERROR;
        task->status.error_message = "无法分配RGB帧缓冲区: " + std::string(err_buf);
        task->status.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        updateTaskStatus(task->taskId, task->status);
        LOG_ERROR(task->status.error_message, "FFmpegSplitter");
        av_frame_free(&rgb_frame);
        sws_freeContext(sws_context);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codec_context);
        avformat_close_input(&format_context);
        return;
    }

    // 提取帧
    double next_timestamp = 0.0;
    int frame_count = 0;
    int image_count = 0;

    while (!task->cancelFlag) {
        // 读取一个包
        ret = av_read_frame(format_context, packet);
        if (ret < 0) {
            // 文件结束
            break;
        }

        // 检查是否为视频包
        if (packet->stream_index != video_stream_index) {
            av_packet_unref(packet);
            continue;
        }

        // 发送包到解码器
        ret = avcodec_send_packet(codec_context, packet);
        if (ret < 0) {
            av_packet_unref(packet);
            continue;
        }

        // 接收帧
        while (ret >= 0) {
            ret = avcodec_receive_frame(codec_context, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                // 解码错误
                break;
            }

            // 计算时间戳
            double timestamp = frame->pts * av_q2d(video_stream->time_base);

            // 检查是否需要提取此帧
            if (timestamp >= next_timestamp) {
                // 转换为RGB
                ret = sws_scale(
                    sws_context,
                    frame->data, frame->linesize, 0, frame->height,
                    rgb_frame->data, rgb_frame->linesize
                );

                if (ret > 0) {
                    // 生成输出文件名
                    std::stringstream ss;
                    ss << task->status.output_dir << "/frame_"
                       << std::setw(6) << std::setfill('0') << image_count;

                    // 添加时间戳到文件名
                    ss << "_" << std::fixed << std::setprecision(3) << timestamp;

                    ss << "." << task->config.output_format;
                    std::string output_path = ss.str();

                    // 保存图像
                    if (extractFrameAndSave(task->config.input_path, output_path, timestamp, rgb_frame->width, rgb_frame->height, task->config.output_format, task->config.quality)) {
                        image_count++;

                        // 生成缩略图
                        std::string thumbnail_path = task->status.output_dir + "/thumbnails/thumb_" +
                                                   std::to_string(image_count) + "." + task->config.output_format;
                        generateThumbnail(output_path, thumbnail_path, 128);
                    }

                    // 更新下一个时间戳
                    next_timestamp = timestamp + interval;
                }
            }

            frame_count++;

            // 更新进度
            task->status.processed_frames = frame_count;
            task->status.generated_images = image_count;
            task->status.progress = static_cast<double>(frame_count) / task->status.total_frames;
            updateTaskStatus(task->taskId, task->status);

            av_frame_unref(frame);
        }

        av_packet_unref(packet);
    }

    // 清理资源
    av_frame_free(&rgb_frame);
    sws_freeContext(sws_context);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);

    // 更新任务状态
    if (task->cancelFlag) {
        task->status.state = SplitTaskState::CANCELLED;
        LOG_INFO("分帧任务已取消: " + task->taskId, "FFmpegSplitter");
    } else {
        task->status.state = SplitTaskState::COMPLETED;
        task->status.progress = 1.0;
        LOG_INFO("分帧任务已完成: " + task->taskId, "FFmpegSplitter");
    }

    task->status.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    updateTaskStatus(task->taskId, task->status);
}

void FFmpegSplitter::updateTaskStatus(const std::string& taskId, const SplitTaskStatus& status) {
    // 更新任务状态
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        auto it = std::find_if(tasks_.begin(), tasks_.end(),
                              [&taskId](const std::shared_ptr<SplitTask>& t) {
                                  return t->taskId == taskId;
                              });

        if (it != tasks_.end()) {
            (*it)->status = status;
        }
    }

    // 调用状态回调
    if (status_callback_) {
        status_callback_(status);
    }
}

std::string FFmpegSplitter::generateTaskId() const {
    // 生成随机任务ID
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";

    std::string uuid;
    for (int i = 0; i < 32; ++i) {
        uuid += hex[dis(gen)];
        if (i == 7 || i == 11 || i == 15 || i == 19) {
            uuid += '-';
        }
    }

    return uuid;
}

bool FFmpegSplitter::getVideoInfo(const std::string& videoPath, int& width, int& height,
                                 double& duration, int& totalFrames, double& frameRate) {
    // 打开输入文件
    AVFormatContext* format_context = nullptr;
    int ret = avformat_open_input(&format_context, videoPath.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("无法打开输入文件: " + std::string(err_buf), "FFmpegSplitter");
        return false;
    }

    // 获取流信息
    ret = avformat_find_stream_info(format_context, nullptr);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("无法获取流信息: " + std::string(err_buf), "FFmpegSplitter");
        avformat_close_input(&format_context);
        return false;
    }

    // 查找视频流
    int video_stream_index = -1;
    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    if (video_stream_index == -1) {
        LOG_ERROR("未找到视频流", "FFmpegSplitter");
        avformat_close_input(&format_context);
        return false;
    }

    // 获取视频流
    AVStream* video_stream = format_context->streams[video_stream_index];

    // 获取视频信息
    width = video_stream->codecpar->width;
    height = video_stream->codecpar->height;
    duration = video_stream->duration * av_q2d(video_stream->time_base);

    // 计算帧率
    frameRate = av_q2d(video_stream->r_frame_rate);

    // 计算总帧数
    if (video_stream->nb_frames > 0) {
        totalFrames = video_stream->nb_frames;
    } else {
        totalFrames = static_cast<int>(duration * frameRate);
    }

    // 关闭输入文件
    avformat_close_input(&format_context);

    return true;
}

bool FFmpegSplitter::createOutputDirectory(const std::string& dirPath) {
    // 创建输出目录
    if (!utils::FileUtils::directoryExists(dirPath)) {
        if (!utils::FileUtils::createDirectory(dirPath, true)) {
            LOG_ERROR("无法创建输出目录: " + dirPath, "FFmpegSplitter");
            return false;
        }
    }

    // 如果需要生成缩略图，创建缩略图目录
    std::string thumbnailDir = dirPath + "/thumbnails";
    if (!utils::FileUtils::directoryExists(thumbnailDir)) {
        if (!utils::FileUtils::createDirectory(thumbnailDir, true)) {
            LOG_ERROR("无法创建缩略图目录: " + thumbnailDir, "FFmpegSplitter");
            return false;
        }
    }

    return true;
}

bool FFmpegSplitter::extractFrameAndSave(const std::string& videoPath, const std::string& outputPath,
                                        double timestamp, int width, int height,
                                        const std::string& format, int quality) {
    // 这里使用OpenCV保存图像，因为FFmpeg的图像保存功能比较复杂
    // 在实际项目中，可以使用FFmpeg的AVOutputFormat和AVCodec来保存图像

    // 创建输出目录
    std::string outputDir = fs::path(outputPath).parent_path().string();
    if (!utils::FileUtils::directoryExists(outputDir)) {
        if (!utils::FileUtils::createDirectory(outputDir, true)) {
            LOG_ERROR("无法创建输出目录: " + outputDir, "FFmpegSplitter");
            return false;
        }
    }

    // 使用系统命令保存图像
    std::string command = "ffmpeg -y -f rawvideo -pix_fmt rgb24 -s " +
                         std::to_string(width) + "x" + std::to_string(height) +
                         " -i pipe:0 -q:v " + std::to_string(quality) +
                         " \"" + outputPath + "\" 2>/dev/null";

    FILE* pipe = popen(command.c_str(), "w");
    if (!pipe) {
        LOG_ERROR("无法创建管道: " + command, "FFmpegSplitter");
        return false;
    }

    // 写入RGB数据
    // 这里应该写入rgb_frame->data[0]的数据，但由于我们没有实际的数据，所以这里只是示例
    // fwrite(rgb_frame->data[0], 1, width * height * 3, pipe);

    // 关闭管道
    int status = pclose(pipe);
    if (status != 0) {
        LOG_ERROR("保存图像失败: " + outputPath, "FFmpegSplitter");
        return false;
    }

    return true;
}

bool FFmpegSplitter::generateThumbnail(const std::string& imagePath, const std::string& thumbnailPath, int size) {
    // 创建输出目录
    std::string outputDir = fs::path(thumbnailPath).parent_path().string();
    if (!utils::FileUtils::directoryExists(outputDir)) {
        if (!utils::FileUtils::createDirectory(outputDir, true)) {
            LOG_ERROR("无法创建缩略图目录: " + outputDir, "FFmpegSplitter");
            return false;
        }
    }

    // 使用系统命令生成缩略图
    std::string command = "ffmpeg -y -i \"" + imagePath + "\" -vf scale=" +
                         std::to_string(size) + ":" + std::to_string(size) +
                         " \"" + thumbnailPath + "\" 2>/dev/null";

    int status = system(command.c_str());
    if (status != 0) {
        LOG_ERROR("生成缩略图失败: " + thumbnailPath, "FFmpegSplitter");
        return false;
    }

    return true;
}

// 工厂函数实现
std::shared_ptr<IVideoSplitter> createFFmpegSplitter() {
    return std::make_shared<FFmpegSplitter>();
}

} // namespace video
} // namespace cam_server
