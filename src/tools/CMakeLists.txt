# 设置源文件
set(CAMERA_TOOL_SOURCES
    camera_tool.cpp
)

# 创建可执行文件（名称统一为 test_for_cam）
add_executable(test_for_cam ${CAMERA_TOOL_SOURCES})

# 设置包含目录（目标名同步更新）
target_include_directories(test_for_cam PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${V4L2_INCLUDE_DIRS}
)

# 链接库（目标名同步更新）
target_link_libraries(test_for_cam
    monitor_module
    ${V4L2_LIBRARIES}
    stdc++fs
    $<IF:$<TARGET_EXISTS:fmt::fmt>,fmt::fmt,fmt>
)

# 安装规则（目标名同步更新）
install(TARGETS test_for_cam
    RUNTIME DESTINATION bin
)