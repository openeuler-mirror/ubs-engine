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

#ifndef UBSE_LCNE_DECODER_ENTRY_H
#define UBSE_LCNE_DECODER_ENTRY_H

#include "ubse_common_def.h"
#include "ubse_http_common.h"
#include "ubse_mmi_def.h"
#include "adapter_plugins/mti/ubse_mti_mami_def.h"
#include "src/include/adapter_plugins/mti/ubse_mti_def.h"

namespace ubse::lcne {
using namespace common::def;
using namespace ubse::http;
using namespace adapter_plugins::mti::mami;
using namespace adapter_plugins::mmi;

class UbseLcneDecoderEntry {
public:
    /* 增加Decoder表项 */
    static UbseResult AddDecoderEntry(const UbseMamiMemImportInfo& importInfo, UbseMamiMemImportResult& importResult,
                                      const adapter_plugins::mti::UbseDecoderTrustRingData& trustRingData = {});

    /* 删除Decoder表项 */
    static UbseResult DeleteDecoderEntry(const UbseMamiMemWithdraw& drawInfo);

    /* 无效Decoder表项 */
    static UbseResult InvalidateDecoderEntry(const UbseMamiMemWithdraw& drawInfo);
};
} // namespace ubse::lcne

#endif // UBSE_LCNE_DECODER_ENTRY_H
