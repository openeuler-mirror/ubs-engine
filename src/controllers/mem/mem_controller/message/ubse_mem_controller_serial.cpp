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

#include "ubse_mem_controller_serial.h"

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_mmi_interface.h"

using namespace ubse::adapter_plugins::mmi;
using namespace ubse::common;
using namespace ubse::log;
using namespace ubse::serial;

namespace ubse::mem::serial {
UBSE_DEFINE_THIS_MODULE("ubse");
constexpr int MAX_SIZE = 1024; // 一个cpu上ummu最多支持1024个导出，一次借用只允许从1个cpu借用。

// 假设128T内存池，分布在4个节点，最小借用128M，平均借用512M。单个节点的最大的借用账本数为: 128T/4/512M = 2^16 = 65536
constexpr int MAX_DEBT_MAP_SIZE = 65536;

// 借出地址段  最大段数=CPU核数，取个可能的短期上线，1024核
constexpr int MAX_EXPORT_ADDR_LIST_SIZE = 1024;

// 目前感知集群最大规模是128
constexpr int MAX_NODE_LIST_SIZE = 128;

// 定义位掩码和位移常量
constexpr uint16_t BIT0_MASK = 0x1;
constexpr uint16_t BIT1_MASK = 0x1;
constexpr uint16_t BIT2_MASK = 0x1;
constexpr uint16_t BIT3_MASK = 0x1;
constexpr uint16_t BIT4_MASK = 0x1;
constexpr uint16_t BIT5_MASK = 0x1;
constexpr uint16_t BIT6_MASK = 0x1;
constexpr uint16_t BIT7_9_MASK = 0x7;
constexpr uint16_t BIT10_15_MASK = 0x3F;

// 定义字段的位移量
constexpr uint16_t BIT0_SHIFT = 0;
constexpr uint16_t BIT1_SHIFT = 1;
constexpr uint16_t BIT2_SHIFT = 2;
constexpr uint16_t BIT3_SHIFT = 3;
constexpr uint16_t BIT4_SHIFT = 4;
constexpr uint16_t BIT5_SHIFT = 5;
constexpr uint16_t BIT6_SHIFT = 6;
constexpr uint16_t BIT7_SHIFT = 7;
constexpr uint16_t BIT10_SHIFT = 10;
void UbseMemDebtNumaInfoSerialization(UbseSerialization& out, const UbseMemDebtNumaInfo& debtNumaInfo)
{
    out << debtNumaInfo.nodeId << debtNumaInfo.socketId << debtNumaInfo.numaId << debtNumaInfo.size
        << debtNumaInfo.chipId << debtNumaInfo.portId;
}

bool UbseMemDebtNumaInfoDeserialization(UbseDeSerialization& in, UbseMemDebtNumaInfo& debtNumaInfo)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemDebtNumaInfo during deserialization";
        return false;
    }
    in >> debtNumaInfo.nodeId >> debtNumaInfo.socketId >> debtNumaInfo.numaId >> debtNumaInfo.size >>
        debtNumaInfo.chipId >> debtNumaInfo.portId;
    return in.Check();
}

void UbseMemAlgoResultSerialization(UbseSerialization& out, const UbseMemAlgoResult& algoResult)
{
    out << (right_v<size_t>(algoResult.exportNumaInfos.size()));
    for (auto& exportNumaInfo : algoResult.exportNumaInfos) {
        UbseMemDebtNumaInfoSerialization(out, exportNumaInfo);
    }
    out << (right_v<size_t>(algoResult.importNumaInfos.size()));
    for (auto& importNumaInfo : algoResult.importNumaInfos) {
        UbseMemDebtNumaInfoSerialization(out, importNumaInfo);
    }
    out << algoResult.blockSize;
    out << algoResult.attachSocketId;
}

bool UbseMemAlgoResultDeserialization(UbseDeSerialization& in, UbseMemAlgoResult& algoResult)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemAlgoResult during deserialization";
        return false;
    }
    size_t exportNumaInfoSize{};
    in >> exportNumaInfoSize;
    if (!in.Check()) {
        return false;
    }
    if (exportNumaInfoSize > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid exportNumaInfoSize during deserialization";
        return false;
    }
    algoResult.exportNumaInfos.resize(exportNumaInfoSize);
    for (size_t i = 0; i < exportNumaInfoSize; ++i) {
        if (!UbseMemDebtNumaInfoDeserialization(in, algoResult.exportNumaInfos[i])) {
            UBSE_LOG_ERROR << "UbseMemDebtNumaInfoDeserialization failed;";
            return false;
        }
    }
    size_t importNumaInfoSize{};
    in >> importNumaInfoSize;
    if (!in.Check()) {
        return false;
    }
    if (importNumaInfoSize > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid importNumaInfoSize during deserialization";
        return false;
    }
    algoResult.importNumaInfos.resize(importNumaInfoSize);
    for (size_t i = 0; i < importNumaInfoSize; ++i) {
        if (!UbseMemDebtNumaInfoDeserialization(in, algoResult.importNumaInfos[i])) {
            UBSE_LOG_ERROR << "UbseMemDebtNumaInfoDeserialization failed;";
            return false;
        }
    }
    in >> algoResult.blockSize;
    in >> algoResult.attachSocketId;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemAlgoResult during deserialization";
        return false;
    }
    return true;
}

void UbseMemObmmMemDescSerialization(UbseSerialization& out, const ubse_mem_obmm_mem_desc& ubseMemObmmMemDesc)
{
    addr_len<uint8_t> addrLen;
    addrLen.ptr = (unsigned char*)&ubseMemObmmMemDesc;
    addrLen.len = sizeof(ubseMemObmmMemDesc);
    out << addrLen;
}

bool UbseMemObmmMemDescDeserialization(UbseDeSerialization& in, ubse_mem_obmm_mem_desc& ubseMemObmmMemDesc)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemObmmMemDesc during deserialization";
        return false;
    }
    addr_len<uint8_t> addrLen;
    addrLen.ptr = (unsigned char*)&ubseMemObmmMemDesc;
    addrLen.len = sizeof(ubseMemObmmMemDesc);
    in >> addrLen;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemObmmMemDesc during deserialization";
        return false;
    }
    return true;
}

void UbseMemObmmInfoSerialization(UbseSerialization& out, const UbseMemObmmInfo& ubseMemObmmInfo)
{
    out << ubseMemObmmInfo.memId;
    UbseMemObmmMemDescSerialization(out, ubseMemObmmInfo.desc);
    out << enum_v(ubseMemObmmInfo.memIdStatus);
}

bool UbseMemObmmInfoDeserialization(UbseDeSerialization& in, UbseMemObmmInfo& ubseMemObmmInfo)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemObmmInfo during deserialization";
        return false;
    }
    in >> ubseMemObmmInfo.memId;
    UbseMemObmmMemDescDeserialization(in, ubseMemObmmInfo.desc);
    in >> enum_v(ubseMemObmmInfo.memIdStatus);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemObmmInfo during deserialization";
        return false;
    }
    return true;
}

void UbseMemExportStatusSerialization(UbseSerialization& out, const UbseMemExportStatus& ubseMemExportStatus)
{
    out << ubseMemExportStatus.errCode << (right_v<size_t>(ubseMemExportStatus.exportObmmInfo.size()));
    for (auto& exportObmmInfo : ubseMemExportStatus.exportObmmInfo) {
        UbseMemObmmInfoSerialization(out, exportObmmInfo);
    }
    out << enum_v(ubseMemExportStatus.expectState) << enum_v(ubseMemExportStatus.state);
}

bool UbseMemExportStatusDeserialization(UbseDeSerialization& in, UbseMemExportStatus& ubseMemExportStatus)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemExportStatus during deserialization";
        return false;
    }
    size_t exportObmmInfosize{};
    in >> ubseMemExportStatus.errCode >> exportObmmInfosize;
    if (exportObmmInfosize > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid exportObmmInfosize during deserialization";
        return false;
    }
    ubseMemExportStatus.exportObmmInfo.resize(exportObmmInfosize);
    for (size_t i = 0; i < exportObmmInfosize; ++i) {
        if (!UbseMemObmmInfoDeserialization(in, ubseMemExportStatus.exportObmmInfo[i])) {
            UBSE_LOG_ERROR << "UbseMemObmmInfoDeserialization Failed ";
            return false;
        }
    }
    in >> enum_v(ubseMemExportStatus.expectState) >> enum_v(ubseMemExportStatus.state);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemExportStatus during deserialization";
        return false;
    }
    return true;
}

void UbseUdsInfoSerialization(UbseSerialization& out, const UbseUdsInfo& udsInfo)
{
    out << udsInfo.uid << udsInfo.gid << udsInfo.pid << udsInfo.username;
}

