# 设置源文件
set(API_SOURCES
    api_server.cpp
    crow_server.cpp
    mjpeg_streamer.cpp
    rest_handler.cpp
    camera_api.cpp
    websocket_camera_streamer.cpp
)

# 查找FFmpeg包
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavcodec
    libavformat
    libavutil
    libswscale
    libavfilter
    libavdevice
)

# 创建库
add_library(api_module STATIC ${API_SOURCES})

# 设置包含目录
target_include_directories(api_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/crow
    ${FFMPEG_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(api_module
    utils_module
    $<IF:$<TARGET_EXISTS:fmt::fmt>,fmt::fmt,fmt>
    camera_module
    video_module
    ${FFMPEG_LIBRARIES}
    avcodec
    avformat
    avutil
    swscale
    avfilter
    avdevice
    pthread
    dl
)
