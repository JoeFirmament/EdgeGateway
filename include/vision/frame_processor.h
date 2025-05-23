#pragma once

#include <memory>
#include <string>
#include <functional>
#include <opencv2/opencv.hpp>
#include "camera/frame.h"

namespace cam_server {
namespace vision {

/**
 * @brief 帧处理结果
 */
struct ProcessingResult {
    cv::Mat processed_frame;           // 处理后的帧
    std::string metadata;              // 处理元数据 (JSON格式)
    double processing_time_ms;         // 处理耗时
    bool success;                      // 处理是否成功

    ProcessingResult() : processing_time_ms(0.0), success(false) {}
};

/**
 * @brief 抽象帧处理器基类
 */
class FrameProcessor {
public:
    virtual ~FrameProcessor() = default;

    /**
     * @brief 处理单帧图像
     * @param input_frame 输入帧
     * @return 处理结果
     */
    virtual ProcessingResult process(const cv::Mat& input_frame) = 0;

    /**
     * @brief 获取处理器名称
     */
    virtual std::string getName() const = 0;

    /**
     * @brief 获取处理器配置
     */
    virtual std::string getConfig() const = 0;

    /**
     * @brief 设置处理器配置
     */
    virtual bool setConfig(const std::string& config) = 0;

    /**
     * @brief 初始化处理器
     */
    virtual bool initialize() = 0;

    /**
     * @brief 清理资源
     */
    virtual void cleanup() = 0;
};

/**
 * @brief 原始帧直通处理器
 */
class RawPassProcessor : public FrameProcessor {
public:
    ProcessingResult process(const cv::Mat& input_frame) override {
        ProcessingResult result;
        result.processed_frame = input_frame.clone();
        result.metadata = R"({"type":"raw","processing":"none"})";
        result.processing_time_ms = 0.0;
        result.success = true;
        return result;
    }

    std::string getName() const override { return "raw_pass"; }
    std::string getConfig() const override { return "{}"; }
    bool setConfig(const std::string& config) override { return true; }
    bool initialize() override { return true; }
    void cleanup() override {}
};

/**
 * @brief 简单的YOLO目标检测处理器 (基础版本)
 */
class YOLOProcessor : public FrameProcessor {
private:
    std::vector<std::string> class_names_;
    float confidence_threshold_;
    float nms_threshold_;
    cv::Size input_size_;
    bool initialized_;

public:
    YOLOProcessor() : confidence_threshold_(0.5f), nms_threshold_(0.4f),
                      input_size_(640, 640), initialized_(false) {}

    ProcessingResult process(const cv::Mat& input_frame) override {
        ProcessingResult result;
        result.processed_frame = input_frame.clone();
        result.metadata = R"({"type":"yolo_detection","detections":[],"detection_count":0,"processing":"placeholder"})";
        result.processing_time_ms = 1.0;
        result.success = true;
        return result;
    }

    std::string getName() const override { return "yolo_detector"; }
    std::string getConfig() const override { return "{}"; }
    bool setConfig(const std::string& config) override { return true; }
    bool initialize() override { initialized_ = true; return true; }
    void cleanup() override { initialized_ = false; }
};

/**
 * @brief OpenCV Homography透视变换处理器
 */
class HomographyProcessor : public FrameProcessor {
private:
    cv::Mat homography_matrix_;
    cv::Size output_size_;
    std::vector<cv::Point2f> src_points_;
    std::vector<cv::Point2f> dst_points_;
    bool matrix_valid_;

public:
    HomographyProcessor() : output_size_(640, 480), matrix_valid_(false) {}

    ProcessingResult process(const cv::Mat& input_frame) override;
    std::string getName() const override { return "homography_transform"; }
    std::string getConfig() const override;
    bool setConfig(const std::string& config) override;
    bool initialize() override;
    void cleanup() override;

private:
    bool calculateHomography();
    void setDefaultPoints();
};

/**
 * @brief 处理管道管理器
 */
class ProcessingPipeline {
private:
    std::map<std::string, std::unique_ptr<FrameProcessor>> processors_;
    std::string active_processor_;
    mutable std::mutex pipeline_mutex_;

public:
    ProcessingPipeline();
    ~ProcessingPipeline();

    /**
     * @brief 注册处理器
     */
    bool registerProcessor(const std::string& name,
                          std::unique_ptr<FrameProcessor> processor);

    /**
     * @brief 设置活动处理器
     */
    bool setActiveProcessor(const std::string& name);

    /**
     * @brief 获取活动处理器名称
     */
    std::string getActiveProcessor() const;

    /**
     * @brief 获取可用处理器列表
     */
    std::vector<std::string> getAvailableProcessors() const;

    /**
     * @brief 处理帧
     */
    ProcessingResult processFrame(const cv::Mat& input_frame);

    /**
     * @brief 获取处理器配置
     */
    std::string getProcessorConfig(const std::string& name) const;

    /**
     * @brief 设置处理器配置
     */
    bool setProcessorConfig(const std::string& name, const std::string& config);
};

/**
 * @brief 处理器工厂
 */
class ProcessorFactory {
public:
    static std::unique_ptr<FrameProcessor> createProcessor(const std::string& type);
    static std::vector<std::string> getSupportedTypes();
};

} // namespace vision
} // namespace cam_server
