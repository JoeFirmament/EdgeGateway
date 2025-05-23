#include "vision/frame_processor.h"
#include <iostream>

namespace cam_server {
namespace vision {

ProcessingResult HomographyProcessor::process(const cv::Mat& input_frame) {
    ProcessingResult result;
    
    if (!matrix_valid_) {
        // 如果没有有效的变换矩阵，直接返回原始帧
        result.processed_frame = input_frame.clone();
        result.metadata = R"({"type":"homography","status":"no_matrix","processing":"passthrough"})";
        result.processing_time_ms = 0.1;
        result.success = true;
        return result;
    }

    try {
        cv::Mat output_frame;
        cv::warpPerspective(input_frame, output_frame, homography_matrix_, output_size_);
        
        result.processed_frame = output_frame;
        result.metadata = R"({"type":"homography","status":"transformed","processing":"perspective_warp"})";
        result.processing_time_ms = 2.0;
        result.success = true;
        
    } catch (const std::exception& e) {
        result.processed_frame = input_frame.clone();
        result.metadata = R"({"type":"homography","status":"error","processing":"failed"})";
        result.processing_time_ms = 0.0;
        result.success = false;
    }

    return result;
}

std::string HomographyProcessor::getConfig() const {
    return R"({
        "src_points": [
            {"x": 0, "y": 0},
            {"x": 640, "y": 0},
            {"x": 640, "y": 480},
            {"x": 0, "y": 480}
        ],
        "dst_points": [
            {"x": 50, "y": 50},
            {"x": 590, "y": 50},
            {"x": 590, "y": 430},
            {"x": 50, "y": 430}
        ],
        "output_width": 640,
        "output_height": 480
    })";
}

bool HomographyProcessor::setConfig(const std::string& config) {
    // 简单实现：设置默认点
    setDefaultPoints();
    return calculateHomography();
}

bool HomographyProcessor::initialize() {
    setDefaultPoints();
    return calculateHomography();
}

void HomographyProcessor::cleanup() {
    matrix_valid_ = false;
}

bool HomographyProcessor::calculateHomography() {
    if (src_points_.size() != 4 || dst_points_.size() != 4) {
        return false;
    }

    try {
        homography_matrix_ = cv::getPerspectiveTransform(src_points_, dst_points_);
        matrix_valid_ = true;
        std::cout << "Homography matrix calculated successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to calculate homography: " << e.what() << std::endl;
        matrix_valid_ = false;
        return false;
    }
}

void HomographyProcessor::setDefaultPoints() {
    // 设置默认的源点和目标点
    src_points_ = {
        cv::Point2f(0, 0),
        cv::Point2f(640, 0),
        cv::Point2f(640, 480),
        cv::Point2f(0, 480)
    };

    dst_points_ = {
        cv::Point2f(50, 50),
        cv::Point2f(590, 50),
        cv::Point2f(590, 430),
        cv::Point2f(50, 430)
    };
}

} // namespace vision
} // namespace cam_server
