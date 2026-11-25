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

#include "ubse_mem_cabinet_ledger_serial.h"

#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_mem_controller_serial.h"
#include "securec.h"

namespace ubse::mem::serial {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::serial;

namespace {
// 单个机柜条目上限（机柜节点数远小于此值），防御异常报文导致的无界分配
constexpr uint64_t MAX_CABINET_ITEMS = 4096;
} // namespace

void UbseCabinetLedgerQueryReqSerialization(UbseSerialization& out, const UbseCabinetLedgerQueryReq& data)
{
    out << data.requesterMasterNodeId << data.requesterGroupId;
}

bool UbseCabinetLedgerQueryReqDeserialization(UbseDeSerialization& in, UbseCabinetLedgerQueryReq& data)
{
    in >> data.requesterMasterNodeId >> data.requesterGroupId;
    return in.Check();
}

void UbseCabinetLedgerQueryRespSerialization(UbseSerialization& out, const UbseCabinetLedgerQueryResp& data)
{
    out << data.ret << data.cabinetMasterNodeId;
    out << array_len_insert(data.items.size());
    for (const auto& item : data.items) {
        int status = static_cast<int>(item.status);
        out << item.nodeId << status;
        NodeMemDebtInfoSerialization(out, item.debtInfo);
    }
}

bool UbseCabinetLedgerQueryRespDeserialization(UbseDeSerialization& in, UbseCabinetLedgerQueryResp& data)
{
    data.items.clear();
    uint64_t len = 0;
    in >> data.ret >> data.cabinetMasterNodeId >> array_len_capture(len);
    if (!in.Check() || len > MAX_CABINET_ITEMS) {
        return false;
    }
    for (uint64_t i = 0; i < len; ++i) {
        UbseCabinetLedgerItem item{};
        int status = 0;
        in >> item.nodeId >> status;
        if (!in.Check()) {
            return false;
        }
        item.status = static_cast<UbseCollectStatus>(status);
        if (!NodeMemDebtInfoDeserialization(in, item.debtInfo)) {
            return false;
        }
        data.items.push_back(std::move(item));
    }
    return in.Check();
}

void UbseCabinetInvalidateForwardReqSerialization(UbseSerialization& out, const UbseCabinetInvalidateForwardReq& data)
{
    out << data.requesterMasterNodeId;
    out << array_len_insert(data.items.size());
    for (const auto& item : data.items) {
        int borrowType = static_cast<int>(item.borrowType);
        out << item.nodeId << borrowType << item.debtName;
    }
}

bool UbseCabinetInvalidateForwardReqDeserialization(UbseDeSerialization& in, UbseCabinetInvalidateForwardReq& data)
{
    data.items.clear();
    uint64_t len = 0;
    in >> data.requesterMasterNodeId >> array_len_capture(len);
    if (!in.Check() || len > MAX_CABINET_ITEMS) {
        return false;
    }
    for (uint64_t i = 0; i < len; ++i) {
        UbseCabinetInvalidateItem item{};
        int borrowType = 0;
        in >> item.nodeId >> borrowType >> item.debtName;
        if (!in.Check()) {
            return false;
        }
        item.borrowType = static_cast<UbseMemBorrowType>(borrowType);
        data.items.push_back(std::move(item));
    }
    return in.Check();
}

void UbseCabinetInvalidateForwardRespSerialization(UbseSerialization& out, const UbseCabinetInvalidateForwardResp& data)
{
    out << data.ret << data.failCount;
}

bool UbseCabinetInvalidateForwardRespDeserialization(UbseDeSerialization& in, UbseCabinetInvalidateForwardResp& data)
{
    in >> data.ret >> data.failCount;
    return in.Check();
}

UbseResult UbseCabinetRespBuffer(UbseSerialization& out, UbseByteBuffer& resp)
{
    if (!out.Check()) {
        UBSE_LOG_ERROR << "[CLOS_CAB] action=pack_agg_resp result=serialize_failed";
        resp = {nullptr, 0, nullptr};
        return UBSE_ERROR;
    }
    // G.RES.02：聚合响应正常量级在数十MB内（由hcom拆包承载），申请前校验长度合法性，防御异常序列化长度
    static constexpr size_t MAX_CABINET_RESP_SIZE = 64UL * 1024UL * 1024UL;
    const size_t size = out.GetLength();
    if (size == 0 || size > MAX_CABINET_RESP_SIZE) {
        UBSE_LOG_ERROR << "[CLOS_CAB] action=pack_agg_resp result=invalid_size size=" << size;
        resp = {nullptr, 0, nullptr};
        return UBSE_ERROR;
    }
    auto ptr = new (std::nothrow) uint8_t[size];
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "[CLOS_CAB] action=pack_agg_resp result=alloc_failed size=" << size;
        resp = {nullptr, 0, nullptr};
        return UBSE_ERROR_NULLPTR;
    }
    resp = {ptr, size, [](uint8_t* p) noexcept {
                delete[] p;
            }};
    if (memcpy_s(resp.data, resp.len, out.GetBuffer(false), size) != EOK) {
        UBSE_LOG_ERROR << "[CLOS_CAB] action=pack_agg_resp result=memcpy_failed size=" << size;
        resp = {nullptr, 0, nullptr};
        delete[] ptr;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::mem::serial
