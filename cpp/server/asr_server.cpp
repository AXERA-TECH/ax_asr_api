/**************************************************************************************************
 *
 * Copyright (c) 2019-2026 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Ningbo) Co., Ltd.
 *
 **************************************************************************************************/
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <vector>
#include <net/if.h>

#include <fcntl.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "asr_server.hpp"
#include "utils/logger.h"
#include "utils/nlohmann/json.hpp"

namespace {

using json = nlohmann::json;

constexpr const char* HEALTH_ENDPOINT = "/healthz";
constexpr const char* MODELS_ENDPOINT = "/v1/models";

const std::map<std::string, AX_ASR_TYPE_E> kModelMap = {
    {"whisper_tiny", AX_WHISPER_TINY},
    {"whisper_base", AX_WHISPER_BASE},
    {"whisper_small", AX_WHISPER_SMALL},
    {"whisper_turbo", AX_WHISPER_TURBO},
    {"sensevoice", AX_SENSEVOICE},
};

const std::map<std::string, std::string> kModelAliasMap = {
    {"gpt-4o-transcribe", "sensevoice"},
    {"gpt-4o-mini-transcribe", "sensevoice"},
    {"whisper-1", "whisper_turbo"},
};

const std::vector<std::string> kSenseVoiceLanguages = {
    "auto", "zh", "en", "yue", "ja", "ko",
};

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string trim_copy(const std::string& value) {
    const auto first = std::find_if_not(value.begin(), value.end(),
                                        [](unsigned char c) { return std::isspace(c) != 0; });
    const auto last = std::find_if_not(value.rbegin(), value.rend(),
                                       [](unsigned char c) { return std::isspace(c) != 0; }).base();
    if (first >= last) {
        return "";
    }
    return std::string(first, last);
}

std::vector<std::string> split_csv(const std::string& csv) {
    std::vector<std::string> values;
    std::stringstream ss(csv);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trim_copy(item);
        if (!item.empty()) {
            values.push_back(item);
        }
    }
    return values;
}

std::string whisper_variant_from_model(const std::string& canonical_model_name) {
    static const std::string prefix = "whisper_";
    if (canonical_model_name.rfind(prefix, 0) != 0) {
        return "";
    }
    return canonical_model_name.substr(prefix.size());
}

std::string file_extension_lower(const std::string& filename) {
    auto ext = std::filesystem::path(filename).extension().string();
    return to_lower_copy(ext);
}

bool is_supported_audio_extension(const std::string& extension) {
    return extension == ".wav" || extension == ".mp3";
}

class TempAudioFile {
public:
    TempAudioFile(const httplib::FormData& file, const std::string& request_id) {
        auto extension = file_extension_lower(file.filename);
        if (!is_supported_audio_extension(extension)) {
            return;
        }

        std::string template_path =
            (std::filesystem::temp_directory_path() /
             ("ax_asr_" + request_id + "_XXXXXX" + extension))
                .string();
        std::vector<char> buffer(template_path.begin(), template_path.end());
        buffer.push_back('\0');

        const int fd = mkstemps(buffer.data(), static_cast<int>(extension.size()));
        if (fd < 0) {
            return;
        }

        path_ = buffer.data();
        const auto* data = file.content.data();
        size_t remaining = file.content.size();
        while (remaining > 0) {
            const ssize_t written = write(fd, data, remaining);
            if (written < 0) {
                close(fd);
                std::remove(path_.c_str());
                path_.clear();
                return;
            }

            data += written;
            remaining -= static_cast<size_t>(written);
        }

        close(fd);
        valid_ = true;
    }

    ~TempAudioFile() {
        if (!path_.empty()) {
            std::remove(path_.c_str());
        }
    }

    bool valid() const { return valid_; }
    const std::string& path() const { return path_; }

private:
    std::string path_;
    bool valid_ = false;
};

bool has_bearer_prefix(const std::string& value) {
    static const std::string prefix = "Bearer ";
    return value.rfind(prefix, 0) == 0;
}


struct ListenAddress {
    std::string ifname;
    std::string ip;
};

