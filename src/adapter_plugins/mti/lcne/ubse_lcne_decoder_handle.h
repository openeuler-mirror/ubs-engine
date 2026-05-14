/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
  *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_LCNE_DECODER_HANDLE_H
#define UBSE_LCNE_DECODER_HANDLE_H

#include "ubse_common_def.h"
#include "ubse_http_common.h"
#include "adapter_plugins/mti/ubse_mti_mami_def.h"

namespace ubse::lcne {
using namespace common::def;
using namespace ubse::http;
using namespace adapter_plugins::mti::mami;

class UbseLcneDecoderHandle {
public:
    static UbseLcneDecoderHandle& GetInstance()
    {
        static UbseLcneDecoderHandle instance;
        return instance;
    }

    /* 查询全部handle */
    UbseResult GetAllMemHandles(const UbseMamiMemHandleQueryInfo& queryInfo,
                                std::vector<UbseMamiMemHandleValue>& handleValues) const;

private:
    UbseLcneDecoderHandle() = default;
};
} // namespace ubse::lcne
#endif // UBSE_LCNE_DECODER_HANDLE_H
