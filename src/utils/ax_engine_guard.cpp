/**************************************************************************************************
 *
 * Copyright (c) 2019-2026 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Ningbo) Co., Ltd.
 *
 **************************************************************************************************/
#include "utils/ax_engine_guard.hpp"

#include <cstring>
#include <stdlib.h>

#include "ax_engine_api.h"
#include "ax_sys_api.h"
#include "utils/logger.h"

thread_local int32_t AxEngineGuard::count_ = 0;

AxEngineGuard::AxEngineGuard() {
  if (count_ == 0) {
    auto ret = AX_SYS_Init();
    if (ret != 0) {
      ALOGE("Failed to call AX_SYS_Init. ret code: %d",
                       static_cast<int32_t>(ret));

      exit(-1);
    }

    AX_ENGINE_NPU_ATTR_T npu_attr;
    memset(&npu_attr, 0, sizeof(npu_attr));
    npu_attr.eHardMode = AX_ENGINE_VIRTUAL_NPU_DISABLE;
    ret = AX_ENGINE_Init(&npu_attr);

    if (ret != 0) {
      ALOGE("Failed to call AX_ENGINE_Init. ret code: %d",
                       static_cast<int32_t>(ret));

      exit(-1);
    }
  }

  ++count_;
}

AxEngineGuard::~AxEngineGuard() {
  --count_;
  if (count_ == 0) {
    AX_ENGINE_Deinit();
    AX_SYS_Deinit();
  }
}