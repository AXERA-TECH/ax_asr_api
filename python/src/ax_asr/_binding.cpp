#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <stdexcept>
#include <cstring>

#include "ax_asr_api.h"

namespace py = pybind11;

static void check_ret(int ret, const char* context) {
    if (ret == AX_ASR_SUCCESS)
        return;
    const char* msg = "Unknown error";
    switch (ret) {
        case AX_ASR_ERR_INVALID_ARGUMENT: msg = "Invalid argument"; break;
        case AX_ASR_ERR_INIT_FAILED:      msg = "Init failed"; break;
        case AX_ASR_ERR_AUDIO_LOAD_FAILED:msg = "Audio load failed"; break;
        case AX_ASR_ERR_RUN_FAILED:       msg = "Run failed"; break;
        case AX_ASR_ERR_NO_MEMORY:        msg = "No memory"; break;
    }
    throw std::runtime_error(std::string(context) + ": " + msg);
}

// Thin wrapper: init → return opaque handle as int
static AX_ASR_HANDLE init_handle(int asr_type, const std::string& model_path) {
    AX_ASR_HANDLE h = AX_ASR_Init(static_cast<AX_ASR_TYPE_E>(asr_type), model_path.c_str());
    if (!h) {
        throw std::runtime_error("AX_ASR_Init returned NULL");
    }
    return h;
}

static void uninit_handle(AX_ASR_HANDLE handle) {
    if (handle) {
        AX_ASR_Uninit(handle);
    }
}

static std::string run_file(AX_ASR_HANDLE handle, const std::string& wav_file,
                             const std::string& language) {
    if (!handle)
        throw std::runtime_error("Handle is null");
    char* result = nullptr;
    int ret = AX_ASR_RunFile(handle, wav_file.c_str(), language.c_str(), &result);
    check_ret(ret, "AX_ASR_RunFile");
    std::string text(result ? result : "");
    AX_ASR_Free(result);
    return text;
}

static std::string run_pcm(AX_ASR_HANDLE handle, py::array_t<float, py::array::c_style> pcm,
                            int sample_rate, const std::string& language) {
    if (!handle)
        throw std::runtime_error("Handle is null");
    auto buf = pcm.request();
    if (buf.ndim != 1)
        throw std::runtime_error("PCM data must be 1-dimensional float32 array");
    const float* data = static_cast<const float*>(buf.ptr);
    int num_samples = static_cast<int>(buf.size);

    char* result = nullptr;
    int ret = AX_ASR_RunPCM(handle, const_cast<float*>(data),
                             num_samples, sample_rate, language.c_str(), &result);
    check_ret(ret, "AX_ASR_RunPCM");
    std::string text(result ? result : "");
    AX_ASR_Free(result);
    return text;
}


// ---- Streaming API ----

static void stream_init(AX_ASR_HANDLE handle) {
    if (!handle) throw std::runtime_error("Handle is null");
    int ret = AX_ASR_StreamInit(handle);
    check_ret(ret, "AX_ASR_StreamInit");
}

static void stream_feed(AX_ASR_HANDLE handle, py::array_t<float, py::array::c_style> pcm,
                         int sample_rate) {
    if (!handle) throw std::runtime_error("Handle is null");
    auto buf = pcm.request();
    if (buf.ndim != 1)
        throw std::runtime_error("PCM data must be 1-dimensional float32 array");
    const float* data = static_cast<const float*>(buf.ptr);
    int n = static_cast<int>(buf.size);
    int ret = AX_ASR_StreamFeed(handle, const_cast<float*>(data), n, sample_rate);
    check_ret(ret, "AX_ASR_StreamFeed");
}

static std::string stream_result(AX_ASR_HANDLE handle) {
    if (!handle) throw std::runtime_error("Handle is null");
    const char* result = nullptr;
    int ret = AX_ASR_StreamResult(handle, &result);
    if (ret == AX_ASR_ERR_STREAM_NOT_SUPPORTED)
        return "";
    check_ret(ret, "AX_ASR_StreamResult");
    return result ? std::string(result) : std::string("");
}

static void stream_reset(AX_ASR_HANDLE handle) {
    if (!handle) throw std::runtime_error("Handle is null");
    int ret = AX_ASR_StreamReset(handle);
    check_ret(ret, "AX_ASR_StreamReset");
}

