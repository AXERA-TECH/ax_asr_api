/**************************************************************************************************
 *
 * Copyright (c) 2019-2026 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Ningbo) Co., Ltd.
 *
 **************************************************************************************************/
#pragma once

#include <atomic>
#include <memory>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "httplib.h"
#include "api/ax_asr_api.h"
#include "openai_err.hpp"

#define DEFAULT_PORT    8080
#define ASR_ENDPOINT    "/v1/audio/transcriptions"

struct ASRServerConfig {
    std::string model_path;
    std::string api_key;
    std::string allowed_origin = "*";
    size_t payload_max_length = 25 * 1024 * 1024;
    int read_timeout_sec = 120;
    int write_timeout_sec = 120;
};

/* OpenAI-Compatible server
   Following API docs from: https://platform.openai.com/docs/api-reference/audio/createTranscription
   post http://IP:PORT/v1/audio/transcriptions

 * Client usage:
 * - cURL:
 *      curl http://IP:PORT/v1/audio/transcriptions \
        -H "Authorization: Bearer $OPENAI_API_KEY" \
        -H "Content-Type: multipart/form-data" \
        -F file="@/path/to/file/audio.mp3" \
        -F model="gpt-4o-transcribe"
 *
 * - Python:
 *  from openai import OpenAI
    client = OpenAI(
        base_url='http://IP:PORT/v1',
        api_key="dummy_key"
    )

    audio_file = open("speech.mp3", "rb")
    transcript = client.audio.transcriptions.create(
        model="sensevoice",
        file=audio_file
    )

    audio_file.close()
    print(transcript.text)

 * - JavaScript:
    import fs from "fs";
    import OpenAI from "openai";

    const openai = new OpenAI(
        apiKey: "YOUR_API_KEY", // Defaults to process.env.OPENAI_API_KEY
        baseURL: "http://IP:PORT/v1", // The custom URL
    );

    async function main() {
    const transcription = await openai.audio.transcriptions.create({
        file: fs.createReadStream("audio.mp3"),
        model: "sensevoice",
    });

    console.log(transcription.text);
    }
    main();

 * Response:
 * Error: check https://platform.openai.com/docs/guides/error-codes
 *  {
        "error": {
            "message": "You exceeded your current quota, please check your plan and billing details.",
            "type": "insufficient_quota",
            "param": null,
            "code": 402
        }
    }

* Success:
    {
        "text": "blahblahblah"
    }
*/
class ASRServer {
public:
    ASRServer() = default;
    ~ASRServer();

    bool init(const ASRServerConfig& config);
    bool start(int port = DEFAULT_PORT);
    void stop();

private:
    struct ModelInstance {
        explicit ModelInstance(AX_ASR_HANDLE h): handle(h) {}
        ~ModelInstance() {
            if (handle) {
                AX_ASR_Uninit(handle);
            }
        }

        AX_ASR_HANDLE handle = nullptr;
        std::mutex mutex;
    };

    void setup_routes_();
    std::shared_ptr<ModelInstance> load_asr_(const std::string& canonical_model_name);
    std::string canonical_model_name_(const std::string& requested_model_name) const;
    std::string default_language_for_model_(const std::string& canonical_model_name) const;
    bool is_supported_audio_file_(const httplib::FormData& file) const;
    bool validate_auth_(const httplib::Request& req, httplib::Response& res) const;
    bool validate_language_(const std::string& canonical_model_name,
                            const std::string& requested_language,
                            std::string& resolved_language,
                            httplib::Response& res) const;
    bool supported_languages_for_model_(const std::string& canonical_model_name,
                                        std::vector<std::string>& languages) const;
    bool parse_response_format_(const httplib::Request& req,
                                std::string& response_format,
                                httplib::Response& res) const;
    std::string create_request_id_();
    void set_CORS_headers_(const httplib::Request& req, httplib::Response& res) const;
    
    /*
     * request: Content-Type: multipart/form-data
     * {
     *      "model": "sensevoice",
      *      "file": binary stream of audio file, supports wav and mp3.
     *      "language": optional. For whisper, defaults to en. For sensevoice, defaults to auto.
     *                  For whisper, check https://whisper-api.com/docs/languages/
     *                  For sensevoice, support auto, zh, en, yue, ja, ko
     * }
    */
    bool check_request_(const httplib::Request& req,
                        std::string& canonical_model_name,
                        std::string& language,
                        std::string& response_format,
                        httplib::Response& res);

private:
    std::map<std::string, std::shared_ptr<ModelInstance>> handles_;
    mutable std::mutex handles_mutex_;
    mutable std::map<std::string, std::vector<std::string>> language_cache_;
    httplib::Server srv_;
    ASRServerConfig config_;
    std::atomic<uint64_t> request_counter_{0};
};
