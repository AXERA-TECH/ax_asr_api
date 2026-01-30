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

#include <map>
#include <string>

#include "httplib.h"
#include "api/ax_asr_api.h"
#include "openai_err.hpp"

#define DEFAULT_PORT    8080
#define ASR_ENDPOINT    "/v1/audio/transcriptions"

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
    ~ASRServer() = default;

    bool init(const std::string& model_path);
    void start(int port = DEFAULT_PORT);
    void stop();

private:
    void setup_routes_();
    AX_ASR_HANDLE load_asr_(const std::string& model_name);
    // 设置CORS头
    void set_CORS_headers_(httplib::Response& res);
    
    /*
     * request: Content-Type: multipart/form-data
     * {
     *      "model": "sensevoice",
     *      "file": binary stream of audio file, supports wav and mp3.
     *      "language": For whisper, check https://whisper-api.com/docs/languages/
     *                  For sensevoice, support auto, zh, en, yue, ja, ko
     * }
    */
    bool check_request_(const httplib::Request& req, httplib::Response& res);

private:
    std::map<std::string, AX_ASR_HANDLE> handles_;
    httplib::Server srv_;
    std::string model_path_;
};