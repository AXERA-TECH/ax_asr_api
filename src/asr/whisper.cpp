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

#include "asr/whisper.hpp"
#include "api/ax_asr_api.h"
#include "ax_model_runner/ax_model_runner.hpp"
#include "utils/nlohmann/json.hpp"
#include "utils/librosa/librosa.h"
#include "utils/base64.h"
#include "utils/logger.h"
#include "utils/resample.h"

using json = nlohmann::json;

#if defined(CHIP_AX650)
    #include "ax_dmadim_api.h"
#endif

#define WHISPER_SAMPLE_RATE 16000
#define WHISPER_N_FFT       400
#define WHISPER_HOP_LENGTH  160
#define WHISPER_CHUNK_SIZE  30
#define WHISPER_FRAME_NUM   3000 // 30 seconds

// utilities
static int argmax(const std::vector<float>& logits) {
    auto max_iter = std::max_element(logits.begin(), logits.end());
    return std::distance(logits.begin(), max_iter); // absolute index of max
}

template<typename T>
std::vector<T> stringToVector(const std::string& str) {
    std::vector<T> result;
    std::istringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, ',')) {
        std::istringstream tokenStream(token);
        T value;
        
        // 尝试将字符串转换为类型T
        if (!(tokenStream >> value)) {
            throw std::runtime_error("Failed to convert token: " + token);
        }
        
        result.push_back(value);
    }
    
    return result;
}

typedef struct _WhisperConfig {
    int sample_rate;
    int n_mels;
    int n_text_layer, n_text_ctx, n_text_state;
    int n_vocab;
    int sot, eot;
    int transcribe, translate;
    int no_timestamps;
} WhisperConfig;

typedef struct _WhisperFeature {
    std::array<int, 4> sot_seq;

    std::vector<float> mel_bank;        // [1, n_mels, n_frames]
    std::vector<int>   mask;            // [n_text_ctx,]
    std::vector<float> self_k_cache;    // [n_text_layer, n_text_ctx, n_text_state]
    std::vector<float> self_v_cache;    // [n_text_layer, n_text_ctx, n_text_state]
    std::vector<float> this_self_k;     // [n_text_layer, 1, n_text_state]
    std::vector<float> this_self_v;     // [n_text_layer, 1, n_text_state]

    std::vector<float> logits;
} WhisperFeature;


// pImpl
class Whisper::Impl {
public:
    ~Impl() {
        encoder_.unload_model();
        decoder_.unload_model();
    }

    bool init(AX_ASR_TYPE_E asr_type, const std::string& model_path) {
        if (type_map_.find(asr_type) == type_map_.end()) {
            ALOGE("Cannot find corresponding model_type of asr_type(%d)", asr_type);
            return false;
        }

        std::string model_type = type_map_[asr_type];
        std::string encoder_path, decoder_path, token_path, config_path;
        encoder_path.reserve(128);
        decoder_path.reserve(128);
        token_path.reserve(128);
        config_path.reserve(128);
        
        encoder_path = model_path + "/" + model_type + "-encoder.axmodel";
        decoder_path = model_path + "/" + model_type + "-decoder.axmodel";
        token_path   = model_path + "/" + model_type + "-tokens.txt";
        config_path  = model_path + "/" + model_type + "_config.json";

        ALOGD("encoder_path: %s", encoder_path.c_str());
        ALOGD("decoder_path: %s", decoder_path.c_str());
        ALOGD("token_path: %s", token_path.c_str());
        ALOGD("config_path: %s", config_path.c_str());

        if (!load_models_(encoder_path, decoder_path)) {
            return false;
        }

        if (!load_config_(config_path)) {
            return false;
        }

        if (!load_tokens_(token_path)) {
            return false;
        }

        init_features_();

        ALOGD("n_mels: %d", config_.n_mels);
        ALOGD("n_vocab: %d", config_.n_vocab);
        ALOGD("n_text_layer: %d", config_.n_text_layer);
        ALOGD("n_text_ctx: %d", config_.n_text_ctx);
        ALOGD("n_text_state: %d", config_.n_text_state);

        return true;
    }

    void uninit() {
        encoder_.unload_model();
        decoder_.unload_model();
    }

