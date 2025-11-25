/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
*/

#include "ubse_mem_controller_serial.h"

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_mem_obj.h"

using namespace ubse::mem::obj;
using namespace ubse::common;
using namespace ubse::log;

namespace ubse::mem::serial {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID);
constexpr int MAX_SIZE = 800; // 校验obmm数组最大值, 800=100G(max size) / 128M(min block size)
void UbseMemDebtNumaInfoSerialization(UbseSerialization &out, const UbseMemDebtNumaInfo &debtNumaInfo)
{
    out << debtNumaInfo.nodeId << debtNumaInfo.socketId << debtNumaInfo.numaId << debtNumaInfo.size;
    return;
}

UbseResult UbseMemDebtNumaInfoDeserialization(UbseDeSerialization &in, UbseMemDebtNumaInfo &debtNumaInfo)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemDebtNumaInfo during deserialization";
        return UBSE_ERROR;
    }
    in >> debtNumaInfo.nodeId >> debtNumaInfo.socketId >> debtNumaInfo.numaId >> debtNumaInfo.size;
    return UBSE_OK;
}

void UbseMemAlgoResultSerialization(UbseSerialization &out, const UbseMemAlgoResult &algoResult)
{
    out << (right_v<size_t>(algoResult.exportNumaInfos.size()));
    for (auto &exportNumaInfo : algoResult.exportNumaInfos) {
        UbseMemDebtNumaInfoSerialization(out, exportNumaInfo);
    }
    out << (right_v<size_t>(algoResult.importNumaInfos.size()));
    for (auto &importNumaInfo : algoResult.importNumaInfos) {
        UbseMemDebtNumaInfoSerialization(out, importNumaInfo);
    }
    out << algoResult.attachSocketId;
    return;
}

UbseResult UbseMemAlgoResultDeserialization(UbseDeSerialization &in, UbseMemAlgoResult &algoResult)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemAlgoResult during deserialization";
        return UBSE_ERROR;
    }
    size_t exportNumaInfoSize{};
    in >> exportNumaInfoSize;
    if (exportNumaInfoSize > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid exportNumaInfoSize during deserialization";
        return UBSE_ERROR;
    }
    algoResult.exportNumaInfos.resize(exportNumaInfoSize);
    for (size_t i = 0; i < exportNumaInfoSize; ++i) {
        if (UbseMemDebtNumaInfoDeserialization(in, algoResult.exportNumaInfos[i]) != UBSE_OK) {
            return UBSE_ERROR;
        }
    }
    size_t importNumaInfoSize{};
    in >> importNumaInfoSize;
    if (importNumaInfoSize > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid importNumaInfoSize during deserialization";
        return UBSE_ERROR;
    }
    algoResult.importNumaInfos.resize(importNumaInfoSize);
    for (size_t i = 0; i < importNumaInfoSize; ++i) {
        if (UbseMemDebtNumaInfoDeserialization(in, algoResult.importNumaInfos[i]) != UBSE_OK) {
            return UBSE_ERROR;
        }
    }
    in >> algoResult.attachSocketId;
    return UBSE_OK;
}

void UbseMemObmmMemDescSerialization(UbseSerialization &out, const ubse_mem_obmm_mem_desc &ubseMemObmmMemDesc)
{
    addr_len<uint8_t> addrLen;
    addrLen.ptr = (unsigned char *)&ubseMemObmmMemDesc;
    addrLen.len = sizeof(ubseMemObmmMemDesc);
    out << addrLen;
    return;
}

UbseResult UbseMemObmmMemDescDeserialization(UbseDeSerialization &in, ubse_mem_obmm_mem_desc &ubseMemObmmMemDesc)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemObmmMemDesc during deserialization";
        return UBSE_ERROR;
    }
    addr_len<uint8_t> addrLen;
    addrLen.ptr = (unsigned char *)&ubseMemObmmMemDesc;
    addrLen.len = sizeof(ubseMemObmmMemDesc);
    in >> addrLen;
    return UBSE_OK;
}

void UbseMemObmmInfoSerialization(UbseSerialization &out, const UbseMemObmmInfo &ubseMemObmmInfo)
{
    out << ubseMemObmmInfo.memId;
    UbseMemObmmMemDescSerialization(out, ubseMemObmmInfo.desc);
    return;
}

