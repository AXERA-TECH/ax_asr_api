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
#include "utils/resample.h"
#include "utils/librosa/eigen3/Eigen/Core"
#include "kaldi-native-fbank/csrc/online-feature.h"

// pImpl
class Sensevoice::Impl {
public:
    bool init(AX_ASR_TYPE_E asr_type, const std::string& model_path) {
        std::string spec_model_path = model_path + "/sensevoice.axmodel";
        std::string token_path = model_path + "/tokens.txt";

        int ret = encoder_.load_model(spec_model_path.c_str());
        if (0 != ret) {
            ALOGE("Load model %s failed!", spec_model_path.c_str());
            return false;
        }

        sample_rate_ = 16000;
        n_mels_ = 80;
        query_num_ = 4;
        padding_ = 16;

        lfr_window_size_ = 7;
        lfr_window_shift_ = 6;

        auto input_shape = encoder_.get_input_shape(0);
        max_seq_len_ = input_shape[1];
        feature_dim_ = input_shape[2];

        mask_.resize(max_seq_len_ + query_num_);
        sub_feat_.resize(max_seq_len_ * feature_dim_);

        auto output_shape = encoder_.get_output_shape(0);
        vocab_size_ = output_shape[2];
        ctc_logits_.resize(output_shape[1] * vocab_size_);

        init_fbank_();

        if (!load_tokens_(token_path)) {
            ALOGE("Load tokens from %s failed!", token_path.c_str());
            return false;
        }
        
        return true;
    }

    void uninit(void) {
        encoder_.unload_model();
    }

