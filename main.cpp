#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "ax_asr_api.h"
#ifdef __cplusplus
}
#endif

int main(int argc, char** argv) {
    AX_ASR_HANDLE handle = AX_ASR_Init(WHISPER_TINY, NULL);
    AX_ASR_Uninit(handle);
    return 0;
}