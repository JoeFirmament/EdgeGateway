# 设置源文件
set(UTILS_SOURCES
    time_utils.cpp
    file_utils.cpp
    string_utils.cpp
    config_manager.cpp
)

# 创建库
add_library(utils_module STATIC ${UTILS_SOURCES})

# 设置包含目录
target_include_directories(utils_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 链接库
target_link_libraries(utils_module
    monitor_module
    $<IF:$<TARGET_EXISTS:fmt::fmt>,fmt::fmt,fmt>
)