    bool run(const std::vector<float>& audio_data, int sample_rate, const std::string& language, std::string& text_result) {
        preprocess_(audio_data, sample_rate, config_.n_mels);
        ALOGD("preprocess finish");

        feature_.sot_seq[1] = get_lang_token_(language);

        encoder_.set_input(0, feature_.mel_bank.data());
        int ret = encoder_.run();
        if (ret) {
            ALOGE("encoder run failed! ret=0x%x", ret);
            return false;
        }
        ALOGD("run encoder finish");

        // copy cross_kv once
        dma_cross_kv_();

        // init mask
        std::fill(feature_.mask.begin(), feature_.mask.end(), 1);

        // init kv_cache
        std::fill(feature_.self_k_cache.begin(), feature_.self_k_cache.end(), 0);
        std::fill(feature_.self_v_cache.begin(), feature_.self_v_cache.end(), 0);

        int offset = 0;
        int idx = 0;
        std::vector<int> tokens;
        tokens.reserve(config_.n_text_ctx);

        // decode SOT
        for (auto token : feature_.sot_seq) {
            idx = run_decoder_(token, offset++);
        }
        ALOGD("run decoder sot finish");

        while (idx != config_.eot && offset < config_.n_text_ctx) {
            tokens.push_back(idx);
            idx = run_decoder_(idx, offset++);
        }

        text_result.clear();
        text_result.reserve(256);
        for (const auto i : tokens) {
            char str[32];
            base64_decode((const uint8_t*)tokens_[i].c_str(), (uint32_t)tokens_[i].size(), str);
            text_result += str;
        }

        // if (language == "zh") {
        //     const opencc::SimpleConverter converter("t2s.json");
        //     text_result = converter.Convert(s);
        // } else {
        //     text_result = s;
        // }
        return true;
    }

private:
    bool load_models_(const std::string& encoder_path, const std::string& decoder_path) {
        int ret = -1;
        ret = encoder_.load_model(encoder_path.c_str());
        if (0 != ret) {
            ALOGE("Load encoder failed! ret=0x%x", ret);
            return false;
        }

        ret = decoder_.load_model(decoder_path.c_str());
        if (0 != ret) {
            ALOGE("Load decoder failed! ret=0x%x", ret);
            return false;
        }
        return true;
    }

    bool load_tokens_(const std::string& token_path) {
        std::ifstream fs(token_path);
        if (!fs.is_open()) {
            ALOGE("Cannot open token file: %s", token_path.c_str());
            return false;
        }

        std::string line;
        tokens_.reserve(config_.n_vocab);
        while (std::getline(fs, line)) {
            size_t i = line.find(' ');
            tokens_.push_back(line.substr(0, i));
        }
        return true;
    }

    bool load_config_(const std::string& config_path) {
        std::ifstream fs(config_path);
        if (!fs.is_open()) {
            ALOGE("Cannot open config file: %s", config_path.c_str());
            return false;
        }
        json config = json::parse(fs);
        fs.close();

        auto lang_tokens = stringToVector<int>(config["all_language_tokens"]);
        auto lang_codes = stringToVector<std::string>(config["all_language_codes"]);

        lang_token_map_.clear();
        for (size_t i = 0; i < lang_tokens.size(); i++) {
            lang_token_map_.insert(std::make_pair(lang_codes[i], lang_tokens[i]));
        }

        config_.sample_rate = 16000;
        config_.n_mels = config["n_mels"];
        config_.n_vocab = config["n_vocab"];
        config_.n_text_layer = config["n_text_layer"];
        config_.n_text_ctx = config["n_text_ctx"];
        config_.n_text_state = config["n_text_state"];
        config_.sot = config["sot"];
        config_.eot = config["eot"];
        config_.no_timestamps = config["no_timestamps"];
        config_.transcribe = config["transcribe"];
        config_.translate = config["translate"];
        
        return true;
    }

    inline int get_lang_token_(const std::string& lang) {
        return lang_token_map_[lang];
    }

    void init_features_(void)  {
        int n_text_state = config_.n_text_state;
        int n_text_ctx = config_.n_text_ctx;
        int n_text_layer = config_.n_text_layer;
        
        feature_.sot_seq = {config_.sot, 0, config_.transcribe, config_.no_timestamps};
        feature_.mel_bank.resize(config_.n_mels * WHISPER_FRAME_NUM);
        feature_.mask.resize(n_text_ctx);
        feature_.self_k_cache.resize(n_text_layer * n_text_ctx * n_text_state);
        feature_.self_v_cache.resize(n_text_layer * n_text_ctx * n_text_state);
        feature_.this_self_k.resize(n_text_layer * n_text_state);
        feature_.this_self_v.resize(n_text_layer * n_text_state);
        feature_.logits.resize(config_.n_vocab);
    }

    void preprocess_(const std::vector<float>& audio_data, int sample_rate, int n_mels) {
        std::vector<float> resampled_data = utils::resample(audio_data, sample_rate, config_.sample_rate);

        int n_samples = resampled_data.size();
        auto mel = librosa::Feature::melspectrogram(resampled_data, WHISPER_SAMPLE_RATE, WHISPER_N_FFT, WHISPER_HOP_LENGTH, "hann", true, "reflect", 2.0f, n_mels, 0.0f, WHISPER_SAMPLE_RATE / 2.0f);
        int n_mel = mel.size();
        int n_frames = mel[0].size();

        // clamping and normalization
        float mmax = -std::numeric_limits<float>::max();
        for (int i = 0; i < n_mels; i++) {
            for (int n = 0; n < n_frames; n++) {
                mel[i][n] = std::log10(std::max(mel[i][n], 1e-10f));

                if (mel[i][n] > mmax) {
                    mmax = mel[i][n] ;
                }
            }
        }

        for (int i = 0; i < n_mels; i++) {
            for (int n = 0; n < n_frames; n++) {
                mel[i][n] = (std::max(mel[i][n], (float)(mmax - 8.0)) + 4.0)/4.0;
                mel[i].resize(WHISPER_FRAME_NUM);
            }
            std::memcpy(feature_.mel_bank.data() + i * WHISPER_FRAME_NUM, mel[i].data(), sizeof(float) * WHISPER_FRAME_NUM);
        }
    }
    
