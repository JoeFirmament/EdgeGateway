# 设置源文件
set(MONITOR_SOURCES
    logger.cpp
)

# 创建库
add_library(monitor_module STATIC ${MONITOR_SOURCES})

# 设置包含目录
target_include_directories(monitor_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 链接库
target_link_libraries(monitor_module
    utils_module)
