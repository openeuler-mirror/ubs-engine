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

#include "ubse_mem_cabinet_ledger_agg.h"

#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "message/ubse_mem_cabinet_ledger_serial.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::mem::serial;
using namespace ubse::serial;

UbseResult CabinetLedgerQueryHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UbseCabinetLedgerQueryResp result{};
    result.ret = UBSE_ERROR;

    UbseCabinetLedgerQueryReq request{};
    const bool reqValid = (req.data != nullptr && req.len > 0);
    if (!reqValid) {
        UBSE_LOG_ERROR << "[CLOS_CAB] action=agg_query result=empty_request";
    } else {
        UbseDeSerialization in(req.data, static_cast<uint32_t>(req.len));
        if (!UbseCabinetLedgerQueryReqDeserialization(in, request)) {
            UBSE_LOG_ERROR << "[CLOS_CAB] action=agg_query result=deserialize_failed";
        } else {
            // 正式实现流程：柜内串行采集 -> 三态打标 -> 按柜过滤 -> 单飞保护，见设计文档 §5.2
            UBSE_LOG_WARN << "[CLOS_CAB] action=agg_query requester=" << request.requesterMasterNodeId
                          << " group=" << request.requesterGroupId << " result=not_implemented";
        }
    }

    UbseSerialization out;
    UbseCabinetLedgerQueryRespSerialization(out, result);
    return UbseCabinetRespBuffer(out, resp);
}
} // namespace ubse::mem::controller
