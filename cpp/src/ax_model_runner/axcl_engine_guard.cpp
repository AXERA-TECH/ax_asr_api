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

#include "ax_model_runner/axcl_engine_guard.hpp"

#include <cstring>
#include <stdlib.h>

#include "axcl.h"
#include "utils/logger.h"

thread_local int32_t AxclEngineGuard::count_ = 0;

AxclEngineGuard::AxclEngineGuard(const char *config, axclrtEngineVNpuKind npuKind, int device_index, const npu_func& func) {
  if (count_ == 0) {
    auto ret = axclInit(config);
    if (ret != 0) {
      ALOGE("Failed to call AX_SYS_Init. ret code: 0x%x", ret);
      exit(-1);
    }

    if (!func(device_index)) {

    }

    ret = axclrtEngineInit(npuKind);
    if (ret != 0) {
        ALOGE("Failed to call axclrtEngineInit. ret code: 0x%x", ret);
        exit(-1);
    }
  }

  ++count_;
}

AxclEngineGuard::~AxclEngineGuard() {
  --count_;
  if (count_ == 0) {
    axclrtEngineFinalize();
    axclFinalize();
  }
}

#endif