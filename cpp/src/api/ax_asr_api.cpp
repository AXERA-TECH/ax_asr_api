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

static std::string g_stream_g_stream_partial_copy_;
#include "utils/AudioLoader.hpp"

#include <string.h>

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
        return AX_ASR_ERR_INVALID_ARGUMENT;
    }    

    if (!wav_file) {
        ALOGE("wav_file is NULL!");
        return AX_ASR_ERR_INVALID_ARGUMENT;
    }      
    
    if (!language) {
        ALOGE("language is NULL!");
        return AX_ASR_ERR_INVALID_ARGUMENT;
    }   
    
    if (!result) {
        ALOGE("result is NULL!");
        return AX_ASR_ERR_INVALID_ARGUMENT;
    } 

    *result = nullptr;

    utils::AudioLoader audio_loader;
    auto interface = static_cast<ASRInterface*>(handle);

    if (!audio_loader.load(wav_file, interface->sample_rate())) {
        ALOGE("load wav failed!\n");
        return AX_ASR_ERR_AUDIO_LOAD_FAILED;
    }

    auto& samples = audio_loader.samples;
    int n_samples = samples.size();
    int sample_rate = audio_loader.get_sample_rate();
    
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
    if (!handle) {
        ALOGE("handle is NULL!");
        return AX_ASR_ERR_INVALID_ARGUMENT;
    }

    if (!pcm_data) {
        ALOGE("pcm_data is NULL!");
        return AX_ASR_ERR_INVALID_ARGUMENT;
    }

    if (num_samples <= 0) {
        ALOGE("num_samples(%d) must be positive!", num_samples);
        return AX_ASR_ERR_INVALID_ARGUMENT;
    }

    if (sample_rate <= 0) {
        ALOGE("sample_rate(%d) must be positive!", sample_rate);
        return AX_ASR_ERR_INVALID_ARGUMENT;
    }

    if (!language) {
        ALOGE("language is NULL!");
        return AX_ASR_ERR_INVALID_ARGUMENT;
    }

    if (!result) {
        ALOGE("result is NULL!");
        return AX_ASR_ERR_INVALID_ARGUMENT;
    }

    *result = nullptr;

    auto interface = static_cast<ASRInterface*>(handle);
    std::vector<float> audio_data(pcm_data, pcm_data + num_samples);    
    std::string text_result;
    
    if (!interface->run(audio_data, sample_rate, std::string(language), text_result)) {
        ALOGE("RunPCM failed!");
        return AX_ASR_ERR_RUN_FAILED;
    }

    *result = strdup(text_result.c_str());
    if (!*result) {
        ALOGE("strdup result failed!");
        return AX_ASR_ERR_NO_MEMORY;
    }

    return AX_ASR_SUCCESS;
}

AX_ASR_API void AX_ASR_Free(char* result) {
    free(result);
}

AX_ASR_API int AX_ASR_StreamInit(AX_ASR_HANDLE handle) {
    if (!handle) return AX_ASR_ERR_INVALID_ARGUMENT;
    auto interface = static_cast<ASRInterface*>(handle);
    interface->stream_init();
    return AX_ASR_SUCCESS;
}

AX_ASR_API int AX_ASR_StreamFeed(AX_ASR_HANDLE handle,
    float* pcm_data, int num_samples, int sample_rate) {
    if (!handle || !pcm_data || num_samples <= 0 || sample_rate <= 0)
        return AX_ASR_ERR_INVALID_ARGUMENT;
    auto interface = static_cast<ASRInterface*>(handle);
    std::vector<float> chunk(pcm_data, pcm_data + num_samples);
    interface->stream_feed(chunk, sample_rate);
    return AX_ASR_SUCCESS;
}

AX_ASR_API int AX_ASR_StreamResult(AX_ASR_HANDLE handle, const char** result) {
    if (!handle || !result) return AX_ASR_ERR_INVALID_ARGUMENT;
    auto interface = static_cast<ASRInterface*>(handle);
    static thread_local std::string partial;
    if (!interface->stream_result(partial)) {
        *result = nullptr;
        return AX_ASR_ERR_STREAM_NOT_SUPPORTED;
    }
    partial_copy_ = partial;  // stable pointer
    *result = partial_copy_.c_str();
    return AX_ASR_SUCCESS;
}

AX_ASR_API int AX_ASR_StreamReset(AX_ASR_HANDLE handle) {
    if (!handle) return AX_ASR_ERR_INVALID_ARGUMENT;
    auto interface = static_cast<ASRInterface*>(handle);
    interface->stream_reset();
    return AX_ASR_SUCCESS;
}

#ifdef __cplusplus
}
#endif                   