    bool run(const std::vector<float>& audio_data, int sample_rate, const std::string& language, std::string& text_result) {
        // convert to uint16
        std::vector<float> buf(audio_data.size());
        for (int32_t i = 0; i != audio_data.size(); ++i) {
            buf[i] = audio_data[i] * 32768;
        }
        // resample 
        auto resample_data = utils::resample(buf, sample_rate, sample_rate_);

        int language_token = lid_dict_[language];

        std::vector<float> features;
        int feat_len;
        preprocess_(resample_data, false, features, feat_len);

        int slice_len = max_seq_len_;
        int slice_num = static_cast<int>(std::ceil(feat_len * 1.0f / slice_len));
        ALOGD("feat_len=%d slice_len=%d slice_num=%d", feat_len, slice_len, slice_num);
        
        std::vector<int> asr_res;
        int slice_start = 0;
        int slice_end = 0;
        int actual_seq_len = 0;
        int ret = -1;

        for (int i = 0; i < slice_num; i++) {
            if (i == 0) {
                slice_start = 0;
                slice_end = std::min(slice_len, feat_len);
            } else {
                slice_start = i * slice_len - padding_;
                slice_end = std::min((i + 1) * slice_len - padding_, feat_len);
            }

            ALOGD("Slice %d: start=%d end=%d", i, slice_start, slice_end);

            actual_seq_len = slice_end - slice_start;
            std::fill(sub_feat_.begin(), sub_feat_.end(), 0.0f);
            memcpy(
                sub_feat_.data(),
                features.data() + slice_start * feature_dim_,
                actual_seq_len * feature_dim_ * sizeof(float)
            );

            sequence_mask_(actual_seq_len);

            encoder_.set_input(0, sub_feat_.data());
            encoder_.set_input(1, mask_.data());
            encoder_.set_input(2, &language_token);

            ret = encoder_.run();
            if (0 != ret) {
                ALOGE("Run encoder failed! ret=0x%x", ret);
                return false;
            }

            encoder_.get_output(0, ctc_logits_.data());
            encoder_.get_output(1, &encoder_out_lens_);

            auto token_int = postprocess_(ctc_logits_, encoder_out_lens_);
            asr_res.insert(asr_res.end(), token_int.begin(), token_int.end());
        }

        text_result.clear();
        text_result.reserve(256);
        ALOGD("asr_res.size() = %u", asr_res.size());
        for (auto i : asr_res) {
            text_result.append(tokens_[i]);
        }

        return true;
    }

private:
    void init_fbank_(void) {
        knf::FbankOptions opts;
        opts.frame_opts.dither = 1.0f;
        opts.frame_opts.snip_edges = true;
        opts.frame_opts.samp_freq = sample_rate_;
        opts.frame_opts.frame_shift_ms = 10;
        opts.frame_opts.frame_length_ms = 25;
        opts.frame_opts.remove_dc_offset = true;
        opts.frame_opts.window_type = "hamming";
        opts.frame_opts.remove_dc_offset = true;

        opts.mel_opts.num_bins = n_mels_;

        opts.mel_opts.high_freq = 0;
        opts.mel_opts.low_freq = 20;
        opts.mel_opts.is_librosa = false;

        fbank_ = std::make_unique<knf::OnlineFbank>(opts);

        neg_mean_ = std::vector<float>{
            -8.311879, -8.600912, -9.615928, -10.43595, -11.21292, -11.88333, -12.36243, -12.63706, -12.8818, -12.83066, -12.89103, -12.95666, -13.19763, -13.40598, -13.49113, -13.5546, -13.55639, -13.51915, -13.68284, -13.53289, -13.42107, -13.65519, -13.50713, -13.75251, -13.76715, -13.87408, -13.73109, -13.70412, -13.56073, -13.53488, -13.54895, -13.56228, -13.59408, -13.62047, -13.64198, -13.66109, -13.62669, -13.58297, -13.57387, -13.4739, -13.53063, -13.48348, -13.61047, -13.64716, -13.71546, -13.79184, -13.90614, -14.03098, -14.18205, -14.35881, -14.48419, -14.60172, -14.70591, -14.83362, -14.92122, -15.00622, -15.05122, -15.03119, -14.99028, -14.92302, -14.86927, -14.82691, -14.7972, -14.76909, -14.71356, -14.61277, -14.51696, -14.42252, -14.36405, -14.30451, -14.23161, -14.19851, -14.16633, -14.15649, -14.10504, -13.99518, -13.79562, -13.3996, -12.7767, -11.71208, -8.311879, -8.600912, -9.615928, -10.43595, -11.21292, -11.88333, -12.36243, -12.63706, -12.8818, -12.83066, -12.89103, -12.95666, -13.19763, -13.40598, -13.49113, -13.5546, -13.55639, -13.51915, -13.68284, -13.53289, -13.42107, -13.65519, -13.50713, -13.75251, -13.76715, -13.87408, -13.73109, -13.70412, -13.56073, -13.53488, -13.54895, -13.56228, -13.59408, -13.62047, -13.64198, -13.66109, -13.62669, -13.58297, -13.57387, -13.4739, -13.53063, -13.48348, -13.61047, -13.64716, -13.71546, -13.79184, -13.90614, -14.03098, -14.18205, -14.35881, -14.48419, -14.60172, -14.70591, -14.83362, -14.92122, -15.00622, -15.05122, -15.03119, -14.99028, -14.92302, -14.86927, -14.82691, -14.7972, -14.76909, -14.71356, -14.61277, -14.51696, -14.42252, -14.36405, -14.30451, -14.23161, -14.19851, -14.16633, -14.15649, -14.10504, -13.99518, -13.79562, -13.3996, -12.7767, -11.71208, -8.311879, -8.600912, -9.615928, -10.43595, -11.21292, -11.88333, -12.36243, -12.63706, -12.8818, -12.83066, -12.89103, -12.95666, -13.19763, -13.40598, -13.49113, -13.5546, -13.55639, -13.51915, -13.68284, -13.53289, -13.42107, -13.65519, -13.50713, -13.75251, -13.76715, -13.87408, -13.73109, -13.70412, -13.56073, -13.53488, -13.54895, -13.56228, -13.59408, -13.62047, -13.64198, -13.66109, -13.62669, -13.58297, -13.57387, -13.4739, -13.53063, -13.48348, -13.61047, -13.64716, -13.71546, -13.79184, -13.90614, -14.03098, -14.18205, -14.35881, -14.48419, -14.60172, -14.70591, -14.83362, -14.92122, -15.00622, -15.05122, -15.03119, -14.99028, -14.92302, -14.86927, -14.82691, -14.7972, -14.76909, -14.71356, -14.61277, -14.51696, -14.42252, -14.36405, -14.30451, -14.23161, -14.19851, -14.16633, -14.15649, -14.10504, -13.99518, -13.79562, -13.3996, -12.7767, -11.71208, -8.311879, -8.600912, -9.615928, -10.43595, -11.21292, -11.88333, -12.36243, -12.63706, -12.8818, -12.83066, -12.89103, -12.95666, -13.19763, -13.40598, -13.49113, -13.5546, -13.55639, -13.51915, -13.68284, -13.53289, -13.42107, -13.65519, -13.50713, -13.75251, -13.76715, -13.87408, -13.73109, -13.70412, -13.56073, -13.53488, -13.54895, -13.56228, -13.59408, -13.62047, -13.64198, -13.66109, -13.62669, -13.58297, -13.57387, -13.4739, -13.53063, -13.48348, -13.61047, -13.64716, -13.71546, -13.79184, -13.90614, -14.03098, -14.18205, -14.35881, -14.48419, -14.60172, -14.70591, -14.83362, -14.92122, -15.00622, -15.05122, -15.03119, -14.99028, -14.92302, -14.86927, -14.82691, -14.7972, -14.76909, -14.71356, -14.61277, -14.51696, -14.42252, -14.36405, -14.30451, -14.23161, -14.19851, -14.16633, -14.15649, -14.10504, -13.99518, -13.79562, -13.3996, -12.7767, -11.71208, -8.311879, -8.600912, -9.615928, -10.43595, -11.21292, -11.88333, -12.36243, -12.63706, -12.8818, -12.83066, -12.89103, -12.95666, -13.19763, -13.40598, -13.49113, -13.5546, -13.55639, -13.51915, -13.68284, -13.53289, -13.42107, -13.65519, -13.50713, -13.75251, -13.76715, -13.87408, -13.73109, -13.70412, -13.56073, -13.53488, -13.54895, -13.56228, -13.59408, -13.62047, -13.64198, -13.66109, -13.62669, -13.58297, -13.57387, -13.4739, -13.53063, -13.48348, -13.61047, -13.64716, -13.71546, -13.79184, -13.90614, -14.03098, -14.18205, -14.35881, -14.48419, -14.60172, -14.70591, -14.83362, -14.92122, -15.00622, -15.05122, -15.03119, -14.99028, -14.92302, -14.86927, -14.82691, -14.7972, -14.76909, -14.71356, -14.61277, -14.51696, -14.42252, -14.36405, -14.30451, -14.23161, -14.19851, -14.16633, -14.15649, -14.10504, -13.99518, -13.79562, -13.3996, -12.7767, -11.71208, -8.311879, -8.600912, -9.615928, -10.43595, -11.21292, -11.88333, -12.36243, -12.63706, -12.8818, -12.83066, -12.89103, -12.95666, -13.19763, -13.40598, -13.49113, -13.5546, -13.55639, -13.51915, -13.68284, -13.53289, -13.42107, -13.65519, -13.50713, -13.75251, -13.76715, -13.87408, -13.73109, -13.70412, -13.56073, -13.53488, -13.54895, -13.56228, -13.59408, -13.62047, -13.64198, -13.66109, -13.62669, -13.58297, -13.57387, -13.4739, -13.53063, -13.48348, -13.61047, -13.64716, -13.71546, -13.79184, -13.90614, -14.03098, -14.18205, -14.35881, -14.48419, -14.60172, -14.70591, -14.83362, -14.92122, -15.00622, -15.05122, -15.03119, -14.99028, -14.92302, -14.86927, -14.82691, -14.7972, -14.76909, -14.71356, -14.61277, -14.51696, -14.42252, -14.36405, -14.30451, -14.23161, -14.19851, -14.16633, -14.15649, -14.10504, -13.99518, -13.79562, -13.3996, -12.7767, -11.71208, -8.311879, -8.600912, -9.615928, -10.43595, -11.21292, -11.88333, -12.36243, -12.63706, -12.8818, -12.83066, -12.89103, -12.95666, -13.19763, -13.40598, -13.49113, -13.5546, -13.55639, -13.51915, -13.68284, -13.53289, -13.42107, -13.65519, -13.50713, -13.75251, -13.76715, -13.87408, -13.73109, -13.70412, -13.56073, -13.53488, -13.54895, -13.56228, -13.59408, -13.62047, -13.64198, -13.66109, -13.62669, -13.58297, -13.57387, -13.4739, -13.53063, -13.48348, -13.61047, -13.64716, -13.71546, -13.79184, -13.90614, -14.03098, -14.18205, -14.35881, -14.48419, -14.60172, -14.70591, -14.83362, -14.92122, -15.00622, -15.05122, -15.03119, -14.99028, -14.92302, -14.86927, -14.82691, -14.7972, -14.76909, -14.71356, -14.61277, -14.51696, -14.42252, -14.36405, -14.30451, -14.23161, -14.19851, -14.16633, -14.15649, -14.10504, -13.99518, -13.79562, -13.3996, -12.7767, -11.71208
        };
        inv_stddev_ = std::vector<float>{
            0.155775, 0.154484, 0.1527379, 0.1518718, 0.1506028, 0.1489256, 0.147067, 0.1447061, 0.1436307, 0.1443568, 0.1451849, 0.1455157, 0.1452821, 0.1445717, 0.1439195, 0.1435867, 0.1436018, 0.1438781, 0.1442086, 0.1448844, 0.1454756, 0.145663, 0.146268, 0.1467386, 0.1472724, 0.147664, 0.1480913, 0.1483739, 0.1488841, 0.1493636, 0.1497088, 0.1500379, 0.1502916, 0.1505389, 0.1506787, 0.1507102, 0.1505992, 0.1505445, 0.1505938, 0.1508133, 0.1509569, 0.1512396, 0.1514625, 0.1516195, 0.1516156, 0.1515561, 0.1514966, 0.1513976, 0.1512612, 0.151076, 0.1510596, 0.1510431, 0.151077, 0.1511168, 0.1511917, 0.151023, 0.1508045, 0.1505885, 0.1503493, 0.1502373, 0.1501726, 0.1500762, 0.1500065, 0.1499782, 0.150057, 0.1502658, 0.150469, 0.1505335, 0.1505505, 0.1505328, 0.1504275, 0.1502438, 0.1499674, 0.1497118, 0.1494661, 0.1493102, 0.1493681, 0.1495501, 0.1499738, 0.1509654, 0.155775, 0.154484, 0.1527379, 0.1518718, 0.1506028, 0.1489256, 0.147067, 0.1447061, 0.1436307, 0.1443568, 0.1451849, 0.1455157, 0.1452821, 0.1445717, 0.1439195, 0.1435867, 0.1436018, 0.1438781, 0.1442086, 0.1448844, 0.1454756, 0.145663, 0.146268, 0.1467386, 0.1472724, 0.147664, 0.1480913, 0.1483739, 0.1488841, 0.1493636, 0.1497088, 0.1500379, 0.1502916, 0.1505389, 0.1506787, 0.1507102, 0.1505992, 0.1505445, 0.1505938, 0.1508133, 0.1509569, 0.1512396, 0.1514625, 0.1516195, 0.1516156, 0.1515561, 0.1514966, 0.1513976, 0.1512612, 0.151076, 0.1510596, 0.1510431, 0.151077, 0.1511168, 0.1511917, 0.151023, 0.1508045, 0.1505885, 0.1503493, 0.1502373, 0.1501726, 0.1500762, 0.1500065, 0.1499782, 0.150057, 0.1502658, 0.150469, 0.1505335, 0.1505505, 0.1505328, 0.1504275, 0.1502438, 0.1499674, 0.1497118, 0.1494661, 0.1493102, 0.1493681, 0.1495501, 0.1499738, 0.1509654, 0.155775, 0.154484, 0.1527379, 0.1518718, 0.1506028, 0.1489256, 0.147067, 0.1447061, 0.1436307, 0.1443568, 0.1451849, 0.1455157, 0.1452821, 0.1445717, 0.1439195, 0.1435867, 0.1436018, 0.1438781, 0.1442086, 0.1448844, 0.1454756, 0.145663, 0.146268, 0.1467386, 0.1472724, 0.147664, 0.1480913, 0.1483739, 0.1488841, 0.1493636, 0.1497088, 0.1500379, 0.1502916, 0.1505389, 0.1506787, 0.1507102, 0.1505992, 0.1505445, 0.1505938, 0.1508133, 0.1509569, 0.1512396, 0.1514625, 0.1516195, 0.1516156, 0.1515561, 0.1514966, 0.1513976, 0.1512612, 0.151076, 0.1510596, 0.1510431, 0.151077, 0.1511168, 0.1511917, 0.151023, 0.1508045, 0.1505885, 0.1503493, 0.1502373, 0.1501726, 0.1500762, 0.1500065, 0.1499782, 0.150057, 0.1502658, 0.150469, 0.1505335, 0.1505505, 0.1505328, 0.1504275, 0.1502438, 0.1499674, 0.1497118, 0.1494661, 0.1493102, 0.1493681, 0.1495501, 0.1499738, 0.1509654, 0.155775, 0.154484, 0.1527379, 0.1518718, 0.1506028, 0.1489256, 0.147067, 0.1447061, 0.1436307, 0.1443568, 0.1451849, 0.1455157, 0.1452821, 0.1445717, 0.1439195, 0.1435867, 0.1436018, 0.1438781, 0.1442086, 0.1448844, 0.1454756, 0.145663, 0.146268, 0.1467386, 0.1472724, 0.147664, 0.1480913, 0.1483739, 0.1488841, 0.1493636, 0.1497088, 0.1500379, 0.1502916, 0.1505389, 0.1506787, 0.1507102, 0.1505992, 0.1505445, 0.1505938, 0.1508133, 0.1509569, 0.1512396, 0.1514625, 0.1516195, 0.1516156, 0.1515561, 0.1514966, 0.1513976, 0.1512612, 0.151076, 0.1510596, 0.1510431, 0.151077, 0.1511168, 0.1511917, 0.151023, 0.1508045, 0.1505885, 0.1503493, 0.1502373, 0.1501726, 0.1500762, 0.1500065, 0.1499782, 0.150057, 0.1502658, 0.150469, 0.1505335, 0.1505505, 0.1505328, 0.1504275, 0.1502438, 0.1499674, 0.1497118, 0.1494661, 0.1493102, 0.1493681, 0.1495501, 0.1499738, 0.1509654, 0.155775, 0.154484, 0.1527379, 0.1518718, 0.1506028, 0.1489256, 0.147067, 0.1447061, 0.1436307, 0.1443568, 0.1451849, 0.1455157, 0.1452821, 0.1445717, 0.1439195, 0.1435867, 0.1436018, 0.1438781, 0.1442086, 0.1448844, 0.1454756, 0.145663, 0.146268, 0.1467386, 0.1472724, 0.147664, 0.1480913, 0.1483739, 0.1488841, 0.1493636, 0.1497088, 0.1500379, 0.1502916, 0.1505389, 0.1506787, 0.1507102, 0.1505992, 0.1505445, 0.1505938, 0.1508133, 0.1509569, 0.1512396, 0.1514625, 0.1516195, 0.1516156, 0.1515561, 0.1514966, 0.1513976, 0.1512612, 0.151076, 0.1510596, 0.1510431, 0.151077, 0.1511168, 0.1511917, 0.151023, 0.1508045, 0.1505885, 0.1503493, 0.1502373, 0.1501726, 0.1500762, 0.1500065, 0.1499782, 0.150057, 0.1502658, 0.150469, 0.1505335, 0.1505505, 0.1505328, 0.1504275, 0.1502438, 0.1499674, 0.1497118, 0.1494661, 0.1493102, 0.1493681, 0.1495501, 0.1499738, 0.1509654, 0.155775, 0.154484, 0.1527379, 0.1518718, 0.1506028, 0.1489256, 0.147067, 0.1447061, 0.1436307, 0.1443568, 0.1451849, 0.1455157, 0.1452821, 0.1445717, 0.1439195, 0.1435867, 0.1436018, 0.1438781, 0.1442086, 0.1448844, 0.1454756, 0.145663, 0.146268, 0.1467386, 0.1472724, 0.147664, 0.1480913, 0.1483739, 0.1488841, 0.1493636, 0.1497088, 0.1500379, 0.1502916, 0.1505389, 0.1506787, 0.1507102, 0.1505992, 0.1505445, 0.1505938, 0.1508133, 0.1509569, 0.1512396, 0.1514625, 0.1516195, 0.1516156, 0.1515561, 0.1514966, 0.1513976, 0.1512612, 0.151076, 0.1510596, 0.1510431, 0.151077, 0.1511168, 0.1511917, 0.151023, 0.1508045, 0.1505885, 0.1503493, 0.1502373, 0.1501726, 0.1500762, 0.1500065, 0.1499782, 0.150057, 0.1502658, 0.150469, 0.1505335, 0.1505505, 0.1505328, 0.1504275, 0.1502438, 0.1499674, 0.1497118, 0.1494661, 0.1493102, 0.1493681, 0.1495501, 0.1499738, 0.1509654, 0.155775, 0.154484, 0.1527379, 0.1518718, 0.1506028, 0.1489256, 0.147067, 0.1447061, 0.1436307, 0.1443568, 0.1451849, 0.1455157, 0.1452821, 0.1445717, 0.1439195, 0.1435867, 0.1436018, 0.1438781, 0.1442086, 0.1448844, 0.1454756, 0.145663, 0.146268, 0.1467386, 0.1472724, 0.147664, 0.1480913, 0.1483739, 0.1488841, 0.1493636, 0.1497088, 0.1500379, 0.1502916, 0.1505389, 0.1506787, 0.1507102, 0.1505992, 0.1505445, 0.1505938, 0.1508133, 0.1509569, 0.1512396, 0.1514625, 0.1516195, 0.1516156, 0.1515561, 0.1514966, 0.1513976, 0.1512612, 0.151076, 0.1510596, 0.1510431, 0.151077, 0.1511168, 0.1511917, 0.151023, 0.1508045, 0.1505885, 0.1503493, 0.1502373, 0.1501726, 0.1500762, 0.1500065, 0.1499782, 0.150057, 0.1502658, 0.150469, 0.1505335, 0.1505505, 0.1505328, 0.1504275, 0.1502438, 0.1499674, 0.1497118, 0.1494661, 0.1493102, 0.1493681, 0.1495501, 0.1499738, 0.1509654
        };
    }

