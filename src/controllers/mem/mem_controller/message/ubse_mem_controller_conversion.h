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

#ifndef UBSE_MANAGER_UBSE_MEM_CONTROLLER_CONVERSION_H
#define UBSE_MANAGER_UBSE_MEM_CONTROLLER_CONVERSION_H
#include <algorithm>
#include "ubse_def.h"
#include "ubse_mem_controller_serial.h"
#include "ubse_serial_util.h"
#include "ubse_mem_obj.h"
namespace ubse::mem::serial {
using namespace ubse::serial;
using namespace ubse::mem::obj;
inline void UbseNumaLocationSerialize(UbseSerialization &out, UbseNumaLocation &data) {}
inline bool UbseNumaLocationDeserialize(UbseDeSerialization &in, UbseNumaLocation &data)
{
    return true;
}
inline void UbseUdsInfoSerialize(UbseSerialization &out, UbseUdsInfo &data)
{
    out << data.uid << data.gid << data.pid;
}
inline void UbseFdOwnerSerialize(UbseSerialization& out, const FdOwner& data)
{
    out << data.uid << data.gid << data.pid << data.mode;
}
inline bool UbseUdsInfoDeserialize(UbseDeSerialization &in, UbseUdsInfo &data)
{
    in >> data.uid >> data.gid >> data.pid;
    return in.Check();
}
inline bool UbseUdsInfoDeserialize(UbseDeSerialization &in, FdOwner &data)
{
    in >> data.uid >> data.gid >> data.pid >> data.mode;
    return in.Check();
}
inline void UbseNodeInfoSerialize(UbseSerialization &out, UbseNodeInfo &data)
{
    out << data.index << data.nodeId << data.hostName << enum_v(data.status);
}
inline bool UbseNodeInfoDeserialize(UbseDeSerialization &in, UbseNodeInfo &data)
{
    in >> data.index >> data.nodeId >> data.hostName >> enum_v(data.status);
    return in.Check();
}
inline void UbseShmRegionDescSerialize(UbseSerialization &out, UbseShmRegionDesc &data)
{
    out << data.nodeNum;
    out << ubse::serial::array_len_insert(data.nodelist.size());
    for (auto &item : data.nodelist) {
        UbseNodeInfoSerialize(out, item);
    }
}
inline bool UbseShmRegionDescDeserialize(UbseDeSerialization &in, UbseShmRegionDesc &data)
{
    in >> data.nodeNum;
    uint8_t vectorSize;
    in >> ubse::serial::array_len_capture(vectorSize);
    if (!in.Check()) {
        return false;
    }
    for (size_t i = 0; i < vectorSize; i++) {
        UbseNodeInfo nodeInfo;
        if (!UbseNodeInfoDeserialize(in, nodeInfo)) {
            return false;
        }
        data.nodelist.push_back(nodeInfo);
    }
    return true;
}

inline void UbseMemBaseBorrowReqSerialize(UbseSerialization &out, UbseMemBaseBorrowReq &data)
{
    out << data.name << data.timestamp;
}
inline bool UbseMemBaseBorrowReqDeserialize(UbseDeSerialization &in, UbseMemBaseBorrowReq &data)
{
    in >> data.name >> data.timestamp;
    return in.Check();
}

inline void UbseMemFdBorrowReqSerialize(UbseSerialization &out, UbseMemFdBorrowReq &data)
{
    out << data.requestNodeId << data.importNodeId << data.size << enum_v(data.distance);
    UbseFdOwnerSerialize(out, data.owner);
    out << ubse::serial::array_len_insert(data.lenderLocs.size());
    for (const auto &item : data.lenderLocs) {
        out << item.nodeId << item.numaId;
    }
    out << ubse::serial::array_len_insert(data.lenderSizes.size());
    for (const auto &item : data.lenderSizes) {
        out << item;
    }
    UbseMemBaseBorrowReqSerialize(out, data);
}
inline bool UbseMemFdBorrowReqDeserialize(UbseDeSerialization &in, UbseMemFdBorrowReq &data)
{
    in >> data.requestNodeId >> data.importNodeId >> data.size >> enum_v(data.distance);
    if (!UbseUdsInfoDeserialize(in, data.owner)) {
        return false;
    }
    uint8_t vectorSize;
    in >> ubse::serial::array_len_capture(vectorSize);
    if (!in.Check()) {
        return false;
    }
    for (size_t i = 0; i < vectorSize; i++) {
        UbseNumaLocation item;
        in >> item.nodeId >> item.numaId;
        if (!in.Check()) {
            return false;
        }
        data.lenderLocs.push_back(item);
    }
    in >> ubse::serial::array_len_capture(vectorSize);
    if (!in.Check()) {
        return false;
    }
    for (size_t i = 0; i < vectorSize; i++) {
        uint64_t item;
        in >> item;
        if (!in.Check()) {
            return false;
        }
        data.lenderSizes.push_back(item);
    }
    if (!UbseMemBaseBorrowReqDeserialize(in, data)) {
        return false;
    }
    return true;
}

inline void UbseMemNumaBorrowReqSerialize(UbseSerialization &out, UbseMemNumaBorrowReq &data)
{
    out << data.srcSocket << data.srcNuma << data.highWatermark << data.lowWatermark;
    UbseMemFdBorrowReqSerialize(out, data);
}
inline bool UbseMemNumaBorrowReqDeserialize(UbseDeSerialization &in, UbseMemNumaBorrowReq &data)
{
    in >> data.srcSocket >> data.srcNuma >> data.highWatermark >> data.lowWatermark;
    return UbseMemFdBorrowReqDeserialize(in, data);
}

inline void UbseMemOperationRespSerialize(UbseSerialization &out, UbseMemOperationResp &data)
{
    out << data.name << data.requestNodeId << data.errorCode << data.errMsg << data.realSize;
    out << ubse::serial::array_len_insert(data.memIdList.size());
    for (const auto &item : data.memIdList) {
        out << item;
    }
    out << data.remoteNumaId;
}
inline bool UbseMemOperationRespDeserialize(UbseDeSerialization &in, UbseMemOperationResp &data)
{
    in >> data.name >> data.requestNodeId >> data.errorCode >> data.errMsg >> data.realSize;
    uint8_t vectorSize;
    in >> ubse::serial::array_len_capture(vectorSize);
    if (!in.Check()) {
        return false;
    }
    for (size_t i = 0; i < vectorSize; i++) {
        uint64_t item;
        in >> item;
        if (!in.Check()) {
            return false;
        }
        data.memIdList.push_back(item);
    }
    in >> data.remoteNumaId;
    return true;
}
inline void UbseMemReturnReqSerialize(UbseSerialization &out, UbseMemReturnReq &data)
{
    out << data.uid << data.gid << data.name << data.requestNodeId << data.isForceDelete << data.smapBack;
}
inline bool UbseMemReturnReqDeserialize(UbseDeSerialization &in, UbseMemReturnReq &data)
{
    in >> data.uid >> data.gid >> data.name >> data.requestNodeId >> data.isForceDelete >> data.smapBack;
    return in.Check();
}
} // namespace ubse::mem::serial
#endif // UBSE_MANAGER_UBSE_MEM_CONTROLLER_CONVERSION_H
