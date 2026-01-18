/**************************************************************************************************
 *
 * Copyright (c) 2019-2025 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Ningbo) Co., Ltd.
 *
 **************************************************************************************************/
#pragma once

#include <string>
#include <vector>

class ASRInterface {
public:
    virtual bool init(const std::string& model_path) = 0;
    virtual void uninit(void) = 0;
    virtual bool run(const std::vector<float>& audio_data, const std::string& language, std::string& text_result) = 0;
};