    bool load_tokens_(const std::string& token_path) {
        std::ifstream fs(token_path);
        if (!fs.is_open()) {
            ALOGE("Cannot open token file: %s", token_path.c_str());
            return false;
        }

        std::string line;
        tokens_.reserve(vocab_size_);
        while (std::getline(fs, line)) {
            tokens_.push_back(line);
        }
        return true;
    }

    void preprocess_(const std::vector<float>& audio_data, bool normalize, std::vector<float>& features, int& num_frames) {
        fbank_->AcceptWaveform(sample_rate_, audio_data.data(), audio_data.size());
        fbank_->InputFinished();

        int32_t n = fbank_->NumFramesReady();

        features.resize(n * n_mels_);

        ALOGD("preprocess: normalize: %d", normalize);
        ALOGD("preprocess: feature dim: %d %d", n, n_mels_);

        float *p = features.data();

        for (int32_t i = 0; i < n; ++i) {
            const float *f = fbank_->GetFrame(i);
            std::copy(f, f + n_mels_, p);
            p += n_mels_;
        }

        if (normalize)
            normalize_features_(features.data(), n, n_mels_);

        features = apply_lfr_(features);
        apply_cmvn_(&features);

        feature_dim_ = n_mels_ * lfr_window_size_;
        num_frames = features.size() / feature_dim_;
        ALOGD("preprocess: final feature dim: %d %d", num_frames, feature_dim_);
    }