UbseResult UbseMemObmmInfoDeserialization(UbseDeSerialization &in, UbseMemObmmInfo &ubseMemObmmInfo)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemObmmInfo during deserialization";
        return UBSE_ERROR;
    }
    in >> ubseMemObmmInfo.memId;
    UbseMemObmmMemDescDeserialization(in, ubseMemObmmInfo.desc);
    return UBSE_OK;
}

void UbseMemExportStatusSerialization(UbseSerialization &out, const UbseMemExportStatus &ubseMemExportStatus)
{
    out << ubseMemExportStatus.errCode << (right_v<size_t>(ubseMemExportStatus.exportObmmInfo.size()));
    for (auto &exportObmmInfo : ubseMemExportStatus.exportObmmInfo) {
        UbseMemObmmInfoSerialization(out, exportObmmInfo);
    }
    out << enum_v(ubseMemExportStatus.expectState) << enum_v(ubseMemExportStatus.state);
    return;
}

UbseResult UbseMemExportStatusDeserialization(UbseDeSerialization &in, UbseMemExportStatus &ubseMemExportStatus)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemExportStatus during deserialization";
        return UBSE_ERROR;
    }
    size_t exportObmmInfosize{};
    in >> ubseMemExportStatus.errCode >> exportObmmInfosize;
    if (exportObmmInfosize > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid exportObmmInfosize during deserialization";
        return UBSE_ERROR;
    }
    ubseMemExportStatus.exportObmmInfo.resize(exportObmmInfosize);
    for (size_t i = 0; i < exportObmmInfosize; ++i) {
        if (UbseMemObmmInfoDeserialization(in, ubseMemExportStatus.exportObmmInfo[i]) != UBSE_OK) {
            return UBSE_ERROR;
        }
    }
    in >> enum_v(ubseMemExportStatus.expectState) >> enum_v(ubseMemExportStatus.state);
    return UBSE_OK;
}

void UbseUdsInfoSerialization(UbseSerialization &out, const UbseUdsInfo &udsInfo)
{
    out << udsInfo.uid << udsInfo.gid << udsInfo.pid;
    return;
}

void UbseFdOwnerSerialization(UbseSerialization& out, const FdOwner& owner)
{
    out << owner.uid << owner.gid << owner.pid << owner.mode;
}
UbseResult UbseUdsInfoDeserialization(UbseDeSerialization &in, UbseUdsInfo &udsInfo)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseUdsInfo during deserialization";
        return UBSE_ERROR;
    }
    in >> udsInfo.uid >> udsInfo.gid >> udsInfo.pid;
    return UBSE_OK;
}
UbseResult UbseFdOwnerDeserialization(UbseDeSerialization& in, FdOwner& owner)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseUdsInfo during deserialization";
        return UBSE_ERROR;
    }
    in >> owner.uid >> owner.gid >> owner.pid >> owner.mode;
    return UBSE_OK;
}
void UbseNumaLocationSerialization(UbseSerialization &out, const UbseNumaLocation &ubseNumaLocation)
{
    out << ubseNumaLocation.nodeId << ubseNumaLocation.numaId;
    return;
}

UbseResult UbseNumaLocationDeserialization(UbseDeSerialization &in, UbseNumaLocation &ubseNumaLocation)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseNumaLocation during deserialization";
        return UBSE_ERROR;
    }
    in >> ubseNumaLocation.nodeId >> ubseNumaLocation.numaId;
    return UBSE_OK;
}

void UbseMemFdBorrowReqSerialization(UbseSerialization &out, const UbseMemFdBorrowReq &req)
{
    out << req.name << req.requestNodeId << req.importNodeId << req.size << enum_v(req.distance) << req.timestamp;
    UbseFdOwnerSerialization(out, req.owner);
    out << (right_v<size_t>(req.lenderLocs.size()));
    for (auto &lenderLoc : req.lenderLocs) {
        UbseNumaLocationSerialization(out, lenderLoc);
    }
    out << (right_v<size_t>(req.lenderSizes.size()));
    for (auto lenderSize : req.lenderSizes) {
        out << lenderSize;
    }
    return;
}

