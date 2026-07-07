/**************************************************************************************************
 *
 * Copyright (c) 2019-2026 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Ningbo) Co., Ltd.
 *
 **************************************************************************************************/
#include "asr_server.hpp"
#include "utils/cmdline.hpp"
#include "utils/logger.h"

#include <stdio.h>

int main(int argc, char** argv) {
    cmdline::parser cmd;
    cmd.add<int>("port", 'p', "On which port to run the server", false, 8080);
#if defined(CHIP_AX650) || defined(CHIP_AX8850)
    cmd.add<std::string>("model_path", 'm', "model path which contains axmodel", false, "./models-ax650");
#else
    cmd.add<std::string>("model_path", 'm', "model path which contains axmodel", false, "./models-ax630c");
#endif
    cmd.add<std::string>("api_key", 'k', "Bearer token required by the server. Empty means auth disabled.", false, "");
    cmd.add<int>("payload_limit_mb", '\0', "Maximum accepted upload size in MiB", false, 25);
    cmd.add<int>("read_timeout_sec", '\0', "Socket read timeout in seconds", false, 120);
    cmd.add<int>("write_timeout_sec", '\0', "Socket write timeout in seconds", false, 120);
    cmd.parse_check(argc, argv);

    auto port = cmd.get<int>("port");
    ASRServerConfig config;
    config.model_path = cmd.get<std::string>("model_path");
    config.api_key = cmd.get<std::string>("api_key");
    config.payload_max_length = static_cast<size_t>(cmd.get<int>("payload_limit_mb")) * 1024 * 1024;
    config.read_timeout_sec = cmd.get<int>("read_timeout_sec");
    config.write_timeout_sec = cmd.get<int>("write_timeout_sec");

    ASRServer server;
    if (!server.init(config)) {
        ALOGE("Init server failed!");
        return -1;
    }

    if (!server.start(port)) {
        ALOGE("Start server failed!");
        return -1;
    }

    return 0;
}
