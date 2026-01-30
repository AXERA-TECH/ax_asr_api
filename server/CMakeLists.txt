cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
project(asr_server)

set(CMAKE_CXX_STANDARD 17)

# 查找所有测试文件
file(GLOB SERVER_SRCS "*.cpp")

add_executable(asr_server ${SERVER_SRCS})
    
# 链接父目录生成的库
target_include_directories(asr_server PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(asr_server PUBLIC ax_asr_api PRIVATE ${MSP_LIBS} pthread dl)

# 安装到CMAKE_INSTALL_BINDIR
install(TARGETS asr_server
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# 设置RPATH
set_target_properties(asr_server PROPERTIES
    INSTALL_RPATH "$ORIGIN/lib"
    BUILD_WITH_INSTALL_RPATH TRUE  # 构建时也使用install的RPATH
    SKIP_BUILD_RPATH FALSE
)