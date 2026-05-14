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
#include "ubse_mem_controller_def_serial.h"

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_node_controller_def.h"

namespace ubse::mem::controller::message {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::nodeController::def;
void UbseNodeSerialization(UbseSerialization& out, const UbseNode& node)
{
    out << node.slotId;
    for (uint32_t socketId : node.socketId) {
        out << socketId;
    }
    for (const auto& socket : node.numaIds) {
        for (uint32_t numaId : socket) {
            out << numaId;
        }
    }
    out << node.hostName;
}

bool UbseNodeDeserialization(UbseDeSerialization& in, UbseNode& node)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseNod during deserialization";
        return false;
    }
    in >> node.slotId;

    for (uint32_t& socketId : node.socketId) {
        in >> socketId;
    }

    for (auto& socket : node.numaIds) {
        for (uint32_t& numaId : socket) {
            in >> numaId;
        }
    }
    in >> node.hostName;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseNod during deserialization";
        return false;
    }
    return true;
}

void UbseTopoNodeSerialization(UbseSerialization& out, const UbseTopoNode& node)
{
    out << node.slotId << node.hostName << (right_v<size_t>(node.socketIdList.size()))
        << (right_v<size_t>(node.numaIdList.size())) << (right_v<size_t>(node.ips.size()));
    for (int16_t socketId : node.socketIdList) {
        out << socketId;
    }
    for (int32_t numaId : node.numaIdList) {
        out << numaId;
    }
    for (auto ip : node.ips) {
        out << ip.af;
        struct in_addr ipv4 = ip.ipv4;
        out << *reinterpret_cast<uint32_t*>(&ipv4);
        const auto* ipv6_parts = reinterpret_cast<const uint64_t*>(&ip.ipv6);
        out << ipv6_parts[0];
        out << ipv6_parts[1];
    }
}
bool UbseTopoNodeDeserialization(UbseDeSerialization& in, UbseTopoNode& node)
{
    size_t socketIdListSize{};
    size_t numaIdListSize{};
    size_t ipsSize{};
    in >> node.slotId >> node.hostName >> socketIdListSize >> numaIdListSize >> ipsSize;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseTopoNode during deserialization";
        return false;
    }
    for (size_t i = 0; i < socketIdListSize; i++) {
        int16_t socketId;
        in >> socketId;
        if (!in.Check()) {
            return false;
        }
        node.socketIdList.push_back(socketId);
    }
    for (size_t i = 0; i < numaIdListSize; i++) {
        int32_t numaId;
        in >> numaId;
        if (!in.Check()) {
            return false;
        }
        node.numaIdList.push_back(numaId);
    }
    for (size_t i = 0; i < ipsSize; i++) {
        UbseTopoIpAddress ip{};
        in >> ip.af;

        uint32_t ipv4_raw;
        in >> ipv4_raw;
        if (!in.Check()) {
            return false;
        }
        *reinterpret_cast<uint32_t*>(&ip.ipv4) = ipv4_raw;

        uint64_t ipv6_part0{};
        uint64_t ipv6_part1{};
        in >> ipv6_part0;
        in >> ipv6_part1;
        if (!in.Check()) {
            return false;
        }
        auto* ipv6_parts = reinterpret_cast<uint64_t*>(&ip.ipv6);
        ipv6_parts[0] = ipv6_part0;
        ipv6_parts[1] = ipv6_part1;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseTopoNode during deserialization";
        return false;
    }
    return true;
}

void UbseErrCodeSerialization(UbseSerialization& out, const uint32_t& mErrCode)
{
    out << mErrCode;
}

bool UbseErrCodeDeserialization(UbseDeSerialization& in, uint32_t& mErrCode)
{
    in >> mErrCode;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check mErrCode during deserialization";
        return false;
    }
    return true;
}

