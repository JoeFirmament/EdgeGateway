# 设置源文件
set(MONITOR_SOURCES
    # 暂时没有源文件，使用一个空文件
)

# 创建库（使用INTERFACE库，不需要源文件）
add_library(monitor_module INTERFACE)

# 设置包含目录
target_include_directories(monitor_module INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 链接库
target_link_libraries(monitor_module INTERFACE
    utils_module)