void UbseTrustChainDataSerialization(UbseSerialization& out, const UbseTrustRingData& chainData)
{
    out << chainData.trustRingId << chainData.reqSignedData;
    out << right_v<size_t>(chainData.lendSignedDatas.size());
    for (const auto lendSignedData : chainData.lendSignedDatas) {
        out << lendSignedData;
    }
}

void UbseFdOwnerSerialization(UbseSerialization& out, const FdOwner& fdOwner)
{
    out << fdOwner.uid << fdOwner.gid << fdOwner.pid << fdOwner.mode;
}

bool UbseUdsInfoDeserialization(UbseDeSerialization& in, UbseUdsInfo& udsInfo)
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

bool UbseTrustChainDataDeserialization(UbseDeSerialization& in, UbseTrustRingData& ringData)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseTrustChainData during deserialization";
        return false;
    }

    in >> ringData.trustRingId >> ringData.reqSignedData;
    size_t lendSignedDatasSize;
    in >> lendSignedDatasSize;
    if (lendSignedDatasSize > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid lendSignedDatasSize during deserialization";
        return false;
    }
    for (size_t i = 0; i < lendSignedDatasSize; i++) {
        std::string lendSignedData{};
        in >> lendSignedData;
        ringData.lendSignedDatas.emplace_back(lendSignedData);
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseTrustChainData during deserialization";
        return false;
    }
    return true;
}

bool UbseFdOwnerDeserialization(UbseDeSerialization& in, FdOwner& fdOwner)
{
    in >> fdOwner.uid >> fdOwner.gid >> fdOwner.pid >> fdOwner.mode;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check FdOwner during deserialization";
        return false;
    }
    return true;
}

void UbseNumaLocationSerialization(UbseSerialization& out, const UbseNumaLocation& ubseNumaLocation)
{
    out << ubseNumaLocation.nodeId << ubseNumaLocation.numaId;
}

bool UbseNumaLocationDeserialization(UbseDeSerialization& in, UbseNumaLocation& ubseNumaLocation)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseNumaLocation during deserialization";
        return false;
    }
    in >> ubseNumaLocation.nodeId >> ubseNumaLocation.numaId;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseNumaLocation during deserialization";
        return false;
    }
    return true;
}

inline uint16_t UbseMemPrivDataToUint16(const UbseMemPrivData& data)
{
    uint16_t val = 0;
    val |= static_cast<uint16_t>(data.onePth) << BIT0_SHIFT;
    val |= static_cast<uint16_t>(data.wrDelayComp) << BIT1_SHIFT;
    val |= static_cast<uint16_t>(data.reduceDelayComp) << BIT2_SHIFT;
    val |= static_cast<uint16_t>(data.cmoDelayComp) << BIT3_SHIFT;
    val |= static_cast<uint16_t>(data.so) << BIT4_SHIFT;
    val |= static_cast<uint16_t>(data.adTrOchip) << BIT5_SHIFT;
    val |= static_cast<uint16_t>(data.cacheableFlag) << BIT6_SHIFT;
    val |= static_cast<uint16_t>(data.marId) << BIT7_SHIFT;
    val |= static_cast<uint16_t>(data.rsv0) << BIT10_SHIFT;
    return val;
}

inline void UbseMemPrivDataFromUint16(uint16_t val, UbseMemPrivData& data)
{
    data.onePth = (val >> BIT0_SHIFT) & BIT0_MASK;
    data.wrDelayComp = (val >> BIT1_SHIFT) & BIT1_MASK;
    data.reduceDelayComp = (val >> BIT2_SHIFT) & BIT2_MASK;
    data.cmoDelayComp = (val >> BIT3_SHIFT) & BIT3_MASK;
    data.so = (val >> BIT4_SHIFT) & BIT4_MASK;
    data.adTrOchip = (val >> BIT5_SHIFT) & BIT5_MASK;
    data.cacheableFlag = (val >> BIT6_SHIFT) & BIT6_MASK;
    data.marId = (val >> BIT7_SHIFT) & BIT7_9_MASK;
    data.rsv0 = (val >> BIT10_SHIFT) & BIT10_15_MASK;
}

inline void UbseMemBaseBorrowReqSerialize(UbseSerialization& out, const UbseMemBaseBorrowReq& data)
{
    out << data.name << data.requestId << data.requestNodeId;
    UbseUdsInfoSerialization(out, data.udsInfo);
    UbseTrustChainDataSerialization(out, data.trustRingData);
    uint16_t privDataVal = UbseMemPrivDataToUint16(data.ubseMemPrivData);
    out << privDataVal;
}

inline void UbseMemBaseBorrowReqDeserialize(UbseDeSerialization& in, UbseMemBaseBorrowReq& data)
{
    in >> data.name >> data.requestId >> data.requestNodeId;
    UbseUdsInfoDeserialization(in, data.udsInfo);
    UbseTrustChainDataDeserialization(in, data.trustRingData);
    uint16_t privDataVal{};
    in >> privDataVal;
    UbseMemPrivDataFromUint16(privDataVal, data.ubseMemPrivData);
}

bool UbseMemFdBorrowReqSerialization(UbseSerialization& out, const UbseMemFdBorrowReq& req)
{
    UbseMemBaseBorrowReqSerialize(out, req);
    out << req.importNodeId << req.size << enum_v(req.distance);
    out << (right_v<size_t>(req.lenderLocs.size()));
    for (auto& lenderLoc : req.lenderLocs) {
        UbseNumaLocationSerialization(out, lenderLoc);
    }
    out << (right_v<size_t>(req.lenderSizes.size()));
    for (auto lenderSize : req.lenderSizes) {
        out << lenderSize;
    }
    out << (right_v<size_t>(req.candidateNodeList.size()));
    for (const auto& candidateNode : req.candidateNodeList) {
        out << candidateNode;
    }
    UbseFdOwnerSerialization(out, req.owner);
    return out.Check();
}

bool UbseMemFdBorrowReqDeserialization(UbseDeSerialization& in, UbseMemFdBorrowReq& req)
{
    UbseMemBaseBorrowReqDeserialize(in, req);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemBaseBorrowReqDeserialize during deserialization";
        return false;
    }
    in >> req.importNodeId >> req.size >> enum_v(req.distance);
    size_t lenderLocsSize{};
    in >> lenderLocsSize;
    if (lenderLocsSize > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid lenderLocsSize during deserialization";
        return false;
    }
    req.lenderLocs.resize(lenderLocsSize);
    for (size_t i = 0; i < lenderLocsSize; ++i) {
        if (!UbseNumaLocationDeserialization(in, req.lenderLocs[i])) {
            UBSE_LOG_ERROR << "UbseNumaLocationDeserialization failed;";
            return false;
        }
    }
    size_t lenderSizesSize{};
    in >> lenderSizesSize;
    if (lenderSizesSize > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid lenderSizesSize during deserialization";
        return false;
    }
    req.lenderSizes.resize(lenderSizesSize);
    for (size_t i = 0; i < lenderSizesSize; ++i) {
        in >> req.lenderSizes[i];
    }
    size_t candidateNodeListSize{};
    in >> candidateNodeListSize;
    if (candidateNodeListSize > MAX_NODE_LIST_SIZE) {
        UBSE_LOG_ERROR << "Invalid candidateNodeListSize during deserialization, size=" << candidateNodeListSize;
        return false;
    }
    req.candidateNodeList.resize(candidateNodeListSize);
    for (size_t i = 0; i < candidateNodeListSize; ++i) {
        in >> req.candidateNodeList[i];
    }
    UbseFdOwnerDeserialization(in, req.owner);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemFdBorrowReq during deserialization";
        return false;
    }
    return true;
}

bool UbseMemFdBorrowExportObjSerialization(UbseSerialization& out, const UbseMemFdBorrowExportObj& fdBorrowExportObj)
{
    out << fdBorrowExportObj.errorCode;
    UbseMemAlgoResultSerialization(out, fdBorrowExportObj.algoResult);
    UbseMemExportStatusSerialization(out, fdBorrowExportObj.status);
    UbseMemFdBorrowReqSerialization(out, fdBorrowExportObj.req);
    UbseMemReturnReqSerialize(out, fdBorrowExportObj.returnReq);
    return out.Check();
}

