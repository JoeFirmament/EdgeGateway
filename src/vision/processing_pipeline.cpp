#include "vision/frame_processor.h"
#include <iostream>
#include <mutex>

namespace cam_server {
namespace vision {

ProcessingPipeline::ProcessingPipeline() : active_processor_("raw") {
    // 注册默认的原始帧处理器
    registerProcessor("raw", std::make_unique<RawPassProcessor>());
}

ProcessingPipeline::~ProcessingPipeline() {
    std::lock_guard<std::mutex> lock(pipeline_mutex_);
    processors_.clear();
}

bool ProcessingPipeline::registerProcessor(const std::string& name,
                                         std::unique_ptr<FrameProcessor> processor) {
    if (!processor) {
        return false;
    }

    std::lock_guard<std::mutex> lock(pipeline_mutex_);
    
    // 初始化处理器
    if (!processor->initialize()) {
        std::cerr << "Failed to initialize processor: " << name << std::endl;
        return false;
    }

    processors_[name] = std::move(processor);
    std::cout << "Registered processor: " << name << std::endl;
    return true;
}

bool ProcessingPipeline::setActiveProcessor(const std::string& name) {
    std::lock_guard<std::mutex> lock(pipeline_mutex_);
    
    if (processors_.find(name) == processors_.end()) {
        std::cerr << "Processor not found: " << name << std::endl;
        return false;
    }

    active_processor_ = name;
    std::cout << "Set active processor: " << name << std::endl;
    return true;
}

std::string ProcessingPipeline::getActiveProcessor() const {
    std::lock_guard<std::mutex> lock(pipeline_mutex_);
    return active_processor_;
}

std::vector<std::string> ProcessingPipeline::getAvailableProcessors() const {
    std::lock_guard<std::mutex> lock(pipeline_mutex_);
    std::vector<std::string> names;
    for (const auto& [name, processor] : processors_) {
        names.push_back(name);
    }
    return names;
}

ProcessingResult ProcessingPipeline::processFrame(const cv::Mat& input_frame) {
    std::lock_guard<std::mutex> lock(pipeline_mutex_);
    
    auto it = processors_.find(active_processor_);
    if (it == processors_.end()) {
        ProcessingResult result;
        result.success = false;
        result.metadata = R"({"error":"Active processor not found"})";
        return result;
    }

    return it->second->process(input_frame);
}

std::string ProcessingPipeline::getProcessorConfig(const std::string& name) const {
    std::lock_guard<std::mutex> lock(pipeline_mutex_);
    
    auto it = processors_.find(name);
    if (it == processors_.end()) {
        return "{}";
    }

    return it->second->getConfig();
}

bool ProcessingPipeline::setProcessorConfig(const std::string& name, const std::string& config) {
    std::lock_guard<std::mutex> lock(pipeline_mutex_);
    
    auto it = processors_.find(name);
    if (it == processors_.end()) {
        return false;
    }

    return it->second->setConfig(config);
}

// ProcessorFactory implementation
std::unique_ptr<FrameProcessor> ProcessorFactory::createProcessor(const std::string& type) {
    if (type == "raw") {
        return std::make_unique<RawPassProcessor>();
    } else if (type == "yolo") {
        return std::make_unique<YOLOProcessor>();
    } else if (type == "homography") {
        return std::make_unique<HomographyProcessor>();
    }
    
    return nullptr;
}

std::vector<std::string> ProcessorFactory::getSupportedTypes() {
    return {"raw", "yolo", "homography"};
}

} // namespace vision
} // namespace cam_server
