/**************************************************************************************************
 *
 * Copyright (c) 2019-2026 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Ningbo) Co., Ltd.
 *
 **************************************************************************************************/
#if defined (CHIP_AX8850)

#pragma once
#include <cstdint>
#include <functional>
#include "axcl.h"

class AxclEngineGuard {
public:
    using npu_func = std::function<bool(int)>;

    explicit AxclEngineGuard(const char *config, 
        axclrtEngineVNpuKind npuKind, 
        int device_index, 
        const npu_func& func);
    ~AxclEngineGuard();

    AxclEngineGuard(const AxclEngineGuard &) = delete;
    AxclEngineGuard &operator=(const AxclEngineGuard &) = delete;

    AxclEngineGuard(AxclEngineGuard &&) = delete;
    AxclEngineGuard &operator=(AxclEngineGuard &&) = delete;

private:
    static thread_local int32_t count_;
};

#endif