#include "api/mjpeg_streamer.h"
#include "camera/camera_manager.h"
#include "monitor/logger.h"
#include "utils/time_utils.h"

#include <chrono>
#include <algorithm>
#include <iostream>  // for std::cerr

// 使用FFmpeg进行JPEG编码
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace cam_server {
namespace api {

MjpegStreamer& MjpegStreamer::getInstance() {
    static MjpegStreamer instance;
    return instance;
}

MjpegStreamer::MjpegStreamer()
    : is_initialized_(false), is_running_(false), current_fps_(0.0), frame_count_(0) {
}

MjpegStreamer::~MjpegStreamer() {
    stop();
}

bool MjpegStreamer::initialize(const MjpegStreamerConfig& config) {
    std::cerr << "[MJPEG][mjpeg_streamer.cpp:initialize] 开始初始化MJPEG流处理器..." << std::endl;

    if (is_initialized_) {
        std::cerr << "[MJPEG][mjpeg_streamer.cpp:initialize] MJPEG流处理器已经初始化" << std::endl;
        return true;
    }

    std::cerr << "[MJPEG][mjpeg_streamer.cpp:initialize] 设置配置..." << std::endl;
    config_ = config;

    // 设置默认值
    std::cerr << "[MJPEG][mjpeg_streamer.cpp:initialize] 设置默认值..." << std::endl;
    if (config_.jpeg_quality <= 0 || config_.jpeg_quality > 100) {
        std::cerr << "[MJPEG][mjpeg_streamer.cpp:initialize] 设置默认JPEG质量: 80" << std::endl;
        config_.jpeg_quality = 80;
    }
    if (config_.max_fps <= 0) {
        std::cerr << "[MJPEG][mjpeg_streamer.cpp:initialize] 设置默认最大帧率: 30" << std::endl;
        config_.max_fps = 30;
    }
    if (config_.max_clients <= 0) {
        std::cerr << "[MJPEG][mjpeg_streamer.cpp:initialize] 设置默认最大客户端数: 10" << std::endl;
        config_.max_clients = 10;
    }

    is_initialized_ = true;
    std::cerr << "[MJPEG][mjpeg_streamer.cpp:initialize] MJPEG流处理器初始化成功" << std::endl;
    LOG_INFO("MJPEG流处理器初始化成功", "MjpegStreamer");
    return true;
}

bool MjpegStreamer::start() {
    std::cerr << "[MJPEG][mjpeg_streamer.cpp:start] 开始启动MJPEG流处理器..." << std::endl;

    if (!is_initialized_) {
        std::cerr << "[MJPEG][mjpeg_streamer.cpp:start] MJPEG流处理器未初始化" << std::endl;
        LOG_ERROR("MJPEG流处理器未初始化", "MjpegStreamer");
        return false;
    }

    if (is_running_) {
        std::cerr << "[MJPEG][mjpeg_streamer.cpp:start] MJPEG流处理器已经在运行中" << std::endl;
        return true;
    }

    // 获取摄像头管理器
    std::cerr << "[MJPEG][mjpeg_streamer.cpp:start] 获取摄像头管理器..." << std::endl;
    auto& camera_manager = camera::CameraManager::getInstance();

    // 设置帧回调
    std::cerr << "[MJPEG][mjpeg_streamer.cpp:start] 设置帧回调..." << std::endl;
    camera_manager.setFrameCallback([this](const camera::Frame& frame) {
        handleFrame(frame);
    });

    is_running_ = true;
    frame_count_ = 0;
    last_fps_time_ = std::chrono::steady_clock::now();

    std::cerr << "[MJPEG][mjpeg_streamer.cpp:start] MJPEG流处理器启动成功" << std::endl;
    LOG_INFO("MJPEG流处理器启动成功", "MjpegStreamer");
    return true;
}

bool MjpegStreamer::stop() {
    if (!is_running_) {
        return true;
    }

    // 获取摄像头管理器
    auto& camera_manager = camera::CameraManager::getInstance();

    // 移除帧回调
    camera_manager.setFrameCallback(nullptr);

    // 清空客户端列表
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.clear();
    }

    is_running_ = false;
    LOG_INFO("MJPEG流处理器停止成功", "MjpegStreamer");
    return true;
}

bool MjpegStreamer::addClient(const std::string& client_id,
                             std::function<void(const std::vector<uint8_t>&)> frame_callback,
                             std::function<void(const std::string&)> error_callback,
                             std::function<void()> close_callback) {
    if (!is_running_) {
        LOG_ERROR("MJPEG流处理器未运行", "MjpegStreamer");
        return false;
    }

    std::lock_guard<std::mutex> lock(clients_mutex_);

    // 检查客户端数量是否已达上限
    if (clients_.size() >= config_.max_clients) {
        LOG_ERROR("客户端数量已达上限", "MjpegStreamer");
        return false;
    }

    // 创建新客户端
    auto client = std::make_shared<MjpegClient>();
    client->id = client_id;
    client->frame_callback = frame_callback;
    client->error_callback = error_callback;
    client->close_callback = close_callback;
    client->last_frame_time = utils::TimeUtils::getCurrentTimeMicros();

    // 添加到客户端列表
    clients_[client_id] = client;

    LOG_INFO("添加MJPEG客户端: " + client_id, "MjpegStreamer");
    return true;
}

bool MjpegStreamer::removeClient(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(clients_mutex_);

    auto it = clients_.find(client_id);
    if (it == clients_.end()) {
        return false;
    }

    // 调用关闭回调
    if (it->second->close_callback) {
        it->second->close_callback();
    }

    // 从客户端列表中移除
    clients_.erase(it);

    LOG_INFO("移除MJPEG客户端: " + client_id, "MjpegStreamer");
    return true;
}

int MjpegStreamer::getClientCount() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    return clients_.size();
}