bool is_valid_listen_ip(const std::string& ip) {
    return !ip.empty() && ip != "0.0.0.0" && ip.rfind("127.", 0) != 0 && ip.rfind("169.254.", 0) != 0;
}

std::string format_listen_url(const std::string& ip, int port) {
    return "http://" + ip + ":" + std::to_string(port) + ASR_ENDPOINT;
}

std::vector<ListenAddress> enumerate_listen_ipv4_addresses() {
    std::vector<ListenAddress> addresses;
    std::set<std::string> seen_ips;
    struct ifaddrs* ifaddr = nullptr;

    if (getifaddrs(&ifaddr) != 0) {
        ALOGW("getifaddrs failed, cannot enumerate listen addresses");
        return addresses;
    }

    for (struct ifaddrs* it = ifaddr; it != nullptr; it = it->ifa_next) {
        if (it->ifa_addr == nullptr || it->ifa_name == nullptr)
            continue;
        if (it->ifa_addr->sa_family != AF_INET)
            continue;
        if ((it->ifa_flags & IFF_UP) == 0 || (it->ifa_flags & IFF_LOOPBACK) != 0)
            continue;

        char ip_buffer[INET_ADDRSTRLEN] = {0};
        const auto* addr = reinterpret_cast<const struct sockaddr_in*>(it->ifa_addr);
        if (inet_ntop(AF_INET, &addr->sin_addr, ip_buffer, sizeof(ip_buffer)) == nullptr)
            continue;

        std::string ip(ip_buffer);
        if (!is_valid_listen_ip(ip) || !seen_ips.insert(ip).second)
            continue;

        addresses.push_back({it->ifa_name, ip});
    }

    freeifaddrs(ifaddr);
    std::sort(addresses.begin(), addresses.end(),
              [](const ListenAddress& lhs, const ListenAddress& rhs) {
                  if (lhs.ifname == rhs.ifname)
                      return lhs.ip < rhs.ip;
                  return lhs.ifname < rhs.ifname;
              });
    return addresses;
}
}  // namespace

ASRServer::~ASRServer() {
    stop();
    std::lock_guard<std::mutex> lock(handles_mutex_);
    handles_.clear();
}

bool ASRServer::init(const ASRServerConfig& config) {
    if (config.model_path.empty()) {
        ALOGE("model_path must not be empty");
        return false;
    }

    config_ = config;
    srv_.set_payload_max_length(config_.payload_max_length);
    srv_.set_read_timeout(config_.read_timeout_sec);
    srv_.set_write_timeout(config_.write_timeout_sec);

    setup_routes_();
    ALOGI("ASRServer init success. model_path=%s payload_max_length=%zu auth=%s",
          config_.model_path.c_str(), config_.payload_max_length,
          config_.api_key.empty() ? "disabled" : "enabled");
    return true;
}

bool ASRServer::start(int port) {
    const auto addresses = enumerate_listen_ipv4_addresses();
    if (addresses.empty()) {
        ALOGW("No non-loopback IPv4 address found, server will listen on 0.0.0.0:%d", port);
    } else {
        for (const auto& address : addresses) {
            ALOGI("Available URL (%s): %s", address.ifname.c_str(),
                  format_listen_url(address.ip, port).c_str());
        }
    }

    ALOGI("Listening on 0.0.0.0:%d", port);
    const bool ok = srv_.listen("0.0.0.0", port);
    if (!ok) {
        ALOGE("Listen on port %d failed", port);
    }
    return ok;
}

void ASRServer::stop() {
    srv_.stop();
}

