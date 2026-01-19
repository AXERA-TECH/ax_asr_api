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

#include "asr/asr_interface.hpp"
#include "utils/logger.h"

class Sensevoice : public ASRInterface {
public:
    ~Sensevoice() {
        ALOGI("Delete Sensevoice");
    }

    bool init(ASR_TYPE_E asr_type, const std::string& model_path) {
        return true;
    }

    void uninit(void) {

    }

    bool run(const std::vector<float>& audio_data, const std::string& language, std::string& text_result) {
        return true;
    }
};