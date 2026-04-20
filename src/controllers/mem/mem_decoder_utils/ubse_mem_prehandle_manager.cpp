/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "ubse_mem_prehandle_manager.h"

#include "src/adapter_plugins/mti/lcne/ubse_lcne_decoder_entry.h"
#include "src/controllers/mem/mem_controller/debt/ubse_mem_debt_info.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_configuration.h"
namespace ubse::mem::decoder::utils {
using namespace common::def;
UBSE_DEFINE_THIS_MODULE("ubse");

void CompareByHandleValue(const std::pair<uint32_t, uint64_t> &value, const DecoderEntryLoc &loc,
                          DecoderLocToIsUsehandleValueMap &preHandleMap)
{
    if (preHandleMap.find(loc) != preHandleMap.end()) {
        for (auto &handleInfo : preHandleMap[loc]) {
            if (handleInfo.importResult.handle == value.second) {
                UBSE_LOG_INFO << "already used handle is " << value.second;
                handleInfo.dcna = value.first;
                handleInfo.isPreImport = true;
            }
        }
    }
}

void CompareHandleValue(DecoderLocToIsUsehandleValueMap &preHandleMap,
                        const decoder::utils::DecoderLocTohandleDcnaMap &importValue)
{
    for (const auto &[loc, values] : importValue) {
        for (const auto &value : values) {
            CompareByHandleValue(value, loc, preHandleMap);
        }
    }
}

void InitAllEntryBlock(const decoder::utils::DecoderLocTohandleValueMap &tmpValue,
                       DecoderLocToIsUsehandleValueMap &preHandleMap)
{
    for (const auto &[loc, values] : tmpValue) {
        for (const auto &value : values) {
            UbseMamiMemImportResult tmpValue;
            tmpValue.handle = value.handle;
            tmpValue.hpa = value.hpa;
            tmpValue.marId = loc.marId;
            PreHandlerInfo tmpHandleInfo{};
            tmpHandleInfo.dcna = 0;
            tmpHandleInfo.importResult = tmpValue;
            tmpHandleInfo.isPreImport = false;
            preHandleMap[loc].push_back(tmpHandleInfo);
        }
    }
}

void InitDcnaByPreImportInfo(const mmi::BasicPreImportInfo &preImportInfo,
                             DecoderLocToIsUsehandleValueMap &preHandleMap, DcnaToSize &dcnaToSize)
{
    for (auto &[loc, values] : preHandleMap) {
        for (auto &value : values) {
            if (value.importResult.hpa == preImportInfo.pa) {
                value.dcna = preImportInfo.dcna;
                value.isPreImport = true;
            }
        }
    }
}

void UbseMemPrehandleManager::PrintPreHandleMap()
{
    for (const auto [loc, value] : preHandleMap) {
        for (const auto &handle : value) {
            UBSE_LOG_DEBUG << "entry loc: ubpuId is " << loc.ubpuId << ", iouId is " << loc.iouId << ", marId is "
                           << loc.marId << ", decoderId is " << loc.decoderId << "pa is " << handle.importResult.hpa
                           << "dcna is " << handle.dcna << ", preImport is " << handle.isPreImport;
        }
    }
}

UbseResult UbseMemPrehandleManager::InitPreHandle(std::vector<mmi::BasicPreImportInfo> preImportInfos)
{
    std::lock_guard<std::mutex> lock(handleLock);
    PreMapClear();
    decoder::utils::DecoderLocTohandleValueMap tmpValue{};
    auto res = ubse::mem::decoder::utils::MemDecoderUtils::GetAllHandles(UB_MEMORY_HANDLE_STATIC_MEM_POOL, tmpValue);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "InitPreHandle failed";
        return res;
    }
    InitAllEntryBlock(tmpValue, preHandleMap);

    for (const auto &preImportInfo : preImportInfos) {
        InitDcnaByPreImportInfo(preImportInfo, preHandleMap, dcnaToSize);
    }
    return UBSE_OK;
}

UbseResult UbseMemPrehandleManager::GetOneUnImportHandle(const decoder::utils::DecoderEntryLoc &loc, uint32_t dcna,
                                                         UbseMamiMemImportResult &handleValue)
{
    std::lock_guard<std::mutex> lock(handleLock);
    if (preHandleMap.find(loc) == preHandleMap.end()) {
        UBSE_LOG_ERROR << "no handle in this loc, loc is ";
        return UBSE_ERROR;
    }
    for (auto &value : preHandleMap[loc]) {
        if (value.dcna == dcna && value.isPreImport) {
            handleValue = value.importResult;
            UBSE_LOG_INFO << "Get one has preImport but no import memory, hpa is " << handleValue.hpa;
            return UBSE_OK;
        }
    }

    return UBSE_ERROR;
}