bool UbseMemFdBorrowExportObjDeserialization(UbseDeSerialization& in, UbseMemFdBorrowExportObj& fdBorrowExportObj)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemFdBorrowExportObj during deserialization";
        return false;
    }
    in >> fdBorrowExportObj.errorCode;
    if (!UbseMemAlgoResultDeserialization(in, fdBorrowExportObj.algoResult)) {
        UBSE_LOG_ERROR << "UbseMemAlgoResultDeserialization failed;";
        return false;
    }
    if (!UbseMemExportStatusDeserialization(in, fdBorrowExportObj.status)) {
        UBSE_LOG_ERROR << "UbseMemExportStatusDeserialization failed;";
        return false;
    }
    if (!UbseMemFdBorrowReqDeserialization(in, fdBorrowExportObj.req)) {
        UBSE_LOG_ERROR << "UbseMemFdBorrowReqDeserialization failed;";
        return false;
    }
    if (!UbseMemReturnReqDeserialize(in, fdBorrowExportObj.returnReq)) {
        UBSE_LOG_ERROR << "Failed to UbseMemReturnReqDeserialize";
        return false;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemFdBorrowExportObj during deserialization";
        return false;
    }
    return true;
}

void UbseMemImportResultSerialization(UbseSerialization& out, const UbseMemImportResult& ubseMemImportResult)
{
    out << ubseMemImportResult.memId << ubseMemImportResult.numaId;
}

bool UbseMemImportResultDeserialization(UbseDeSerialization& in, UbseMemImportResult& ubseMemImportResult)
{
    in >> ubseMemImportResult.memId >> ubseMemImportResult.numaId;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemFdBorrowExportObj during deserialization";
        return false;
    }
    return true;
}

void UbseMemImportStatusSerialization(UbseSerialization& out, const UbseMemImportStatus& ubseMemImportStatus)
{
    out << ubseMemImportStatus.errCode << ubseMemImportStatus.scna
        << (right_v<size_t>(ubseMemImportStatus.importResults.size()));
    for (auto& importResult : ubseMemImportStatus.importResults) {
        UbseMemImportResultSerialization(out, importResult);
    }
    out << (right_v<size_t>(ubseMemImportStatus.decoderResult.size()));
    for (auto& decoderResult : ubseMemImportStatus.decoderResult) {
        out << decoderResult.marId << decoderResult.hpa << decoderResult.handle << decoderResult.valid;
    }
    out << enum_v(ubseMemImportStatus.expectState) << enum_v(ubseMemImportStatus.state);
}

bool UbseMemImportStatusDeserialization(UbseDeSerialization& in, UbseMemImportStatus& ubseMemImportStatus)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemImportStatus during deserialization";
        return false;
    }
    size_t size{};
    in >> ubseMemImportStatus.errCode >> ubseMemImportStatus.scna >> size;
    if (!in.Check()) {
        return false;
    }
    if (size > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid UbseMemImportStatus during deserialization";
        return false;
    }
    ubseMemImportStatus.importResults.resize(size);
    for (size_t i = 0; i < size; ++i) {
        if (!UbseMemImportResultDeserialization(in, ubseMemImportStatus.importResults[i])) {
            UBSE_LOG_ERROR << "UbseMemImportResultDeserialization failed;";
            return false;
        }
    }
    in >> size;
    if (!in.Check()) {
        return false;
    }
    ubseMemImportStatus.decoderResult.resize(size);
    for (size_t i = 0; i < size; ++i) {
        in >> ubseMemImportStatus.decoderResult[i].marId >> ubseMemImportStatus.decoderResult[i].hpa >>
            ubseMemImportStatus.decoderResult[i].handle >> ubseMemImportStatus.decoderResult[i].valid;
        if (!in.Check()) {
            UBSE_LOG_ERROR << "Failed to check UbseMemFdBorrowExportObj during deserialization";
            return false;
        }
    }

    in >> enum_v(ubseMemImportStatus.expectState) >> enum_v(ubseMemImportStatus.state);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemImportStatus during deserialization";
        return false;
    }
    return true;
}

bool UbseMemFdBorrowImportObjSerialization(UbseSerialization& out,
                                           const UbseMemFdBorrowImportObj& ubseMemFdBorrowImportObj)
{
    out << ubseMemFdBorrowImportObj.errorCode;
    UbseMemAlgoResultSerialization(out, ubseMemFdBorrowImportObj.algoResult);
    out << (right_v<size_t>(ubseMemFdBorrowImportObj.exportObmmInfo.size()));
    for (auto exportObmmInfo : ubseMemFdBorrowImportObj.exportObmmInfo) {
        UbseMemObmmInfoSerialization(out, exportObmmInfo);
    }
    UbseMemImportStatusSerialization(out, ubseMemFdBorrowImportObj.status);
    UbseMemFdBorrowReqSerialization(out, ubseMemFdBorrowImportObj.req);
    UbseMemReturnReqSerialize(out, ubseMemFdBorrowImportObj.returnReq);
    return out.Check();
}

bool UbseMemFdBorrowImportObjDeserialization(UbseDeSerialization& in,
                                             UbseMemFdBorrowImportObj& ubseMemFdBorrowImportObj)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemFdBorrowImportObj during deserialization";
        return false;
    }
    in >> ubseMemFdBorrowImportObj.errorCode;
    if (!UbseMemAlgoResultDeserialization(in, ubseMemFdBorrowImportObj.algoResult)) {
        UBSE_LOG_ERROR << "UbseMemAlgoResultDeserialization failed;";
        return false;
    }
    size_t size{};
    in >> size;
    if (!in.Check() || size > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid size during deserialization";
        return false;
    }
    ubseMemFdBorrowImportObj.exportObmmInfo.resize(size);
    for (size_t i = 0; i < size; ++i) {
        if (!UbseMemObmmInfoDeserialization(in, ubseMemFdBorrowImportObj.exportObmmInfo[i])) {
            UBSE_LOG_ERROR << "UbseMemObmmInfoDeserialization failed;";
            return false;
        }
    }
    if (!UbseMemImportStatusDeserialization(in, ubseMemFdBorrowImportObj.status)) {
        UBSE_LOG_ERROR << "UbseMemImportStatusDeserialization failed;";
        return false;
    }
    if (!UbseMemFdBorrowReqDeserialization(in, ubseMemFdBorrowImportObj.req)) {
        UBSE_LOG_ERROR << "UbseMemFdBorrowReqDeserialization failed;";
        return false;
    }
    if (!UbseMemReturnReqDeserialize(in, ubseMemFdBorrowImportObj.returnReq)) {
        UBSE_LOG_ERROR << "Failed to UbseMemReturnReqDeserialize";
        return false;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemFdBorrowImportObj during deserialization";
        return false;
    }
    return true;
}
bool UbseMemLenderLinkInfoSerialization(UbseSerialization& out, const UbseMemLenderLinkInfo& linkInfo)
{
    out << linkInfo.lenderNode << linkInfo.lenderSocketId << linkInfo.lenderPort;
    return out.Check();
}

bool UbseMemLenderLinkInfoDeSerialization(UbseDeSerialization& in, UbseMemLenderLinkInfo& linkInfo)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemLenderLinkInfo during deserialization";
        return false;
    }
    in >> linkInfo.lenderNode >> linkInfo.lenderSocketId >> linkInfo.lenderPort;
    return true;
}

inline void SerializeUsrInfo(UbseSerialization& out, const uint8_t* usrInfo, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        out << usrInfo[i];
    }
}

bool UbseMemNumaBorrowReqSerialization(UbseSerialization& out, const UbseMemNumaBorrowReq& req)
{
    UbseMemFdBorrowReq fdBorrowReq;
    UbseMemFdBorrowReqSerialization(out, req);
    out << req.srcSocket << req.srcNuma << req.highWatermark << req.lowWatermark << req.requestId;
    UbseMemLenderLinkInfoSerialization(out, req.linkInfo);
    // 新增字段：usrInfo
    SerializeUsrInfo(out, req.usrInfo, UBSE_MAX_USR_INFO_LEN);
    return out.Check();
}

inline bool UbseUsrInfoDeserialize(UbseDeSerialization& in, uint8_t* usrInfo, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        in >> usrInfo[i];
    }
    return in.Check();
}

bool UbseMemNumaBorrowReqDeserialization(UbseDeSerialization& in, UbseMemNumaBorrowReq& req)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaBorrowReq during deserialization";
        return false;
    }
    if (!UbseMemFdBorrowReqDeserialization(in, req)) {
        UBSE_LOG_ERROR << "UbseMemAlgoResultDeserialization failed;";
        return false;
    }
    in >> req.srcSocket >> req.srcNuma >> req.highWatermark >> req.lowWatermark >> req.requestId;
    if (!UbseMemLenderLinkInfoDeSerialization(in, req.linkInfo)) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaBorrowReq during deserialization";
        return false;
    }
    // 读取usrInfo
    if (!UbseUsrInfoDeserialize(in, req.usrInfo, UBSE_MAX_USR_INFO_LEN)) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaBorrowReq during deserialization";
        return false;
    }
    return true;
}

