#include "video/ffmpeg_recorder.h"
#include "utils/file_utils.h"
#include "utils/string_utils.h"
#include "monitor/logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
}

#include <chrono>
#include <thread>
#include <filesystem>

namespace fs = std::filesystem;

namespace cam_server {
namespace video {

FFmpegRecorder::FFmpegRecorder()
    : is_initialized_(false),
      start_time_(0),
      last_frame_timestamp_(0),
      segment_index_(0),
      format_context_(nullptr),
      codec_context_(nullptr),
      video_stream_(nullptr),
      frame_(nullptr),
      packet_(nullptr),
      sws_context_(nullptr) {

    // 初始化状态
    status_.state = RecordingState::IDLE;
    status_.current_file = "";
    status_.duration = 0.0;
    status_.frame_count = 0;
    status_.file_size = 0;
    status_.error_message = "";
}

FFmpegRecorder::~FFmpegRecorder() {
    stopRecording();
    cleanupFFmpeg();
}

bool FFmpegRecorder::initialize(const RecordingConfig& config) {
    std::lock_guard<std::mutex> lock(status_mutex_);

    // 检查状态
    if (status_.state != RecordingState::IDLE) {
        stopRecording();
    }

    // 保存配置
    config_ = config;

    // 确保输出目录存在
    std::string output_dir = fs::path(config_.output_path).parent_path().string();
    if (!utils::FileUtils::createDirectory(output_dir, true)) {
        status_.error_message = "无法创建输出目录: " + output_dir;
        status_.state = RecordingState::ERROR;
        LOG_ERROR(status_.error_message, "FFmpegRecorder");
        return false;
    }

    // 初始化FFmpeg
    if (!initFFmpeg()) {
        status_.error_message = "初始化FFmpeg失败";
        status_.state = RecordingState::ERROR;
        LOG_ERROR(status_.error_message, "FFmpegRecorder");
        return false;
    }

    is_initialized_ = true;
    LOG_INFO("视频录制器初始化成功", "FFmpegRecorder");
    return true;
}

bool FFmpegRecorder::startRecording() {
    std::lock_guard<std::mutex> lock(status_mutex_);

    // 检查是否已初始化
    if (!is_initialized_) {
        status_.error_message = "录制器未初始化";
        status_.state = RecordingState::ERROR;
        LOG_ERROR(status_.error_message, "FFmpegRecorder");
        return false;
    }

    // 检查状态
    if (status_.state == RecordingState::RECORDING) {
        return true;  // 已经在录制中
    }

    // 生成输出文件名
    if (config_.output_path.empty()) {
        config_.output_path = generateFileName();
    }

    // 创建输出格式上下文
    if (!createOutputFormatContext()) {
        status_.error_message = "创建输出格式上下文失败";
        status_.state = RecordingState::ERROR;
        LOG_ERROR(status_.error_message, "FFmpegRecorder");
        return false;
    }

    // 创建视频流
    if (!createVideoStream()) {
        status_.error_message = "创建视频流失败";
        status_.state = RecordingState::ERROR;
        LOG_ERROR(status_.error_message, "FFmpegRecorder");
        cleanupFFmpeg();
        return false;
    }

    // 打开编码器
    if (!openEncoder()) {
        status_.error_message = "打开编码器失败";
        status_.state = RecordingState::ERROR;
        LOG_ERROR(status_.error_message, "FFmpegRecorder");
        cleanupFFmpeg();
        return false;
    }

    // 写入文件头
    if (!writeHeader()) {
        status_.error_message = "写入文件头失败";
        status_.state = RecordingState::ERROR;
        LOG_ERROR(status_.error_message, "FFmpegRecorder");
        cleanupFFmpeg();
        return false;
    }

    // 更新状态
    status_.state = RecordingState::RECORDING;
    status_.current_file = config_.output_path;
    status_.duration = 0.0;
    status_.frame_count = 0;
    status_.file_size = 0;
    status_.error_message = "";

    // 记录开始时间
    start_time_ = av_gettime();
    last_frame_timestamp_ = start_time_;

    // 调用状态回调
    if (status_callback_) {
        status_callback_(status_);
    }

    LOG_INFO("开始录制视频: " + config_.output_path, "FFmpegRecorder");
    return true;
}

bool FFmpegRecorder::stopRecording() {
    std::lock_guard<std::mutex> lock(status_mutex_);

    // 检查状态
    if (status_.state != RecordingState::RECORDING && status_.state != RecordingState::PAUSED) {
        return true;  // 没有在录制
    }

    // 写入文件尾
    if (format_context_) {
        writeTrailer();
    }

    // 清理FFmpeg资源
    cleanupFFmpeg();

    // 更新状态
    status_.state = RecordingState::IDLE;

    // 调用状态回调
    if (status_callback_) {
        status_callback_(status_);
    }

    LOG_INFO("停止录制视频", "FFmpegRecorder");
    return true;
}

bool FFmpegRecorder::pauseRecording() {
    std::lock_guard<std::mutex> lock(status_mutex_);

    // 检查状态
    if (status_.state != RecordingState::RECORDING) {
        return false;  // 没有在录制
    }

    // 更新状态
    status_.state = RecordingState::PAUSED;

    // 调用状态回调
    if (status_callback_) {
        status_callback_(status_);
    }

    LOG_INFO("暂停录制视频", "FFmpegRecorder");
    return true;
}

bool FFmpegRecorder::resumeRecording() {
    std::lock_guard<std::mutex> lock(status_mutex_);

    // 检查状态
    if (status_.state != RecordingState::PAUSED) {
        return false;  // 没有在暂停
    }

    // 更新状态
    status_.state = RecordingState::RECORDING;

    // 调用状态回调
    if (status_callback_) {
        status_callback_(status_);
    }

    LOG_INFO("恢复录制视频", "FFmpegRecorder");
    return true;
}

bool FFmpegRecorder::processFrame(const camera::Frame& frame) {
    std::lock_guard<std::mutex> lock(status_mutex_);

    // 检查状态
    if (status_.state != RecordingState::RECORDING) {
        return false;  // 没有在录制
    }

    // 检查是否需要分段
    if (checkSegmentation()) {
        if (!createNewSegment()) {
            status_.error_message = "创建新分段失败";
            status_.state = RecordingState::ERROR;
            LOG_ERROR(status_.error_message, "FFmpegRecorder");
            return false;
        }
    }

    // 编码并写入帧
    if (!encodeAndWriteFrame(frame)) {
        status_.error_message = "编码帧失败";
        status_.state = RecordingState::ERROR;
        LOG_ERROR(status_.error_message, "FFmpegRecorder");
        return false;
    }

    // 更新状态
    status_.frame_count++;
    status_.duration = (av_gettime() - start_time_) / 1000000.0;

    // 更新文件大小
    if (!status_.current_file.empty()) {
        status_.file_size = utils::FileUtils::getFileSize(status_.current_file);
    }

    // 调用状态回调
    if (status_callback_) {
        status_callback_(status_);
    }

    return true;
}

RecordingStatus FFmpegRecorder::getStatus() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
}

void FFmpegRecorder::setStatusCallback(std::function<void(const RecordingStatus&)> callback) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_callback_ = callback;
}

