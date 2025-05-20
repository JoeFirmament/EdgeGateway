# 设置源文件
set(API_SOURCES
    api_server.cpp
    web_server.cpp
    mjpeg_streamer.cpp
    rest_handler.cpp
    camera_api.cpp
)

# 设置Mongoose源文件
set(MONGOOSE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/mongoose/mongoose.c
)

# 为Mongoose源文件设置C编译选项
set_source_files_properties(${MONGOOSE_SOURCES} PROPERTIES LANGUAGE C)

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
add_library(api_module STATIC ${API_SOURCES} ${MONGOOSE_SOURCES})

# 设置包含目录
target_include_directories(api_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/mongoose
    ${FFMPEG_INCLUDE_DIRS}
)

# 添加Mongoose所需的定义
target_compile_definitions(api_module PRIVATE
    MG_ENABLE_HTTP=1
    MG_ENABLE_HTTP_STREAMING_MULTIPART=1
    MG_ENABLE_LINES=1
    MG_ENABLE_DIRECTORY_LISTING=1
    MG_ENABLE_SSI=0
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
