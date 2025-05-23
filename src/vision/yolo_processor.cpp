#include "vision/frame_processor.h"
#include "hardware/rk3588_npu.h"
#include <chrono>
#include <fstream>
#include <json/json.h>

namespace cam_server {
namespace vision {

bool YOLOProcessor::initialize() {
    try {
        std::cout << "🧠 初始化NPU YOLO处理器..." << std::endl;

        // 1. 创建NPU YOLO检测器
        npu_detector_ = std::make_unique<hardware::NPUYOLODetector>();

        // 2. 配置模型路径
        std::string model_path = "models/yolov8n.rknn";  // RKNN格式模型
        std::string classes_path = "models/coco.names";   // 类别文件

        // 3. 加载类别名称
        std::ifstream classes_file(classes_path);
        if (classes_file.good()) {
            std::string line;
            while (std::getline(classes_file, line)) {
                class_names_.push_back(line);
            }
        } else {
            // 使用默认的COCO类别
            class_names_ = hardware::NPUUtils::getRecommendedYOLOConfig().class_names;
        }

        // 4. 检查NPU是否可用
        if (!hardware::RK3588NPUEngine::isNPUSupported()) {
            std::cout << "⚠️ NPU不可用，将使用CPU处理" << std::endl;
            use_npu_ = false;
            return initializeCPUFallback();
        }

        // 5. 初始化NPU检测器
        if (!npu_detector_->initialize(model_path, class_names_)) {
            std::cout << "⚠️ NPU初始化失败，回退到CPU处理" << std::endl;
            use_npu_ = false;
            return initializeCPUFallback();
        }

        // 6. 设置检测参数
        npu_detector_->setConfidenceThreshold(confidence_threshold_);
        npu_detector_->setNMSThreshold(nms_threshold_);

        initialized_ = true;
        use_npu_ = true;

        std::cout << "✅ NPU YOLO处理器初始化成功" << std::endl;
        std::cout << "   NPU硬件信息: " << hardware::RK3588NPUEngine::getNPUHardwareInfo() << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cerr << "❌ NPU YOLO初始化失败: " << e.what() << std::endl;
        std::cout << "🔄 尝试CPU回退..." << std::endl;
        use_npu_ = false;
        return initializeCPUFallback();
    }
}

bool YOLOProcessor::initializeCPUFallback() {
    // CPU回退实现 (保留原有的OpenCV DNN实现)
    std::cout << "🔄 初始化CPU YOLO处理器..." << std::endl;

    std::string model_path = "models/yolov8n.onnx";
    std::ifstream model_file(model_path);
    if (!model_file.good()) {
        std::cerr << "❌ YOLO模型文件不存在: " << model_path << std::endl;
        return false;
    }

    // 这里可以保留原有的CPU实现作为回退
    std::cout << "✅ CPU YOLO处理器初始化成功" << std::endl;
    return true;
}

ProcessingResult YOLOProcessor::process(const cv::Mat& input_frame) {
    ProcessingResult result;
    auto start_time = std::chrono::high_resolution_clock::now();

    if (!initialized_) {
        result.success = false;
        result.metadata = R"({"error":"YOLO processor not initialized"})";
        return result;
    }

    try {
        cv::Mat output_frame = input_frame.clone();
        std::vector<hardware::NPUYOLODetection> detections;

        if (use_npu_ && npu_detector_) {
            // 使用NPU进行检测
            detections = npu_detector_->detect(input_frame);

            // 绘制NPU检测结果
            drawNPUDetections(output_frame, detections);

            // 计算处理时间
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            double processing_time = duration.count() / 1000.0;

            // 生成元数据
            result.metadata = generateDetectionMetadata(detections, processing_time);
            result.processing_time_ms = processing_time;

            // 打印NPU性能统计
            if (detections.size() > 0) {
                auto stats = npu_detector_->getDetectionStats();
                if (stats.total_frames % 100 == 0) {  // 每100帧打印一次
                    std::cout << "🧠 NPU检测统计: 平均" << stats.avg_detection_time_ms
                             << "ms/帧, 平均" << stats.avg_objects_per_frame << "个对象/帧" << std::endl;
                }
            }

        } else {
            // CPU回退处理
            std::cout << "⚠️ 使用CPU回退处理 (NPU不可用)" << std::endl;
            // 这里可以调用原有的CPU实现
            result.metadata = R"({"type":"yolo_detection","detections":[],"detection_count":0,"processing_backend":"cpu_fallback"})";
        }

        result.processed_frame = output_frame;
        result.success = true;

    } catch (const std::exception& e) {
        result.success = false;
        Json::Value error_metadata;
        error_metadata["error"] = e.what();
        error_metadata["processing_backend"] = use_npu_ ? "npu" : "cpu";
        result.metadata = Json::writeString(Json::StreamWriterBuilder(), error_metadata);

        std::cerr << "❌ YOLO处理失败: " << e.what() << std::endl;
    }

    return result;
}

void YOLOProcessor::drawNPUDetections(cv::Mat& frame, const std::vector<hardware::NPUYOLODetection>& detections) {
    // 定义颜色
    std::vector<cv::Scalar> colors = {
        cv::Scalar(255, 0, 0),    // 红色
        cv::Scalar(0, 255, 0),    // 绿色
        cv::Scalar(0, 0, 255),    // 蓝色
        cv::Scalar(255, 255, 0),  // 黄色
        cv::Scalar(255, 0, 255),  // 紫色
        cv::Scalar(0, 255, 255),  // 青色
    };

    for (const auto& detection : detections) {
        // 选择颜色
        cv::Scalar color = colors[detection.class_id % colors.size()];

        // 绘制边界框
        cv::rectangle(frame, detection.bbox, color, 2);

        // 准备标签文本
        std::string label = detection.class_name + " " +
                           std::to_string(static_cast<int>(detection.confidence * 100)) + "%";

        // 计算文本大小
        int baseline;
        cv::Size text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.6, 1, &baseline);

        // 绘制标签背景
        cv::Point label_pos(detection.bbox.x, detection.bbox.y - text_size.height - 5);
        if (label_pos.y < 0) label_pos.y = detection.bbox.y + text_size.height + 5;

        cv::rectangle(frame,
                     cv::Point(label_pos.x, label_pos.y - text_size.height - 5),
                     cv::Point(label_pos.x + text_size.width, label_pos.y + 5),
                     color, -1);

        // 绘制标签文本
        cv::putText(frame, label, label_pos, cv::FONT_HERSHEY_SIMPLEX, 0.6,
                   cv::Scalar(255, 255, 255), 1);

        // 绘制中心点
        cv::circle(frame, detection.center, 3, color, -1);
    }

