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

#include "ubse_obmm_executor.h"
#include <array>
#include <iostream>
#include "ubse_mem_def.h"
#include "ubse_mem_common_utils.h"
#include "ubse_obmm_utils.h"

namespace ubse::mmi {

static const std::string OBMM_LOG_INFO = "#######UB[OBMM]########";

UbseResult RmObmmExecutor::Init()
{
    auto ret = RmObmmUtils::GetInstance().GetBlockSize(blockSize);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get blockSize failed.";
    }
    ret = DlOpenLib(OBMM_PATH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get obmm funcs failed from libobmm.so.";
        return ret;
    }
    return UBSE_OK;
}

UbseResult RmObmmExecutor::Exit()
{
    UBSE_LOG_INFO << "Close obmm executor.";
    if (handle != nullptr) {
        dlclose(handle);
        handle = nullptr;
    }
    obmmExportFunc = nullptr;
    obmmUnexportFunc = nullptr;
    obmmImportFunc = nullptr;
    obmmUnimportFunc = nullptr;
    return UBSE_OK;
}

UbseResult RmObmmExecutor::DlOpenLib(const std::string &obmmPath)
{
    handle = dlopen(obmmPath.c_str(), RTLD_NOW);
    if (handle == nullptr) {
        UBSE_LOG_ERROR << "Dlopen obmm.so failed, error is " << dlerror();
        return UBSE_ERROR_INVAL;
    }
    auto ret = DlOpenFunName(obmmExportFunc, "obmm_export");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    ret = DlOpenFunName(obmmUnexportFunc, "obmm_unexport");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    ret = DlOpenFunName(obmmImportFunc, "obmm_import");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    ret = DlOpenFunName(obmmUnimportFunc, "obmm_unimport");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    return UBSE_OK;
}

static void PrintObmmMemDesc(const obmm_mem_desc &obmmMemDesc)
{
    UBSE_LOG_DEBUG << "obmmMemDesc.scna " << obmmMemDesc.scna;
    UBSE_LOG_DEBUG << "obmmMemDesc.length " << obmmMemDesc.length;
    UBSE_LOG_DEBUG << "obmmMemDesc.dcna " << obmmMemDesc.dcna;
    UBSE_LOG_DEBUG << "obmmMemDesc.tokenid " << obmmMemDesc.tokenid;
    UBSE_LOG_DEBUG << "obmmMemDesc.addr " << obmmMemDesc.addr;
    UBSE_LOG_DEBUG << "obmmMemDesc.deid " << std::string(std::begin(obmmMemDesc.deid), std::end(obmmMemDesc.deid));
    UBSE_LOG_DEBUG << "obmmMemDesc.seid " << std::string(std::begin(obmmMemDesc.seid), std::end(obmmMemDesc.seid));
    UBSE_LOG_DEBUG << "obmmMemDesc.scna " << obmmMemDesc.scna;
    UBSE_LOG_DEBUG << "obmmMemDesc.priv_len " << obmmMemDesc.priv_len;
}

/**
 * 导出内存。
 * 根据提供的大小和标志，使用OBMM（内存管理模块）导出内存，并返回内存描述符。
 * @param size 每个NUMA节点的内存大小数组。
 * @param opParam
 * @param flags 导出内存的标志。
 * @param desc 导出内存后的描述符。
 * @return 成功时返回导出的内存ID，失败时返回INVALID_MEM_ID。
 */
