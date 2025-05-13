# 添加子目录
add_subdirectory(camera)
add_subdirectory(video)
add_subdirectory(api)
add_subdirectory(storage)
add_subdirectory(monitor)
add_subdirectory(utils)
add_subdirectory(system)

# 设置主程序源文件
set(MAIN_SOURCES
    main.cpp
)

# 创建可执行文件
add_executable(cam_server ${MAIN_SOURCES})

# 链接库
target_link_libraries(cam_server
    camera_module
    video_module
    api_module
    storage_module
    monitor_module
    utils_module
    system_module
    ${FFMPEG_LIBRARIES}
    ${V4L2_LIBRARIES}
    pthread
    dl
    m
)

if(ROCKCHIP_MPP_FOUND)
    target_link_libraries(cam_server ${ROCKCHIP_MPP_LIBRARIES})
endif()

if(LIBRGA_FOUND)
    target_link_libraries(cam_server ${LIBRGA_LIBRARIES})
endif()

# 设置包含目录
target_include_directories(cam_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
