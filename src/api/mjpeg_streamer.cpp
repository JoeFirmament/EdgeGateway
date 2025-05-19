#include "api/mjpeg_streamer.h"
#include "camera/camera_manager.h"
#include "monitor/logger.h"
#include "utils/time_utils.h"

#include <chrono>
#include <algorithm>
#include <iostream>  // for std::cerr
#include <cstring>   // for memcpy

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
    : is_initialized_(false),
      is_running_(false),
      current_fps_(0.0),
      frame_count_(0),
      clients_(),
      camera_clients_(),
      clients_mutex_(),
      last_fps_time_() {
    std::cerr << "[MJPEG][mjpeg_streamer.cpp:MjpegStreamer] 创建 MjpegStreamer 实例" << std::endl;
}

MjpegStreamer::~MjpegStreamer() {
    std::cerr << "[MJPEG][mjpeg_streamer.cpp:~MjpegStreamer] 销毁 MjpegStreamer 实例" << std::endl;
    stop();
}

bool MjpegStreamer::initialize(const MjpegStreamerConfig& config) {
    std::cerr << "[MJPEG][mjpeg_streamer.cpp:initialize] 开始初始化MJPEG流处理器..." << std::endl;

    if (is_initialized_) {
        std::cerr << "[MJPEG][mjpeg_streamer.cpp:initialize] MJPEG流处理器已经初始化" << std::endl;
        return true;
    }

    config_ = config;

    // 设置默认值
    if (config_.jpeg_quality <= 0 || config_.jpeg_quality > 100) {
        std::cerr << "[MJPEG][mjpeg_streamer.cpp:initialize] 设置默认JPEG质量: 80" << std::endl;
        config_.jpeg_quality = 80;
    }
    if (config_.max_fps <= 0) {
        std::cerr << "[MJPEG][mjpeg_streamer.cpp:initialize] 设置默认最大帧率: 30" << std::endl;
        config_.max_fps = 30;
    }
    // 强制限制最大客户端数量为2，确保系统稳定
    if (config_.max_clients <= 0 || config_.max_clients > 2) {
        std::cerr << "[MJPEG][mjpeg_streamer.cpp:initialize] 设置默认最大客户端数: 2" << std::endl;
        LOG_INFO("限制最大客户端数为2，以确保系统稳定性", "MjpegStreamer");
        config_.max_clients = 2;
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
                             const std::string& camera_id,
                             std::function<void(const std::vector<uint8_t>&)> frame_callback,
                             std::function<void(const std::string&)> error_callback,
                             std::function<void()> close_callback) {
    std::cerr << "[MJPEG][mjpeg_streamer.cpp:addClient] 客户端连接建立 - "
              << "ID: " << client_id 
              << ", 摄像头: " << (camera_id.empty() ? "无" : camera_id) 
              << ", 回调函数: "
              << "frame:" << (void*)&frame_callback << " "
              << "error:" << (void*)&error_callback << " "
              << "close:" << (void*)&close_callback << std::endl;
    if (!is_running_) {
        LOG_ERROR("MJPEG流处理器未运行", "MjpegStreamer");
        return false;
    }

    std::lock_guard<std::mutex> lock(clients_mutex_);

    // 检查客户端数量是否已达上限
    if (clients_.size() >= static_cast<size_t>(config_.max_clients)) {
        LOG_ERROR("客户端数量已达上限: " + std::to_string(clients_.size()) +
                 "/" + std::to_string(config_.max_clients) +
                 "，拒绝新客户端: " + client_id, "MjpegStreamer");

        // 如果有错误回调，通知客户端
        if (error_callback) {
            error_callback("服务器已达到最大连接数限制，请稍后再试");
        }

        return false;
    }

    // 检查摄像头是否已有客户端连接
    if (!camera_id.empty() && camera_clients_.find(camera_id) != camera_clients_.end() &&
        !camera_clients_[camera_id].empty()) {
        // 每个摄像头只允许一个客户端连接
        LOG_WARNING("摄像头 " + camera_id + " 已有客户端连接，拒绝新客户端: " + client_id, "MjpegStreamer");

        // 如果有错误回调，通知客户端
        if (error_callback) {
            error_callback("该摄像头已被其他用户使用，请选择其他摄像头或稍后再试");
        }

        return false;
    }

    // 检查客户端ID是否已存在
    if (clients_.find(client_id) != clients_.end()) {
        LOG_WARNING("客户端ID已存在，将替换现有客户端: " + client_id, "MjpegStreamer");

        // 调用现有客户端的关闭回调
        if (clients_[client_id]->close_callback) {
            clients_[client_id]->close_callback();
        }

        // 从摄像头客户端映射中移除
        std::string old_camera_id = clients_[client_id]->camera_id;
        if (!old_camera_id.empty() && camera_clients_.find(old_camera_id) != camera_clients_.end()) {
            camera_clients_[old_camera_id].erase(client_id);
            LOG_INFO("将客户端 " + client_id + " 从摄像头 " + old_camera_id + " 解除关联", "MjpegStreamer");

            // 如果摄像头没有关联的客户端，从映射中移除
            if (camera_clients_[old_camera_id].empty()) {
                camera_clients_.erase(old_camera_id);
                LOG_INFO("摄像头 " + old_camera_id + " 没有关联的客户端，从映射中移除", "MjpegStreamer");
            }
        }

        // 从客户端列表中移除
        clients_.erase(client_id);
    }

    // 创建新客户端
    auto client = std::make_shared<MjpegClient>();
    client->id = client_id;
    client->camera_id = camera_id;
    client->frame_callback = frame_callback;
    client->error_callback = error_callback;
    client->close_callback = close_callback;
    client->last_frame_time = utils::TimeUtils::getCurrentTimeMicros();
    client->last_activity_time = utils::TimeUtils::getCurrentTimeMicros();

    // 添加到客户端列表
    clients_[client_id] = client;

    // 添加到摄像头客户端映射
    if (!camera_id.empty()) {
        camera_clients_[camera_id].insert(client_id);
        LOG_INFO("将客户端 " + client_id + " 关联到摄像头 " + camera_id, "MjpegStreamer");
    }

    LOG_INFO("添加MJPEG客户端: " + client_id +
             "，当前客户端数量: " + std::to_string(clients_.size()) +
             "/" + std::to_string(config_.max_clients), "MjpegStreamer");

    // 输出当前所有客户端ID，便于调试
    std::string client_list = "当前客户端列表: ";
    for (const auto& pair : clients_) {
        client_list += pair.first + ", ";
    }
    LOG_DEBUG(client_list, "MjpegStreamer");

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

    // 从摄像头客户端映射中移除
    std::string camera_id = it->second->camera_id;
    if (!camera_id.empty() && camera_clients_.find(camera_id) != camera_clients_.end()) {
        camera_clients_[camera_id].erase(client_id);
        LOG_INFO("将客户端 " + client_id + " 从摄像头 " + camera_id + " 解除关联", "MjpegStreamer");

        // 如果摄像头没有关联的客户端，从映射中移除
        if (camera_clients_[camera_id].empty()) {
            camera_clients_.erase(camera_id);
            LOG_INFO("摄像头 " + camera_id + " 没有关联的客户端，从映射中移除", "MjpegStreamer");
        }
    }

    // 从客户端列表中移除
    clients_.erase(it);

    LOG_INFO("移除MJPEG客户端: " + client_id +
             "，当前客户端数量: " + std::to_string(clients_.size()) +
             "/" + std::to_string(config_.max_clients), "MjpegStreamer");
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
    if (!is_running_) {
        return;
    }

    try {
        std::cerr << "[MJPEG][mjpeg_streamer.cpp:handleFrame] 开始处理帧 - "
              << "分辨率: " << frame.getWidth() << "x" << frame.getHeight()
              << ", 客户端数: " << clients_.size() << std::endl;

        // 编码为JPEG
        std::vector<uint8_t> jpeg_data;
        if (!encodeToJpeg(frame, jpeg_data)) {
            std::cerr << "[MJPEG][mjpeg_streamer.cpp:handleFrame] JPEG编码失败" << std::endl;
            LOG_ERROR("JPEG编码失败", "MjpegStreamer");
            return;
        }


        // 更新帧率统计
        frame_count_++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_fps_time_).count();
        if (elapsed >= 1) {
            current_fps_ = static_cast<double>(frame_count_) / elapsed;
            frame_count_ = 0;
            last_fps_time_ = now;
            }

        // 发送到所有客户端
        std::vector<std::shared_ptr<MjpegClient>> clients_copy;
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            for (const auto& pair : clients_) {
                clients_copy.push_back(pair.second);
            }
        }

        for (auto& client : clients_copy) {
            try {
                // 检查客户端是否仍然有效
                bool client_valid = false;
                {
                    std::lock_guard<std::mutex> lock(clients_mutex_);
                    client_valid = clients_.find(client->id) != clients_.end();
                }

                if (!client_valid) {
                    std::cerr << "[MJPEG][mjpeg_streamer.cpp:handleFrame] 客户端已断开 - ID: " << client->id << std::endl;
                    continue;
                }

                if (!client->frame_callback) {
                    LOG_WARNING("客户端回调无效: " + client->id, "MjpegStreamer");
                    continue;
                }

                try {
                    // 执行回调
                    client->frame_callback(jpeg_data);
                    client->last_frame_time = utils::TimeUtils::getCurrentTimestamp();
                } catch (const std::exception& e) {
                    std::cerr << "[MJPEG][mjpeg_streamer.cpp:handleFrame] 回调执行异常 - "
                              << "客户端: " << client->id 
                              << ", 错误: " << e.what() << std::endl;
                    removeClient(client->id);
                }
            } catch (const std::exception& e) {
                std::cerr << "[MJPEG][mjpeg_streamer.cpp:handleFrame] 处理客户端异常 - "
                          << "客户端: " << client->id 
                          << ", 错误: " << e.what() << std::endl;
                removeClient(client->id);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[MJPEG][mjpeg_streamer.cpp:handleFrame] 处理帧时发生异常: " << e.what() << std::endl;
        LOG_ERROR("处理帧时发生异常: " + std::string(e.what()), "MjpegStreamer");
    }
}

bool MjpegStreamer::encodeToJpeg(const camera::Frame& frame, std::vector<uint8_t>& jpeg_data) {
    // 打印详细的帧信息

    // 验证输入数据
    if (frame.getData().empty()) {
        LOG_ERROR("输入帧数据为空", "MjpegStreamer");
        return false;
    }

    // 检查数据头部是否为JPEG魔数
    if (frame.getData().size() >= 2 && 
        frame.getData()[0] == 0xFF && 
        frame.getData()[1] == 0xD8) {
        
        
        // 直接使用数据
        jpeg_data = frame.getData();
        return true;
    }

    // 如果是YUYV格式，需要转换为JPEG
    if (frame.getFormat() == camera::PixelFormat::YUYV) {
        AVCodecContext* codec_ctx = nullptr;
        SwsContext* sws_ctx = nullptr;
        AVFrame* yuv_frame = nullptr;
        AVFrame* jpeg_frame = nullptr;
        AVPacket* packet = nullptr;
        bool success = false;

        try {
            // 创建编码器上下文
            const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
            if (!codec) {
                LOG_ERROR("找不到MJPEG编码器", "MjpegStreamer");
                return false;
            }

            codec_ctx = avcodec_alloc_context3(codec);
            if (!codec_ctx) {
                LOG_ERROR("无法创建编码器上下文", "MjpegStreamer");
                return false;
            }

            // 设置编码参数
            codec_ctx->width = frame.getWidth();
            codec_ctx->height = frame.getHeight();
            codec_ctx->time_base = (AVRational){1, 25};
            codec_ctx->pix_fmt = AV_PIX_FMT_YUVJ422P;
            codec_ctx->flags |= AV_CODEC_FLAG_QSCALE;
            codec_ctx->global_quality = FF_QP2LAMBDA * config_.jpeg_quality;

            // 打开编码器
            if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
                LOG_ERROR("无法打开编码器", "MjpegStreamer");
                throw std::runtime_error("无法打开编码器");
            }

            // 创建图像转换上下文
            sws_ctx = sws_getContext(
                frame.getWidth(), frame.getHeight(), AV_PIX_FMT_YUYV422,
                frame.getWidth(), frame.getHeight(), AV_PIX_FMT_YUVJ422P,
                SWS_BICUBIC, nullptr, nullptr, nullptr);
            if (!sws_ctx) {
                LOG_ERROR("无法创建图像转换上下文", "MjpegStreamer");
                throw std::runtime_error("无法创建图像转换上下文");
            }

            // 分配帧缓冲区
            yuv_frame = av_frame_alloc();
            jpeg_frame = av_frame_alloc();
            if (!yuv_frame || !jpeg_frame) {
                LOG_ERROR("无法分配帧缓冲区", "MjpegStreamer");
                throw std::runtime_error("无法分配帧缓冲区");
            }

            yuv_frame->width = frame.getWidth();
            yuv_frame->height = frame.getHeight();
            yuv_frame->format = AV_PIX_FMT_YUYV422;
            
            jpeg_frame->width = frame.getWidth();
            jpeg_frame->height = frame.getHeight();
            jpeg_frame->format = AV_PIX_FMT_YUVJ422P;

            // 分配图像数据缓冲区
            if (av_image_alloc(yuv_frame->data, yuv_frame->linesize,
                             frame.getWidth(), frame.getHeight(),
                             AV_PIX_FMT_YUYV422, 1) < 0) {
                LOG_ERROR("无法分配YUV图像缓冲区", "MjpegStreamer");
                throw std::runtime_error("无法分配YUV图像缓冲区");
            }
                            
            if (av_image_alloc(jpeg_frame->data, jpeg_frame->linesize,
                              frame.getWidth(), frame.getHeight(),
                              AV_PIX_FMT_YUVJ422P, 1) < 0) {
                LOG_ERROR("无法分配JPEG图像缓冲区", "MjpegStreamer");
                throw std::runtime_error("无法分配JPEG图像缓冲区");
            }

            // 复制输入数据
            memcpy(yuv_frame->data[0], frame.getData().data(), frame.getData().size());

            // 转换颜色空间
            sws_scale(sws_ctx,
                      yuv_frame->data, yuv_frame->linesize, 0, frame.getHeight(),
                      jpeg_frame->data, jpeg_frame->linesize);

            // 编码为JPEG
            packet = av_packet_alloc();
            if (!packet) {
                LOG_ERROR("无法分配AVPacket", "MjpegStreamer");
                throw std::runtime_error("无法分配AVPacket");
            }

            int ret = avcodec_send_frame(codec_ctx, jpeg_frame);
            if (ret < 0) {
                LOG_ERROR("发送帧到编码器失败", "MjpegStreamer");
                throw std::runtime_error("发送帧到编码器失败");
            }

            ret = avcodec_receive_packet(codec_ctx, packet);
            if (ret < 0) {
                LOG_ERROR("从编码器接收数据失败", "MjpegStreamer");
                throw std::runtime_error("从编码器接收数据失败");
            }

            // 复制编码后的数据
            jpeg_data.resize(packet->size);
            memcpy(jpeg_data.data(), packet->data, packet->size);
            success = true;
        } catch (const std::exception& e) {
            LOG_ERROR(std::string("JPEG编码异常: ") + e.what(), "MjpegStreamer");
            success = false;
        }

        // 清理资源
        if (yuv_frame) {
            if (yuv_frame->data[0]) {
                av_freep(&yuv_frame->data[0]);
            }
            av_frame_free(&yuv_frame);
        }
        if (jpeg_frame) {
            if (jpeg_frame->data[0]) {
                av_freep(&jpeg_frame->data[0]);
            }
            av_frame_free(&jpeg_frame);
        }
        if (packet) {
            av_packet_free(&packet);
        }
        if (sws_ctx) {
            sws_freeContext(sws_ctx);
        }
        if (codec_ctx) {
            avcodec_free_context(&codec_ctx);
        }

        return success && !jpeg_data.empty();
    }

    // 如果没有JPEG魔数，记录错误
    LOG_ERROR("无效的JPEG数据", "MjpegStreamer");
    return false;
}

