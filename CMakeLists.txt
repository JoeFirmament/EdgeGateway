cmake_minimum_required(VERSION 3.10)
project(cam_server_cpp VERSION 0.1.0 LANGUAGES C CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 添加CMake模块路径
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# 设置编译选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

# 查找依赖库
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)
pkg_check_modules(V4L2 REQUIRED libv4l2)

# 可选：查找RK3588特定的库
pkg_check_modules(ROCKCHIP_MPP QUIET rockchip_mpp)
pkg_check_modules(LIBRGA QUIET librga)

# 添加Mongoose库
set(MONGOOSE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mongoose)
if(NOT EXISTS ${MONGOOSE_DIR})
    file(MAKE_DIRECTORY ${MONGOOSE_DIR})
    file(DOWNLOAD
        "https://raw.githubusercontent.com/cesanta/mongoose/master/mongoose.c"
        "${MONGOOSE_DIR}/mongoose.c"
        SHOW_PROGRESS
    )
    file(DOWNLOAD
        "https://raw.githubusercontent.com/cesanta/mongoose/master/mongoose.h"
        "${MONGOOSE_DIR}/mongoose.h"
        SHOW_PROGRESS
    )
endif()

# 添加包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
    ${V4L2_INCLUDE_DIRS}
    ${MONGOOSE_DIR}
)

if(ROCKCHIP_MPP_FOUND)
    include_directories(${ROCKCHIP_MPP_INCLUDE_DIRS})
    add_definitions(-DUSE_ROCKCHIP_MPP)
endif()

if(LIBRGA_FOUND)
    include_directories(${LIBRGA_INCLUDE_DIRS})
    add_definitions(-DUSE_LIBRGA)
endif()

# 添加链接库目录
link_directories(
    ${FFMPEG_LIBRARY_DIRS}
    ${V4L2_LIBRARY_DIRS}
)

if(ROCKCHIP_MPP_FOUND)
    link_directories(${ROCKCHIP_MPP_LIBRARY_DIRS})
endif()

if(LIBRGA_FOUND)
    link_directories(${LIBRGA_LIBRARY_DIRS})
endif()

# 设置源文件目录
set(CAMERA_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/camera)
set(VIDEO_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/video)
set(API_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/api)
set(STORAGE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/storage)
set(MONITOR_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/monitor)
set(UTILS_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/utils)

# 设置头文件目录
set(CAMERA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/camera)
set(VIDEO_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/video)
set(API_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/api)
set(STORAGE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/storage)
set(MONITOR_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/monitor)
set(UTILS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/utils)

# 添加子目录
add_subdirectory(src)
add_subdirectory(src/tools)

# 可选：添加测试
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 安装规则
install(TARGETS cam_server
    RUNTIME DESTINATION bin
)

# 打印配置信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "FFmpeg found: ${FFMPEG_FOUND}")
message(STATUS "V4L2 found: ${V4L2_FOUND}")
message(STATUS "Rockchip MPP found: ${ROCKCHIP_MPP_FOUND}")
message(STATUS "LibRGA found: ${LIBRGA_FOUND}")