void ASRServer::setup_routes_() {
    srv_.set_error_handler([this](const httplib::Request& req, httplib::Response& res) {
        if (!res.body.empty()) {
            return;
        }

        set_CORS_headers_(req, res);
        if (res.status == OPENAI_ERR_PAYLOAD_TOO_LARGE) {
            ErrorResponse(OPENAI_ERR_PAYLOAD_TOO_LARGE,
                          "Uploaded audio exceeds the configured payload limit.",
                          "file")
                .to_res(res);
            return;
        }

        if (res.status == 404) {
            ErrorResponse(OPENAI_ERR_NOT_FOUND,
                          "Requested resource does not exist.",
                          "")
                .to_res(res);
        }
    });

    srv_.set_exception_handler([this](const httplib::Request& req, httplib::Response& res,
                                      std::exception_ptr ep) {
        set_CORS_headers_(req, res);
        try {
            if (ep) {
                std::rethrow_exception(ep);
            }
        } catch (const std::exception& ex) {
            ALOGE("Unhandled exception: %s", ex.what());
        } catch (...) {
            ALOGE("Unhandled unknown exception");
        }

        ErrorResponse(OPENAI_ERR_INTERNAL_SERVER_ERROR,
                      "Unhandled server exception.",
                      "")
            .to_res(res);
    });

    srv_.Options(ASR_ENDPOINT, [this](const httplib::Request& req, httplib::Response& res) {
        set_CORS_headers_(req, res);
        res.status = 204;
    });

    srv_.Get(HEALTH_ENDPOINT, [this](const httplib::Request& req, httplib::Response& res) {
        set_CORS_headers_(req, res);
        json payload = {
            {"status", "ok"},
            {"auth_enabled", !config_.api_key.empty()},
        };
        res.status = 200;
        res.set_content(payload.dump(), "application/json");
    });

    srv_.Get(MODELS_ENDPOINT, [this](const httplib::Request& req, httplib::Response& res) {
        set_CORS_headers_(req, res);
        if (!validate_auth_(req, res)) {
            return;
        }

        json models = json::array();
        for (const auto& entry : kModelMap) {
            models.push_back({
                {"id", entry.first},
                {"object", "model"},
                {"owned_by", "axera"},
            });
        }

        for (const auto& entry : kModelAliasMap) {
            models.push_back({
                {"id", entry.first},
                {"object", "model"},
                {"owned_by", "axera"},
                {"root", entry.second},
            });
        }

        res.status = 200;
        res.set_content(json{{"object", "list"}, {"data", models}}.dump(),
                        "application/json");
    });

    srv_.Post(ASR_ENDPOINT, [this](const httplib::Request& req, httplib::Response& res) {
        set_CORS_headers_(req, res);

        if (!validate_auth_(req, res)) {
            return;
        }

        std::string model_name;
        std::string language;
        std::string response_format;
        if (!check_request_(req, model_name, language, response_format, res)) {
            return;
        }

        auto handle = load_asr_(model_name);
        if (!handle || !handle->handle) {
            ErrorResponse(OPENAI_ERR_INTERNAL_SERVER_ERROR,
                          "Model initialization failed on server side.",
                          "model")
                .to_res(res);
            return;
        }

        const auto& file = req.form.get_file("file");
        const std::string request_id = create_request_id_();
        res.set_header("x-request-id", request_id);

        TempAudioFile temp_file(file, request_id);
        if (!temp_file.valid()) {
            ErrorResponse(OPENAI_ERR_INTERNAL_SERVER_ERROR,
                          "Failed to persist uploaded audio.",
                          "file")
                .to_res(res);
            return;
        }

        char* text = nullptr;
        int ret = AX_ASR_SUCCESS;
        {
            std::lock_guard<std::mutex> guard(handle->mutex);
            ret = AX_ASR_RunFile(handle->handle, temp_file.path().c_str(),
                                 language.c_str(), &text);
        }

        if (ret != AX_ASR_SUCCESS) {
            ALOGE("AX_ASR_RunFile failed! ret=%d request_id=%s", ret, request_id.c_str());
            ErrorResponse(OPENAI_ERR_INTERNAL_SERVER_ERROR,
                          "Transcription failed on server side.",
                          "file")
                .to_res(res);
            AX_ASR_Free(text);
            return;
        }

        std::string transcription = text ? text : "";
        AX_ASR_Free(text);

        if (response_format == "text") {
            res.status = 200;
            res.set_content(transcription, "text/plain; charset=utf-8");
            return;
        }

        json payload = {{"text", transcription}};
        if (response_format == "verbose_json") {
            payload["model"] = model_name;
            payload["language"] = language;
            payload["request_id"] = request_id;
        }

        res.status = 200;
        res.set_content(payload.dump(), "application/json");
    });
}

