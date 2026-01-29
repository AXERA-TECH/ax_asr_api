#include <stdio.h>
#include "utils/cmdline.hpp"
#include "utils/timer.hpp"
#include "utils/AudioLoader.hpp"

#ifdef __cplusplus
extern "C" {
#endif
#include "ax_asr_api.h"
#ifdef __cplusplus
}
#endif

int main(int argc, char** argv) {
    cmdline::parser cmd;
    cmd.add<std::string>("audio", 'a', "audio file, support wav and mp3", true, "");
#if defined(CHIP_AX650) || defined(CHIP_AX8850) 
    cmd.add<std::string>("model_path", 'p', "model path which contains axmodel", false, "./models-ax650/whisper");
#else
    cmd.add<std::string>("model_path", 'p', "model path which contains axmodel", false, "./models-ax630c/whisper");
#endif
    cmd.add<std::string>("language", 'l', "en, zh, ja, ko, etc.", false, "zh");
    cmd.parse_check(argc, argv);

    auto audio_file = cmd.get<std::string>("audio");
    auto model_path = cmd.get<std::string>("model_path");
    auto language = cmd.get<std::string>("language");

    utils::AudioLoader audio_loader;
    if (!audio_loader.load(audio_file)) {
        printf("load audio failed!\n");
        return -1;
    }

    int n_samples = audio_loader.get_num_samples();
    float duration = n_samples * 1.f / 16000;

    Timer timer;

    timer.start();
    AX_ASR_HANDLE handle = AX_ASR_Init(AX_WHISPER_BASE, model_path.c_str());
    timer.stop();

    if (!handle) {
        printf("AX_ASR_Init failed!\n");
        return -1;
    }

    printf("Init asr success, take %.4fseconds\n", timer.elapsed<std::chrono::seconds>());

    // Run
    timer.start();
    char* result;
    if (0 != AX_ASR_RunFile(handle, audio_file.c_str(), language.c_str(), &result)) {
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