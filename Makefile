# 🔨 摄像头视频流服务器 Makefile
# 为什么用Makefile：提供更精细的编译控制，支持增量编译，便于调试

# 编译器和标志设置
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
DEBUG_FLAGS = -DDEBUG -O0 -g3
RELEASE_FLAGS = -DNDEBUG -O3

# 包含路径 - 为什么这样设置：支持模块化的头文件组织
# 添加third_party/crow路径以支持crow/common.h等头文件
INCLUDES = -I. -Iinclude -Isrc -Ithird_party -Ithird_party/crow

# 库链接 - 为什么需要这些库：pthread用于多线程，v4l2用于摄像头
# 注意：暂时移除OpenCV依赖，因为vision模块是占位实现
LIBS = -lpthread -lv4l2

# 目录定义
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

# 源文件列表 - 模块化组织
MAIN_SOURCES = main_server.cpp

# Web模块源文件
WEB_SOURCES = $(SRC_DIR)/web/video_server.cpp \
              $(SRC_DIR)/web/http_routes.cpp \
              $(SRC_DIR)/web/websocket_handler.cpp \
              $(SRC_DIR)/web/frame_extraction_routes.cpp \
              $(SRC_DIR)/web/system_routes.cpp \
              $(SRC_DIR)/web/serial_routes.cpp

# 核心模块源文件 (暂时排除vision模块，因为需要OpenCV)
CORE_SOURCES = $(SRC_DIR)/camera/v4l2_camera.cpp \
               $(SRC_DIR)/camera/camera_manager.cpp \
               $(SRC_DIR)/camera/frame.cpp \
               $(SRC_DIR)/camera/format_utils.cpp \
               $(SRC_DIR)/system/system_monitor.cpp \
               $(SRC_DIR)/monitor/logger.cpp \
               $(SRC_DIR)/utils/file_utils.cpp \
               $(SRC_DIR)/utils/config_manager.cpp \
               $(SRC_DIR)/utils/string_utils.cpp

# 所有源文件
ALL_SOURCES = $(MAIN_SOURCES) $(WEB_SOURCES) $(CORE_SOURCES)