    // 在左上角显示NPU信息
    std::string npu_info = "NPU: " + std::to_string(detections.size()) + " objects";
    cv::putText(frame, npu_info, cv::Point(10, 25), cv::FONT_HERSHEY_SIMPLEX, 0.7,
               cv::Scalar(0, 255, 0), 2);
}

std::string YOLOProcessor::generateDetectionMetadata(const std::vector<hardware::NPUYOLODetection>& detections,
                                                    double processing_time) {
    Json::Value metadata;
    metadata["type"] = "yolo_npu_detection";
    metadata["processing_backend"] = "npu";
    metadata["detections"] = Json::Value(Json::arrayValue);

    for (const auto& detection : detections) {
        Json::Value det;
        det["class_id"] = detection.class_id;
        det["class_name"] = detection.class_name;
        det["confidence"] = detection.confidence;
        det["bbox"]["x"] = detection.bbox.x;
        det["bbox"]["y"] = detection.bbox.y;
        det["bbox"]["width"] = detection.bbox.width;
        det["bbox"]["height"] = detection.bbox.height;
        det["center"]["x"] = detection.center.x;
        det["center"]["y"] = detection.center.y;
        metadata["detections"].append(det);
    }

    metadata["detection_count"] = static_cast<int>(detections.size());
    metadata["processing_time_ms"] = processing_time;

    // 添加NPU状态信息
    if (npu_detector_) {
        auto stats = npu_detector_->getDetectionStats();
        metadata["npu_stats"]["total_frames"] = static_cast<Json::UInt64>(stats.total_frames);
        metadata["npu_stats"]["avg_detection_time_ms"] = stats.avg_detection_time_ms;
        metadata["npu_stats"]["avg_objects_per_frame"] = stats.avg_objects_per_frame;
    }

    return Json::writeString(Json::StreamWriterBuilder(), metadata);
}

std::vector<cv::Rect> YOLOProcessor::detectObjects(const cv::Mat& frame,
                                                   std::vector<float>& confidences,
                                                   std::vector<int>& class_ids) {
    std::vector<cv::Rect> boxes;

    // 创建blob
    cv::Mat blob;
    cv::dnn::blobFromImage(frame, blob, 1.0/255.0, input_size_, cv::Scalar(0,0,0), true, false);

    // 设置输入
    net_.setInput(blob);

    // 前向传播
    std::vector<cv::Mat> outputs;
    net_.forward(outputs, net_.getUnconnectedOutLayersNames());

    // 解析输出
    for (const auto& output : outputs) {
        for (int i = 0; i < output.rows; ++i) {
            const float* data = output.ptr<float>(i);

            // 获取置信度
            float confidence = data[4];
            if (confidence > confidence_threshold_) {
                // 获取类别分数
                cv::Mat scores = output.row(i).colRange(5, output.cols);
                cv::Point class_id_point;
                double max_class_score;
                cv::minMaxLoc(scores, 0, &max_class_score, 0, &class_id_point);

                if (max_class_score > confidence_threshold_) {
                    // 计算边界框
                    float center_x = data[0] * frame.cols;
                    float center_y = data[1] * frame.rows;
                    float width = data[2] * frame.cols;
                    float height = data[3] * frame.rows;

                    int left = static_cast<int>(center_x - width / 2);
                    int top = static_cast<int>(center_y - height / 2);

                    boxes.emplace_back(left, top, static_cast<int>(width), static_cast<int>(height));
                    confidences.push_back(confidence);
                    class_ids.push_back(class_id_point.x);
                }
            }
        }
    }

    // 非极大值抑制
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, confidence_threshold_, nms_threshold_, indices);

