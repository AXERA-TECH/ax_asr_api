#include <stdio.h>
#include <map>
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

static std::map<std::string, AX_ASR_TYPE_E>     model_type_map = {
    {"whisper_tiny", AX_WHISPER_TINY},
    {"whisper_base", AX_WHISPER_BASE},
    {"whisper_small", AX_WHISPER_SMALL},
    {"whisper_turbo", AX_WHISPER_TURBO},
    {"sensevoice", AX_SENSEVOICE}
};

int main(int argc, char** argv) {
    cmdline::parser cmd;
    cmd.add<std::string>("audio", 'a', "audio file, support wav and mp3", true, "");
    cmd.add<std::string>("model_type", 't', "Choose from whisper_tiny, whisper_base, whisper_small, whisper_turbo, sensevoice", true, "");
#if defined(CHIP_AX650) || defined(CHIP_AX8850)
    cmd.add<std::string>("model_path", 'p', "model path which contains axmodel", false, "./models-ax650");
#else
    cmd.add<std::string>("model_path", 'p', "model path which contains axmodel", false, "./models-ax630c");
#endif
    cmd.add<std::string>("language", 'l', "en, zh", false, "zh");
    cmd.parse_check(argc, argv);

    // 0. get app args, can be removed from user's app
    auto audio_file = cmd.get<std::string>("audio");
    auto model_type_key = cmd.get<std::string>("model_type");
    auto model_path = cmd.get<std::string>("model_path");
    auto language = cmd.get<std::string>("language");

    if (model_type_map.find(model_type_key) == model_type_map.end()) {
        fprintf(stderr, "Cannot find model_type: %s, please check help", model_type_key.c_str());
        return -1;
    }

    auto model_type = model_type_map[model_type_key];

    utils::AudioLoader audio_loader;
    if (!audio_loader.load(audio_file)) {
        printf("load audio failed!\n");
        return -1;
    }

    int n_samples = audio_loader.get_num_samples();
    float duration = n_samples * 1.f / 16000;

    Timer timer;

    timer.start();
    AX_ASR_HANDLE handle = AX_ASR_Init(model_type, model_path.c_str());
    timer.stop();

    if (!handle) {
        printf("AX_ASR_Init failed!\n");
        return -1;
    }

    printf("Init asr: %s success, take %.4fseconds\n", model_type_key.c_str(), timer.elapsed<std::chrono::seconds>());

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