std::shared_ptr<ASRServer::ModelInstance> ASRServer::load_asr_(const std::string& canonical_model_name) {
    std::lock_guard<std::mutex> lock(handles_mutex_);

    auto it = handles_.find(canonical_model_name);
    if (it != handles_.end()) {
        return it->second;
    }

    auto model_it = kModelMap.find(canonical_model_name);
    if (model_it == kModelMap.end()) {
        ALOGE("Unknown model %s", canonical_model_name.c_str());
        return nullptr;
    }

    ALOGI("Initializing %s ...", canonical_model_name.c_str());
    AX_ASR_HANDLE handle = AX_ASR_Init(model_it->second, config_.model_path.c_str());
    if (!handle) {
        ALOGE("Init asr %s failed!", canonical_model_name.c_str());
        return nullptr;
    }

    auto instance = std::make_shared<ModelInstance>(handle);
    handles_.emplace(canonical_model_name, instance);
    return instance;
}

std::string ASRServer::canonical_model_name_(const std::string& requested_model_name) const {
    const auto key = to_lower_copy(trim_copy(requested_model_name));

    auto model_it = kModelMap.find(key);
    if (model_it != kModelMap.end()) {
        return model_it->first;
    }

    auto alias_it = kModelAliasMap.find(key);
    if (alias_it != kModelAliasMap.end()) {
        return alias_it->second;
    }

    return "";
}

std::string ASRServer::default_language_for_model_(const std::string& canonical_model_name) const {
    return canonical_model_name == "sensevoice" ? "auto" : "en";
}

bool ASRServer::is_supported_audio_file_(const httplib::FormData& file) const {
    if (file.filename.empty() || file.content.empty()) {
        return false;
    }

    return is_supported_audio_extension(file_extension_lower(file.filename));
}

bool ASRServer::validate_auth_(const httplib::Request& req, httplib::Response& res) const {
    if (config_.api_key.empty()) {
        return true;
    }

    if (!req.has_header("Authorization")) {
        ErrorResponse(OPENAI_ERR_UNAUTHORIZED,
                      "Missing Authorization header.",
                      "Authorization")
            .to_res(res);
        return false;
    }

    const std::string auth_value = req.get_header_value("Authorization");
    if (!has_bearer_prefix(auth_value) ||
        auth_value.substr(std::string("Bearer ").size()) != config_.api_key) {
        ErrorResponse(OPENAI_ERR_UNAUTHORIZED,
                      "Invalid bearer token.",
                      "Authorization")
            .to_res(res);
        return false;
    }

    return true;
}

bool ASRServer::validate_language_(const std::string& canonical_model_name,
                                   const std::string& requested_language,
                                   std::string& resolved_language,
                                   httplib::Response& res) const {
    resolved_language = requested_language.empty()
                            ? default_language_for_model_(canonical_model_name)
                            : to_lower_copy(trim_copy(requested_language));

    std::vector<std::string> supported_languages;
    if (!supported_languages_for_model_(canonical_model_name, supported_languages)) {
        ErrorResponse(OPENAI_ERR_INTERNAL_SERVER_ERROR,
                      "Failed to load supported language list.",
                      "language")
            .to_res(res);
        return false;
    }

    const auto it = std::find(supported_languages.begin(), supported_languages.end(),
                              resolved_language);
    if (it == supported_languages.end()) {
        std::ostringstream oss;
        oss << "Unsupported language \"" << resolved_language
            << "\" for model " << canonical_model_name << ".";
        ErrorResponse(OPENAI_ERR_BAD_REQUEST, oss.str(), "language").to_res(res);
        return false;
    }

    return true;
}

