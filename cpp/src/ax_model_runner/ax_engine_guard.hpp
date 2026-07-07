/**************************************************************************************************
 *
 * Copyright (c) 2019-2026 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Ningbo) Co., Ltd.
 *
 **************************************************************************************************/
#if defined (CHIP_AX650) || defined(CHIP_AX630C) || defined(CHIP_AX620Q)

#pragma once
#include <cstdint>

class AxEngineGuard {
public:
    AxEngineGuard();
    ~AxEngineGuard();

    AxEngineGuard(const AxEngineGuard &) = delete;
    AxEngineGuard &operator=(const AxEngineGuard &) = delete;

    AxEngineGuard(AxEngineGuard &&) = delete;
    AxEngineGuard &operator=(AxEngineGuard &&) = delete;

private:
    static thread_local int32_t count_;
};

#endif