mem_id RmObmmExecutor::ObmmExport(size_t size[MAX_NUMA_NODES], const ObmmOpParam &opParam, ubse_mem_obmm_mem_desc &desc)
{
    UBSE_LOG_DEBUG << OBMM_LOG_INFO << "Start to use Obmm Export interface, opParam is " << opParam.toString()
                 << ", size list is: ";
    for (int i = 0; i < MAX_NUMA_NODES; ++i) {
        if (size[i] != 0) {
            UBSE_LOG_DEBUG << OBMM_LOG_INFO << "numaid: " << i << " size " << size[i];
        }
    }
    auto obmmFlags = 0;
    if (opParam.borrowType == UbseBorrowType::FD_BORROW) {
        obmmFlags = obmmFlags | OBMM_EXPORT_FLAG_ALLOW_MMAP;
    }
    for (int i = 0; i < MAX_NUMA_NODES; ++i) {
        if (size[i] > 0) {
            UBSE_LOG_INFO << OBMM_LOG_INFO << "obmm_export numa: " << i << " ,size: "
                        << size[i] << " flag " << obmmFlags;
        }
    }
    auto obmmMemDesc = RmObmmUtils::GetInstance().ConstructExportMemDesc(opParam.customMeta);
    if (obmmMemDesc == nullptr) {
        UBSE_LOG_ERROR << "ConstructExportMemDesc return null.";
        return INVALID_MEM_ID;
    }
    if (obmmExportFunc == nullptr) {
        UBSE_LOG_ERROR << "ObmmExportFunc is nullptr, please check.";
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        return INVALID_MEM_ID;
    }
    mem_id memId = obmmExportFunc(size, obmmFlags, obmmMemDesc);
    UBSE_LOG_INFO << OBMM_LOG_INFO << "ObmmExport returned! memid: " << memId << " obmmFlags: " << obmmFlags;
    PrintObmmMemDesc(*obmmMemDesc);
    if (memId == INVALID_MEM_ID) {
        char buf[STR_ERROR_BUF_SIZE] = {0};
        UBSE_LOG_ERROR << OBMM_LOG_INFO << "ObmmExport error! memid: " << ", flag is " << obmmFlags << " errno is "
                     << errno << ", errMsg is "
                     << RmCommonUtils::GetInstance().GetStrError(errno, buf, STR_ERROR_BUF_SIZE);
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        return memId;
    }
    RmObmmUtils::GetInstance().CopyObmmMemDescValue(obmmMemDesc, desc);
    RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
    return ObmmDevChangeUidGid(memId, false, opParam);
}

/**
 * 取消导出内存。
 * 根据提供的内存ID，使用OBMM取消导出内存。
 * @param id 要取消导出的内存ID。
 * @return 成功时返回成功标志，失败时返回错误代码。
 */