# 对象文件 - 为什么分离：支持增量编译，加快编译速度
MAIN_OBJECTS = $(MAIN_SOURCES:%.cpp=$(OBJ_DIR)/%.o)
WEB_OBJECTS = $(WEB_SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
CORE_OBJECTS = $(CORE_SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
ALL_OBJECTS = $(MAIN_OBJECTS) $(WEB_OBJECTS) $(CORE_OBJECTS)

# 可执行文件
MAIN_TARGET = main_server
LEGACY_TARGET = websocket_video_stream_test

# 默认目标 - 编译新的模块化版本
.PHONY: all clean debug release legacy help install

all: $(MAIN_TARGET)

# 主要目标：模块化服务器
$(MAIN_TARGET): $(ALL_OBJECTS) | $(BUILD_DIR)
	@echo "🔗 链接模块化服务器..."
	$(CXX) $(ALL_OBJECTS) $(LIBS) -o $@
	@echo "✅ 编译完成: $@"
	@echo "🚀 运行方式: ./$@ -p 8081"

# 调试版本 - 为什么需要：便于调试和开发
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: $(MAIN_TARGET)
	@echo "🐛 调试版本编译完成"

# 发布版本 - 为什么需要：生产环境优化
release: CXXFLAGS += $(RELEASE_FLAGS)
release: $(MAIN_TARGET)
	@echo "🚀 发布版本编译完成"

# 传统版本 - 保留原有的单文件版本作为参考
legacy: $(LEGACY_TARGET)

$(LEGACY_TARGET): websocket_video_stream_test.cpp $(CORE_OBJECTS) | $(BUILD_DIR)
	@echo "🔗 编译传统版本..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(CORE_OBJECTS) $(LIBS) -o $@
	@echo "✅ 传统版本编译完成: $@"

# 对象文件编译规则

# 主程序对象文件
$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	@echo "🔨 编译: $<"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Web模块对象文件
$(OBJ_DIR)/web/%.o: $(SRC_DIR)/web/%.cpp | $(OBJ_DIR)
	@echo "🌐 编译Web模块: $<"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 核心模块对象文件
$(OBJ_DIR)/camera/%.o: $(SRC_DIR)/camera/%.cpp | $(OBJ_DIR)
	@echo "📹 编译摄像头模块: $<"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/system/%.o: $(SRC_DIR)/system/%.cpp | $(OBJ_DIR)
	@echo "🖥️ 编译系统模块: $<"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/monitor/%.o: $(SRC_DIR)/monitor/%.cpp | $(OBJ_DIR)
	@echo "📊 编译监控模块: $<"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/vision/%.o: $(SRC_DIR)/vision/%.cpp | $(OBJ_DIR)
	@echo "👁️ 编译视觉模块: $<"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/utils/%.o: $(SRC_DIR)/utils/%.cpp | $(OBJ_DIR)
	@echo "🔧 编译工具模块: $<"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 目录创建
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

# 清理 - 为什么需要：清除编译产物，重新开始
clean:
	@echo "🧹 清理编译文件..."
	rm -rf $(BUILD_DIR)
	rm -f $(MAIN_TARGET) $(LEGACY_TARGET)
	@echo "✅ 清理完成"

# 安装 - 为什么需要：部署到系统目录
install: $(MAIN_TARGET)
	@echo "📦 安装到系统..."
	sudo cp $(MAIN_TARGET) /usr/local/bin/
	@echo "✅ 安装完成"

# 帮助信息 - 为什么需要：用户友好的使用指南
help:
	@echo "🔨 摄像头视频流服务器编译系统"
	@echo "================================"
	@echo ""
	@echo "可用目标:"
	@echo "  all      - 编译模块化服务器 (默认)"
	@echo "  debug    - 编译调试版本"
	@echo "  release  - 编译发布版本"
	@echo "  legacy   - 编译传统单文件版本"
	@echo "  clean    - 清理编译文件"
	@echo "  install  - 安装到系统"
	@echo "  help     - 显示此帮助"
	@echo ""
	@echo "使用示例:"
	@echo "  make           # 编译默认版本"
	@echo "  make debug     # 编译调试版本"
	@echo "  make clean     # 清理文件"
	@echo "  make legacy    # 编译传统版本"
	@echo ""
	@echo "运行方式:"
	@echo "  ./$(MAIN_TARGET) -p 8081"
	@echo "  ./$(LEGACY_TARGET)"

# 依赖关系检查 - 为什么需要：确保头文件变化时重新编译
-include $(ALL_OBJECTS:.o=.d)

# 生成依赖文件 - 为什么需要：自动处理头文件依赖
$(OBJ_DIR)/%.d: %.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $(@:.d=.o) $< > $@

$(OBJ_DIR)/%.d: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $(@:.d=.o) $< > $@

# 并行编译支持 - 为什么需要：利用多核CPU加速编译
.PARALLEL:

# 静默模式 - 为什么需要：减少编译输出，专注于错误信息
ifndef VERBOSE
.SILENT:
endif

# 检查必要的工具和库
check-deps:
	@echo "🔍 检查编译依赖..."
	@which $(CXX) > /dev/null || (echo "❌ 未找到 $(CXX)" && exit 1)
	@pkg-config --exists opencv4 || (echo "❌ 未找到 OpenCV" && exit 1)
	@echo "✅ 依赖检查通过"

# 代码统计 - 为什么需要：了解项目规模
stats:
	@echo "📊 代码统计:"
	@echo "源文件数量: $(words $(ALL_SOURCES))"
	@find . -name "*.cpp" -o -name "*.h" | xargs wc -l | tail -1
	@echo "模块分布:"
	@echo "  Web模块: $(words $(WEB_SOURCES)) 文件"
	@echo "  核心模块: $(words $(CORE_SOURCES)) 文件"

# 快速测试编译 - 为什么需要：验证代码语法正确性
test-compile:
	@echo "🧪 测试编译..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only $(ALL_SOURCES)
	@echo "✅ 语法检查通过"