UbseResult UbseMemFdBorrowReqDeserialization(UbseDeSerialization &in, UbseMemFdBorrowReq &req)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemFdBorrowReq during deserialization";
        return UBSE_ERROR;
    }
    in >> req.name >> req.requestNodeId >> req.importNodeId >> req.size >> enum_v(req.distance) >> req.timestamp;
    UbseFdOwnerDeserialization(in, req.owner);
    size_t lenderLocsSize{};
    in >> lenderLocsSize;
    if (lenderLocsSize > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid lenderLocsSize during deserialization";
        return UBSE_ERROR;
    }
    req.lenderLocs.resize(lenderLocsSize);
    for (size_t i = 0; i < lenderLocsSize; ++i) {
        if (UbseNumaLocationDeserialization(in, req.lenderLocs[i]) != UBSE_OK) {
            return UBSE_ERROR;
        }
    }
    size_t lenderSizesSize{};
    in >> lenderSizesSize;
    if (lenderSizesSize > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid lenderSizesSize during deserialization";
        return UBSE_ERROR;
    }
    req.lenderSizes.resize(lenderSizesSize);
    for (size_t i = 0; i < lenderSizesSize; ++i) {
        in >> req.lenderSizes[i];
    }
    return UBSE_OK;
}

void UbseMemFdBorrowExportObjSerialization(UbseSerialization &out, const UbseMemFdBorrowExportObj &fdBorrowExportObj)
{
    out << fdBorrowExportObj.errorCode;
    UbseMemAlgoResultSerialization(out, fdBorrowExportObj.algoResult);
    UbseMemExportStatusSerialization(out, fdBorrowExportObj.status);
    UbseMemFdBorrowReqSerialization(out, fdBorrowExportObj.req);
    return;
}

UbseResult UbseMemFdBorrowExportObjDeserialization(UbseDeSerialization &in, UbseMemFdBorrowExportObj &fdBorrowExportObj)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemFdBorrowExportObj during deserialization";
        return UBSE_ERROR;
    }
    in >> fdBorrowExportObj.errorCode;
    if (UbseMemAlgoResultDeserialization(in, fdBorrowExportObj.algoResult) != UBSE_OK) {
        return UBSE_ERROR;
    }
    if (UbseMemExportStatusDeserialization(in, fdBorrowExportObj.status) != UBSE_OK) {
        return UBSE_ERROR;
    }
    if (UbseMemFdBorrowReqDeserialization(in, fdBorrowExportObj.req) != UBSE_OK) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseMemImportResultSerialization(UbseSerialization &out, const UbseMemImportResult &ubseMemImportResult)
{
    out << ubseMemImportResult.memId << ubseMemImportResult.numaId;
    return;
}

UbseResult UbseMemImportResultDeserialization(UbseDeSerialization &in, UbseMemImportResult &ubseMemImportResult)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemFdBorrowExportObj during deserialization";
        return UBSE_ERROR;
    }
    in >> ubseMemImportResult.memId >> ubseMemImportResult.numaId;
    return UBSE_OK;
}

void UbseMemImportStatusSerialization(UbseSerialization &out, const UbseMemImportStatus &ubseMemImportStatus)
{
    out << ubseMemImportStatus.errCode << ubseMemImportStatus.scna
        << (right_v<size_t>(ubseMemImportStatus.importResults.size()));
    for (auto &importResult : ubseMemImportStatus.importResults) {
        UbseMemImportResultSerialization(out, importResult);
    }

    out << (right_v<size_t>(ubseMemImportStatus.decoderResult.size()));
    for(auto &decoderResult : ubseMemImportStatus.decoderResult) {
        out << decoderResult.marId << decoderResult.hpa << decoderResult.handle;
    }
    out << enum_v(ubseMemImportStatus.expectState) << enum_v(ubseMemImportStatus.state);
    return;
}