void UbseUdsInfoSerialization(UbseSerialization& out, const def::UbseUdsInfo& udsInfo)
{
    out << udsInfo.uid << udsInfo.gid << udsInfo.pid << udsInfo.username;
}
bool UbseUdsInfoDeserialization(UbseDeSerialization& in, def::UbseUdsInfo& udsInfo)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseUdsInfo during deserialization";
        return false;
    }
    in >> udsInfo.uid >> udsInfo.gid >> udsInfo.pid >> udsInfo.username;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseUdsInfo during deserialization";
        return false;
    }
    return true;
}
void UbseMemFdDescSerialization(UbseSerialization& out, const def::UbseMemFdDesc& desc)
{
    out << desc.name << (right_v<size_t>(desc.memIds.size()));
    for (uint64_t memId : desc.memIds) {
        out << memId;
    }
    out << desc.totalMemSize << desc.unitSize;
    UbseNodeSerialization(out, desc.exportNode);
    UbseNodeSerialization(out, desc.importNode);
    out << enum_v(desc.state);
}

bool UbseMemFdDescDeserialization(UbseDeSerialization& in, def::UbseMemFdDesc& desc)
{
    size_t memIdsSize;
    in >> desc.name >> memIdsSize;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemFdDesc during deserialization";
        return false;
    }
    for (size_t i = 0; i < memIdsSize; i++) {
        uint64_t memId;
        in >> memId;
        if (!in.Check()) {
            return false;
        }
        desc.memIds.push_back(memId);
    }
    in >> desc.totalMemSize >> desc.unitSize;
    if (!UbseNodeDeserialization(in, desc.exportNode)) {
        UBSE_LOG_ERROR << "Failed to deserialize exportNode during deserialization";
        return false;
    }
    if (!UbseNodeDeserialization(in, desc.importNode)) {
        UBSE_LOG_ERROR << "Failed to deserialize importNode during deserialization";
        return false;
    }
    in >> enum_v(desc.state);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemFdDesc during deserialization";
        return false;
    }
    return true;
}

void UbseMemFdDescListSerialization(UbseSerialization& out, const std::vector<def::UbseMemFdDesc>& descList)
{
    out << (right_v<size_t>(descList.size()));
    for (const def::UbseMemFdDesc& desc : descList) {
        UbseMemFdDescSerialization(out, desc);
    }
}

bool UbseMemFdDescListDeserialization(UbseDeSerialization& in, std::vector<def::UbseMemFdDesc>& descList)
{
    size_t descListSize;
    in >> descListSize;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemFdDescList during deserialization";
        return false;
    }
    for (size_t i = 0; i < descListSize; i++) {
        def::UbseMemFdDesc desc;
        if (!UbseMemFdDescDeserialization(in, desc)) {
            UBSE_LOG_ERROR << "Failed to deserializeUbseMemFdDesc during deserialization";
            return false;
        }
        descList.push_back(desc);
    }
    return true;
}

void UbseDefMemNumaDescSerialization(UbseSerialization& out, const def::UbseMemNumaDesc& desc)
{
    out << desc.name << desc.numaId << desc.size << enum_v(desc.state);
    UbseNodeSerialization(out, desc.importNode);
    UbseNodeSerialization(out, desc.exportNode);
    for (uint8_t usr : desc.usrInfo) {
        out << usr;
    }
}

bool UbseDefMemNumaDescDeSerialization(UbseDeSerialization& in, def::UbseMemNumaDesc& desc)
{
    in >> desc.name >> desc.numaId >> desc.size >> enum_v(desc.state);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaDesc during deserialization";
        return false;
    }

    if (!UbseNodeDeserialization(in, desc.importNode)) {
        UBSE_LOG_ERROR << "Failed to deserialize importNode during deserialization";
        return false;
    }
    if (!UbseNodeDeserialization(in, desc.exportNode)) {
        UBSE_LOG_ERROR << "Failed to deserialize exportNode during deserialization";
        return false;
    }
    for (uint8_t& usr : desc.usrInfo) {
        in >> usr;
        if (!in.Check()) {
            return false;
        }
    }
    return true;
}

void UbseDefMemNumaDescListSerialization(UbseSerialization& out, const std::vector<def::UbseMemNumaDesc>& descList)
{
    out << (right_v<size_t>(descList.size()));
    for (const def::UbseMemNumaDesc& desc : descList) {
        UbseDefMemNumaDescSerialization(out, desc);
    }
}

