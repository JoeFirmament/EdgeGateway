# 查找CURL库
find_package(CURL REQUIRED)

# 设置源文件
set(STATIC_PAGE_TEST_SOURCES
    static_page_test.cpp
)

# 创建可执行文件
add_executable(static_page_test ${STATIC_PAGE_TEST_SOURCES})

# 设置包含目录
target_include_directories(static_page_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CURL_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(static_page_test
    api_module
    utils_module
    monitor_module
    system_module
    ${CURL_LIBRARIES}
    pthread
)

# 添加测试
add_test(
    NAME StaticPageTest
    COMMAND static_page_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# 设置简单服务器测试源文件
set(SIMPLE_SERVER_TEST_SOURCES
    simple_server_test.cpp
)

# 创建简单服务器测试可执行文件
add_executable(simple_server_test ${SIMPLE_SERVER_TEST_SOURCES})

# 设置包含目录
target_include_directories(simple_server_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 链接库
target_link_libraries(simple_server_test
    api_module
    utils_module
    monitor_module
    system_module
    pthread
)

# 添加测试
add_test(
    NAME SimpleServerTest
    COMMAND simple_server_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
