cmake_minimum_required(VERSION 3.10)
project(cam_server VERSION 1.0 LANGUAGES C CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

# 编译选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

# 依赖库配置
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)
pkg_check_modules(V4L2 REQUIRED libv4l2)
find_package(fmt REQUIRED)

# 可选：RK3588特定库
pkg_check_modules(ROCKCHIP_MPP QUIET rockchip_mpp)
pkg_check_modules(LIBRGA QUIET librga)

# 已移除 Mongoose 库，迁移至 Crow 框架

# 添加子模块
add_subdirectory(src/api)
add_subdirectory(src/camera)
add_subdirectory(src/video)
add_subdirectory(src/storage)
add_subdirectory(src/monitor)
add_subdirectory(src/system)
add_subdirectory(src/utils)
add_subdirectory(src/tools)

# 主可执行文件
add_executable(cam_server src/main.cpp)

# 链接所有子模块和第三方库
target_link_libraries(cam_server
    PRIVATE
    api_module
    camera_module
    video_module
    storage_module
    monitor_module
    system_module
    utils_module
    ${FFMPEG_LIBRARIES}
    ${V4L2_LIBRARIES}
    fmt::fmt
    pthread dl m
)

# 可选库链接
if(ROCKCHIP_MPP_FOUND)
    target_link_libraries(cam_server PRIVATE ${ROCKCHIP_MPP_LIBRARIES})
    target_compile_definitions(cam_server PRIVATE USE_ROCKCHIP_MPP)
endif()

if(LIBRGA_FOUND)
    target_link_libraries(cam_server PRIVATE ${LIBRGA_LIBRARIES})
    target_compile_definitions(cam_server PRIVATE USE_LIBRGA)
endif()

# 包含目录
target_include_directories(cam_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
    ${V4L2_INCLUDE_DIRS}
)

# 旧版GCC文件系统库支持
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(cam_server PRIVATE stdc++fs)
endif()

# 安装规则
install(TARGETS cam_server RUNTIME DESTINATION bin)

# 打印配置信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "FFmpeg found: ${FFMPEG_FOUND}")
message(STATUS "Rockchip MPP: ${ROCKCHIP_MPP_FOUND}")
message(STATUS "LibRGA: ${LIBRGA_FOUND}")