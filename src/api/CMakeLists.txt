# 设置源文件
set(API_SOURCES
    api_server.cpp
    web_server.cpp
    mjpeg_streamer.cpp
    rest_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/mongoose/mongoose.c
)

# 创建库
add_library(api_module STATIC ${API_SOURCES})

# 设置包含目录
target_include_directories(api_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/mongoose
)

# 链接库
target_link_libraries(api_module
    utils_module
    camera_module
    ${FFMPEG_LIBRARIES}
    pthread
    dl
)