double MjpegStreamer::getCurrentFps() const {
    return current_fps_.load();
}

void MjpegStreamer::handleFrame(const camera::Frame& frame) {
    // 不要在每一帧都输出调试信息，这会导致大量输出
    // std::cerr << "[MJPEG][mjpeg_streamer.cpp:handleFrame] 处理帧..." << std::endl;

    if (!is_running_) {
        return;
    }

    // 更新帧率计算
    frame_count_++;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_time_).count();
    if (elapsed >= 1000) {
        current_fps_ = frame_count_ * 1000.0 / elapsed;
        frame_count_ = 0;
        last_fps_time_ = now;

        // 每秒输出一次帧率信息
        std::cerr << "[MJPEG][mjpeg_streamer.cpp:handleFrame] 当前帧率: " << current_fps_ << " FPS" << std::endl;
    }

    // 调整帧大小（如果需要）
    camera::Frame processed_frame;
    if (config_.output_width > 0 && config_.output_height > 0 &&
        (frame.width != config_.output_width || frame.height != config_.output_height)) {
        if (!resizeFrame(frame, processed_frame)) {
            LOG_ERROR("调整帧大小失败", "MjpegStreamer");
            return;
        }
    } else {
        processed_frame = frame;
    }

    // 编码为JPEG
    std::vector<uint8_t> jpeg_data;
    if (!encodeToJpeg(processed_frame, jpeg_data)) {
        LOG_ERROR("编码为JPEG失败", "MjpegStreamer");
        return;
    }

    // 发送给所有客户端
    std::lock_guard<std::mutex> lock(clients_mutex_);

    // 每秒输出一次客户端数量信息
    if (elapsed >= 1000) {
        std::cerr << "[MJPEG][mjpeg_streamer.cpp:handleFrame] 当前客户端数量: " << clients_.size() << std::endl;
    }

    for (auto it = clients_.begin(); it != clients_.end();) {
        auto& client = it->second;

        // 检查帧率限制
        int64_t now_micros = utils::TimeUtils::getCurrentTimeMicros();
        int64_t elapsed_micros = now_micros - client->last_frame_time;
        int64_t min_interval_micros = 1000000 / config_.max_fps;

        if (elapsed_micros >= min_interval_micros) {
            try {
                // 发送帧
                client->frame_callback(jpeg_data);
                client->last_frame_time = now_micros;
                ++it;
            } catch (const std::exception& e) {
                // 发送失败，移除客户端
                std::cerr << "[MJPEG][mjpeg_streamer.cpp:handleFrame] 发送帧失败: " << e.what() << std::endl;
                LOG_ERROR("发送帧失败: " + std::string(e.what()), "MjpegStreamer");
                if (client->error_callback) {
                    client->error_callback(e.what());
                }
                it = clients_.erase(it);
            }
        } else {
            ++it;
        }
    }
}