bool UbseDefMemNumaDescListDeSerialization(UbseDeSerialization& in, std::vector<def::UbseMemNumaDesc>& descList)
{
    size_t descListSize;
    in >> descListSize;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaDescList during deserialization";
        return false;
    }
    for (size_t i = 0; i < descListSize; i++) {
        def::UbseMemNumaDesc desc;
        if (!UbseDefMemNumaDescDeSerialization(in, desc)) {
            UBSE_LOG_ERROR << "Failed to deserialize UbseMemNumaDesc during deserialization";
            return false;
        }
        descList.push_back(desc);
    }
    return true;
}
void UbseMemNumaDescSerialization(UbseSerialization& out, const UbseMemNumaDesc& desc)
{
    out << desc.name << desc.numaId << desc.size;
    UbseTopoNodeSerialization(out, desc.exportNode);
    UbseTopoNodeSerialization(out, desc.importNode);
    for (uint8_t usrInfo : desc.usrInfo) {
        out << usrInfo;
    }
}

bool UbseMemNumaDescDeSerialization(UbseDeSerialization& in, UbseMemNumaDesc& desc)
{
    in >> desc.name >> desc.numaId >> desc.size;
    if (!UbseTopoNodeDeserialization(in, desc.exportNode)) {
        UBSE_LOG_ERROR << "Failed to deserialize exportNode during deserialization";
        return false;
    }
    if (!UbseTopoNodeDeserialization(in, desc.importNode)) {
        UBSE_LOG_ERROR << "Failed to deserialize importNode during deserialization";
        return false;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaDesc during deserialization";
        return false;
    }
    for (uint8_t& usrInfo : desc.usrInfo) {
        in >> usrInfo;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaDesc during deserialization";
        return false;
    }
    return true;
}
void UbseMemProcessLenderSerialization(UbseSerialization& out, const UbseMemProcessLender& lender)
{
    out << lender.slotId << lender.socketId << lender.pid << (right_v<size_t>(lender.vaLists.size()));
    for (auto vaList : lender.vaLists) {
        out << vaList.addr << vaList.size;
    }
}
bool UbseMemProcessLenderDeserialization(UbseDeSerialization& in, UbseMemProcessLender& lender)
{
    in >> lender.slotId >> lender.socketId >> lender.pid;
    size_t vaListsSize;
    in >> vaListsSize;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemProcessLender during deserialization";
        return false;
    }
    for (size_t i = 0; i < vaListsSize; i++) {
        UbseMemAddrBorrowLocAndSizeByPid vaList;
        in >> vaList.addr >> vaList.size;
        if (!in.Check()) {
            return false;
        }
        lender.vaLists.push_back(vaList);
    }
    return true;
}
void UbseMemAddrDescSerialization(UbseSerialization& out, const UbseMemAddrDesc& desc)
{
    out << desc.name << desc.numaId << desc.size;
    UbseMemProcessLenderSerialization(out, desc.lender);
    UbseTopoNodeSerialization(out, desc.importNode);
}

bool UbseMemAddrDescDeSerialization(UbseDeSerialization& in, UbseMemAddrDesc& desc)
{
    in >> desc.name >> desc.numaId >> desc.size;
    if (!UbseMemProcessLenderDeserialization(in, desc.lender)) {
        UBSE_LOG_ERROR << "Failed to deserialize lender during deserialization";
        return false;
    }
    if (!UbseTopoNodeDeserialization(in, desc.importNode)) {
        UBSE_LOG_ERROR << "Failed to deserialize importNode during deserialization";
        return false;
    }
    return true;
}
void UbseMemShmImportDescSerialization(UbseSerialization& out, const def::UbseMemShmImportDesc& desc)
{
    out << (right_v<size_t>(desc.memIds.size()));
    for (uint64_t memId : desc.memIds) {
        out << memId;
    }
    out << enum_v(desc.state);
    UbseNodeSerialization(out, desc.importNode);
}
bool UbseMemShmImportDescDeSerialization(UbseDeSerialization& in, def::UbseMemShmImportDesc& desc)
{
    size_t memIdsSize;
    in >> memIdsSize;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemShmImportDesc during deserialization";
        return false;
    }
    for (size_t i = 0; i < memIdsSize; i++) {
        uint64_t memId;
        in >> memId;
        if (!in.Check()) {
            return false;
        }
        desc.memIds.push_back(memId);
    }
    in >> enum_v(desc.state);
    if (!UbseNodeDeserialization(in, desc.importNode)) {
        UBSE_LOG_ERROR << "Failed to deserialize importNode during deserialization";
        return false;
    }
    return true;
}
void UbseMemShmDescSerialization(UbseSerialization& out, const def::UbseMemShmDesc& desc)
{
    out << desc.name << desc.totalMemSize << desc.unitSize << enum_v(desc.state);
    UbseNodeSerialization(out, desc.exportNode);
    for (uint8_t userInfo : desc.userInfo) {
        out << userInfo;
    }
    out << (right_v<size_t>(desc.importDesc.size()));
    for (const def::UbseMemShmImportDesc& importDesc : desc.importDesc) {
        UbseMemShmImportDescSerialization(out, importDesc);
    }
    out << (right_v<size_t>(desc.region.size()));
    for (const auto nodeId : desc.region) {
        out << nodeId;
    }
}

