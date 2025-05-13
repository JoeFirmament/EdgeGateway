# 设置源文件
set(CAMERA_SOURCES
    camera_manager.cpp
    camera_device.cpp
    v4l2_camera.cpp
)

# 创建库
add_library(camera_module STATIC ${CAMERA_SOURCES})

# 设置包含目录
target_include_directories(camera_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 链接库
target_link_libraries(camera_module
    utils_module
    ${V4L2_LIBRARIES}
)