UbseResult UbseMemImportStatusDeserialization(UbseDeSerialization &in, UbseMemImportStatus &ubseMemImportStatus)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemImportStatus during deserialization";
        return UBSE_ERROR;
    }
    size_t size{};
    in >> ubseMemImportStatus.errCode >> ubseMemImportStatus.scna >> size;
    if (size > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid UbseMemImportStatus during deserialization";
        return UBSE_ERROR;
    }
    ubseMemImportStatus.importResults.resize(size);
    for (size_t i = 0; i < size; ++i) {
        if (UbseMemImportResultDeserialization(in, ubseMemImportStatus.importResults[i]) != UBSE_OK) {
            return UBSE_ERROR;
        }
    }

    in >> size;
    ubseMemImportStatus.decoderResult.resize(size);
    for (size_t i = 0; i < size; ++i) {
        if (!in.Check()) {
            UBSE_LOG_ERROR << "Failed to check UbseMemFdBorrowExportObj during deserialization";
            return UBSE_ERROR;
        }
        in >> ubseMemImportStatus.decoderResult[i].marId >> ubseMemImportStatus.decoderResult[i].hpa >>
            ubseMemImportStatus.decoderResult[i].handle;
    }
    in >> enum_v(ubseMemImportStatus.expectState) >> enum_v(ubseMemImportStatus.state);
    return UBSE_OK;
}

void UbseMemFdBorrowImportObjSerialization(UbseSerialization &out,
                                           const UbseMemFdBorrowImportObj &ubseMemFdBorrowImportObj)
{
    out << ubseMemFdBorrowImportObj.errorCode;
    UbseMemAlgoResultSerialization(out, ubseMemFdBorrowImportObj.algoResult);
    out << (right_v<size_t>(ubseMemFdBorrowImportObj.exportObmmInfo.size()));
    for (auto exportObmmInfo : ubseMemFdBorrowImportObj.exportObmmInfo) {
        UbseMemObmmInfoSerialization(out, exportObmmInfo);
    }
    UbseMemImportStatusSerialization(out, ubseMemFdBorrowImportObj.status);
    UbseMemFdBorrowReqSerialization(out, ubseMemFdBorrowImportObj.req);
}

UbseResult UbseMemFdBorrowImportObjDeserialization(UbseDeSerialization &in,
                                                   UbseMemFdBorrowImportObj &ubseMemFdBorrowImportObj)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemFdBorrowImportObj during deserialization";
        return UBSE_ERROR;
    }
    in >> ubseMemFdBorrowImportObj.errorCode;
    if (UbseMemAlgoResultDeserialization(in, ubseMemFdBorrowImportObj.algoResult) != UBSE_OK) {
        return UBSE_ERROR;
    }
    size_t size{};
    in >> size;
    if (size > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid size during deserialization";
        return UBSE_ERROR;
    }
    ubseMemFdBorrowImportObj.exportObmmInfo.resize(size);
    for (size_t i = 0; i < size; ++i) {
        if (UbseMemObmmInfoDeserialization(in, ubseMemFdBorrowImportObj.exportObmmInfo[i]) != UBSE_OK) {
            return UBSE_ERROR;
        }
    }
    if (UbseMemImportStatusDeserialization(in, ubseMemFdBorrowImportObj.status) != UBSE_OK) {
        return UBSE_ERROR;
    }
    if (UbseMemFdBorrowReqDeserialization(in, ubseMemFdBorrowImportObj.req) != UBSE_OK) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseMemNumaBorrowReqSerialization(UbseSerialization &out, const UbseMemNumaBorrowReq &req)
{
    UbseMemFdBorrowReq fdBorrowReq;
    fdBorrowReq.name = req.name;
    fdBorrowReq.requestNodeId = req.requestNodeId;
    fdBorrowReq.importNodeId = req.importNodeId;
    fdBorrowReq.size = req.size;
    fdBorrowReq.distance = req.distance;
    fdBorrowReq.udsInfo = req.udsInfo;
    fdBorrowReq.lenderLocs = req.lenderLocs;
    fdBorrowReq.lenderSizes = req.lenderSizes;
    UbseMemFdBorrowReqSerialization(out, fdBorrowReq);
    out << req.srcSocket << req.srcNuma << req.highWatermark << req.lowWatermark;
    return;
}

