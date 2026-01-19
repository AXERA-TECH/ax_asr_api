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
    static ASRInterface* create(ASR_TYPE_E asr_type, const std::string& model_path) {
        ASRInterface* interface = nullptr;
        
        std::string spec_model_path;
        spec_model_path.reserve(64);

        switch (asr_type)
        {
        case WHISPER_TINY: {
            interface = new Whisper();
            spec_model_path = model_path + "/whisper/tiny/";
            break;
        }
        case WHISPER_BASE: {
            interface = new Whisper();
            spec_model_path = model_path + "/whisper/base/";
            break;
        }
        case WHISPER_SMALL: {
            interface = new Whisper();
            spec_model_path = model_path + "/whisper/small/";
            break;
        }
        case WHISPER_TURBO: {
            interface = new Whisper();
            spec_model_path = model_path + "/whisper/turbo/";
            break;
        }
        case SENSEVOICE: {
            interface = new Sensevoice();
            spec_model_path = model_path + "/sensevoice/";
            break;
        }
        default:
            ALOGE("Unknown asr_type %d", asr_type);
            break;
        }

        if (!interface->init(asr_type, spec_model_path)) {
            ALOGE("Init asr failed!");
            delete interface;
            return nullptr;
        }

        return interface;
    }
};