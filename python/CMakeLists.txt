cmake_minimum_required(VERSION 3.13)
project(ax_asr_pybind LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(pybind11 CONFIG REQUIRED)

# Build the pybind11 extension module
pybind11_add_module(_ax_asr_core src/ax_asr/_binding.cpp)

# Link against libax_asr_api (static library)
if(DEFINED AX_ASR_LIB_DIR)
    target_link_directories(_ax_asr_core PRIVATE ${AX_ASR_LIB_DIR})
endif()
target_link_libraries(_ax_asr_core PRIVATE ax_asr_api)

# Link BSP dependencies required by libax_asr_api
if(DEFINED MSP_LIB_DIR)
    target_link_directories(_ax_asr_core PRIVATE ${MSP_LIB_DIR})
endif()
target_link_libraries(_ax_asr_core PRIVATE
    ax_sys ax_engine ax_interpreter dmadim pthread dl)

# Include the public API header
if(DEFINED AX_ASR_INCLUDE_DIR)
    target_include_directories(_ax_asr_core PRIVATE ${AX_ASR_INCLUDE_DIR})
else()
    target_include_directories(_ax_asr_core PRIVATE
        ${CMAKE_SOURCE_DIR}/../../cpp/src/api)
endif()

install(TARGETS _ax_asr_core LIBRARY DESTINATION ax_asr)