bool FFmpegRecorder::setConfig(const RecordingConfig& config) {
    std::lock_guard<std::mutex> lock(status_mutex_);

    // 检查状态
    if (status_.state == RecordingState::RECORDING || status_.state == RecordingState::PAUSED) {
        LOG_ERROR("无法在录制过程中更改配置", "FFmpegRecorder");
        return false;
    }

    config_ = config;
    return true;
}

RecordingConfig FFmpegRecorder::getConfig() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return config_;
}

bool FFmpegRecorder::initFFmpeg() {
    // 分配AVFrame
    frame_ = av_frame_alloc();
    if (!frame_) {
        LOG_ERROR("无法分配AVFrame", "FFmpegRecorder");
        return false;
    }

    // 分配AVPacket
    packet_ = av_packet_alloc();
    if (!packet_) {
        LOG_ERROR("无法分配AVPacket", "FFmpegRecorder");
        av_frame_free(&frame_);
        return false;
    }

    return true;
}

void FFmpegRecorder::cleanupFFmpeg() {
    // 释放SwsContext
    if (sws_context_) {
        sws_freeContext(sws_context_);
        sws_context_ = nullptr;
    }

    // 释放AVFrame
    if (frame_) {
        av_frame_free(&frame_);
        frame_ = nullptr;
    }

    // 释放AVPacket
    if (packet_) {
        av_packet_free(&packet_);
        packet_ = nullptr;
    }

    // 关闭编码器
    if (codec_context_) {
        avcodec_free_context(&codec_context_);
        codec_context_ = nullptr;
    }

    // 关闭输出格式上下文
    if (format_context_) {
        if (!(format_context_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&format_context_->pb);
        }
        avformat_free_context(format_context_);
        format_context_ = nullptr;
    }

    video_stream_ = nullptr;
}

bool FFmpegRecorder::createOutputFormatContext() {
    // 猜测输出格式
    const char* format_name = nullptr;
    if (!config_.container_format.empty()) {
        format_name = config_.container_format.c_str();
    }

    // 创建输出格式上下文
    int ret = avformat_alloc_output_context2(&format_context_, nullptr, format_name, config_.output_path.c_str());
    if (ret < 0 || !format_context_) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("无法创建输出格式上下文: " + std::string(err_buf), "FFmpegRecorder");
        return false;
    }

    // 打开输出文件
    if (!(format_context_->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&format_context_->pb, config_.output_path.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
            LOG_ERROR("无法打开输出文件: " + std::string(err_buf), "FFmpegRecorder");
            avformat_free_context(format_context_);
            format_context_ = nullptr;
            return false;
        }
    }

    return true;
}