PYBIND11_MODULE(_ax_asr_core, m) {
    m.doc() = "Low-level pybind11 binding for ax_asr_api";

    py::enum_<AX_ASR_TYPE_E>(m, "AsrType")
        .value("WHISPER_TINY", AX_WHISPER_TINY)
        .value("WHISPER_BASE", AX_WHISPER_BASE)
        .value("WHISPER_SMALL", AX_WHISPER_SMALL)
        .value("WHISPER_TURBO", AX_WHISPER_TURBO)
        .value("SENSEVOICE", AX_SENSEVOICE)
        .export_values();

    py::enum_<AX_ASR_STATUS_E>(m, "AsrStatus")
        .value("SUCCESS", AX_ASR_SUCCESS)
        .value("ERR_INVALID_ARGUMENT", AX_ASR_ERR_INVALID_ARGUMENT)
        .value("ERR_INIT_FAILED", AX_ASR_ERR_INIT_FAILED)
        .value("ERR_AUDIO_LOAD_FAILED", AX_ASR_ERR_AUDIO_LOAD_FAILED)
        .value("ERR_RUN_FAILED", AX_ASR_ERR_RUN_FAILED)
        .value("ERR_NO_MEMORY", AX_ASR_ERR_NO_MEMORY)
        .export_values();

    m.def("init", &init_handle, py::arg("asr_type"), py::arg("model_path"),
          "Initialize ASR handle. Returns opaque handle.");
    m.def("uninit", &uninit_handle, py::arg("handle"),
          "Release ASR handle.");
    m.def("run_file", &run_file, py::arg("handle"), py::arg("wav_file"),
          py::arg("language"), "Transcribe audio file, return text.");
    m.def("run_pcm", &run_pcm, py::arg("handle"), py::arg("pcm"),
          py::arg("sample_rate"), py::arg("language"),
          "Transcribe PCM float32 array, return text.");

    // Streaming
    m.def("stream_init", &stream_init, py::arg("handle"),
          "Initialize streaming recognition state.");
    m.def("stream_feed", &stream_feed, py::arg("handle"), py::arg("pcm"),
          py::arg("sample_rate"),
          "Feed audio chunk for streaming recognition.");
    m.def("stream_result", &stream_result, py::arg("handle"),
          "Get current partial streaming result.");
    m.def("stream_reset", &stream_reset, py::arg("handle"),
          "Reset streaming state.");
}

    m.doc() = "Low-level pybind11 binding for ax_asr_api";

    py::enum_<AX_ASR_TYPE_E>(m, "AsrType")
        .value("WHISPER_TINY", AX_WHISPER_TINY)
        .value("WHISPER_BASE", AX_WHISPER_BASE)
        .value("WHISPER_SMALL", AX_WHISPER_SMALL)
        .value("WHISPER_TURBO", AX_WHISPER_TURBO)
        .value("SENSEVOICE", AX_SENSEVOICE)
        .export_values();

    py::enum_<AX_ASR_STATUS_E>(m, "AsrStatus")
        .value("SUCCESS", AX_ASR_SUCCESS)
        .value("ERR_INVALID_ARGUMENT", AX_ASR_ERR_INVALID_ARGUMENT)
        .value("ERR_INIT_FAILED", AX_ASR_ERR_INIT_FAILED)
        .value("ERR_AUDIO_LOAD_FAILED", AX_ASR_ERR_AUDIO_LOAD_FAILED)
        .value("ERR_RUN_FAILED", AX_ASR_ERR_RUN_FAILED)
        .value("ERR_NO_MEMORY", AX_ASR_ERR_NO_MEMORY)
        .export_values();

    m.def("init", &init_handle, py::arg("asr_type"), py::arg("model_path"),
          "Initialize ASR handle. Returns opaque handle.");
    m.def("uninit", &uninit_handle, py::arg("handle"),
          "Release ASR handle.");
    m.def("run_file", &run_file, py::arg("handle"), py::arg("wav_file"),
          py::arg("language"), "Transcribe audio file, return text.");
    m.def("run_pcm", &run_pcm, py::arg("handle"), py::arg("pcm"),
          py::arg("sample_rate"), py::arg("language"),
          "Transcribe PCM float32 array, return text.");

    // Streaming
    m.def("stream_init", &stream_init, py::arg("handle"),
          "Initialize streaming recognition state.");
    m.def("stream_feed", &stream_feed, py::arg("handle"), py::arg("pcm"),
          py::arg("sample_rate"),
          "Feed audio chunk for streaming recognition.");
    m.def("stream_result", &stream_result, py::arg("handle"),
          "Get current partial streaming result.");
    m.def("stream_reset", &stream_reset, py::arg("handle"),
          "Reset streaming state.");
}
