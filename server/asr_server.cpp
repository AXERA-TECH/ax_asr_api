/**************************************************************************************************
 *
 * Copyright (c) 2019-2026 Axera Semiconductor (Ningbo) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Ningbo) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Ningbo) Co., Ltd.
 *
 **************************************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>
 
#include "asr_server.hpp"
#include "utils/logger.h"
#include "openai_err.hpp"
#include "utils/nlohmann/json.hpp"

static std::map<std::string, AX_ASR_TYPE_E> MODEL_MAP = {
        {"whisper_tiny", AX_WHISPER_TINY},
        {"whisper_base", AX_WHISPER_BASE},
        {"whisper_small", AX_WHISPER_SMALL},
        {"whisper_turbo", AX_WHISPER_TURBO},
        {"sensevoice", AX_SENSEVOICE}
    };

namespace {

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
        if (it->ifa_addr == nullptr || it->ifa_name == nullptr) {
            continue;
        }
        if (it->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if ((it->ifa_flags & IFF_UP) == 0 || (it->ifa_flags & IFF_LOOPBACK) != 0) {
            continue;
        }

        char ip_buffer[INET_ADDRSTRLEN] = {0};
        const auto* addr = reinterpret_cast<const struct sockaddr_in*>(it->ifa_addr);
        if (inet_ntop(AF_INET, &addr->sin_addr, ip_buffer, sizeof(ip_buffer)) == nullptr) {
            continue;
        }

        std::string ip(ip_buffer);
        if (!is_valid_listen_ip(ip) || !seen_ips.insert(ip).second) {
            continue;
        }

        addresses.push_back({it->ifa_name, ip});
    }

    freeifaddrs(ifaddr);
    std::sort(addresses.begin(), addresses.end(), [](const ListenAddress& lhs, const ListenAddress& rhs) {
        if (lhs.ifname == rhs.ifname) {
            return lhs.ip < rhs.ip;
        }
        return lhs.ifname < rhs.ifname;
    });
    return addresses;
}

}  // namespace

bool ASRServer::init(const std::string& model_path) {
    model_path_ = model_path;

    this->setup_routes_();
    ALOGI("ASRServer init success");
    return true;
}

void ASRServer::start(int port) {
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
    this->srv_.listen("0.0.0.0", port);
}

void ASRServer::stop() {
    ALOGI("Terminate server.");
    this->srv_.stop();
}

// ================ PRIVATE ================
void ASRServer::setup_routes_() {
    this->srv_.Post(ASR_ENDPOINT, [this](const httplib::Request& req, httplib::Response& res) {
        // 1. 设置CORS头
        set_CORS_headers_(res);

        // 2. 检查参数
        if (!this->check_request_(req, res)) {
            ALOGE("Check request param failed!");
            return;
        }

        // 3. 获取参数
        std::string model = req.form.get_field("model");
        std::string language = req.form.get_field("language");
        const auto& file = req.form.get_file("file");

        // 4. 加载asr模型, 不会重复加载
        auto handle = this->load_asr_(model);

        // Save to disk
        std::string filename = std::filesystem::path(file.filename).filename().string();
        std::string tmppath = "/tmp/" + filename;
        std::ofstream ofs(tmppath, std::ios::binary);
        ofs << file.content;

        // 5. 运行模型
        char* text = NULL;
        int ret = AX_ASR_RunFile(handle, tmppath.c_str(), language.c_str(), &text);
        if (ret != 0) {
            ALOGE("AX_ASR_RunFile failed! ret=0x%08x", ret);
            ErrorResponse openai_res(OPENAI_ERR_INTERNAL_SERVER_ERROR, "AX_ASR_RunFile failed! Please contact us.", "");
            openai_res.to_res(res);
            free(text);
            return;            
        }

        nlohmann::json response;
        response["text"] = std::string(text);
        free(text);

        res.status = 200;
        res.set_content(response.dump(), "application/json");
        return;
    });
}

AX_ASR_HANDLE ASRServer::load_asr_(const std::string& model_name) {
    if (this->handles_.find(model_name) != this->handles_.end()) {
        return this->handles_.at(model_name);
    } else {
        // try to new one
        if (MODEL_MAP.find(model_name) == MODEL_MAP.end()) {
            ALOGE("Cannot find model of %s", model_name.c_str());
            return nullptr;
        }

        AX_ASR_TYPE_E asr_type = MODEL_MAP.at(model_name);
        ALOGI("Initializing %s ...", model_name.c_str());
        AX_ASR_HANDLE new_handle = AX_ASR_Init(asr_type, this->model_path_.c_str());
        if (new_handle == nullptr) {
            ALOGE("Init asr %s failed!", model_name.c_str());
            return nullptr;
        }
        this->handles_.insert({model_name, new_handle});
        return new_handle;
    }
}

void ASRServer::set_CORS_headers_(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", 
                    "Content-Type, X-Array-Name, X-Array-Description, X-Array-Size");
}

bool ASRServer::check_request_(const httplib::Request& req, httplib::Response& res) {
    // 1. 检查Content-Type
    if (!req.has_header("Content-Type") ||
        req.get_header_value("Content-Type").find("multipart/form-data") == std::string::npos) {
            ALOGE("Content-Type must be multipart/form-data.");
            ErrorResponse openai_res(OPENAI_ERR_BAD_REQUEST, "Content-Type must be multipart/form-data.", "Content-Type");
            openai_res.to_res(res);
            return false;
    }

    // 2. 检查model
    {
        if (!req.form.has_field("model")) {
            ALOGE("\"model\" field must be provided.");
            ErrorResponse openai_res(OPENAI_ERR_BAD_REQUEST, "\"model\" field must be provided.", "model");
            openai_res.to_res(res);
            return false;
        }

        std::string model = req.form.get_field("model");
        if (MODEL_MAP.find(model) == MODEL_MAP.end()) {
            ALOGE("%s not found in server.", model.c_str());
            ErrorResponse openai_res(OPENAI_ERR_NOT_FOUND, model + "not found in server.", "model");
            openai_res.to_res(res);
            return false;
        }

        // 获取模型
        auto handle = this->load_asr_(model);
        if (!handle) {
            ALOGE("Load asr failed!");
            ErrorResponse openai_res(OPENAI_ERR_NOT_FOUND, "Load asr failed.", "model");
            openai_res.to_res(res);
            return false;
        }
    }
    
    // 3. 检查language
    {
        if (!req.form.has_field("language")) {
            ErrorResponse openai_res(OPENAI_ERR_BAD_REQUEST, "\"language\" field must be provided.", "language");
            openai_res.to_res(res);
            return false;
        }
    }

    // 4. 检查file
    {
        if (!req.form.has_file("file")) {
            ErrorResponse openai_res(OPENAI_ERR_BAD_REQUEST, "\"file\" field must be provided.", "file");
            openai_res.to_res(res);
            return false;
        }
    }

    return true;
}
