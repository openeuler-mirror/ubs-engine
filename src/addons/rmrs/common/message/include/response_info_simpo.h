/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef RESPONSE_INFO_SIMPO_H
#define RESPONSE_INFO_SIMPO_H

#include "common_delete_func.h"
#include "rmrs_serialize.h"
namespace mempooling {
using rmrs::serialize::RmrsInStream;
using rmrs::serialize::RmrsOutStream;

struct ResponseInfo {
    unsigned int code;
    std::string message;
};

class ResponseInfoSimpo {
public:
    ResponseInfoSimpo() = default;
    explicit ResponseInfoSimpo(ResponseInfo responseInfoInput) : responseInfo_(std::move(responseInfoInput)) {}

    inline ResponseInfo GetResponseInfo()
    {
        return responseInfo_;
    }

    inline void SetResponseInfo(const int code, const std::string &message)
    {
        responseInfo_.code = code;
        responseInfo_.message = message;
    }

    std::string ToString() const
    {
        return "code=" + std::to_string(responseInfo_.code) + ", message=" + responseInfo_.message;
    };
    
    ResponseInfo responseInfo_{};
};
} // namespace mempooling

#endif // RESPONSE_INFO_SIMPO_H
