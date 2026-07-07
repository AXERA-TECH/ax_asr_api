/**************************************************************************************************
 *
 * Copyright (c) 2019-2026 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Ningbo) Co., Ltd.
 *
 **************************************************************************************************/
#pragma once

#include <string>
#include <vector>
#include "api/ax_asr_api.h"

class ASRInterface {
public:
    virtual ~ASRInterface() {}
    virtual int sample_rate() = 0;
    virtual bool init(AX_ASR_TYPE_E asr_type, const std::string& model_path) = 0;
    virtual void uninit(void) = 0;
    virtual bool run(const std::vector<float>& audio_data, int sample_rate, const std::string& language, std::string& text_result) = 0;
};