    void normalize_features_(float *p, int32_t num_frames,
                                      int32_t feature_dim) {
        using RowMajorMat =
            Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

        Eigen::Map<RowMajorMat> x(p, num_frames, feature_dim);

        Eigen::RowVectorXf mean = x.colwise().mean();
        Eigen::RowVectorXf var =
            (x.array().square().colwise().mean() - mean.array().square())
                .max(0.0f);  // avoid negative due to FP error

        Eigen::RowVectorXf inv_std = (var.array().sqrt() + 1e-5f).inverse();

        x.array() =
            (x.array().rowwise() - mean.array()).rowwise() * inv_std.array();
    }

    std::vector<float> apply_lfr_(const std::vector<float> &in) const {
        int32_t in_feat_dim = n_mels_;

        int32_t in_num_frames = in.size() / in_feat_dim;
        int32_t out_num_frames =
            (in_num_frames - lfr_window_size_) / lfr_window_shift_ + 1;
        int32_t out_feat_dim = in_feat_dim * lfr_window_size_;

        std::vector<float> out(out_num_frames * out_feat_dim);

        const float *p_in = in.data();
        float *p_out = out.data();

        for (int32_t i = 0; i != out_num_frames; ++i) {
            std::copy(p_in, p_in + out_feat_dim, p_out);

            p_out += out_feat_dim;
            p_in += lfr_window_shift_ * in_feat_dim;
        }

        return out;
    }