bool MjpegStreamer::encodeToJpeg(const camera::Frame& frame, std::vector<uint8_t>& jpeg_data) {
    // 如果已经是MJPEG格式，直接返回数据
    if (frame.format == camera::PixelFormat::MJPEG) {
        jpeg_data = frame.data;
        return true;
    }

    // 使用FFmpeg编码为JPEG
    // 这里简化实现，实际项目中应该使用更高效的方法
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!codec) {
        LOG_ERROR("找不到MJPEG编码器", "MjpegStreamer");
        return false;
    }

    AVCodecContext* codec_context = avcodec_alloc_context3(codec);
    if (!codec_context) {
        LOG_ERROR("无法分配编码器上下文", "MjpegStreamer");
        return false;
    }

    // 设置编码参数
    codec_context->width = frame.width;
    codec_context->height = frame.height;
    codec_context->time_base = {1, 25};
    codec_context->pix_fmt = AV_PIX_FMT_YUVJ420P;
    codec_context->global_quality = config_.jpeg_quality * FF_QP2LAMBDA;
    codec_context->flags |= AV_CODEC_FLAG_QSCALE;

    if (avcodec_open2(codec_context, codec, nullptr) < 0) {
        LOG_ERROR("无法打开编码器", "MjpegStreamer");
        avcodec_free_context(&codec_context);
        return false;
    }

    // 创建帧
    AVFrame* av_frame = av_frame_alloc();
    if (!av_frame) {
        LOG_ERROR("无法分配帧", "MjpegStreamer");
        avcodec_free_context(&codec_context);
        return false;
    }

    av_frame->width = frame.width;
    av_frame->height = frame.height;
    av_frame->format = codec_context->pix_fmt;

    if (av_frame_get_buffer(av_frame, 0) < 0) {
        LOG_ERROR("无法分配帧缓冲区", "MjpegStreamer");
        av_frame_free(&av_frame);
        avcodec_free_context(&codec_context);
        return false;
    }

    // 转换像素格式
    SwsContext* sws_context = nullptr;
    int src_format;

    switch (frame.format) {
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
            LOG_ERROR("不支持的像素格式", "MjpegStreamer");
            av_frame_free(&av_frame);
            avcodec_free_context(&codec_context);
            return false;
    }

    sws_context = sws_getContext(
        frame.width, frame.height, (AVPixelFormat)src_format,
        frame.width, frame.height, codec_context->pix_fmt,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!sws_context) {
        LOG_ERROR("无法创建SwsContext", "MjpegStreamer");
        av_frame_free(&av_frame);
        avcodec_free_context(&codec_context);
        return false;
    }

    // 转换像素格式
    const uint8_t* src_data[4] = {frame.data.data(), nullptr, nullptr, nullptr};
    int src_linesize[4] = {frame.width * (src_format == AV_PIX_FMT_YUYV422 ? 2 : (src_format == AV_PIX_FMT_NV12 ? 1 : 3)), 0, 0, 0};

    sws_scale(sws_context, src_data, src_linesize, 0, frame.height,
              av_frame->data, av_frame->linesize);

    // 编码帧
    if (avcodec_send_frame(codec_context, av_frame) < 0) {
        LOG_ERROR("发送帧到编码器失败", "MjpegStreamer");
        sws_freeContext(sws_context);
        av_frame_free(&av_frame);
        avcodec_free_context(&codec_context);
        return false;
    }

    // 获取编码后的包
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        LOG_ERROR("无法分配包", "MjpegStreamer");
        sws_freeContext(sws_context);
        av_frame_free(&av_frame);
        avcodec_free_context(&codec_context);
        return false;
    }

    int ret = avcodec_receive_packet(codec_context, packet);
    if (ret < 0) {
        LOG_ERROR("从编码器接收包失败", "MjpegStreamer");
        av_packet_free(&packet);
        sws_freeContext(sws_context);
        av_frame_free(&av_frame);
        avcodec_free_context(&codec_context);
        return false;
    }

    // 复制编码后的数据
    jpeg_data.resize(packet->size);
    memcpy(jpeg_data.data(), packet->data, packet->size);

    // 释放资源
    av_packet_free(&packet);
    sws_freeContext(sws_context);
    av_frame_free(&av_frame);
    avcodec_free_context(&codec_context);

    return true;
}