bool ASRServer::supported_languages_for_model_(const std::string& canonical_model_name,
                                               std::vector<std::string>& languages) const {
    std::lock_guard<std::mutex> lock(handles_mutex_);

    auto cache_it = language_cache_.find(canonical_model_name);
    if (cache_it != language_cache_.end()) {
        languages = cache_it->second;
        return true;
    }

    if (canonical_model_name == "sensevoice") {
        languages = kSenseVoiceLanguages;
        language_cache_.emplace(canonical_model_name, languages);
        return true;
    }

    const std::string variant = whisper_variant_from_model(canonical_model_name);
    if (variant.empty()) {
        return false;
    }

    const auto config_path = std::filesystem::path(config_.model_path) / "whisper" /
                             variant / (variant + "_config.json");
    std::ifstream fs(config_path);
    if (!fs.is_open()) {
        ALOGE("Cannot open whisper config: %s", config_path.c_str());
        return false;
    }

    json config = json::parse(fs, nullptr, false);
    if (config.is_discarded() || !config.contains("all_language_codes")) {
        ALOGE("Invalid whisper config: %s", config_path.c_str());
        return false;
    }

    languages = split_csv(config["all_language_codes"].get<std::string>());
    language_cache_.emplace(canonical_model_name, languages);
    return true;
}

bool ASRServer::parse_response_format_(const httplib::Request& req,
                                       std::string& response_format,
                                       httplib::Response& res) const {
    response_format = "json";
    if (!req.form.has_field("response_format")) {
        return true;
    }

    response_format = to_lower_copy(trim_copy(req.form.get_field("response_format")));
    if (response_format == "json" || response_format == "text" ||
        response_format == "verbose_json") {
        return true;
    }

    ErrorResponse(OPENAI_ERR_BAD_REQUEST,
                  "response_format must be one of json, text, verbose_json.",
                  "response_format")
        .to_res(res);
    return false;
}

std::string ASRServer::create_request_id_() {
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    const auto count = request_counter_.fetch_add(1, std::memory_order_relaxed);
    return "axasr-" + std::to_string(now_ms) + "-" + std::to_string(count);
}

void ASRServer::set_CORS_headers_(const httplib::Request& req, httplib::Response& res) const {
    res.headers.erase("Access-Control-Allow-Origin");
    res.headers.erase("Vary");
    res.headers.erase("Access-Control-Allow-Methods");
    res.headers.erase("Access-Control-Allow-Headers");
    res.headers.erase("Access-Control-Expose-Headers");

    if (config_.allowed_origin == "*" || !req.has_header("Origin")) {
        res.set_header("Access-Control-Allow-Origin", config_.allowed_origin);
    } else {
        res.set_header("Access-Control-Allow-Origin", config_.allowed_origin);
        res.set_header("Vary", "Origin");
    }

    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Authorization, Content-Type");
    res.set_header("Access-Control-Expose-Headers", "x-request-id");
}

bool ASRServer::check_request_(const httplib::Request& req,
                               std::string& canonical_model_name,
                               std::string& language,
                               std::string& response_format,
                               httplib::Response& res) {
    if (!req.has_header("Content-Type") ||
        req.get_header_value("Content-Type").find("multipart/form-data") == std::string::npos) {
        ErrorResponse(OPENAI_ERR_BAD_REQUEST,
                      "Content-Type must be multipart/form-data.",
                      "Content-Type")
            .to_res(res);
        return false;
    }

    if (!req.form.has_field("model")) {
        ErrorResponse(OPENAI_ERR_BAD_REQUEST,
                      "\"model\" field must be provided.",
                      "model")
            .to_res(res);
        return false;
    }

    canonical_model_name = canonical_model_name_(req.form.get_field("model"));
    if (canonical_model_name.empty()) {
        ErrorResponse(OPENAI_ERR_NOT_FOUND,
                      "Requested model is not available on this server.",
                      "model")
            .to_res(res);
        return false;
    }

    if (!req.form.has_file("file")) {
        ErrorResponse(OPENAI_ERR_BAD_REQUEST,
                      "\"file\" field must be provided.",
                      "file")
            .to_res(res);
        return false;
    }

    const auto& file = req.form.get_file("file");
    if (!is_supported_audio_file_(file)) {
        ErrorResponse(OPENAI_ERR_BAD_REQUEST,
                      "Only non-empty .wav and .mp3 uploads are supported.",
                      "file")
            .to_res(res);
        return false;
    }

    const std::string requested_language =
        req.form.has_field("language") ? req.form.get_field("language") : "";
    if (!validate_language_(canonical_model_name, requested_language, language, res)) {
        return false;
    }

    if (!parse_response_format_(req, response_format, res)) {
        return false;
    }

    return true;
}
