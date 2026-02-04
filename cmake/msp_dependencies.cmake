# bsp
if(NOT BSP_MSP_DIR)
    if (CHIP_AX650)
        set(BSP_MSP_DIR ${CMAKE_SOURCE_DIR}/ax650n_bsp_sdk/msp/out)

    elseif(CHIP_AX630C)
        set(BSP_MSP_DIR ${CMAKE_SOURCE_DIR}/ax620e_bsp_sdk/msp/out/arm64_glibc)

    elseif(CHIP_AX620Q)
        set(BSP_MSP_DIR ${CMAKE_SOURCE_DIR}/ax620e_bsp_sdk/msp/out/arm_uclibc)

    elseif(CHIP_AX8850)
        set(BSP_MSP_DIR ${CMAKE_SOURCE_DIR}/axcl_bsp_sdk/out)
        list(APPEND MSP_INC_DIR ${BSP_MSP_DIR}/bsp)
    else()
        message(FATAL_ERROR "Unknown chip_type")
    endif()
endif()

message(STATUS "BSP_MSP_DIR = ${BSP_MSP_DIR}")
if (CHIP_AX650)
    add_definitions(-DCHIP_AX650)
    list(APPEND MSP_LIBS
        libax_sys.a
        libax_engine.a
        libax_interpreter.a
        libax_dmadim.a
    )
elseif(CHIP_AX630C)
    add_definitions(-DCHIP_AX630C)
    list(APPEND MSP_LIBS
        libax_sys.a
        libax_engine.a
        libax_interpreter.a
    )
elseif(CHIP_AX620Q)
    add_definitions(-DCHIP_AX630C)
    list(APPEND MSP_LIBS
        libax_sys.a
        libax_engine.a
        libax_interpreter.a
    )
elseif(CHIP_AX8850)
    add_definitions(-DCHIP_AX8850)
    list(APPEND MSP_INC_DIR ${BSP_MSP_DIR}/bsp)
    list(APPEND MSP_LIBS 
        axcl_rt
        axcl_pkg
        axcl_comm
        axcl_npu
        spdlog
        axcl_token
        axcl_pcie_msg
        axcl_pcie_dma
    )
else()
    message(FATAL_ERROR "Unknown chip_type")
endif()


list(APPEND MSP_INC_DIR ${BSP_MSP_DIR}/include)
set(MSP_LIB_DIR ${BSP_MSP_DIR}/lib)