    void apply_cmvn_(std::vector<float> *v) const {
        int32_t dim = static_cast<int32_t>(neg_mean_.size());
        int32_t num_frames = static_cast<int32_t>(v->size()) / dim;
        Eigen::Map<
            Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>
            mat(v->data(), num_frames, dim);
        Eigen::Map<const Eigen::RowVectorXf> neg_mean_vec(neg_mean_.data(), dim);

        Eigen::Map<const Eigen::RowVectorXf> inv_stddev_vec(inv_stddev_.data(), dim);
        mat.array() = (mat.array().rowwise() + neg_mean_vec.array()).rowwise() *
                    inv_stddev_vec.array();
    }

    void sequence_mask_(int actual_seq_len) {
        std::fill(mask_.begin(), mask_.end(), 0);
        std::fill(mask_.begin(), mask_.begin() + actual_seq_len, 1);
    }

    std::vector<int> postprocess_(const std::vector<float>& ctc_logits, int encoder_out_lens) {
        ALOGD("postprocess: encoder_out_lens=%d", encoder_out_lens);

        std::vector<int> token_int;
        std::vector<int> yseq(encoder_out_lens - 4);
        for (int i = 4; i < encoder_out_lens; i++) {
            auto max_it = std::max_element(
                ctc_logits.begin() + i * vocab_size_, 
                ctc_logits.begin() + (i + 1) * vocab_size_);
            yseq[i - 4] = std::distance(ctc_logits.begin() + i * vocab_size_, max_it);
        }

        ALOGD("before unique_consecutive: yseq.size() = %u", yseq.size());
        unique_consecutive_(yseq);
        ALOGD("after unique_consecutive: yseq.size() = %u", yseq.size());

        token_int.reserve(encoder_out_lens - 4);
        for (auto i : yseq) {
            if (i != 0) {
                token_int.push_back(i);
            }
        }
        return token_int;
    }

