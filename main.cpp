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
    cmdline::parser cmd;
    cmd.add<std::string>("wav", 'w', "wav file", true, "");
#if defined(CHIP_AX650)    
    cmd.add<std::string>("model_path", 'p', "model path which contains whisper/ sensevoice/", false, "./models-ax650");
#else
    cmd.add<std::string>("model_path", 'p', "model path which contains whisper/ sensevoice/", false, "./models-ax630c");
#endif
    cmd.add<std::string>("language", 'l', "en, zh", false, "zh");
    cmd.parse_check(argc, argv);

    // 0. get app args, can be removed from user's app
    auto wav_file = cmd.get<std::string>("wav");
    auto model_path = cmd.get<std::string>("model_path");
    auto language = cmd.get<std::string>("language");

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
    AX_ASR_HANDLE handle = AX_ASR_Init(AX_WHISPER_TINY, model_path.c_str());
    timer.stop();

    if (!handle) {
        printf("AX_ASR_Init failed!\n");
        return -1;
    }

    printf("Init asr success, take %.4fseconds\n", timer.elapsed<std::chrono::seconds>());

    // Run
    timer.start();
    char* result;
    if (0 != AX_ASR_RunFile(handle, wav_file.c_str(), language.c_str(), &result)) {
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