bool UbseMemShmDescDeSerialization(UbseDeSerialization& in, def::UbseMemShmDesc& desc)
{
    size_t importDescSize;
    in >> desc.name >> desc.totalMemSize >> desc.unitSize >> enum_v(desc.state);
    if (!UbseNodeDeserialization(in, desc.exportNode)) {
        UBSE_LOG_ERROR << "Failed to deserialize exportNode during deserialization";
        return false;
    }
    for (uint8_t& userInfo : desc.userInfo) {
        in >> userInfo;
    }
    in >> importDescSize;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemShmDesc during deserialization";
        return false;
    }
    for (size_t i = 0; i < importDescSize; i++) {
        def::UbseMemShmImportDesc importDesc;
        if (!UbseMemShmImportDescDeSerialization(in, importDesc)) {
            UBSE_LOG_ERROR << "Failed to deserialize importDesc during deserialization";
            return false;
        }
        desc.importDesc.push_back(importDesc);
    }
    size_t regionSize;
    in >> regionSize;
    if (!in.Check()) {
        return false;
    }
    for (size_t i = 0; i < regionSize; i++) {
        uint32_t nodeId;
        in >> nodeId;
        if (!in.Check()) {
            return false;
        }
        desc.region.push_back(nodeId);
    }
    return true;
}

void UbseMemShmDescListSerialization(UbseSerialization& out, const std::vector<def::UbseMemShmDesc>& descList)
{
    out << (right_v<size_t>(descList.size()));
    for (const def::UbseMemShmDesc& desc : descList) {
        UbseMemShmDescSerialization(out, desc);
    }
}

bool UbseMemShmDescListDeSerialization(UbseDeSerialization& in, std::vector<def::UbseMemShmDesc>& descList)
{
    size_t descListSize;
    in >> descListSize;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemShmDescList during deserialization";
        return false;
    }
    for (size_t i = 0; i < descListSize; i++) {
        def::UbseMemShmDesc desc;
        if (!UbseMemShmDescDeSerialization(in, desc)) {
            UBSE_LOG_ERROR << "Failed to deserialize UbseMemShmDesc during deserialization";
            return false;
        }
        descList.push_back(desc);
    }
    return true;
}

void UbseMemShmMemStatusDescSerialization(UbseSerialization& out, const def::UbseMemShmMemStatusDesc& desc)
{
    out << (right_v<size_t>(desc.memIds.size()));
    for (uint64_t memId : desc.memIds) {
        out << memId;
    }
    out << (right_v<size_t>(desc.faultTypes.size()));
    for (auto faultType : desc.faultTypes) {
        out << enum_v(faultType);
    }
}

