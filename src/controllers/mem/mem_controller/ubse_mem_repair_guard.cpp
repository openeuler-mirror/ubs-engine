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

#include "ubse_mem_repair_guard.h"

#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "message/ubse_mem_cabinet_ledger_serial.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::mem::serial;
using namespace ubse::serial;

UbseRepairConfirmResult ConfirmBeforeDestructiveRepair(UbseMemBorrowType borrowType, UbseRepairQuerySide side,
                                                       const def::UbseMemDebtQueryRequest& request)
{
    // 正式实现需经对端柜主 CLOS 点查实时状态后判定，见设计文档 §5.3
    // 骨架阶段一律按"确认失败"保守放弃，A 侧不会据此执行任何破坏性动作
    (void)borrowType;
    (void)side;
    (void)request;
    UBSE_LOG_WARN << "[CLOS_CAB] action=confirm result=not_implemented";
    return UbseRepairConfirmResult::CONFIRM_FAILED;
}

UbseResult ForwardCrossCabinetInvalidate(const std::vector<UbseCabinetInvalidateItem>& items, uint32_t& failCount)
{
    // 正式实现需按对端柜主分组转发既有 0x0017 单导入失效通道，见设计文档 §5.4
    // 骨架阶段不转发；失败计入 failCount，A 侧仅记录不重试
    (void)items;
    failCount = 0;
    UBSE_LOG_WARN << "[CLOS_CAB] action=invalidate_forward result=not_implemented";
    return UBSE_ERROR;
}

UbseResult CabinetInvalidateForwardHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UbseCabinetInvalidateForwardResp result{};
    result.ret = UBSE_ERROR;

    UbseCabinetInvalidateForwardReq request{};
    const bool reqValid = (req.data != nullptr && req.len > 0);
    if (!reqValid) {
        UBSE_LOG_ERROR << "[CLOS_CAB] action=invalidate result=empty_request";
    } else {
        UbseDeSerialization in(req.data, static_cast<uint32_t>(req.len));
        if (!UbseCabinetInvalidateForwardReqDeserialization(in, request)) {
            UBSE_LOG_ERROR << "[CLOS_CAB] action=invalidate result=deserialize_failed";
        } else {
            // 正式实现需逐条转发既有 0x0017 单导入失效通道并统计 failCount，见设计文档 §5.4
            UBSE_LOG_WARN << "[CLOS_CAB] action=invalidate requester=" << request.requesterMasterNodeId
                          << " items=" << request.items.size() << " result=not_implemented";
        }
    }

    UbseSerialization out;
    UbseCabinetInvalidateForwardRespSerialization(out, result);
    return UbseCabinetRespBuffer(out, resp);
}

UbseResult CrossCabinetQueryForwardHandler(uint16_t opCode, const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    // 正式实现需按 opCode 定位目标导入/导出对象，柜内直达 R 现拉实时状态后原样回包，见设计文档 §4.1.3
    // 骨架阶段不回包：A 侧按"确认失败"保守放弃本次破坏性动作
    (void)req;
    resp = {nullptr, 0, nullptr};
    UBSE_LOG_WARN << "[CLOS_CAB] action=confirm opcode=" << opCode << " result=not_implemented";
    return UBSE_ERROR;
}
} // namespace ubse::mem::controller
