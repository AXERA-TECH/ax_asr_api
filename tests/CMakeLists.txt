cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
project(tests)

set(CMAKE_CXX_STANDARD 17)

# 查找所有测试文件
file(GLOB TEST_SOURCES "*.cpp")

# 为每个测试文件创建可执行程序
foreach(test_file ${TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    
    add_executable(${test_name} ${test_file})
    
    # 链接父目录生成的库
    target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${test_name} PUBLIC ax_asr_api PRIVATE ${MSP_LIBS})

    # 安装到CMAKE_INSTALL_BINDIR
    install(TARGETS ${test_name}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # 设置RPATH
    set_target_properties(${test_name} PROPERTIES
        INSTALL_RPATH "$ORIGIN/lib"
        BUILD_WITH_INSTALL_RPATH TRUE  # 构建时也使用install的RPATH
        SKIP_BUILD_RPATH FALSE
    )
endforeach()