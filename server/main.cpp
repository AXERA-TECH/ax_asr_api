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
    cmd.parse_check(argc, argv);

    // 0. get app args, can be removed from user's app
    auto port = cmd.get<int>("port");
    auto model_path = cmd.get<std::string>("model_path");

    ASRServer server;
    if (!server.init(model_path)) {
        ALOGE("Init server failed!");
        return -1;
    }

    server.start(port);

    return 0;
}