    void dma_cross_kv_() {
        // encoder output:
        // cross_k, cross_v, ...

        // decoder input:
        // tokens, self_k, self_v, cross_k, cross_v, offset, mask

        const int n_text_layer = config_.n_text_layer;
        int cross_kv_num = 2;
        int decoder_start_index = 3;

        for (int i = 0; i < cross_kv_num; i++) {
    #if defined(CHIP_AX650)      
            AX_U64 phySrc = encoder_.get_output_phy_addr(i);
            AX_U64 phyDst = decoder_.get_input_phy_addr(decoder_start_index + i);
            int size = encoder_.get_output_size(i);

            int ret = AX_DMA_MemCopy(phyDst, phySrc, (AX_U64)size);
            if (ret) {
                ALOGW("AX_DMA_MemCopy failed! ret=0x%x, fallback to sys memcpy", ret);

                decoder_.set_input(decoder_start_index + i, encoder_.get_output_ptr(i));
                return;
            }
    #else
            decoder_.set_input(decoder_start_index + i, encoder_.get_output_ptr(i));
    #endif                
        }
    }

    void causal_mask_1d_(int offset) {
        if (offset > 0) {
            // std::fill(m_feature.mask.begin(), m_feature.mask.begin() + n, 0);
            feature_.mask[offset - 1] = 0;
        }
    }

    int run_decoder_(int token, int offset) {
        // decoder input
        // token + self_k + self_v + cross_k + cross_v + offset + mask

        // decoder output
        // logits + this_self_k + this_self_v
        const int n_text_layer = config_.n_text_layer;
        const int n_text_ctx = config_.n_text_ctx;
        const int n_text_state = config_.n_text_state;
        const int self_kv_index = 1;
        const int offset_index = 1 + 2 + 2;
        const int mask_index = offset_index + 1;
        const int this_kv_index = 1;

        causal_mask_1d_(offset);

        decoder_.set_input(0, &token);
        decoder_.set_input(1, feature_.self_k_cache.data());
        decoder_.set_input(2, feature_.self_v_cache.data());

        // dma_cross_kv();

        decoder_.set_input(offset_index, &offset);
        decoder_.set_input(mask_index, feature_.mask.data());

        int ret = decoder_.run();
        if (ret) {
            ALOGE("decoder run failed! ret=0x%x", ret);
            return false;
        }

        // Update kv cache
        int k_output_index = 1;
        int v_output_index = 2;

        decoder_.get_output(k_output_index, feature_.this_self_k.data());
        decoder_.get_output(v_output_index, feature_.this_self_v.data());
        
        for (int i = 0; i < n_text_layer; i++) {
            float* k_dst_ptr = feature_.self_k_cache.data() + 
                            i * n_text_ctx * n_text_state + 
                            offset * n_text_state;
            float* k_src_ptr = feature_.this_self_k.data() +
                            i * n_text_state;
            memcpy(k_dst_ptr, k_src_ptr, sizeof(float) * n_text_state);
            
            float* v_dst_ptr = feature_.self_v_cache.data() + 
                            i * n_text_ctx * n_text_state + 
                            offset * n_text_state;
            float* v_src_ptr = feature_.this_self_v.data() +
                            i * n_text_state;
            memcpy(v_dst_ptr, v_src_ptr, sizeof(float) * n_text_state);
        }
        
        decoder_.get_output(0, feature_.logits.data());
        return argmax(feature_.logits);
    }

private:
    std::mutex mutex_;
    AxModelRunner encoder_, decoder_;
    std::vector<std::string> tokens_;
    std::map<std::string, int> lang_token_map_;
    WhisperConfig config_;
    WhisperFeature feature_;
    std::map<AX_ASR_TYPE_E, std::string> type_map_{
        {AX_WHISPER_TINY,  std::string("tiny")},
        {AX_WHISPER_BASE,  std::string("base")},
        {AX_WHISPER_SMALL, std::string("small")},
        {AX_WHISPER_TURBO, std::string("turbo")}
    };
};


Whisper::Whisper():
    impl_(std::make_unique<Whisper::Impl>()) {

}

Whisper::~Whisper() = default;

bool Whisper::init(AX_ASR_TYPE_E asr_type, const std::string& model_path) {
    return impl_->init(asr_type, model_path);
}

void Whisper::uninit(void) {
    impl_->uninit();
}

bool Whisper::run(const std::vector<float>& audio_data, int sample_rate, const std::string& language, std::string& text_result) {
    return impl_->run(audio_data, sample_rate, language, text_result);
}