    void unique_consecutive_(std::vector<int>& arr) {
        auto new_end = std::unique(arr.begin(), arr.end());
        arr.erase(new_end, arr.end());
    }

private:
    AxModelRunner encoder_;    
    int sample_rate_;
    int n_mels_;
    std::unique_ptr<knf::OnlineFbank> fbank_;
    int lfr_window_size_, lfr_window_shift_;
    std::vector<float> neg_mean_, inv_stddev_;
    std::vector<int> mask_;
    std::vector<float> sub_feat_;
    int max_seq_len_, feature_dim_;
    std::map<std::string, int> lid_dict_{
        {"auto", 0},
        {"zh",   3},
        {"en",   4},
        {"yue",  7},
        {"ja",  11},
        {"ko",  12}
    };
    int query_num_;
    int padding_;
    int vocab_size_;
    std::vector<float> ctc_logits_;
    int encoder_out_lens_;
    std::vector<std::string> tokens_;
};

Sensevoice::Sensevoice():
    impl_(std::make_unique<Sensevoice::Impl>()) {

}

Sensevoice::~Sensevoice() {
    uninit();
}

bool Sensevoice::init(AX_ASR_TYPE_E asr_type, const std::string& model_path) {
    return impl_->init(asr_type, model_path);
}

void Sensevoice::uninit(void) {
    impl_->uninit();
}

bool Sensevoice::run(const std::vector<float>& audio_data, int sample_rate, const std::string& language, std::string& text_result) {
    return impl_->run(audio_data, sample_rate, language, text_result);
}