bool UbseMemNumaBorrowExportObjSerialization(UbseSerialization& out,
                                             const UbseMemNumaBorrowExportObj& ubseMemNumaBorrowExportObj)
{
    out << ubseMemNumaBorrowExportObj.errorCode;
    UbseMemAlgoResultSerialization(out, ubseMemNumaBorrowExportObj.algoResult);
    UbseMemExportStatusSerialization(out, ubseMemNumaBorrowExportObj.status);
    UbseMemNumaBorrowReqSerialization(out, ubseMemNumaBorrowExportObj.req);
    UbseMemReturnReqSerialize(out, ubseMemNumaBorrowExportObj.returnReq);
    return out.Check();
}

bool UbseMemNumaBorrowExportObjDeserialization(UbseDeSerialization& in,
                                               UbseMemNumaBorrowExportObj& ubseMemNumaBorrowExportObj)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaBorrowExportObj during deserialization";
        return false;
    }
    in >> ubseMemNumaBorrowExportObj.errorCode;
    if (!UbseMemAlgoResultDeserialization(in, ubseMemNumaBorrowExportObj.algoResult)) {
        UBSE_LOG_ERROR << "UbseMemAlgoResultDeserialization failed;";
        return false;
    }
    if (!UbseMemExportStatusDeserialization(in, ubseMemNumaBorrowExportObj.status)) {
        UBSE_LOG_ERROR << "UbseMemExportStatusDeserialization failed;";
        return false;
    }
    if (!UbseMemNumaBorrowReqDeserialization(in, ubseMemNumaBorrowExportObj.req)) {
        UBSE_LOG_ERROR << "UbseMemNumaBorrowReqDeserialization failed;";
        return false;
    }
    if (!UbseMemReturnReqDeserialize(in, ubseMemNumaBorrowExportObj.returnReq)) {
        UBSE_LOG_ERROR << "Failed to UbseMemReturnReqDeserialize";
        return false;
    }
    return true;
}

bool UbseMemNumaBorrowImportObjSerialization(UbseSerialization& out,
                                             const UbseMemNumaBorrowImportObj& ubseMemNumaBorrowImportObj)
{
    out << ubseMemNumaBorrowImportObj.errorCode;
    UbseMemAlgoResultSerialization(out, ubseMemNumaBorrowImportObj.algoResult);
    out << (right_v<size_t>(ubseMemNumaBorrowImportObj.exportObmmInfo.size()));
    for (auto& exportObmmInfo : ubseMemNumaBorrowImportObj.exportObmmInfo) {
        UbseMemObmmInfoSerialization(out, exportObmmInfo);
    }
    UbseMemImportStatusSerialization(out, ubseMemNumaBorrowImportObj.status);
    UbseMemNumaBorrowReqSerialization(out, ubseMemNumaBorrowImportObj.req);
    UbseMemReturnReqSerialize(out, ubseMemNumaBorrowImportObj.returnReq);
    return out.Check();
}

bool UbseMemNumaBorrowImportObjDeserialization(UbseDeSerialization& in,
                                               UbseMemNumaBorrowImportObj& ubseMemNumaBorrowImportObj)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaBorrowImportObj during deserialization";
        return false;
    }
    in >> ubseMemNumaBorrowImportObj.errorCode;
    if (!UbseMemAlgoResultDeserialization(in, ubseMemNumaBorrowImportObj.algoResult)) {
        UBSE_LOG_ERROR << "UbseMemAlgoResultDeserialization failed;";
        return false;
    }
    size_t size{};
    in >> size;
    if (size > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid size during deserialization";
        return false;
    }
    ubseMemNumaBorrowImportObj.exportObmmInfo.resize(size);
    for (size_t i = 0; i < size; ++i) {
        if (!UbseMemObmmInfoDeserialization(in, ubseMemNumaBorrowImportObj.exportObmmInfo[i])) {
            UBSE_LOG_ERROR << "UbseMemObmmInfoDeserialization failed;";
            return false;
        }
    }
    if (!UbseMemImportStatusDeserialization(in, ubseMemNumaBorrowImportObj.status)) {
        UBSE_LOG_ERROR << "UbseMemImportStatusDeserialization failed;";
        return false;
    }
    if (!UbseMemNumaBorrowReqDeserialization(in, ubseMemNumaBorrowImportObj.req)) {
        UBSE_LOG_ERROR << "UbseMemNumaBorrowReqDeserialization failed;";
        return false;
    }
    if (!UbseMemReturnReqDeserialize(in, ubseMemNumaBorrowImportObj.returnReq)) {
        UBSE_LOG_ERROR << "Failed to UbseMemReturnReqDeserialize";
        return false;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaBorrowImportObj during deserialization";
        return false;
    }
    return true;
}

void UbseMemAddrInfoSerialization(UbseSerialization& out, const UbseMemAddrInfo& ubseMemAddrInfo)
{
    out << ubseMemAddrInfo.addr << ubseMemAddrInfo.size;
}

bool UbseMemAddrInfoDeserialization(UbseDeSerialization& in, UbseMemAddrInfo& ubseMemAddrInfo)
{
    in >> ubseMemAddrInfo.addr >> ubseMemAddrInfo.size;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemAddrInfo during deserialization";
        return false;
    }
    return true;
}

void UbseMemAddrBorrowReqSerialization(UbseSerialization& out, const UbseMemAddrBorrowReq& req)
{
    out << req.importNodeId << req.importPid << req.exportNodeId << req.exportPid << req.srcSocket << req.srcNuma
        << req.dstSocket << req.dstNuma;
    out << (right_v<size_t>(req.exportAddrList.size()));
    for (auto addrInfo : req.exportAddrList) {
        UbseMemAddrInfoSerialization(out, addrInfo);
    }
    UbseMemBaseBorrowReqSerialize(out, req);
}

bool UbseMemAddrBorrowReqDeserialization(UbseDeSerialization& in, UbseMemAddrBorrowReq& req)
{
    in >> req.importNodeId >> req.importPid >> req.exportNodeId >> req.exportPid >> req.srcSocket >> req.srcNuma >>
        req.dstSocket >> req.dstNuma;
    size_t size{};
    in >> size;
    if (!in.Check()) {
        return false;
    }
    if (size > MAX_EXPORT_ADDR_LIST_SIZE) {
        UBSE_LOG_ERROR << "Invalid exportAddrListSize during deserialization, size=" << size;
        return false;
    }
    req.exportAddrList.resize(size);
    for (size_t i = 0; i < size; ++i) {
        if (!UbseMemAddrInfoDeserialization(in, req.exportAddrList[i])) {
            UBSE_LOG_ERROR << "UbseMemAddrBorrowImportObjDeserialization failed;";
            return false;
        }
    }
    UbseMemBaseBorrowReqDeserialize(in, req);
    return in.Check();
}

bool UbseMemAddrBorrowExportObjSerialization(UbseSerialization& out,
                                             const UbseMemAddrBorrowExportObj& ubseMemAddrBorrowExportObj)
{
    out << ubseMemAddrBorrowExportObj.errorCode;
    UbseMemAddrBorrowReqSerialization(out, ubseMemAddrBorrowExportObj.req);
    UbseMemReturnReqSerialize(out, ubseMemAddrBorrowExportObj.returnReq);
    UbseMemAlgoResultSerialization(out, ubseMemAddrBorrowExportObj.algoResult);
    UbseMemExportStatusSerialization(out, ubseMemAddrBorrowExportObj.status);
    return true;
}

bool UbseMemAddrBorrowExportObjDeserialization(UbseDeSerialization& in,
                                               UbseMemAddrBorrowExportObj& ubseMemAddrBorrowExportObj)
{
    in >> ubseMemAddrBorrowExportObj.errorCode;
    if (!UbseMemAddrBorrowReqDeserialization(in, ubseMemAddrBorrowExportObj.req)) {
        UBSE_LOG_ERROR << "UbseMemAddrBorrowReqDeserialization failed;";
        return false;
    }
    if (!UbseMemReturnReqDeserialize(in, ubseMemAddrBorrowExportObj.returnReq)) {
        UBSE_LOG_ERROR << "Failed to UbseMemReturnReqDeserialize";
        return false;
    }
    if (!UbseMemAlgoResultDeserialization(in, ubseMemAddrBorrowExportObj.algoResult)) {
        UBSE_LOG_ERROR << "UbseMemAlgoResultDeserialization failed;";
        return false;
    }
    if (!UbseMemExportStatusDeserialization(in, ubseMemAddrBorrowExportObj.status)) {
        UBSE_LOG_ERROR << "UbseMemExportStatusDeserialization failed;";
        return false;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemAddrBorrowExportObj during deserialization";
        return false;
    }
    return true;
}

