# Try to find the Crow library
# Once done this will define:
#  CROW_FOUND - System has Crow
#  CROW_INCLUDE_DIRS - The Crow include directories
#  CROW_LIBRARIES - The libraries needed to use Crow
#  CROW_DEFINITIONS - Compiler switches required for using Crow

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# 查找 ASIO 头文件
find_path(ASIO_INCLUDE_DIR
    NAMES asio.hpp
    PATHS
        /usr/include
        /usr/local/include
    PATH_SUFFIXES asio
)

if(NOT ASIO_INCLUDE_DIR)
    message(FATAL_ERROR "ASIO library not found. Please install libasio-dev package.")
endif()

message(STATUS "Found ASIO include dir: ${ASIO_INCLUDE_DIR}")

# 设置Crow的包含路径
set(CROW_POSSIBLE_INCLUDE_PATHS
    ${CMAKE_SOURCE_DIR}/third_party/crow/include
    ${CMAKE_SOURCE_DIR}/third_party/crow
    /usr/local/include
    /usr/local/include/crow
    /usr/include
    /usr/include/crow
    $ENV{HOME}/.local/include
    $ENV{HOME}/.local/include/crow
)

# 查找头文件
find_path(CROW_INCLUDE_DIR 
    NAMES crow.h
    PATHS ${CROW_POSSIBLE_INCLUDE_PATHS}
    PATH_SUFFIXES crow
    NO_DEFAULT_PATH
)

# 如果没找到，再尝试默认路径
if(NOT CROW_INCLUDE_DIR)
    find_path(CROW_INCLUDE_DIR NAMES crow.h)
endif()

# 处理查找结果
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Crow DEFAULT_MSG
    CROW_INCLUDE_DIR
    Threads_FOUND
    OPENSSL_FOUND
    ZLIB_FOUND
)

if(CROW_FOUND)
    message(STATUS "Found Crow include directory: ${CROW_INCLUDE_DIR}")
    
    # 设置包含目录
    set(CROW_INCLUDE_DIRS 
        ${CROW_INCLUDE_DIR} 
        ${ASIO_INCLUDE_DIR}
        ${OPENSSL_INCLUDE_DIR} 
        ${ZLIB_INCLUDE_DIRS}
    )
    
    # 设置链接库
    set(CROW_LIBRARIES 
        ${CMAKE_THREAD_LIBS_INIT} 
        ${OPENSSL_LIBRARIES} 
        ${ZLIB_LIBRARIES}
    )
    
    # 创建导入目标
    if(NOT TARGET Crow::Crow)
        add_library(Crow::Crow INTERFACE IMPORTED)
        set_target_properties(Crow::Crow PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${CROW_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES "${CROW_LIBRARIES}"
            INTERFACE_COMPILE_DEFINITIONS "CROW_ENABLE_SSL"
        )
        message(STATUS "Created Crow::Crow imported target")
    endif()
else()
    message(STATUS "Crow not found, please install it first")
    message(STATUS "You can install it using: git clone https://github.com/CrowCpp/Crow.git")
    message(STATUS "Then add the include directory to CMAKE_PREFIX_PATH")
endif()

mark_as_advanced(CROW_INCLUDE_DIR CROW_INCLUDE_DIRS CROW_LIBRARIES)