bool FFmpegRecorder::createVideoStream() {
    // 查找编码器
    const AVCodec* codec = nullptr;
    if (!config_.encoder_name.empty()) {
        codec = avcodec_find_encoder_by_name(config_.encoder_name.c_str());
    } else {
        // 使用默认编码器
        if (config_.use_hw_accel) {
            // 尝试使用硬件加速编码器
            codec = avcodec_find_encoder_by_name("h264_rkmpp");
            if (!codec) {
                codec = avcodec_find_encoder_by_name("h264_v4l2m2m");
            }
        }

        // 如果没有找到硬件加速编码器，使用软件编码器
        if (!codec) {
            codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        }
    }

    if (!codec) {
        LOG_ERROR("无法找到编码器", "FFmpegRecorder");
        return false;
    }

    // 创建视频流
    video_stream_ = avformat_new_stream(format_context_, nullptr);
    if (!video_stream_) {
        LOG_ERROR("无法创建视频流", "FFmpegRecorder");
        return false;
    }

    // 创建编码器上下文
    codec_context_ = avcodec_alloc_context3(codec);
    if (!codec_context_) {
        LOG_ERROR("无法创建编码器上下文", "FFmpegRecorder");
        return false;
    }

    // 设置编码器参数
    codec_context_->codec_id = codec->id;
    codec_context_->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_context_->width = config_.width;
    codec_context_->height = config_.height;
    codec_context_->time_base.num = 1;
    codec_context_->time_base.den = config_.fps;
    codec_context_->framerate.num = config_.fps;
    codec_context_->framerate.den = 1;
    codec_context_->gop_size = config_.gop;
    codec_context_->max_b_frames = 0;  // 不使用B帧
    codec_context_->pix_fmt = AV_PIX_FMT_YUV420P;

    // 设置比特率
    if (config_.bitrate > 0) {
        codec_context_->bit_rate = config_.bitrate;
    } else {
        // 根据分辨率和帧率计算合适的比特率
        codec_context_->bit_rate = config_.width * config_.height * config_.fps * 0.1;
    }

    // 设置编码器特定选项
    if (codec_context_->codec_id == AV_CODEC_ID_H264) {
        // 设置H.264编码器选项
        av_dict_set(&format_context_->metadata, "preset", "ultrafast", 0);
        av_dict_set(&format_context_->metadata, "tune", "zerolatency", 0);
    }

    // 复制编码器参数到视频流
    int ret = avcodec_parameters_from_context(video_stream_->codecpar, codec_context_);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("无法复制编码器参数到视频流: " + std::string(err_buf), "FFmpegRecorder");
        return false;
    }

    return true;
}

bool FFmpegRecorder::openEncoder() {
    // 打开编码器
    int ret = avcodec_open2(codec_context_, codec_context_->codec, nullptr);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("无法打开编码器: " + std::string(err_buf), "FFmpegRecorder");
        return false;
    }

    // 设置帧参数
    frame_->format = codec_context_->pix_fmt;
    frame_->width = codec_context_->width;
    frame_->height = codec_context_->height;

    // 分配帧缓冲区
    ret = av_frame_get_buffer(frame_, 0);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("无法分配帧缓冲区: " + std::string(err_buf), "FFmpegRecorder");
        return false;
    }

    return true;
}

bool FFmpegRecorder::writeHeader() {
    // 写入文件头
    int ret = avformat_write_header(format_context_, nullptr);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("无法写入文件头: " + std::string(err_buf), "FFmpegRecorder");
        return false;
    }

    return true;
}

bool FFmpegRecorder::writeTrailer() {
    // 写入文件尾
    int ret = av_write_trailer(format_context_);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("无法写入文件尾: " + std::string(err_buf), "FFmpegRecorder");
        return false;
    }

    return true;
}

