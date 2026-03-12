/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef RESPONSE_INFO_MESSAGE_H
#define RESPONSE_INFO_MESSAGE_H

#include "base_message.h"

namespace vm {
/**
 * @brief Request response information
 *
 */
struct ResponseInfo {
    unsigned int code{};
    std::string message{};
};

class ResponseInfoMessage : public BaseMessage {
public:
    ResponseInfoMessage() = default;
    explicit ResponseInfoMessage(ResponseInfo responseInfoInput) : responseInfo(std::move(responseInfoInput)) {}
    explicit ResponseInfoMessage(uint8_t *rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    VmResult Serialize() override;

    VmResult Deserialize() override;

    inline ResponseInfo GetResponseInfo() const
    {
        return responseInfo;
    }

    inline void SetResponseInfo(unsigned int code, const std::string &message)
    {
        responseInfo.code = code;
        responseInfo.message = message;
    }

    inline std::string ToString() const override
    {
        return "code: " + std::to_string(responseInfo.code) + "; message: " + responseInfo.message;
    };

private:
    ResponseInfo responseInfo{};
};
}

#endif // RESPONSE_INFO_MESSAGE_H