bool MjpegStreamer::resizeFrame(const camera::Frame& frame, camera::Frame& resized_frame) {
    // 创建SwsContext
    SwsContext* sws_context = nullptr;
    int src_format;

    switch (frame.format) {
        case camera::PixelFormat::YUYV:
            src_format = AV_PIX_FMT_YUYV422;
            break;
        case camera::PixelFormat::MJPEG:
            src_format = AV_PIX_FMT_YUVJ420P;
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
            LOG_ERROR("不支持的像素格式", "MjpegStreamer");
            return false;
    }

    sws_context = sws_getContext(
        frame.width, frame.height, (AVPixelFormat)src_format,
        config_.output_width, config_.output_height, (AVPixelFormat)src_format,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!sws_context) {
        LOG_ERROR("无法创建SwsContext", "MjpegStreamer");
        return false;
    }

    // 计算目标帧的数据大小
    int dst_bufsize;
    switch (frame.format) {
        case camera::PixelFormat::YUYV:
            dst_bufsize = config_.output_width * config_.output_height * 2;
            break;
        case camera::PixelFormat::RGB24:
        case camera::PixelFormat::BGR24:
            dst_bufsize = config_.output_width * config_.output_height * 3;
            break;
        case camera::PixelFormat::NV12:
            dst_bufsize = config_.output_width * config_.output_height * 3 / 2;
            break;
        case camera::PixelFormat::MJPEG:
            // 对于MJPEG，我们不能简单地计算大小，这里简化处理
            dst_bufsize = frame.data.size();
            break;
        default:
            sws_freeContext(sws_context);
            return false;
    }

    // 准备目标帧
    resized_frame.width = config_.output_width;
    resized_frame.height = config_.output_height;
    resized_frame.format = frame.format;
    resized_frame.timestamp = frame.timestamp;
    resized_frame.data.resize(dst_bufsize);

    // 转换
    const uint8_t* src_data[4] = {frame.data.data(), nullptr, nullptr, nullptr};
    uint8_t* dst_data[4] = {resized_frame.data.data(), nullptr, nullptr, nullptr};

    int src_linesize[4], dst_linesize[4];

    // 计算行大小
    switch (frame.format) {
        case camera::PixelFormat::YUYV:
            src_linesize[0] = frame.width * 2;
            dst_linesize[0] = config_.output_width * 2;
            break;
        case camera::PixelFormat::RGB24:
        case camera::PixelFormat::BGR24:
            src_linesize[0] = frame.width * 3;
            dst_linesize[0] = config_.output_width * 3;
            break;
        case camera::PixelFormat::NV12:
            src_linesize[0] = frame.width;
            dst_linesize[0] = config_.output_width;
            break;
        case camera::PixelFormat::MJPEG:
            // 对于MJPEG，我们不能简单地计算行大小，这里简化处理
            src_linesize[0] = frame.width;
            dst_linesize[0] = config_.output_width;
            break;
        default:
            sws_freeContext(sws_context);
            return false;
    }

    src_linesize[1] = src_linesize[2] = src_linesize[3] = 0;
    dst_linesize[1] = dst_linesize[2] = dst_linesize[3] = 0;

    sws_scale(sws_context, src_data, src_linesize, 0, frame.height,
              dst_data, dst_linesize);

    sws_freeContext(sws_context);
    return true;
}

} // namespace api
} // namespace cam_server