bool MjpegStreamer::resizeFrame(const camera::Frame& frame, camera::Frame& resized_frame) {
    // 检查是否需要调整大小
    if (frame.getWidth() == config_.output_width && 
        frame.getHeight() == config_.output_height) {
        resized_frame = frame;
        return true;
    }

    // 创建图像转换上下文
    SwsContext* sws_ctx = sws_getContext(
        frame.getWidth(), frame.getHeight(),
        frame.getFormat() == camera::PixelFormat::YUYV ? AV_PIX_FMT_YUYV422 : AV_PIX_FMT_RGB24,
        config_.output_width, config_.output_height,
        frame.getFormat() == camera::PixelFormat::YUYV ? AV_PIX_FMT_YUYV422 : AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!sws_ctx) {
        LOG_ERROR("无法创建图像转换上下文", "MjpegStreamer");
        return false;
    }

    // 设置输出帧参数
    resized_frame.setWidth(config_.output_width);
    resized_frame.setHeight(config_.output_height);
    resized_frame.setFormat(frame.getFormat());
    resized_frame.getData().resize(config_.output_width * config_.output_height * 
        (frame.getFormat() == camera::PixelFormat::YUYV ? 2 : 3));

    // 转换图像
    const uint8_t* src_data[4] = {frame.getData().data(), nullptr, nullptr, nullptr};
    uint8_t* dst_data[4] = {resized_frame.getData().data(), nullptr, nullptr, nullptr};
    int src_linesize[4] = {frame.getWidth() * (frame.getFormat() == camera::PixelFormat::YUYV ? 2 : 3), 0, 0, 0};
    int dst_linesize[4] = {config_.output_width * (frame.getFormat() == camera::PixelFormat::YUYV ? 2 : 3), 0, 0, 0};

    sws_scale(sws_ctx, src_data, src_linesize, 0, frame.getHeight(),
              dst_data, dst_linesize);

    // 清理资源
    sws_freeContext(sws_ctx);

    return true;
}

bool MjpegStreamer::isClientConnected(const std::string& client_id) const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    return clients_.find(client_id) != clients_.end();
}

} // namespace api
} // namespace cam_server
