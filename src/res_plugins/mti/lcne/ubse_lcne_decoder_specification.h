/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_LCNE_DECODER_SPECIFICATION_H
#define UBSE_LCNE_DECODER_SPECIFICATION_H

#include "ubse_common_def.h"
#include "ubse_http_common.h"
#include "ubse_lcne_def.h"
#include "ubse_mem_mami_def.h"

namespace ubse::lcne {
using namespace common::def;
using namespace ubse::http;
using namespace mem::mami;

class UbseLcneDecoderSpecification {
public:
    static UbseLcneDecoderSpecification &GetInstance()
    {
        static UbseLcneDecoderSpecification instance("127.0.0.1", LcneServer::realPort);
        return instance;
    }

    /* 设置Decoder表规格 */
    UbseResult SetDecoderSpecification(const UbseMamiMemDecoderSetInfo &setInfo);

private:
    UbseLcneDecoderSpecification(std::string host, int port) : host(std::move(host)), port(port) {}

    std::string host;
    int port{};
    const std::string DECODER_SPECIFICATION_URL =
        "/restconf/operations/huawei-vbussw-service:ub-memory-decoder-specification";
};
}


#endif // UBSE_LCNE_DECODER_SPECIFICATION_H
