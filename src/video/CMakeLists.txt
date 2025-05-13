# 设置源文件
set(VIDEO_SOURCES
    video_recorder.cpp
    video_splitter.cpp
    frame_converter.cpp
)

# 创建库
add_library(video_module STATIC ${VIDEO_SOURCES})

# 设置包含目录
target_include_directories(video_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 链接库
target_link_libraries(video_module
    utils_module
    ${FFMPEG_LIBRARIES}
)

if(ROCKCHIP_MPP_FOUND)
    target_link_libraries(video_module ${ROCKCHIP_MPP_LIBRARIES})
endif()

if(LIBRGA_FOUND)
    target_link_libraries(video_module ${LIBRGA_LIBRARIES})
endif()