UbseResult UbseMemNumaBorrowReqDeserialization(UbseDeSerialization &in, UbseMemNumaBorrowReq &req)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaBorrowReq during deserialization";
        return UBSE_ERROR;
    }
    UbseMemFdBorrowReq fdBorrowReq;
    if (UbseMemFdBorrowReqDeserialization(in, fdBorrowReq) != UBSE_OK) {
        return UBSE_ERROR;
    }
    req.name = fdBorrowReq.name;
    req.requestNodeId = fdBorrowReq.requestNodeId;
    req.importNodeId = fdBorrowReq.importNodeId;
    req.size = fdBorrowReq.size;
    req.distance = fdBorrowReq.distance;
    req.udsInfo = fdBorrowReq.udsInfo;
    req.lenderLocs = fdBorrowReq.lenderLocs;
    req.lenderSizes = fdBorrowReq.lenderSizes;
    in >> req.srcSocket >> req.srcNuma >> req.highWatermark >> req.lowWatermark;
    return UBSE_OK;
}

void UbseMemNumaBorrowExportObjSerialization(UbseSerialization &out,
                                             const UbseMemNumaBorrowExportObj &ubseMemNumaBorrowExportObj)
{
    out << ubseMemNumaBorrowExportObj.errorCode;
    UbseMemAlgoResultSerialization(out, ubseMemNumaBorrowExportObj.algoResult);
    UbseMemExportStatusSerialization(out, ubseMemNumaBorrowExportObj.status);
    UbseMemNumaBorrowReqSerialization(out, ubseMemNumaBorrowExportObj.req);
    return;
}

UbseResult UbseMemNumaBorrowExportObjDeserialization(UbseDeSerialization &in,
                                                     UbseMemNumaBorrowExportObj &ubseMemNumaBorrowExportObj)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaBorrowExportObj during deserialization";
        return UBSE_ERROR;
    }
    in >> ubseMemNumaBorrowExportObj.errorCode;
    if (UbseMemAlgoResultDeserialization(in, ubseMemNumaBorrowExportObj.algoResult) != UBSE_OK) {
        return UBSE_ERROR;
    }
    if (UbseMemExportStatusDeserialization(in, ubseMemNumaBorrowExportObj.status) != UBSE_OK) {
        return UBSE_ERROR;
    }
    if (UbseMemNumaBorrowReqDeserialization(in, ubseMemNumaBorrowExportObj.req) != UBSE_OK) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseMemNumaBorrowImportObjSerialization(UbseSerialization &out,
                                             const UbseMemNumaBorrowImportObj &ubseMemNumaBorrowImportObj)
{
    out << ubseMemNumaBorrowImportObj.errorCode;
    UbseMemAlgoResultSerialization(out, ubseMemNumaBorrowImportObj.algoResult);
    out << (right_v<size_t>(ubseMemNumaBorrowImportObj.exportObmmInfo.size()));
    for (auto &exportObmmInfo : ubseMemNumaBorrowImportObj.exportObmmInfo) {
        UbseMemObmmInfoSerialization(out, exportObmmInfo);
    }
    UbseMemImportStatusSerialization(out, ubseMemNumaBorrowImportObj.status);
    UbseMemNumaBorrowReqSerialization(out, ubseMemNumaBorrowImportObj.req);
    return;
}

UbseResult UbseMemNumaBorrowImportObjDeserialization(UbseDeSerialization &in,
                                                     UbseMemNumaBorrowImportObj &ubseMemNumaBorrowImportObj)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaBorrowImportObj during deserialization";
        return UBSE_ERROR;
    }
    in >> ubseMemNumaBorrowImportObj.errorCode;
    if (UbseMemAlgoResultDeserialization(in, ubseMemNumaBorrowImportObj.algoResult) != UBSE_OK) {
        return UBSE_ERROR;
    }
    size_t size{};
    in >> size;
    if (size > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid size during deserialization";
        return UBSE_ERROR;
    }
    ubseMemNumaBorrowImportObj.exportObmmInfo.resize(size);
    for (size_t i = 0; i < size; ++i) {
        if (UbseMemObmmInfoDeserialization(in, ubseMemNumaBorrowImportObj.exportObmmInfo[i]) != UBSE_OK) {
            return UBSE_ERROR;
        }
    }
    if (UbseMemImportStatusDeserialization(in, ubseMemNumaBorrowImportObj.status) != UBSE_OK) {
        return UBSE_ERROR;
    }
    if (UbseMemNumaBorrowReqDeserialization(in, ubseMemNumaBorrowImportObj.req) != UBSE_OK) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseMemAddrInfoSerialization(UbseSerialization &out, const UbseMemAddrInfo &ubseMemAddrInfo)
{
    out << ubseMemAddrInfo.addr << ubseMemAddrInfo.size;
    return;
}

