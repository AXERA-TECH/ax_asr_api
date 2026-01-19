/**************************************************************************************************
 *
 * Copyright (c) 2019-2026 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Ningbo) Co., Ltd.
 *
 **************************************************************************************************/
#include <mutex>
#include <map>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <vector>
#include <limits>
#include <memory>

#include "asr/sensevoice.hpp"
#include "api/ax_asr_api.h"
#include "ax_model_runner/ax_model_runner.hpp"
#include "utils/nlohmann/json.hpp"
#include "utils/librosa/librosa.h"
#include "utils/logger.h"

// pImpl
class Sensevoice::Impl {
public:
    bool init(ASR_TYPE_E asr_type, const std::string& model_path) {
        return true;
    }

    void uninit(void) {
    }

    bool run(const std::vector<float>& audio_data, const std::string& language, std::string& text_result) {
        return true;
    }

private:
    AxModelRunner encoder_;    
};

Sensevoice::Sensevoice():
    impl_(std::make_unique<Sensevoice::Impl>()) {

}

Sensevoice::~Sensevoice() = default;

bool Sensevoice::init(ASR_TYPE_E asr_type, const std::string& model_path) {
    return impl_->init(asr_type, model_path);
}

void Sensevoice::uninit(void) {
    impl_->uninit();
}

bool Sensevoice::run(const std::vector<float>& audio_data, const std::string& language, std::string& text_result) {
    return impl_->run(audio_data, language, text_result);
}