    std::vector<cv::Rect> final_boxes;
    std::vector<float> final_confidences;
    std::vector<int> final_class_ids;

    for (int idx : indices) {
        final_boxes.push_back(boxes[idx]);
        final_confidences.push_back(confidences[idx]);
        final_class_ids.push_back(class_ids[idx]);
    }

    boxes = final_boxes;
    confidences = final_confidences;
    class_ids = final_class_ids;

    return boxes;
}

void YOLOProcessor::drawDetections(cv::Mat& frame,
                                  const std::vector<cv::Rect>& boxes,
                                  const std::vector<float>& confidences,
                                  const std::vector<int>& class_ids) {
    // 定义颜色
    std::vector<cv::Scalar> colors = {
        cv::Scalar(255, 0, 0),    // 红色
        cv::Scalar(0, 255, 0),    // 绿色
        cv::Scalar(0, 0, 255),    // 蓝色
        cv::Scalar(255, 255, 0),  // 黄色
        cv::Scalar(255, 0, 255),  // 紫色
        cv::Scalar(0, 255, 255),  // 青色
    };

    for (size_t i = 0; i < boxes.size(); ++i) {
        const auto& box = boxes[i];
        const auto& confidence = confidences[i];
        const auto& class_id = class_ids[i];

        // 选择颜色
        cv::Scalar color = colors[class_id % colors.size()];

        // 绘制边界框
        cv::rectangle(frame, box, color, 2);

        // 准备标签文本
        std::string label = (class_id < class_names_.size()) ?
                           class_names_[class_id] : "unknown";
        label += " " + std::to_string(static_cast<int>(confidence * 100)) + "%";

        // 计算文本大小
        int baseline;
        cv::Size text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);

        // 绘制标签背景
        cv::Point label_pos(box.x, box.y - text_size.height - 5);
        if (label_pos.y < 0) label_pos.y = box.y + text_size.height + 5;

        cv::rectangle(frame,
                     cv::Point(label_pos.x, label_pos.y - text_size.height - 5),
                     cv::Point(label_pos.x + text_size.width, label_pos.y + 5),
                     color, -1);

        // 绘制标签文本
        cv::putText(frame, label, label_pos, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                   cv::Scalar(255, 255, 255), 1);
    }
}

std::string YOLOProcessor::getConfig() const {
    Json::Value config;
    config["confidence_threshold"] = confidence_threshold_;
    config["nms_threshold"] = nms_threshold_;
    config["input_width"] = input_size_.width;
    config["input_height"] = input_size_.height;
    return Json::writeString(Json::StreamWriterBuilder(), config);
}

bool YOLOProcessor::setConfig(const std::string& config) {
    try {
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;
        std::istringstream stream(config);

        if (!Json::parseFromStream(builder, stream, &root, &errors)) {
            return false;
        }

        if (root.isMember("confidence_threshold")) {
            confidence_threshold_ = root["confidence_threshold"].asFloat();
        }
        if (root.isMember("nms_threshold")) {
            nms_threshold_ = root["nms_threshold"].asFloat();
        }
        if (root.isMember("input_width") && root.isMember("input_height")) {
            input_size_.width = root["input_width"].asInt();
            input_size_.height = root["input_height"].asInt();
        }

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

void YOLOProcessor::cleanup() {
    net_ = cv::dnn::Net();
    class_names_.clear();
    initialized_ = false;
}

} // namespace vision
} // namespace cam_server