UbseResult RmObmmExecutor::ObmmUnExport(mem_id id)
{
    UBSE_LOG_DEBUG << OBMM_LOG_INFO << "Start to use Obmm Unexport interface, memid=" << id;
    unsigned long flags = 0;
    if (obmmUnexportFunc == nullptr) {
        UBSE_LOG_ERROR << "ObmmUnexportFunc is nullptr, please check.";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = obmmUnexportFunc(id, flags);
    if (ret && errno != ENOENT) {
        char buf[STR_ERROR_BUF_SIZE] = {0};
        UBSE_LOG_ERROR << OBMM_LOG_INFO << "ObmmUnExport error! memid=" << id << " ret=" << ret << ", flag is "
                    << flags << " errno is " << errno << ", errMsg is "
                    << RmCommonUtils::GetInstance().GetStrError(errno, buf, STR_ERROR_BUF_SIZE);
        return E_CODE_OBMM_OP_FAILED;
    }
    UBSE_LOG_INFO << OBMM_LOG_INFO << "obmm_unexport ok memid: " << id << ", flag is " << flags;
    return HOK;
}

/**
 * 导入内存。
 * 根据提供的内存描述符、标志和NUMA节点ID，使用OBMM导入内存。
 * @param desc 内存描述符。
 * @param opParam obmm
 * @param flags 导入内存的标志。
 * @param numa 导入的NUMA节点ID。
 * @return 成功时返回导入的内存ID，失败时返回INVALID_MEM_ID。
 */
mem_id RmObmmExecutor::ObmmImport(const ubse_mem_obmm_mem_desc &desc, const ObmmOpParam &opParam, int *numa)
{
    if (numa != nullptr) {
        UBSE_LOG_DEBUG << OBMM_LOG_INFO << "Start to use Obmm Import interface, opParam is " << opParam.toString()
                     << ", numa is" << *numa;
        if (*numa == 0) {
            UBSE_LOG_ERROR << "Numa is 0 when import";
            return INVALID_MEM_ID;
        }
    } else {
        UBSE_LOG_DEBUG << OBMM_LOG_INFO << "Start to use Obmm Import interface, opParam is " << opParam.toString();
    }
    auto obmmFlags = 0;
    if (opParam.borrowType == UbseBorrowType::NUMA_BORROW) {
        obmmFlags = obmmFlags | OBMM_IMPORT_FLAG_NUMA_REMOTE;
    }
    if (opParam.borrowType == UbseBorrowType::FD_BORROW) {
        obmmFlags = obmmFlags | OBMM_IMPORT_FLAG_ALLOW_MMAP;
    }
    auto obmmMemDesc = RmObmmUtils::ConstructImportMemDesc(opParam.customMeta, desc);
    if (obmmMemDesc == nullptr) {
        UBSE_LOG_ERROR << "ConstructImportMemDesc return null.";
        return INVALID_MEM_ID;
    }
    PrintObmmMemDesc(*obmmMemDesc);
    mem_id memid = INVALID_MEM_ID;
    if (obmmImportFunc == nullptr) {
        UBSE_LOG_ERROR << "ObmmImportFunc is nullptr, please check.";
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        return INVALID_MEM_ID;
    }
    memid = obmmImportFunc(obmmMemDesc, obmmFlags, 0, numa);
    if (memid == INVALID_MEM_ID) {
        char buf[STR_ERROR_BUF_SIZE] = {0};
        UBSE_LOG_ERROR << OBMM_LOG_INFO << "ObmmImport error! memid=" << memid << " flag=" << obmmFlags << " errno is "
                     << errno << "errMsg is "
                     << RmCommonUtils::GetInstance().GetStrError(errno, buf, STR_ERROR_BUF_SIZE);
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        return memid;
    }
    UBSE_LOG_INFO << OBMM_LOG_INFO << "obmm_import memid: " << memid << " memid :" << memid
                << " opParam: " << opParam.toString();
    RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
    return ObmmDevChangeUidGid(memid, true, opParam);
}

/**
 * 取消导入内存。
 * 根据提供的内存ID和标志，使用OBMM取消导入内存。
 * @param id 要取消导入的内存ID。
 * @param flags 取消导入的标志。
 * @return 成功时返回成功标志，失败时返回错误代码。
 */
UbseResult RmObmmExecutor::ObmmUnImport(mem_id id)
{
    UBSE_LOG_DEBUG << OBMM_LOG_INFO << "Start to use Obmm Unimport interface, mem_id=" << id;
    auto obmmFlags = 0;
    int ret;
    errno = 0;
    if (obmmUnimportFunc == nullptr) {
        UBSE_LOG_ERROR << "ObmmUnimportFunc is nullptr, please check.";
        return UBSE_ERROR_NULLPTR;
    }
    ret = obmmUnimportFunc(id, obmmFlags);
    if (ret && errno != ENOENT) {
        char buf[STR_ERROR_BUF_SIZE] = {0};
        UBSE_LOG_ERROR << OBMM_LOG_INFO << "obmm_unimport error! memid:" << id << " ret:" << ret
                    << " flag:" << obmmFlags << " errno is " << errno << "errMsg is "
                    << RmCommonUtils::GetInstance().GetStrError(errno, buf, STR_ERROR_BUF_SIZE);
        return E_CODE_OBMM_OP_FAILED;
    }
    UBSE_LOG_INFO << OBMM_LOG_INFO << "obmm_unimport memid=" << id << " ret=" << ret;
    return HOK;
}

mem_id RmObmmExecutor::ObmmDevChangeUidGid(uint64_t memId, bool importMem, const ObmmOpParam &opParam)
{
    if (opParam.borrowType == UbseBorrowType::NUMA_BORROW) {
        return memId;
    }
    if (opParam.borrowType == UbseBorrowType::FD_BORROW && !importMem) {
        return memId;
    }
    if (!BaseObmmDevChangeUidGid(memId, opParam.uid, opParam.gid, opParam.mode)) {
        if (importMem) {
            ObmmUnImport(memId);
            UBSE_LOG_ERROR << "Change uid error, unimport memid=" << memId;
        } else {
            ObmmUnExport(memId);
            UBSE_LOG_ERROR << "Change uid error, unexport memid=" << memId;
        }
        return INVALID_MEM_ID;
    }
    UBSE_LOG_INFO << "memid=" << memId << ", isimport=" << importMem << ", opParam=" << opParam.toString();
    return memId;
}

std::vector<mem_id> RmObmmExecutor::ObmmImport(const std::vector<UbseMemObmmInfo> &desc, ObmmOpParam &opParam,
                                               int *numa)
{
    if (desc.empty()) {
        UBSE_LOG_ERROR << "The mem desc is empty.";
        return {};
    }
    std::vector<mem_id> result;
    result.reserve(desc.size());
    for (int i = 0; i < desc.size(); i++) {
        opParam.customMeta.memidCount = desc.size();
        opParam.customMeta.exportMemid = desc[i].memId;
        auto importMemid = ObmmImport(desc[i].desc, opParam, numa);
        if (importMemid == 0) {
            ObmmUnImport(result);
            UBSE_LOG_ERROR << "OBMM import error, memIds=" << RmCommonUtils::GetInstance().MemToStr(result);
            return {};
        }
        result.push_back(importMemid);
    }
    return result;
}

UbseResult RmObmmExecutor::ObmmUnImport(const std::vector<mem_id> &id)
{
    std::vector<uint64_t> successfulList;
    for (mem_id memId : id) {
        const auto ret = ObmmUnImport(memId);
        if (ret != HOK) {
            UBSE_LOG_ERROR << "All memIds=" << RmCommonUtils::GetInstance().MemToStr(id)
                         << ", successfulList=" << RmCommonUtils::GetInstance().MemToStr(successfulList)
                         << ", errCode=" << ret;
            return ret;
        }
        successfulList.push_back(memId);
    }
    return HOK;
}

std::vector<mem_id> RmObmmExecutor::ObmmExport(size_t size[MAX_NUMA_NODES], ObmmOpParam &opParam,
                                               std::vector<ubse_mem_obmm_mem_desc> &desc)
{
    std::vector<std::array<size_t, MAX_NUMA_NODES>> resultTemp;
    if (!RmObmmUtils::GetInstance().SplitObmmExportSize(size, blockSize, resultTemp)) {
        UBSE_LOG_ERROR << "SplitObmmExportSize error, borrowTypeCode=" << static_cast<uint32_t>(opParam.borrowType)
                     << ", blockSize=" << blockSize;
        return {};
    }
    if (desc.size() != resultTemp.size()) {
        UBSE_LOG_ERROR << "The desc.size=" << desc.size() << ", The resultTemp.size=" << resultTemp.size();
        return {};
    }
    std::vector<mem_id> result;
    result.reserve(resultTemp.size());
    for (int i = 0; i < resultTemp.size(); ++i) {
        opParam.customMeta.memidCount = resultTemp.size();
        auto id = ObmmExport(resultTemp[i].data(), opParam, desc[i]);
        if (id == 0) {
            ObmmUnExport(result);
            UBSE_LOG_ERROR << "OBMM export error, memIds=" << RmCommonUtils::GetInstance().MemToStr(result);
            return {};
        }
        result.push_back(id);
    }
    return result;
}

UbseResult RmObmmExecutor::ObmmUnExport(const std::vector<mem_id> &id)
{
    std::vector<uint64_t> successfulList;
    for (mem_id memId : id) {
        const auto ret = ObmmUnExport(memId);
        if (ret != HOK) {
            UBSE_LOG_ERROR << "All memids=" << RmCommonUtils::GetInstance().MemToStr(id)
                         << ", successfulList=" << RmCommonUtils::GetInstance().MemToStr(successfulList)
                         << ", errCode=" << ret;
            return ret;
        }
        successfulList.push_back(memId);
    }
    return HOK;
}

} // namespace ubse::mmi
