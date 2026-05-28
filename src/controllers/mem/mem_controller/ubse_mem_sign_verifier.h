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

#ifndef UBSE_MEM_SIGN_VERIFIER_H
#define UBSE_MEM_SIGN_VERIFIER_H

#include "ubse_common_def.h"
#include "ubse_http_common.h"
#include "ubse_mem_debt_info.h"

namespace ubse::mem::controller {
using common::def::UbseResult;
using ubse::adapter_plugins::mmi::UbseMemObmmInfo;

struct UbseExportSignReq {
    std::string reqSignedData; // 请求签名信息, 用于验签
    std::string type;          // 借用类型
    std::vector<UbseMemObmmInfo> exportObmmInfo;
    std::string trustRingId; // 信任环id
};

class UbseMemSignVerifier {
public:
    static UbseResult Sign(const std::string& type, std::string& signedData, std::string& trustRingId);

    static UbseResult SignAndVerify(const UbseExportSignReq& signReq, std::vector<std::string>& lendSignedDatas);
};

bool IsHighSafety();
} // namespace ubse::mem::controller
#endif // UBSE_MEM_SIGN_VERIFIER_H