UbseResult UbseMemAddrInfoDeserialization(UbseDeSerialization &in, UbseMemAddrInfo &ubseMemAddrInfo)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemAddrInfo during deserialization";
        return UBSE_ERROR;
    }
    in >> ubseMemAddrInfo.addr >> ubseMemAddrInfo.size;
    return UBSE_OK;
}

void UbseMemBorrowExportBaseObjSerialization(UbseSerialization &out, const UbseMemBorrowExportBaseObj &data)
{
    UbseMemAlgoResultSerialization(out, data.algoResult);
    UbseMemExportStatusSerialization(out, data.status);
    return;
}

UbseResult UbseMemBorrowExportBaseObjDeserialization(UbseDeSerialization &in, UbseMemBorrowExportBaseObj &data)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemBorrowExportBaseObj during deserialization";
        return UBSE_ERROR;
    }
    if (UbseMemAlgoResultDeserialization(in, data.algoResult) != UBSE_OK) {
        return UBSE_ERROR;
    }
    if (UbseMemExportStatusDeserialization(in, data.status) != UBSE_OK) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseNodeInfoSerialization(UbseSerialization &out, const UbseNodeInfo &data)
{
    out << data.index << data.nodeId << data.hostName << enum_v(data.status);
    return;
}

UbseResult UbseNodeInfoDeserialization(UbseDeSerialization &in, UbseNodeInfo &data)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseNodeInfo during deserialization";
        return UBSE_ERROR;
    }
    in >> data.index >> data.nodeId >> data.hostName >> enum_v(data.status);
    return UBSE_OK;
}

void UbseMemBorrowImportBaseObjSerialization(UbseSerialization &out, const UbseMemBorrowImportBaseObj &data)
{
    out << data.errorCode;
    UbseMemAlgoResultSerialization(out, data.algoResult);
    out << ubse::serial::array_len_insert(data.exportObmmInfo.size());
    for (const auto &item : data.exportObmmInfo) {
        UbseMemObmmInfoSerialization(out, item);
    }
    UbseMemImportStatusSerialization(out, data.status);
    return;
}

UbseResult UbseMemBorrowImportBaseObjDeserialization(UbseDeSerialization &in, UbseMemBorrowImportBaseObj &data)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemBorrowImportBaseObj during deserialization";
        return UBSE_ERROR;
    }
    in >> data.errorCode;
    size_t size{};
    if (UbseMemAlgoResultDeserialization(in, data.algoResult) != UBSE_OK) {
        return UBSE_ERROR;
    }
    uint64_t obmmInfoSize;
    in >> array_len_capture(obmmInfoSize);
    if (obmmInfoSize > MAX_SIZE) {
        UBSE_LOG_ERROR << "Invalid obmmInfoSize during deserialization";
        return UBSE_ERROR;
    }
    data.exportObmmInfo.resize(obmmInfoSize);
    for (size_t i = 0; i < obmmInfoSize; ++i) {
        if (UbseMemObmmInfoDeserialization(in, data.exportObmmInfo[i]) != UBSE_OK) {
            return UBSE_ERROR;
        }
    }
    if (UbseMemImportStatusDeserialization(in, data.status) != UBSE_OK) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseMemFdImportObjMapSerialization(UbseSerialization &out, const UbseMemFdImportObjMap &data)
{
    out << (right_v<size_t>(data.size()));
    for (const auto kv : data) {
        out << kv.first;
        UbseMemFdBorrowImportObjSerialization(out, kv.second);
    }
    return;
}

UbseResult UbseMemFdImportObjMapDeserialization(UbseDeSerialization &in, UbseMemFdImportObjMap &data)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemFdImportObjMap during deserialization";
        return UBSE_ERROR;
    }
    size_t size{};
    in >> size;
    data.reserve(size);
    for (size_t i = 0; i < size; i++) {
        std::string key{};
        in >> key;
        UbseMemFdBorrowImportObj importObj{};
        if (UbseMemFdBorrowImportObjDeserialization(in, importObj) != UBSE_OK) {
            return UBSE_ERROR;
        }
        data[key] = importObj;
    }
    return UBSE_OK;
}