uint32_t UbseMemPrehandleManager::GetPreHandleByDcna(const decoder::utils::DecoderEntryLoc &loc, uint32_t dcna,
                                                     UbseMamiMemImportResult &handleValue)
{
    std::lock_guard<std::mutex> lock(handleLock);
    if (preHandleMap.find(loc) == preHandleMap.end()) {
        UBSE_LOG_ERROR << "no handle in this loc, loc decoderId : " << loc.decoderId << ", loc ubpuId : " << loc.ubpuId
                       << ", loc iouId : " << loc.iouId << ", loc marId : " << loc.marId;
        return UBSE_ERROR;
    }
    for (auto &value : preHandleMap[loc]) {
        if (value.dcna == dcna && value.isPreImport) {
            handleValue = value.importResult;
            UBSE_LOG_INFO << "Get one has preImport but no import memory, hpa is " << handleValue.hpa;
            return UBSE_OK;
        }
    }

    return UBSE_ERROR;
}

void UbseMemPrehandleManager::CreatePreHandle(const decoder::utils::DecoderEntryLoc &loc,
                                              const UbseMamiMemImportResult &handleValue, uint32_t dcna,
                                              uint32_t importSize)
{
    std::lock_guard<std::mutex> lock(handleLock);
    PreHandlerInfo tmpInfo{};
    tmpInfo.dcna = dcna;
    tmpInfo.isPreImport = true;
    tmpInfo.importResult = handleValue;
    preHandleMap[loc].emplace_back(tmpInfo);
}

void UbseMemPrehandleManager::GetOneUnPreImportHandle(const decoder::utils::DecoderEntryLoc &loc, uint32_t dcna,
                                                      UbseMamiMemImportResult &outValue)
{
    std::lock_guard<std::mutex> lock(handleLock);
    for (auto &value : preHandleMap[loc]) {
        if (value.dcna == 0 && !value.isPreImport) {
            outValue = value.importResult;
            value.isPreImport = true;
            value.dcna = dcna;
            UBSE_LOG_INFO << "Get one no preImport memory"
                          << ", pa is " << outValue.hpa << "dcna is " << dcna;
            return;
        }
    }
    outValue.hpa = 0;
    UBSE_LOG_INFO << "All handle already preImport, loc: ubpuId is " << loc.ubpuId << ", iouId is " << loc.iouId
                  << ", marId" << loc.marId;
}

bool UbseMemPrehandleManager::IsNeedPreOnline(const decoder::utils::DecoderEntryLoc &loc, uint32_t dcna,
                                              UbseMamiMemImportResult &outValue)
{
    std::lock_guard<std::mutex> lock(handleLock);
    outValue.hpa = 0;
    // todo :调试打印
    PrintPreHandleMap();
    if (preHandleMap.find(loc) == preHandleMap.end()) {
        return true;
    }

    for (auto &value : preHandleMap[loc]) {
        if (value.dcna == dcna && value.isPreImport) {
            outValue = value.importResult;
            return false;
        } else if (value.dcna == 0 && !value.isPreImport) {
            outValue = value.importResult;
            value.dcna = dcna;
            value.isPreImport = true;
            return true;
        } else if (value.dcna == dcna && !value.isPreImport) {
            outValue = value.importResult;
            value.isPreImport = true;
            return true;
        }
    }
    return true;
}

uint64_t UbseMemPrehandleManager::GetPresizeByDcna(uint32_t dcna)
{
    std::lock_guard<std::mutex> lock(handleLock);
    UBSE_LOG_INFO << "dcna is " << dcna << ", size is " << dcnaToSize[dcna];
    return dcnaToSize[dcna];
}

void UbseMemPrehandleManager::RollbackHandle(const decoder::utils::DecoderEntryLoc &loc,
                                             const std::vector<uint64_t> &handles)
{
    std::lock_guard<std::mutex> lock(handleLock);
    for (const auto &handle : handles) {
        for (auto &preInfo : preHandleMap[loc]) {
            if (preInfo.importResult.handle == handle) {
                continue;
            }
        }
    }
}

void UbseMemPrehandleManager::RollbackHandle(const decoder::utils::DecoderEntryLoc &loc, uint64_t handle)
{
    std::lock_guard<std::mutex> lock(handleLock);
    for (auto &preInfo : preHandleMap[loc]) {
        if (preInfo.importResult.handle == handle) {
            continue;
        }
    }
}

void UbseMemPrehandleManager::RollbackPreImportHandle(const decoder::utils::DecoderEntryLoc &loc)
{
    std::lock_guard<std::mutex> lock(handleLock);
    for (auto &preInfo : preHandleMap[loc]) {
        preInfo.dcna = 0;
        preInfo.isPreImport = false;
    }
}

} // namespace ubse::mem::decoder::utils