bool UbseMemAddrBorrowImportObjSerialization(UbseSerialization& out,
                                             const UbseMemAddrBorrowImportObj& ubseMemAddrBorrowImportObj)
{
    out << ubseMemAddrBorrowImportObj.errorCode;
    UbseMemAddrBorrowReqSerialization(out, ubseMemAddrBorrowImportObj.req);
    UbseMemReturnReqSerialize(out, ubseMemAddrBorrowImportObj.returnReq);
    UbseMemAlgoResultSerialization(out, ubseMemAddrBorrowImportObj.algoResult);
    out << (right_v<size_t>(ubseMemAddrBorrowImportObj.exportObmmInfo.size()));
    for (const auto& i : ubseMemAddrBorrowImportObj.exportObmmInfo) {
        UbseMemObmmInfoSerialization(out, i);
    }
    UbseMemImportStatusSerialization(out, ubseMemAddrBorrowImportObj.status);
    return out.Check();
}

bool UbseMemAddrBorrowImportObjDeserialization(UbseDeSerialization& in,
                                               UbseMemAddrBorrowImportObj& ubseMemAddrBorrowImportObj)
{
    in >> ubseMemAddrBorrowImportObj.errorCode;
    if (!UbseMemAddrBorrowReqDeserialization(in, ubseMemAddrBorrowImportObj.req)) {
        UBSE_LOG_ERROR << "UbseMemAddrBorrowImportObjDeserialization failed;";
        return false;
    }
    if (!UbseMemReturnReqDeserialize(in, ubseMemAddrBorrowImportObj.returnReq)) {
        UBSE_LOG_ERROR << "Failed to UbseMemReturnReqDeserialize";
        return false;
    }
    if (!UbseMemAlgoResultDeserialization(in, ubseMemAddrBorrowImportObj.algoResult)) {
        UBSE_LOG_ERROR << "UbseMemAddrBorrowImportObjDeserialization failed;";
        return false;
    }
    size_t size{};
    in >> size;
    if (!in.Check()) {
        return false;
    }
    ubseMemAddrBorrowImportObj.exportObmmInfo.resize(size);
    for (size_t i = 0; i < size; ++i) {
        if (!UbseMemObmmInfoDeserialization(in, ubseMemAddrBorrowImportObj.exportObmmInfo[i])) {
            UBSE_LOG_ERROR << "UbseMemAddrBorrowImportObjDeserialization failed;";
            return false;
        }
    }
    if (!UbseMemImportStatusDeserialization(in, ubseMemAddrBorrowImportObj.status)) {
        UBSE_LOG_ERROR << "UbseMemAddrBorrowImportObjDeserialization failed;";
        return false;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemAddrBorrowImportObj during deserialization";
        return false;
    }
    return true;
}

void UbseMemAttachResourceShareAttrSerialization(UbseSerialization& out, const UbseMemAttachResourceShareAttr& data)
{
    out << data.uid << data.gid << data.size << data.owner.uid << data.owner.gid << data.owner.pid << data.owner.mode;
}

bool UbseMemAttachResourceShareAttrDeserialization(UbseDeSerialization& in, UbseMemAttachResourceShareAttr& data)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemAttachResourceShareAttr during deserialization";
        return false;
    }
    in >> data.uid >> data.gid >> data.size >> data.owner.uid >> data.owner.gid >> data.owner.pid >> data.owner.mode;
    return true;
}

void UbseMemBorrowExportBaseObjSerialization(UbseSerialization& out, const UbseMemBorrowExportBaseObj& data)
{
    UbseMemAlgoResultSerialization(out, data.algoResult);
    UbseMemExportStatusSerialization(out, data.status);
}

bool UbseMemBorrowExportBaseObjDeserialization(UbseDeSerialization& in, UbseMemBorrowExportBaseObj& data)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemBorrowExportBaseObj during deserialization";
        return false;
    }
    if (!UbseMemAlgoResultDeserialization(in, data.algoResult)) {
        UBSE_LOG_ERROR << "UbseMemAlgoResultDeserialization failed;";
        return false;
    }
    if (!UbseMemExportStatusDeserialization(in, data.status)) {
        UBSE_LOG_ERROR << "UbseMemExportStatusDeserialization failed;";
        return false;
    }
    return true;
}

void UbseNodeInfoSerialization(UbseSerialization& out, const UbseNodeInfo& data)
{
    out << data.index << data.nodeId << data.hostName << enum_v(data.status);
}

bool UbseNodeInfoDeserialization(UbseDeSerialization& in, UbseNodeInfo& data)
{
    in >> data.index >> data.nodeId >> data.hostName >> enum_v(data.status);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseNodeInfo during deserialization";
        return false;
    }
    return true;
}

void UbseShmRegionDescSerialization(UbseSerialization& out, const UbseShmRegionDesc& data)
{
    out << data.nodeNum;
    out << ubse::serial::array_len_insert(data.nodelist.size());
    for (auto& item : data.nodelist) {
        UbseNodeInfoSerialization(out, item);
    }
}

bool UbseShmRegionDescDeserialization(UbseDeSerialization& in, UbseShmRegionDesc& data)
{
    in >> data.nodeNum;
    uint64_t vectorSize;
    in >> ubse::serial::array_len_capture(vectorSize);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Check failed;";
        return false;
    }
    if (vectorSize > MAX_NODE_LIST_SIZE) {
        UBSE_LOG_ERROR << "Invalid vectorSize during deserialization, size=" << vectorSize;
        return false;
    }
    for (size_t i = 0; i < vectorSize; i++) {
        UbseNodeInfo nodeInfo;
        if (!UbseNodeInfoDeserialization(in, nodeInfo)) {
            UBSE_LOG_ERROR << "UbseNodeInfoDeserialization failed;";
            return false;
        }
        data.nodelist.push_back(nodeInfo);
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseShmRegionDesc during deserialization";
        return false;
    }
    return true;
}
inline void UbseUdsInfoSerialize(UbseSerialization& out, const UbseUdsInfo& data)
{
    out << data.uid << data.gid << data.pid;
}

inline void UbseNodeInfoSerialize(UbseSerialization& out, const UbseNodeInfo& data)
{
    out << data.index << data.nodeId << data.hostName << enum_v(data.status);
}
inline void UbseShmRegionDescSerialize(UbseSerialization& out, const UbseShmRegionDesc& data)
{
    out << data.nodeNum;
    out << ubse::serial::array_len_insert(data.nodelist.size());
    for (auto& item : data.nodelist) {
        UbseNodeInfoSerialize(out, item);
    }
}

inline void UbseShmProvierSerialize(UbseSerialization& out, const std::vector<std::string>& providerList)
{
    out << ubse::serial::array_len_insert(providerList.size());
    for (auto& item : providerList) {
        out << item;
    }
}

inline void SerializeWithAffinity(UbseSerialization& out, const UbseMemShmAffinitySocketInfo& data)
{
    out << data.affinitySocketId << data.createReqNodeId << data.enableCreateWithAffinity;
}

bool UbseMemShareBorrowReqSerialization(UbseSerialization& out, const UbseMemShareBorrowReq& data)
{
    UbseMemBaseBorrowReqSerialize(out, data);
    out << data.baseNodeId << data.size << enum_v(data.distance);
    UbseShmRegionDescSerialize(out, data.shmRegion);

    UbseShmProvierSerialize(out, data.providerList);
    SerializeUsrInfo(out, data.usrInfo, UBSE_MAX_USR_INFO_LEN);

    out << data.shmAnonymous;

    SerializeWithAffinity(out, data.withAffinity);
    out << data.lenderInfo.lender_size << data.lenderInfo.nodeId << data.lenderInfo.socketId << data.lenderInfo.numaId
        << data.lenderInfo.portId;
    return out.Check();
}
inline bool UbseUdsInfoDeserialize(UbseDeSerialization& in, UbseUdsInfo& data)
{
    in >> data.uid >> data.gid >> data.pid;
    return in.Check();
}

inline bool UbseNodeInfoDeserialize(UbseDeSerialization& in, UbseNodeInfo& data)
{
    in >> data.index >> data.nodeId >> data.hostName >> enum_v(data.status);
    return in.Check();
}
inline bool UbseShmRegionDescDeserialize(UbseDeSerialization& in, UbseShmRegionDesc& data)
{
    in >> data.nodeNum;
    uint64_t vectorSize;
    in >> ubse::serial::array_len_capture(vectorSize);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Check failed;";
        return false;
    }
    if (vectorSize > MAX_NODE_LIST_SIZE) {
        UBSE_LOG_ERROR << "Invalid vectorSize during deserialization, size=" << vectorSize;
        return false;
    }
    for (size_t i = 0; i < vectorSize; i++) {
        UbseNodeInfo nodeInfo;
        if (!UbseNodeInfoDeserialize(in, nodeInfo)) {
            UBSE_LOG_ERROR << "UbseNodeInfoDeserialize failed;";
            return false;
        }
        data.nodelist.push_back(nodeInfo);
    }
    return true;
}

