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

#ifndef UBSE_LCNE_DECODER_ENTRY_H
#define UBSE_LCNE_DECODER_ENTRY_H

#include "ubse_common_def.h"
#include "ubse_http_common.h"
#include "ubse_lcne_def.h"
#include "ubse_mem_mami_def.h"

namespace ubse::lcne {
using namespace common::def;
using namespace ubse::http;
using namespace mem::mami;

class UbseLcneDecoderEntry {
public:
    static UbseLcneDecoderEntry &GetInstance()
    {
        static UbseLcneDecoderEntry instance("127.0.0.1", LcneServer::realPort);
        return instance;
    }

    /* 增加Decoder表项 */
    UbseResult AddDecoderEntry(const UbseMamiMemImportInfo &importInfo, UbseMamiMemImportResult &importResult);

    /* 删除Decoder表项 */
    UbseResult DeleteDecoderEntry(const UbseMamiMemWithdraw &drawInfo);

private:
    UbseLcneDecoderEntry(std::string host, int port) : host(std::move(host)), port(port) {}

    UbseResult SendRequest(const std::string &body, UbseHttpResponse &rsp, const std::string &operation);

    std::string host;
    int port{};
    const std::string ADD_DECODER_ENTRY_URL = "/restconf/operations/huawei-vbussw-service:ub-memory-decoder";
    const std::string DELETE_DECODER_ENTRY_URL = "/restconf/operations/huawei-vbussw-service:ub-memory-decoder-delete";
};
}

#endif // UBSE_LCNE_DECODER_ENTRY_H
