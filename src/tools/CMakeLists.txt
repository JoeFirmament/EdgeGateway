# 设置源文件
set(CAMERA_TOOL_SOURCES
    camera_tool.cpp
)

# 创建可执行文件
add_executable(camera_tool ${CAMERA_TOOL_SOURCES})

# 设置包含目录
target_include_directories(camera_tool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${V4L2_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(camera_tool
    ${V4L2_LIBRARIES}
    stdc++fs
)

# 安装规则
install(TARGETS camera_tool
    RUNTIME DESTINATION bin
)
