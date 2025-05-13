# 设置源文件
set(UTILS_SOURCES
    config_manager.cpp
    error_handler.cpp
    string_utils.cpp
    file_utils.cpp
    time_utils.cpp
)

# 创建库
add_library(utils_module STATIC ${UTILS_SOURCES})

# 设置包含目录
target_include_directories(utils_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