inline bool UbseShmProvierDeSerialize(UbseDeSerialization& in, std::vector<std::string>& providerList)
{
    uint64_t vectorSize;
    in >> ubse::serial::array_len_capture(vectorSize);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Check failed;";
        return false;
    }
    if (vectorSize > MAX_NODE_LIST_SIZE) {
        UBSE_LOG_ERROR << "Invalid vectorSize during deserialization, size=" << vectorSize;
        return false;
    }
    for (auto i = 0; i < vectorSize; i++) {
        std::string nodeId;
        in >> nodeId;
        if (!in.Check()) {
            UBSE_LOG_ERROR << "Check failed;";
            return false;
        }
        providerList.push_back(nodeId);
    }
    return in.Check();
}

inline bool UbseWithAffinityDeserialize(UbseDeSerialization& in, UbseMemShmAffinitySocketInfo& withAffinity)
{
    in >> withAffinity.affinitySocketId;
    in >> withAffinity.createReqNodeId;
    in >> withAffinity.enableCreateWithAffinity;
    return in.Check();
}

inline bool UbseLenderDeserialize(UbseDeSerialization& in, std::vector<UbseNumaLocation>& lenderLocs,
                                  std::vector<uint64_t>& lenderSizes)
{
    size_t lenderLocsSize{};
    in >> lenderLocsSize;
    if (lenderLocsSize > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid lenderLocsSize during deserialization";
        return false;
    }
    lenderLocs.resize(lenderLocsSize);
    for (size_t i = 0; i < lenderLocsSize; ++i) {
        if (!UbseNumaLocationDeserialization(in, lenderLocs[i])) {
            UBSE_LOG_ERROR << "UbseNumaLocationDeserialization failed;";
            return false;
        }
    }
    size_t lenderSizesSize{};
    in >> lenderSizesSize;
    if (lenderSizesSize > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid lenderSizesSize during deserialization";
        return false;
    }
    lenderSizes.resize(lenderSizesSize);
    for (size_t i = 0; i < lenderSizesSize; ++i) {
        in >> lenderSizes[i];
    }
    return in.Check();
}

bool UbseMemShareBorrowReqDeserialization(UbseDeSerialization& in, UbseMemShareBorrowReq& data)
{
    UbseMemBaseBorrowReqDeserialize(in, data);
    in >> data.baseNodeId >> data.size >> enum_v(data.distance);
    // 读取 udsInfo 和 shmRegion
    if (!UbseShmRegionDescDeserialize(in, data.shmRegion)) {
        UBSE_LOG_ERROR << "Failed to check UbseMemShareBorrowReq during deserialization";
        return false;
    }
    if (!UbseShmProvierDeSerialize(in, data.providerList)) {
        UBSE_LOG_ERROR << "Failed to check UbseMemShareBorrowReq during deserialization";
        return false;
    }
    // 读取usrInfo
    if (!UbseUsrInfoDeserialize(in, data.usrInfo, UBSE_MAX_USR_INFO_LEN)) {
        UBSE_LOG_ERROR << "Failed to check UbseMemShareBorrowReq during deserialization";
        return false;
    }

    // 读取 flag
    in >> data.shmAnonymous;
    // 读取withAffinity
    if (!UbseWithAffinityDeserialize(in, data.withAffinity)) {
        UBSE_LOG_ERROR << "Failed to check UbseMemShareBorrowReq during deserialization";
        return false;
    }
    in >> data.lenderInfo.lender_size >> data.lenderInfo.nodeId >> data.lenderInfo.socketId >> data.lenderInfo.numaId >>
        data.lenderInfo.portId;
    return in.Check();
}

bool UbseMemShareAttachReqSerialization(UbseSerialization& out, const UbseMemShareAttachReq& data)
{
    UbseMemBaseBorrowReqSerialize(out, data);
    out << data.importNodeId << data.size;
    UbseFdOwnerSerialization(out, data.owner);
    return out.Check();
}

bool UbseMemShareAttachReqDeserialization(UbseDeSerialization& in, UbseMemShareAttachReq& data)
{
    UbseMemBaseBorrowReqDeserialize(in, data);
    in >> data.importNodeId >> data.size;
    return UbseFdOwnerDeserialization(in, data.owner);
}

bool UbseMemShareDetachReqSerialization(UbseSerialization& out, UbseMemShareDetachReq& data)
{
    UbseMemBaseBorrowReqSerialize(out, data);
    out << data.unImportNodeId;
    return out.Check();
}

bool UbseMemShareDetachReqDeserialization(UbseDeSerialization& in, UbseMemShareDetachReq& data)
{
    UbseMemBaseBorrowReqDeserialize(in, data);
    in >> data.unImportNodeId;
    return in.Check();
}

bool UbseMemShareBorrowExportObjSerialization(UbseSerialization& out, const UbseMemShareBorrowExportObj& data)
{
    out << data.errorCode;
    UbseMemShareBorrowReqSerialization(out, data.req);
    UbseMemReturnReqSerialize(out, data.returnReq);
    UbseMemBorrowExportBaseObjSerialization(out, data);
    return out.Check();
}

bool UbseMemShareBorrowExportObjDeserialization(UbseDeSerialization& in, UbseMemShareBorrowExportObj& data)
{
    in >> data.errorCode;
    if (!UbseMemShareBorrowReqDeserialization(in, data.req)) {
        UBSE_LOG_ERROR << "Failed to UbseMemShareBorrowReqDeserialization";
        return false;
    }
    if (!UbseMemReturnReqDeserialize(in, data.returnReq)) {
        UBSE_LOG_ERROR << "Failed to UbseMemReturnReqDeserialize";
        return false;
    }
    if (!UbseMemBorrowExportBaseObjDeserialization(in, data)) {
        UBSE_LOG_ERROR << "Failed to UbseMemBorrowExportBaseObjDeserialization";
        return false;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemShareBorrowExportObj during deserialization";
        return false;
    }
    return true;
}

void UbseMemBorrowImportBaseObjSerialization(UbseSerialization& out, const UbseMemBorrowImportBaseObj& data)
{
    out << data.errorCode;
    UbseMemAlgoResultSerialization(out, data.algoResult);
    out << ubse::serial::array_len_insert(data.exportObmmInfo.size());
    for (const auto& item : data.exportObmmInfo) {
        UbseMemObmmInfoSerialization(out, item);
    }
    UbseMemImportStatusSerialization(out, data.status);
}

bool UbseMemBorrowImportBaseObjDeserialization(UbseDeSerialization& in, UbseMemBorrowImportBaseObj& data)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemBorrowImportBaseObj during deserialization";
        return false;
    }
    in >> data.errorCode;
    if (!UbseMemAlgoResultDeserialization(in, data.algoResult)) {
        UBSE_LOG_ERROR << "UbseMemAlgoResultDeserialization failed;";
        return false;
    }
    uint64_t obmmInfoSize;
    in >> array_len_capture(obmmInfoSize);
    if (obmmInfoSize > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid obmmInfoSize during deserialization";
        return false;
    }
    data.exportObmmInfo.resize(obmmInfoSize);
    for (size_t i = 0; i < obmmInfoSize; ++i) {
        if (!UbseMemObmmInfoDeserialization(in, data.exportObmmInfo[i])) {
            UBSE_LOG_ERROR << "UbseMemObmmInfoDeserialization failed;";
            return false;
        }
    }
    if (!UbseMemImportStatusDeserialization(in, data.status)) {
        UBSE_LOG_ERROR << "UbseMemImportStatusDeserialization failed;";
        return false;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemBorrowImportBaseObj during deserialization";
        return false;
    }
    return true;
}

bool UbseMemShareBorrowImportObjSerialization(UbseSerialization& out, const UbseMemShareBorrowImportObj& data)
{
    out << data.realExe << data.importNodeId;
    UbseMemAttachResourceShareAttrSerialization(out, data.shareAttr);
    UbseMemShareBorrowReqSerialization(out, data.req);
    UbseMemReturnReqSerialize(out, data.returnReq);
    UbseMemBorrowImportBaseObjSerialization(out, data);
    return out.Check();
}