bool UbseMemShmMemStatusDescDeSerialization(UbseDeSerialization& in, def::UbseMemShmMemStatusDesc& desc)
{
    size_t memIdsSize;
    in >> memIdsSize;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemShmMemStatusDesc during deserialization";
        return false;
    }
    for (size_t i = 0; i < memIdsSize; i++) {
        uint64_t memId;
        in >> memId;
        if (!in.Check()) {
            return false;
        }
        desc.memIds.push_back(memId);
    }
    size_t faultTypesSize;
    in >> faultTypesSize;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemShmMemStatusDesc during deserialization";
        return false;
    }
    for (size_t i = 0; i < faultTypesSize; i++) {
        adapter_plugins::mmi::UbMemFaultType faultType;
        in >> enum_v(faultType);
        if (!in.Check()) {
            return false;
        }
        desc.faultTypes.push_back(faultType);
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemShmMemStatusDesc during deserialization";
        return false;
    }
    return true;
}

bool UbseNodeBorrowInfoSerialize(UbseSerialization& serialization, const def::UbseNodeBorrowInfo& nodeBorrowInfo)
{
    serialization << nodeBorrowInfo.borrowSlotId << nodeBorrowInfo.borrowHostname << nodeBorrowInfo.lendSlotId
                  << nodeBorrowInfo.lendHostname << nodeBorrowInfo.size;
    return serialization.Check();
}

bool UbseNodeBorrowInfoDeserialize(UbseDeSerialization& deSerialization, def::UbseNodeBorrowInfo& nodeBorrowInfo)
{
    deSerialization >> nodeBorrowInfo.borrowSlotId >> nodeBorrowInfo.borrowHostname >> nodeBorrowInfo.lendSlotId >>
        nodeBorrowInfo.lendHostname >> nodeBorrowInfo.size;
    return deSerialization.Check();
}

bool UbseNodeBorrowInfosSerialize(UbseSerialization& serialization,
                                  const std::vector<def::UbseNodeBorrowInfo>& nodeBorrowInfos)
{
    serialization << array_len_insert(nodeBorrowInfos.size());
    for (const auto& nodeBorrowInfo : nodeBorrowInfos) {
        if (!UbseNodeBorrowInfoSerialize(serialization, nodeBorrowInfo)) {
            UBSE_LOG_ERROR << "Serialize UbseNodeBorrowInfo failed";
            return false;
        }
    }
    return true;
}

bool UbseNodeBorrowInfosDeserialize(UbseDeSerialization& deSerialization,
                                    std::vector<def::UbseNodeBorrowInfo>& nodeBorrowInfos)
{
    nodeBorrowInfos.clear();
    size_t size;
    deSerialization >> array_len_capture(size);
    if (!deSerialization.Check()) {
        return false;
    }
    for (size_t i = 0; i < size; i++) {
        def::UbseNodeBorrowInfo nodeBorrowInfo{};
        if (!UbseNodeBorrowInfoDeserialize(deSerialization, nodeBorrowInfo)) {
            UBSE_LOG_ERROR << "Deserialize UbseNodeBorrowInfo failed";
            return false;
        }
        nodeBorrowInfos.push_back(nodeBorrowInfo);
    }
    return true;
}

bool UbseCliShmDescSerialize(const ubse::mem::def::UbseMemShmDesc& shmDesc, const std::string& importNodeId,
                             UbseSerialization& serialization)
{
    // 序列化基础信息
    serialization << shmDesc.name << shmDesc.totalMemSize << shmDesc.exportNode.slotId << enum_v(shmDesc.state);
    uint32_t importSlotId = 0;
    std::vector<uint64_t> memIds;
    ubse::mem::controller::UbseMemStage importState = UbseMemStage::UBSE_NOT_EXIST;
    for (const auto& importDesc : shmDesc.importDesc) {
        if (std::to_string(importDesc.importNode.slotId) == importNodeId) {
            importSlotId = importDesc.importNode.slotId;
            memIds = importDesc.memIds;
            importState = importDesc.state;
            break;
        }
    }
    serialization << importSlotId << enum_v(importState);
    // 序列化内存ID数组
    serialization << array_len_insert(memIds.size());
    for (auto memId : memIds) {
        serialization << memId;
    }
    // 序列化slotId
    serialization << array_len_insert(shmDesc.region.size());
    for (auto slotId : shmDesc.region) {
        serialization << slotId;
    }
    return serialization.Check();
}
} // namespace ubse::mem::controller::message
