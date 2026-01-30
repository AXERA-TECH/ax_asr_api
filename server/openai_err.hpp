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
#include "utils/nlohmann/json.hpp"

enum OPENAI_ERR_CODE {
    OPENAI_ERR_BAD_REQUEST = 400,
    OPENAI_ERR_NOT_FOUND = 404,
    OPENAI_ERR_INTERNAL_SERVER_ERROR = 500
};

static std::map<OPENAI_ERR_CODE, std::string> OPENAI_ERR_TYPE_MAP = {
    {OPENAI_ERR_BAD_REQUEST, "Bad request! Your request was malformed or missing some required parameters."},
    {OPENAI_ERR_NOT_FOUND, "Requested resource does not exist."},
    {OPENAI_ERR_INTERNAL_SERVER_ERROR, "Issue on our side, please check log on server side and contact us."}
};

class ErrorResponse {
private:
    OPENAI_ERR_CODE code_;
    std::string type_;
    std::string message_;
    std::string param_;
    nlohmann::json content_;

public:
    explicit ErrorResponse(OPENAI_ERR_CODE code, const std::string& message, const std::string& param):
        code_(code),
        type_(OPENAI_ERR_TYPE_MAP.at(code)),
        message_(message),
        param_(param) {
        
        content_ = nlohmann::json::parse(R"({
                "error": {
                    "message": "",
                    "type": "",
                    "param": "",
                    "code": 0
                }
            })");
        content_["error"]["message"] = message;
        content_["error"]["type"] = type_;
        content_["error"]["param"] = param_;
        content_["error"]["code"] = code_;
    }

    ErrorResponse& to_res(httplib::Response& res) {
        res.status = code_;
        res.set_content(content_.dump(), "application/json");
        return *this;
    }
};