bool UbseMemShareBorrowImportObjDeserialization(UbseDeSerialization& in, UbseMemShareBorrowImportObj& data)
{
    in >> data.realExe >> data.importNodeId;
    if (!UbseMemAttachResourceShareAttrDeserialization(in, data.shareAttr)) {
        UBSE_LOG_ERROR << "UbseMemAttachResourceShareAttrDeserialization failed;";
        return false;
    }
    if (!UbseMemShareBorrowReqDeserialization(in, data.req)) {
        UBSE_LOG_ERROR << "UbseMemShareBorrowReqDeserialization failed;";
        return false;
    }
    if (!UbseMemReturnReqDeserialize(in, data.returnReq)) {
        UBSE_LOG_ERROR << "Failed to UbseMemReturnReqDeserialize";
        return false;
    }
    if (!UbseMemBorrowImportBaseObjDeserialization(in, data)) {
        UBSE_LOG_ERROR << "UbseMemBorrowImportBaseObjDeserialization failed;";
        return false;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemShareBorrowImportObj during deserialization";
        return false;
    }
    return true;
}

void UbseMemFdImportObjMapSerialization(UbseSerialization& out, const UbseMemFdImportObjMap& data)
{
    out << (right_v<size_t>(data.size()));
    for (const auto& kv : data) {
        out << kv.first;
        UbseMemFdBorrowImportObjSerialization(out, kv.second);
    }
}

bool UbseMemFdImportObjMapDeserialization(UbseDeSerialization& in, UbseMemFdImportObjMap& data)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemFdImportObjMap during deserialization";
        return false;
    }
    size_t size{};
    in >> size;
    if (size > MAX_DEBT_MAP_SIZE) {
        UBSE_LOG_ERROR << "Invalid debtSize during deserialization, size=" << size;
        return false;
    }
    data.reserve(size);
    for (size_t i = 0; i < size; i++) {
        std::string key{};
        in >> key;
        UbseMemFdBorrowImportObj importObj{};
        if (!UbseMemFdBorrowImportObjDeserialization(in, importObj)) {
            return false;
        }
        data[key] = importObj;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemFdImportObjMap during deserialization";
        return false;
    }
    return true;
}

void UbseMemFdExportObjMapSerialization(UbseSerialization& out, const UbseMemFdExportObjMap& data)
{
    out << (right_v<size_t>(data.size()));
    for (const auto& kv : data) {
        out << kv.first;
        UbseMemFdBorrowExportObjSerialization(out, kv.second);
    }
}

bool UbseMemFdExportObjMapDeserialization(UbseDeSerialization& in, UbseMemFdExportObjMap& data)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check FdExportObjMap during deserialization";
        return false;
    }
    size_t size{};
    in >> size;
    if (size > MAX_DEBT_MAP_SIZE) {
        UBSE_LOG_ERROR << "Invalid debtSize during deserialization, size=" << size;
        return false;
    }
    data.reserve(size);
    for (size_t i = 0; i < size; i++) {
        std::string key{};
        in >> key;
        UbseMemFdBorrowExportObj exportObj{};
        if (!UbseMemFdBorrowExportObjDeserialization(in, exportObj)) {
            UBSE_LOG_ERROR << "UbseMemFdBorrowExportObjDeserialization failed;";
            return false;
        }
        data[key] = exportObj;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check FdExportObjMap during deserialization";
        return false;
    }
    return true;
}

void UbseMemNumaImportObjMapSerialization(UbseSerialization& out, const UbseMemNumaImportObjMap& data)
{
    out << (right_v<size_t>(data.size()));
    for (const auto& kv : data) {
        out << kv.first;
        UbseMemNumaBorrowImportObjSerialization(out, kv.second);
    }
}

bool UbseMemNumaImportObjMapDeserialization(UbseDeSerialization& in, UbseMemNumaImportObjMap& data)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaImportObjMap during deserialization";
        return false;
    }
    size_t size{};
    in >> size;
    if (size > MAX_DEBT_MAP_SIZE) {
        UBSE_LOG_ERROR << "Invalid debtSize during deserialization, size=" << size;
        return false;
    }
    data.reserve(size);
    for (size_t i = 0; i < size; i++) {
        std::string key{};
        in >> key;
        UbseMemNumaBorrowImportObj importObj{};
        if (!UbseMemNumaBorrowImportObjDeserialization(in, importObj)) {
            UBSE_LOG_ERROR << "UbseMemNumaBorrowImportObjDeserialization failed;";
            return false;
        }
        data[key] = importObj;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaImportObjMap during deserialization";
        return false;
    }
    return true;
}

void UbseMemNumaExportObjMapSerialization(UbseSerialization& out, const UbseMemNumaExportObjMap& data)
{
    out << (right_v<size_t>(data.size()));
    for (const auto& kv : data) {
        out << kv.first;
        UbseMemNumaBorrowExportObjSerialization(out, kv.second);
    }
}

bool UbseMemNumaExportObjMapDeserialization(UbseDeSerialization& in, UbseMemNumaExportObjMap& data)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaExportObjMap during deserialization";
        return false;
    }
    size_t size{};
    in >> size;
    if (size > MAX_DEBT_MAP_SIZE) {
        UBSE_LOG_ERROR << "Invalid debtSize during deserialization, size=" << size;
        return false;
    }
    data.reserve(size);
    for (size_t i = 0; i < size; i++) {
        std::string key{};
        in >> key;
        UbseMemNumaBorrowExportObj exportObj{};
        if (!UbseMemNumaBorrowExportObjDeserialization(in, exportObj)) {
            UBSE_LOG_ERROR << "UbseMemNumaBorrowExportObjDeserialization failed;";
            return false;
        }
        data[key] = exportObj;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaExportObjMap during deserialization";
        return false;
    }
    return true;
}

void UbseMemShareImportObjMapSerialization(UbseSerialization& out, const UbseMemShareImportObjMap& data)
{
    out << (right_v<size_t>(data.size()));
    for (const auto& kv : data) {
        out << kv.first;
        UbseMemShareBorrowImportObjSerialization(out, kv.second);
    }
}

bool UbseMemShareImportObjMapDeserialization(UbseDeSerialization& in, UbseMemShareImportObjMap& data)
{
    size_t size{};
    in >> size;
    if (size > MAX_DEBT_MAP_SIZE) {
        UBSE_LOG_ERROR << "Invalid debtSize during deserialization, size=" << size;
        return false;
    }
    data.reserve(size);
    for (size_t i = 0; i < size; i++) {
        std::string key{};
        in >> key;
        UbseMemShareBorrowImportObj importObj{};
        if (!UbseMemShareBorrowImportObjDeserialization(in, importObj)) {
            UBSE_LOG_ERROR << "UbseMemShareBorrowImportObjDeserialization failed;";
            return false;
        }
        data[key] = importObj;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemShareImportObjMap during deserialization";
        return false;
    }
    return true;
}

void UbseMemShareExportObjMapSerialization(UbseSerialization& out, const UbseMemShareExportObjMap& data)
{
    out << (right_v<size_t>(data.size()));
    for (const auto& kv : data) {
        out << kv.first;
        UbseMemShareBorrowExportObjSerialization(out, kv.second);
    }
}

bool UbseMemShareExportObjMapDeserialization(UbseDeSerialization& in, UbseMemShareExportObjMap& data)
{
    size_t size{};
    in >> size;
    if (size > MAX_DEBT_MAP_SIZE) {
        UBSE_LOG_ERROR << "Invalid debtSize during deserialization, size=" << size;
        return false;
    }
    data.reserve(size);
    for (size_t i = 0; i < size; i++) {
        std::string key{};
        in >> key;
        UbseMemShareBorrowExportObj exportObj{};
        if (!UbseMemShareBorrowExportObjDeserialization(in, exportObj)) {
            UBSE_LOG_ERROR << "UbseMemShareBorrowExportObjDeserialization failed;";
            return false;
        }
        data[key] = exportObj;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemShareExportObj during deserialization";
        return false;
    }
    return true;
}

void UbseMemAddrImportObjMapSerialization(UbseSerialization& out, const UbseMemAddrImportObjMap& data)
{
    out << (right_v<size_t>(data.size()));
    for (const auto& kv : data) {
        out << kv.first;
        UbseMemAddrBorrowImportObjSerialization(out, kv.second);
    }
}

bool UbseMemAddrImportObjMapDeserialization(UbseDeSerialization& in, UbseMemAddrImportObjMap& data)
{
    size_t size{};
    in >> size;
    if (size > MAX_DEBT_MAP_SIZE) {
        UBSE_LOG_ERROR << "Invalid debtSize during deserialization, size=" << size;
        return false;
    }
    data.reserve(size);
    for (size_t i = 0; i < size; i++) {
        std::string key{};
        in >> key;
        UbseMemAddrBorrowImportObj importObj{};
        if (!UbseMemAddrBorrowImportObjDeserialization(in, importObj)) {
            UBSE_LOG_ERROR << "UbseMemAddrBorrowImportObjDeserialization failed;";
            return false;
        }
        data[key] = importObj;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check  UbseMemAddrImportObjMap during deserialization";
        return false;
    }
    return true;
}

