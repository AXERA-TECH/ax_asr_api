cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
project(ax_asr_api)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 17)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_CXX_FLAGS "-fvisibility=hidden -g -O0")
    add_definitions("-D__DEBUG__")
    add_definitions("-D__LOG_LEVEL_DEBUG__")
elseif (CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_CXX_FLAGS "-fvisibility=hidden -O2")
endif()

# 编译选项
option(BUILD_TESTS "Build unit tests from tests/" OFF)
option(LOG_LEVEL_DEBUG "Print debug level logs" OFF)

# 日志水平
if (LOG_LEVEL_DEBUG)
    message(STATUS "LOG_LEVEL: DEBUG")
    add_definitions("-D__LOG_LEVEL_DEBUG__")
endif()

# 设置安装路径
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "Installation prefix")
set(CMAKE_INSTALL_LIBDIR "lib")
set(CMAKE_INSTALL_INCLUDEDIR "include")
set(CMAKE_INSTALL_BINDIR ".")

# Axera BSP
include(cmake/msp_dependencies.cmake)
add_definitions(-DENV_HAS_STD_FILESYSTEM)
add_definitions(-DENV_HAS_POSIX_FILE_STAT)
include_directories(${MSP_INC_DIR})
link_directories(${MSP_LIB_DIR})

# 第三方库
include_directories(third_party/kaldi-native-fbank/include)
link_directories(third_party/kaldi-native-fbank/lib/${CMAKE_SYSTEM_PROCESSOR})
list(APPEND KALDI_LIBS kaldi-native-fbank-core kissfft-float)

# Project sources
aux_source_directory(src SRC)
aux_source_directory(src/utils SRC)
aux_source_directory(src/ax_model_runner SRC)
aux_source_directory(src/asr SRC)

# libax_asr_api.so - 共享库
add_library(ax_asr_api SHARED src/api/ax_asr_api.cpp ${SRC})

# 设置库属性
set_target_properties(ax_asr_api PROPERTIES
    OUTPUT_NAME "ax_asr_api"
    PUBLIC_HEADER "src/api/ax_asr_api.h"
    POSITION_INDEPENDENT_CODE ON
)

# 链接依赖库到 ax_asr_api 库
target_link_libraries(ax_asr_api PRIVATE ${MSP_LIBS} ${KALDI_LIBS})

# 设置包含目录
target_include_directories(ax_asr_api
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/api>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ax_model_runner
        ${MSP_INC_DIR}
)

# main executable - 依赖于 ax_asr_api 库
add_executable(main main.cpp)
target_include_directories(main PRIVATE src)
target_link_libraries(main PUBLIC ax_asr_api PRIVATE ${MSP_LIBS})

# 单元测试
if (BUILD_TESTS)
    add_subdirectory(tests)
endif()

# 安装 ax_asr_api 库和头文件
install(TARGETS ax_asr_api
        EXPORT ax_asr_api-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# 安装头文件（额外的安装方式，确保头文件被安装）
install(FILES src/api/ax_asr_api.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# 安装可执行文件
install(TARGETS main
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 创建配置文件供 find_package 使用
install(EXPORT ax_asr_api-targets
        FILE ax_asr_api-config.cmake
        NAMESPACE ax::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ax_asr_api
)

# 可选：为开发人员提供导出目标
export(EXPORT ax_asr_api-targets
       FILE ${CMAKE_CURRENT_BINARY_DIR}/ax_asr_api-targets.cmake
       NAMESPACE ax::
)

# 设置RPATH
set_target_properties(main PROPERTIES
    INSTALL_RPATH "$ORIGIN/lib"
    BUILD_WITH_INSTALL_RPATH TRUE  # 构建时也使用install的RPATH
    SKIP_BUILD_RPATH FALSE
)
