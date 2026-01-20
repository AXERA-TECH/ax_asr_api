/**************************************************************************************************
 *
 * Copyright (c) 2019-2026 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Ningbo) Co., Ltd.
 *
 **************************************************************************************************/
#ifndef _AX_ASR_API_H_
#define _AX_ASR_API_H_

#ifdef __cplusplus
extern "C" {
#endif

#define AX_ASR_API __attribute__((visibility("default")))


// Supported asr
enum AX_ASR_TYPE_E {
    AX_WHISPER_TINY = 0,
    AX_WHISPER_BASE,
    AX_WHISPER_SMALL,
    AX_WHISPER_TURBO,
    AX_SENSEVOICE
};

/**
 * @brief Opaque handle type for asr ASR context
 * 
 * This handle encapsulates all internal state of the asr ASR system.
 * The actual implementation is hidden from C callers to maintain ABI stability.
 */
typedef void* AX_ASR_HANDLE;

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
AX_ASR_API AX_ASR_HANDLE AX_ASR_Init(AX_ASR_TYPE_E asr_type, const char* model_path);

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
AX_ASR_API void AX_ASR_Uninit(AX_ASR_HANDLE handle);

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
                   char** result);

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
                   char** result);                   

#ifdef __cplusplus
}
#endif

#endif // _AX_ASR_API_H_