void UbseMemAddrExportObjMapSerialization(UbseSerialization& out, const UbseMemAddrExportObjMap& data)
{
    out << (right_v<size_t>(data.size()));
    for (const auto& kv : data) {
        out << kv.first;
        UbseMemAddrBorrowExportObjSerialization(out, kv.second);
    }
}

bool UbseMemAddrExportObjMapDeserialization(UbseDeSerialization& in, UbseMemAddrExportObjMap& data)
{
    size_t size{};
    in >> size;
    if (size > MAX_DEBT_MAP_SIZE) {
        UBSE_LOG_ERROR << "Invalid debtSize during deserialization, size=" << size;
        return false;
    }
    data.reserve(size);
    for (size_t i = 0; i < size; i++) {
        std::string key{};
        in >> key;
        UbseMemAddrBorrowExportObj exportObj{};
        if (!UbseMemAddrBorrowExportObjDeserialization(in, exportObj)) {
            UBSE_LOG_ERROR << "UbseMemAddrBorrowImportObjDeserialization failed;";
            return false;
        }
        data[key] = exportObj;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check  UbseMemAddrExportObjMap during deserialization";
        return false;
    }
    return true;
}

void NodeMemDebtInfoSerialization(UbseSerialization& out, const NodeMemDebtInfo& data)
{
    UbseMemFdImportObjMapSerialization(out, data.fdImportObjMap);
    UbseMemFdExportObjMapSerialization(out, data.fdExportObjMap);
    UbseMemNumaImportObjMapSerialization(out, data.numaImportObjMap);
    UbseMemNumaExportObjMapSerialization(out, data.numaExportObjMap);
    UbseMemShareImportObjMapSerialization(out, data.shareImportObjMap);
    UbseMemShareExportObjMapSerialization(out, data.shareExportObjMap);
    UbseMemAddrImportObjMapSerialization(out, data.addrImportObjMap);
    UbseMemAddrExportObjMapSerialization(out, data.addrExportObjMap);
}

bool NodeMemDebtInfoDeserialization(UbseDeSerialization& in, NodeMemDebtInfo& data)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check  NodeMemDebtInfo during deserialization";
        return false;
    }
    if (!UbseMemFdImportObjMapDeserialization(in, data.fdImportObjMap)) {
        UBSE_LOG_ERROR << "UbseMemFdImportObjMapDeserialization failed;";
        return false;
    }
    if (!UbseMemFdExportObjMapDeserialization(in, data.fdExportObjMap)) {
        UBSE_LOG_ERROR << "UbseMemFdExportObjMapDeserialization failed;";
        return false;
    }
    if (!UbseMemNumaImportObjMapDeserialization(in, data.numaImportObjMap)) {
        UBSE_LOG_ERROR << "UbseMemNumaImportObjMapDeserialization failed;";
        return false;
    }
    if (!UbseMemNumaExportObjMapDeserialization(in, data.numaExportObjMap)) {
        UBSE_LOG_ERROR << "UbseMemNumaExportObjMapDeserialization failed;";
        return false;
    }
    if (!UbseMemShareImportObjMapDeserialization(in, data.shareImportObjMap)) {
        UBSE_LOG_ERROR << "UbseMemShareImportObjMapDeserialization failed;";
        return false;
    }
    if (!UbseMemShareExportObjMapDeserialization(in, data.shareExportObjMap)) {
        UBSE_LOG_ERROR << "UbseMemShareExportObjMapDeserialization failed;";
        return false;
    }
    if (!UbseMemAddrImportObjMapDeserialization(in, data.addrImportObjMap)) {
        UBSE_LOG_ERROR << "UbseMemAddrImportObjMapDeserialization failed;";
        return false;
    }
    if (!UbseMemAddrExportObjMapDeserialization(in, data.addrExportObjMap)) {
        UBSE_LOG_ERROR << "UbseMemAddrExportObjMapDeserializatifailed;";
        return false;
    }
    return true;
}

bool UbseMemFdPermissionReqSerialize(UbseSerialization& out, const UbseMemFdPermissionReq& req)
{
    UbseMemBaseBorrowReqSerialize(out, req);
    serial::UbseFdOwnerSerialization(out, req.fdOwner);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize UbseMemFdPermissionReq.";
        return false;
    }
    return true;
}

bool UbseMemFdPermissionReqDeserialize(UbseDeSerialization& in, UbseMemFdPermissionReq& req)
{
    UbseMemBaseBorrowReqDeserialize(in, req);
    serial::UbseFdOwnerDeserialization(in, req.fdOwner);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize UbseMemFdPermissionReq.";
        return false;
    }
    return true;
}

bool UbseMemReturnReqSerialize(UbseSerialization& out, const UbseMemReturnReq& data)
{
    UbseMemBaseBorrowReqSerialize(out, data);
    out << data.uid << data.gid << data.importNodeId;
    return out.Check();
}

bool UbseMemReturnReqDeserialize(UbseDeSerialization& in, UbseMemReturnReq& data)
{
    UbseMemBaseBorrowReqDeserialize(in, data);
    in >> data.uid >> data.gid >> data.importNodeId;
    return in.Check();
}

bool UbseMemOperationRespSerialize(UbseSerialization& out, UbseMemOperationResp& data)
{
    out << data.name << data.requestNodeId << data.errorCode << data.errMsg << data.realSize;
    out << ubse::serial::array_len_insert(data.memIdList.size());
    for (const auto& item : data.memIdList) {
        out << item;
    }
    out << data.remoteNumaId << data.requestId;
    return out.Check();
}

bool UbseMemOperationRespDeserialize(UbseDeSerialization& in, UbseMemOperationResp& data)
{
    in >> data.name >> data.requestNodeId >> data.errorCode >> data.errMsg >> data.realSize;
    uint64_t vectorSize;
    in >> ubse::serial::array_len_capture(vectorSize);
    if (!in.Check()) {
        return false;
    }
    if (vectorSize > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid vectorSize during deserialization, size=" << vectorSize;
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
    in >> data.remoteNumaId >> data.requestId;
    return in.Check();
}

bool ShareHandleInfoVecSerialize(UbseSerialization& out, const def::ShareHandleInfoVec& data)
{
    out << ubse::serial::array_len_insert(data.size());
    for (const auto& item : data) {
        out << item.name << item.memIds << item.udsInfo.uid << item.udsInfo.gid << item.udsInfo.pid
            << item.udsInfo.username;
    }
    return out.Check();
}

bool ShareHandleInfoVecDeserialize(UbseDeSerialization& in, def::ShareHandleInfoVec& data)
{
    uint64_t vectorSize;
    in >> ubse::serial::array_len_capture(vectorSize);
    if (!in.Check()) {
        return false;
    }
    if (vectorSize > MAX_DEBT_MAP_SIZE) {
        UBSE_LOG_ERROR << "Invalid vectorSize during deserialization, size=" << vectorSize;
        return false;
    }
    for (size_t i = 0; i < vectorSize; i++) {
        def::ShareHandleInfo item;
        in >> item.name >> item.memIds >> item.udsInfo.uid >> item.udsInfo.gid >> item.udsInfo.pid >>
            item.udsInfo.username;
        if (!in.Check()) {
            return false;
        }
        data.push_back(item);
    }
    return in.Check();
}

bool NumaHandleInfoVecSerialize(UbseSerialization& out, const def::NumaHandleInfoVec& data)
{
    out << ubse::serial::array_len_insert(data.size());
    for (const auto& item : data) {
        out << item.name << item.numaIds << item.udsInfo.uid << item.udsInfo.gid << item.udsInfo.pid
            << item.udsInfo.username;
    }
    return out.Check();
}

bool NumaHandleInfoVecDeserialize(UbseDeSerialization& in, def::NumaHandleInfoVec& data)
{
    uint64_t vectorSize;
    in >> ubse::serial::array_len_capture(vectorSize);
    if (!in.Check()) {
        return false;
    }
    if (vectorSize > MAX_DEBT_MAP_SIZE) {
        UBSE_LOG_ERROR << "Invalid vectorSize during deserialization, size=" << vectorSize;
        return false;
    }
    for (size_t i = 0; i < vectorSize; i++) {
        def::NumaHandleInfo item;
        in >> item.name >> item.numaIds >> item.udsInfo.uid >> item.udsInfo.gid >> item.udsInfo.pid >>
            item.udsInfo.username;
        if (!in.Check()) {
            return false;
        }
        data.push_back(item);
    }
    return in.Check();
}

} // namespace ubse::mem::serial
