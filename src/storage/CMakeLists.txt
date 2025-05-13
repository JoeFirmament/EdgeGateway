# 设置源文件
set(STORAGE_SOURCES
    file_manager.cpp
    storage_manager.cpp
    archive_manager.cpp
)

# 创建库
add_library(storage_module STATIC ${STORAGE_SOURCES})

# 设置包含目录
target_include_directories(storage_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 链接库
target_link_libraries(storage_module
    utils_module
)
