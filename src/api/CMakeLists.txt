# 设置源文件
set(API_SOURCES
    api_server.cpp
    rest_handler.cpp
    web_server.cpp
    mjpeg_streamer.cpp
)

# 创建库
add_library(api_module STATIC ${API_SOURCES})

# 设置包含目录
target_include_directories(api_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 链接库
target_link_libraries(api_module
    utils_module
    camera_module
    ${FFMPEG_LIBRARIES}
)