void UbseMemFdExportObjMapSerialization(UbseSerialization &out, const UbseMemFdExportObjMap &data)
{
    out << (right_v<size_t>(data.size()));
    for (const auto kv : data) {
        out << kv.first;
        UbseMemFdBorrowExportObjSerialization(out, kv.second);
    }
    return;
}

UbseResult UbseMemFdExportObjMapDeserialization(UbseDeSerialization &in, UbseMemFdExportObjMap &data)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check FdExportObjMap during deserialization";
        return UBSE_ERROR;
    }
    size_t size{};
    in >> size;
    data.reserve(size);
    for (size_t i = 0; i < size; i++) {
        std::string key{};
        in >> key;
        UbseMemFdBorrowExportObj exportObj{};
        if (UbseMemFdBorrowExportObjDeserialization(in, exportObj) != UBSE_OK) {
            return UBSE_ERROR;
        }
        data[key] = exportObj;
    }
    return UBSE_OK;
}

void UbseMemNumaImportObjMapSerialization(UbseSerialization &out, const UbseMemNumaImportObjMap &data)
{
    out << (right_v<size_t>(data.size()));
    for (const auto kv : data) {
        out << kv.first;
        UbseMemNumaBorrowImportObjSerialization(out, kv.second);
    }
    return;
}

UbseResult UbseMemNumaImportObjMapDeserialization(UbseDeSerialization &in, UbseMemNumaImportObjMap &data)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaImportObjMap during deserialization";
        return UBSE_ERROR;
    }
    size_t size{};
    in >> size;
    data.reserve(size);
    for (size_t i = 0; i < size; i++) {
        std::string key{};
        in >> key;
        UbseMemNumaBorrowImportObj importObj{};
        if (UbseMemNumaBorrowImportObjDeserialization(in, importObj) != UBSE_OK) {
            return UBSE_ERROR;
        }
        data[key] = importObj;
    }
    return UBSE_OK;
}

void UbseMemNumaExportObjMapSerialization(UbseSerialization &out, const UbseMemNumaExportObjMap &data)
{
    out << (right_v<size_t>(data.size()));
    for (const auto kv : data) {
        out << kv.first;
        UbseMemNumaBorrowExportObjSerialization(out, kv.second);
    }
    return;
}

UbseResult UbseMemNumaExportObjMapDeserialization(UbseDeSerialization &in, UbseMemNumaExportObjMap &data)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemNumaExportObjMap during deserialization";
        return UBSE_ERROR;
    }
    size_t size{};
    in >> size;
    data.reserve(size);
    for (size_t i = 0; i < size; i++) {
        std::string key{};
        in >> key;
        UbseMemNumaBorrowExportObj exportObj{};
        if (UbseMemNumaBorrowExportObjDeserialization(in, exportObj) != UBSE_OK) {
            return UBSE_ERROR;
        }
        data[key] = exportObj;
    }
    return UBSE_OK;
}

void NodeMemDebtInfoSerialization(UbseSerialization &out, const NodeMemDebtInfo &data)
{
    UbseMemFdImportObjMapSerialization(out, data.fdImportObjMap);
    UbseMemFdExportObjMapSerialization(out, data.fdExportObjMap);
    UbseMemNumaImportObjMapSerialization(out, data.numaImportObjMap);
    UbseMemNumaExportObjMapSerialization(out, data.numaExportObjMap);
    return;
}

UbseResult NodeMemDebtInfoDeserialization(UbseDeSerialization &in, NodeMemDebtInfo &data)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check  NodeMemDebtInfo during deserialization";
        return UBSE_ERROR;
    }
    if (UbseMemFdImportObjMapDeserialization(in, data.fdImportObjMap) != UBSE_OK) {
        return UBSE_ERROR;
    }
    if (UbseMemFdExportObjMapDeserialization(in, data.fdExportObjMap) != UBSE_OK) {
        return UBSE_ERROR;
    }
    if (UbseMemNumaImportObjMapDeserialization(in, data.numaImportObjMap) != UBSE_OK) {
        return UBSE_ERROR;
    }
    if (UbseMemNumaExportObjMapDeserialization(in, data.numaExportObjMap) != UBSE_OK) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::mem::serial