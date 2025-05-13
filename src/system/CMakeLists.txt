# 设置源文件
set(SYSTEM_SOURCES
    system_monitor.cpp
)

# 创建库
add_library(system_module STATIC ${SYSTEM_SOURCES})

# 设置包含目录
target_include_directories(system_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 链接库
target_link_libraries(system_module
    monitor_module
)