bool FFmpegRecorder::encodeAndWriteFrame(const camera::Frame& frame) {
    // 确保帧可写
    int ret = av_frame_make_writable(frame_);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("无法使帧可写: " + std::string(err_buf), "FFmpegRecorder");
        return false;
    }

    // 根据源格式选择转换方式
    int src_format;
    switch (frame.getFormat()) {
        case camera::PixelFormat::YUYV:
            src_format = AV_PIX_FMT_YUYV422;
            break;
        case camera::PixelFormat::RGB24:
            src_format = AV_PIX_FMT_RGB24;
            break;
        case camera::PixelFormat::BGR24:
            src_format = AV_PIX_FMT_BGR24;
            break;
        case camera::PixelFormat::NV12:
            src_format = AV_PIX_FMT_NV12;
            break;
        default:
            LOG_ERROR("不支持的像素格式: " + std::to_string(static_cast<int>(frame.getFormat())), "FFmpegRecorder");
            return false;
    }

    // 创建图像转换上下文
    SwsContext* sws_context = sws_getContext(
        frame.getWidth(), frame.getHeight(), (AVPixelFormat)src_format,
        frame.getWidth(), frame.getHeight(), codec_context_->pix_fmt,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    // 设置源数据
    const uint8_t* src_data[4] = {frame.getData().data(), nullptr, nullptr, nullptr};
    int src_linesize[4] = {frame.getWidth() * 2, 0, 0, 0};  // 假设YUYV格式

    // 执行图像转换
    sws_scale(sws_context, src_data, src_linesize, 0, frame.getHeight(),
              frame_->data, frame_->linesize);

    // 设置帧时间戳
    frame_->pts = av_rescale_q(
        status_.frame_count,
        av_make_q(1, config_.fps),
        codec_context_->time_base
    );

    // 编码帧
    ret = avcodec_send_frame(codec_context_, frame_);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("无法发送帧到编码器: " + std::string(err_buf), "FFmpegRecorder");
        return false;
    }

    // 获取编码后的包
    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_context_, packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
            LOG_ERROR("无法从编码器接收包: " + std::string(err_buf), "FFmpegRecorder");
            return false;
        }

        // 转换时间戳
        packet_->stream_index = video_stream_->index;
        av_packet_rescale_ts(packet_, codec_context_->time_base, video_stream_->time_base);

        // 写入包
        ret = av_interleaved_write_frame(format_context_, packet_);
        if (ret < 0) {
            char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
            LOG_ERROR("无法写入包: " + std::string(err_buf), "FFmpegRecorder");
            return false;
        }
    }

    return true;
}

bool FFmpegRecorder::checkSegmentation() {
    // 检查是否需要分段
    if (config_.max_duration > 0) {
        // 根据时长分段
        if (status_.duration >= config_.max_duration) {
            return true;
        }
    }

    if (config_.max_size > 0) {
        // 根据文件大小分段
        if (status_.file_size >= config_.max_size) {
            return true;
        }
    }

    return false;
}

bool FFmpegRecorder::createNewSegment() {
    // 写入当前文件尾
    writeTrailer();

    // 关闭当前输出文件
    if (format_context_ && !(format_context_->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&format_context_->pb);
    }

    // 增加分段索引
    segment_index_++;

    // 生成新的输出文件名
    std::string base_name = config_.output_path;
    std::string extension = fs::path(base_name).extension().string();
    std::string stem = fs::path(base_name).stem().string();
    std::string new_path = fs::path(base_name).parent_path().string() + "/" +
                          stem + "_part" + std::to_string(segment_index_) + extension;

    // 更新配置
    config_.output_path = new_path;

    // 创建新的输出格式上下文
    if (!createOutputFormatContext()) {
        return false;
    }

    // 创建新的视频流
    if (!createVideoStream()) {
        return false;
    }

    // 打开编码器
    if (!openEncoder()) {
        return false;
    }

    // 写入文件头
    if (!writeHeader()) {
        return false;
    }

    // 更新状态
    status_.current_file = new_path;
    status_.file_size = 0;

    LOG_INFO("创建新的分段文件: " + new_path, "FFmpegRecorder");
    return true;
}

std::string FFmpegRecorder::generateFileName() {
    // 获取当前日期时间
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    localtime_r(&time_t_now, &tm_now);

    char time_str[100];
    std::strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", &tm_now);

    // 生成文件名
    std::string file_name = std::string(time_str) + "_" +
                           std::to_string(config_.width) + "x" + std::to_string(config_.height) + "_" +
                           std::to_string(config_.fps) + "fps";

    // 添加编码器信息
    if (!config_.encoder_name.empty()) {
        file_name += "_" + config_.encoder_name;
    }

    // 添加扩展名
    std::string extension = ".mp4";
    if (!config_.container_format.empty()) {
        if (config_.container_format == "matroska") {
            extension = ".mkv";
        } else if (config_.container_format == "avi") {
            extension = ".avi";
        }
    }

    // 完整路径
    std::string output_dir;
    if (fs::is_directory(fs::path(config_.output_path))) {
        output_dir = config_.output_path;
    } else {
        output_dir = fs::path(config_.output_path).parent_path().string();
    }

    if (output_dir.empty()) {
        output_dir = "./videos";
        utils::FileUtils::createDirectory(output_dir, true);
    }

    return fs::path(output_dir) / (file_name + extension);
}

// 工厂函数实现
std::shared_ptr<IVideoRecorder> createFFmpegRecorder() {
    return std::make_shared<FFmpegRecorder>();
}

} // namespace video
} // namespace cam_server
