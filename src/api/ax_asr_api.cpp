/**************************************************************************************************
 *
 * Copyright (c) 2019-2026 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Ningbo) Co., Ltd.
 *
 **************************************************************************************************/
#include "api/ax_asr_api.h"
#include "asr/asr_factory.hpp"
#include "utils/logger.h"
#include "utils/AudioFile.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the asr ASR system with specific configuration
 * 
 * Creates and initializes a new asr ASR context with the specified
 * model type, model path, and language. This function loads the appropriate
 * models, configures the recognizer, and prepares it for speech recognition.
 * 
 * @param model_type Type of asr model to use
 * @param model_path Directory path where model files are stored
 *                   Model files are expected to be in the format: *.axmodel
 * 
 * @return AX_ASR_HANDLE Opaque handle to the initialized asr context,
 *         or NULL if initialization fails
 * 
 * @note The caller is responsible for calling AX_ASR_Uninit() to free
 *       resources when the handle is no longer needed.
 * @example
 *   // Initialize recognition with whisper tiny model
 *   AX_ASR_HANDLE handle = AX_ASR_Init(WHISPER_TINY, "./models-ax650/");
 *   
 */
AX_ASR_API AX_ASR_HANDLE AX_ASR_Init(AX_ASR_TYPE_E asr_type, const char* model_path) {
    if (!model_path) {
        ALOGE("model_path is NULL!");
        return NULL;
    }

    ASRInterface* handle = ASRFactory::create(asr_type, std::string(model_path));
    if (!handle) {
        ALOGE("Create asr failed!");
        return NULL;
    }

    return static_cast<AX_ASR_HANDLE>(handle);
}

/**
 * @brief Deinitialize and release asr ASR resources
 * 
 * Cleans up all resources associated with the asr context, including
 * unloading models, freeing memory, and releasing hardware resources.
 * 
 * @param handle asr context handle obtained from AX_ASR_Init()
 * 
 * @warning After calling this function, the handle becomes invalid and
 *          should not be used in any subsequent API calls.
 */
AX_ASR_API void AX_ASR_Uninit(AX_ASR_HANDLE handle) {
    if (handle) {
        auto interface = static_cast<ASRInterface*>(handle);
        interface->uninit();
        delete interface;
    }
}

/**
 * @brief Perform speech recognition and return dynamically allocated string
 * 
 * @param handle asr context handle
 * @param wav_file Path to the input 16k pcmf32 WAV audio file
 * @param language Preferred language, 
 *      For whisper, check https://whisper-api.com/docs/languages/
 *      For sensevoice, support auto, zh, en, yue, ja, ko
 * @param result Pointer to receive the allocated result string
 * 
 * @return int Status code (0 = success, <0 = error)
 * 
 * @note The returned string is allocated with malloc() and must be freed
 *       by the caller using free() when no longer needed.
 */
AX_ASR_API int AX_ASR_RunFile(AX_ASR_HANDLE handle, 
                   const char* wav_file, 
                   const char* language,
                   char** result) {
    if (!handle) {
        ALOGE("handle is NULL!");
        return -1;
    }    

    if (!wav_file) {
        ALOGE("wav_file is NULL!");
        return -1;
    }      
    
    if (!language) {
        ALOGE("language is NULL!");
        return -1;
    }   
    
    if (!result) {
        ALOGE("result is NULL!");
        return -1;
    } 

    AudioFile<float> audio_file;

    if (!audio_file.load(wav_file)) {
        ALOGE("load wav failed!\n");
        return -1;
    }

    auto& samples = audio_file.samples[0];
    int n_samples = samples.size();
    int sample_rate = audio_file.getSampleRate();

    ALOGD("Audio info: sample_rate=%d, num_samples=%d, num_channels=%d", sample_rate, n_samples, audio_file.getNumChannels());
    
    // convert to mono
    if (audio_file.isStereo()) {
        for (int i = 0; i < n_samples; i++) {
            samples[i] = (samples[i] + audio_file.samples[1][i]) / 2;
        }
    }
    
    return AX_ASR_RunPCM(handle, samples.data(), n_samples, sample_rate, language, result);
}

/**
 * @brief Perform speech recognition and return dynamically allocated string
 * 
 * @param handle asr context handle
 * @param pcm_data 16k Mono PCM f32 data, range from -1.0 to 1.0,
 *      will be resampled if not 16k
 * @param num_samples Sample num of PCM data
 * @param sample_rate Sample rate of input audio
 * @param language Preferred language, 
 *      For whisper, check https://whisper-api.com/docs/languages/
 *      For sensevoice, support auto, zh, en, yue, ja, ko
 * @param result Pointer to receive the allocated result string
 * 
 * @return int Status code (0 = success, <0 = error)
 * 
 * @note The returned string is allocated with malloc() and must be freed
 *       by the caller using free() when no longer needed.
 */
AX_ASR_API int AX_ASR_RunPCM(AX_ASR_HANDLE handle, 
                   float* pcm_data, 
                   int num_samples,
                   int sample_rate,
                   const char* language,
                   char** result) {
    auto interface = static_cast<ASRInterface*>(handle);
    std::vector<float> audio_data(pcm_data, pcm_data + num_samples);    
    std::string text_result;
    
    if (!interface->run(audio_data, sample_rate, std::string(language), text_result)) {
        ALOGE("RunPCM failed!");
        return -1;
    }

    *result = strdup(text_result.c_str());

    return 0;
}

#ifdef __cplusplus
}
#endif                   