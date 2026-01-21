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
#include <memory>
#include <vector>

namespace utils {

// Load audio file, convert to mono and resample
class AudioLoader {
public:
    AudioLoader();
    ~AudioLoader();

    enum AUDIO_FORMAT {
        AUDIO_FORMAT_WAV = 0,
        AUDIO_FORMAT_MP3
    };

    // if target_sr <= 0, it doesn't resample
    bool load(const std::string& audio_path, int target_sr = 16000);

    int get_channels();
    int get_num_samples();
    int get_sample_rate();

    inline AUDIO_FORMAT get_audio_format() const {
        return audio_format_;
    }

public:
    std::vector<float> samples;    

private:
    AUDIO_FORMAT audio_format_;

    class WavImpl;
    class Mp3Impl;
    std::unique_ptr<WavImpl> wav_impl_;
    std::unique_ptr<Mp3Impl> mp3_impl_;
};

} // namespace utils