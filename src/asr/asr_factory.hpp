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

#include <memory>
#include "asr/asr_interface.hpp"
#include "api/ax_asr_api.h"
#include "utils/logger.h"
#include "asr/whisper.hpp"
#include "asr/sensevoice.hpp"

class ASRFactory {
public:
    static ASRInterface* create(AX_ASR_TYPE_E asr_type, const std::string& model_path) {
        ASRInterface* interface = nullptr;
        
        std::string spec_model_path;
        spec_model_path.reserve(64);

        switch (asr_type)
        {
        case AX_WHISPER_TINY: {
            interface = new Whisper();
            spec_model_path = model_path + "/tiny/";
            break;
        }
        case AX_WHISPER_BASE: {
            interface = new Whisper();
            spec_model_path = model_path + "/base/";
            break;
        }
        case AX_WHISPER_SMALL: {
            interface = new Whisper();
            spec_model_path = model_path + "/small/";
            break;
        }
        case AX_WHISPER_TURBO: {
            interface = new Whisper();
            spec_model_path = model_path + "/turbo/";
            break;
        }
        case AX_SENSEVOICE: {
            interface = new Sensevoice();
            spec_model_path = model_path;
            break;
        }
        default:
            ALOGE("Unknown asr_type %d", asr_type);
            return nullptr;
        }

        if (!interface->init(asr_type, spec_model_path)) {
            ALOGE("Init asr failed!");
            delete interface;
            return nullptr;
        }

        return interface;
    }
};