#include <stdio.h>
#include "utils/cmdline.hpp"
#include "utils/timer.hpp"
#include "utils/AudioFile.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "ax_asr_api.h"
#ifdef __cplusplus
}
#endif

int main(int argc, char** argv) {
    const char* wav_file = "./demo.wav";
#if defined(CHIP_AX650)    
    const char* model_path = "./models-ax650";
#else
    const char* model_path = "./models-ax630c";
#endif    
    const char* language = "zh";

    AudioFile<float> audio_file;
    if (!audio_file.load(wav_file)) {
        printf("load wav failed!\n");
        return -1;
    }

    auto& samples = audio_file.samples[0];
    int n_samples = samples.size();
    float duration = n_samples * 1.f / 16000;

    Timer timer;

    timer.start();
    AX_ASR_HANDLE handle = AX_ASR_Init(WHISPER_SMALL, model_path);
    timer.stop();

    if (!handle) {
        printf("AX_ASR_Init failed!\n");
        return -1;
    }

    printf("Init asr success, take %.4fseconds\n", timer.elapsed<std::chrono::seconds>());

    // Run
    timer.start();
    char* result;
    if (0 != AX_ASR_RunFile(handle, wav_file, language, &result)) {
        printf("AX_ASR_RunFile failed!\n");
        AX_ASR_Uninit(handle);
        return -1;
    }
    timer.stop();
    float inference_time = timer.elapsed<std::chrono::seconds>();

    printf("Result: %s\n", result);
    printf("RTF(%.2f / %.2f) = %.4f\n", inference_time, duration, inference_time / duration);

    free(result);
    AX_ASR_Uninit(handle);
    return 0;
}