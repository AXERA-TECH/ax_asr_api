/**************************************************************************************************
 *
 * Copyright (c) 2019-2026 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Ningbo) Co., Ltd.
 *
 **************************************************************************************************/
#include "utils/AudioLoader.hpp"

#include "utils/AudioFile.h"
#include "utils/resample.h"
#include "utils/logger.h"
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#include "minimp3.h"
#include "minimp3_ext.h"
#include "utils/memory_utils.hpp"

// get file extension
static std::string getFileExt(const std::string& file_path) {
    size_t i = file_path.rfind('.', file_path.length());
    if (i != std::string::npos) {
        return file_path.substr(i + 1, file_path.length() - i);
    }

    return std::string("");
}


namespace utils {

class AudioLoader::WavImpl {
public:
    WavImpl() = default;
    ~WavImpl() = default;

    bool load(const std::string& audio_path, int target_sr) {
        if (!audio_file_.load(audio_path)) {
            return false;
        }

        // mono
        if (audio_file_.isStereo()) {
            for (int i = 0; i < audio_file_.getNumSamplesPerChannel(); i++) {
                audio_file_.samples[0][i] = (audio_file_.samples[0][i] + audio_file_.samples[1][i]) / 2;
            }
        }

        ALOGD("Audio info: format=wav, sample_rate=%d, num_samples=%d, num_channels=%d", audio_file_.getSampleRate(), audio_file_.getNumSamplesPerChannel(), audio_file_.getNumChannels());

        // resample
        if (target_sr > 0) {
            samples = utils::resample(audio_file_.samples[0], audio_file_.getSampleRate(), target_sr);
            sample_rate_ = target_sr;
        } else {
            sample_rate_ = audio_file_.getSampleRate();
        }

        return true;
    }

    inline int get_channels() const {
        return 1;
    }

    inline int get_num_samples() const {
        return (int)samples.size();
    }

    inline int get_sample_rate() const {
        return sample_rate_;
    }

    inline AUDIO_FORMAT get_audio_format() const {
        return AudioLoader::AUDIO_FORMAT_WAV;
    }

public:
    std::vector<float> samples;

private:
    AudioFile<float> audio_file_;    
    int sample_rate_;
};


class AudioLoader::Mp3Impl {
public:
    Mp3Impl() = default;
    ~Mp3Impl() = default;

    bool load(const std::string& audio_path, int target_sr) {
        mp3dec_init(&mp3d_);

        mp3dec_file_info_t info;
        if (mp3dec_load(&mp3d_, audio_path.c_str(), &info, NULL, NULL))
        {
            ALOGE("Load mp3 from %s failed!", audio_path.c_str());
            return false;
        }

        ALOGD("Audio info: format=mp3, sample_rate=%d, num_samples=%d, num_channels=%d", 
            info.hz, info.samples, info.channels);

        // mono
        if (info.channels == 1) {
            samples = std::vector<float>(info.buffer, info.buffer + info.samples);
        } else {
            samples.resize(info.samples / info.channels);
            for (int i = 0; i < samples.size(); i++) {
                samples[i] = (info.buffer[i * 2] + info.buffer[i * 2 + 1]) / 2.0f;
            }
        }

        // resample
        if (target_sr > 0) {
            samples = utils::resample(samples, info.hz, target_sr);
            sample_rate_ = target_sr;
        } else {
            sample_rate_ = info.hz;
        }

        // free
        free((void*)info.buffer);

        return true;
    }

    inline int get_channels() const {
        return 1;
    }

    inline int get_num_samples() const {
        return (int)samples.size();
    }

    inline int get_sample_rate() const {
        return sample_rate_;
    }

    inline AUDIO_FORMAT get_audio_format() const {
        return AudioLoader::AUDIO_FORMAT_MP3;
    }

public:
    std::vector<float> samples;

private:
    int sample_rate_;
    mp3dec_t mp3d_;
};


AudioLoader::AudioLoader():
    wav_impl_(std::make_unique<AudioLoader::WavImpl>()),
    mp3_impl_(std::make_unique<AudioLoader::Mp3Impl>()) {

}

AudioLoader::~AudioLoader() {
    wav_impl_.reset();
    mp3_impl_.reset();
}

bool AudioLoader::load(const std::string& audio_path, int target_sr) {
    auto ext = getFileExt(audio_path);
    if (ext.empty()) {
        ALOGE("Cannot find extension of %s", audio_path.c_str());
        return false;
    }

    bool ret = false;
    if (ext.compare("wav") == 0) {
        ret = wav_impl_->load(audio_path, target_sr);
        if (!ret) {
            ALOGE("Load wav %s failed!", audio_path.c_str());
            return false;
        }

        audio_format_ = wav_impl_->get_audio_format();
        samples = wav_impl_->samples;
    } else if (ext.compare("mp3") == 0) {
        ret = mp3_impl_->load(audio_path, target_sr);
        if (!ret) {
            ALOGE("Load mp3 %s failed!", audio_path.c_str());
            return false;
        }

        audio_format_ = mp3_impl_->get_audio_format();
        samples = mp3_impl_->samples;
    } else {
        ALOGE("Unknown format of %s", audio_path.c_str());
        return false;
    }

    return true;
}

int AudioLoader::get_channels() {
    if (audio_format_ == AUDIO_FORMAT_WAV) {
        return wav_impl_->get_channels();
    } else {
        return mp3_impl_->get_channels();
    }
}

int AudioLoader::get_num_samples() {
    if (audio_format_ == AUDIO_FORMAT_WAV) {
        return wav_impl_->get_num_samples();
    } else {
        return mp3_impl_->get_num_samples();
    }
}

int AudioLoader::get_sample_rate() {
    if (audio_format_ == AUDIO_FORMAT_WAV) {
        return wav_impl_->get_sample_rate();
    } else {
        return mp3_impl_->get_sample_rate();
    }
}

} // namespace utils