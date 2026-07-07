# 检测ARM架构
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    # AArch64 (ARMv8) 默认支持NEON
    set(HAVE_NEON TRUE)
    add_definitions(-DHAVE_NEON=1)
    message(STATUS "AArch64 detected - NEON support assumed")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    # ARMv7需要检测NEON支持
    include(CheckCXXSourceCompiles)
    
    # 检测NEON编译支持
    set(CMAKE_REQUIRED_FLAGS "-mfpu=neon")
    check_cxx_source_compiles("
        #include <arm_neon.h>
        int main() {
            float32x4_t v1 = vdupq_n_f32(1.0f);
            float32x4_t v2 = vdupq_n_f32(2.0f);
            float32x4_t result = vaddq_f32(v1, v2);
            return 0;
        }
    " HAVE_NEON_COMPILE)
    
    if(HAVE_NEON_COMPILE)
        set(HAVE_NEON TRUE)
        add_definitions(-DHAVE_NEON=1)
        add_definitions(-mfpu=neon)
        message(STATUS "ARMv7 with NEON support detected")
    else()
        message(STATUS "ARMv7 without NEON support")
    endif()
endif()