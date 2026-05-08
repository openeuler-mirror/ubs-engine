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
#include <array>
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_prehandle_manager.h"
#include "ubse_mem_common_utils.h"
#include "ubse_mem_def.h"
#include "ubse_obmm_executor.h"
#include "ubse_obmm_meta_restore.h"
#include "ubse_obmm_utils.h"

namespace ubse::mmi {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::security;

std::vector<__u32> overrideCap = {CAP_DAC_OVERRIDE};
static const std::string OBMM_LOG_INFO = "#######UB[OBMM]########";

UbseResult RmObmmExecutor::Init()
{
    auto ret = RmObmmUtils::GetInstance().GetPreOnlineSwitch(preOnlineSwitch);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "GetPreOnlineSwitch failed.";
        return ret;
    }
    ret = DlOpenLib(OBMM_PATH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get obmm funcs failed from libobmm.so.";
        return ret;
    }
    return UBSE_OK;
}

UbseResult RmObmmExecutor::Exit()
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "Close obmm executor.";
    return UBSE_OK;
}

UbseResult RmObmmExecutor::DlOpenLib(const std::string &obmmPath)
{
    handle = dlopen(obmmPath.c_str(), RTLD_NOW);
    if (handle == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Dlopen obmm.so failed, error is " << dlerror();
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
    ret = DlOpenFunName(obmmExportByPidFunc, "obmm_export_useraddr");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    ret = DlOpenFunName(obmmQueryPaByMemIdFunc, "obmm_query_pa_by_memid");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    ret = DlOpenFunName(obmmPreImportFunc, "obmm_preimport");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    ret = DlOpenFunName(obmmUnPreImportFunc, "obmm_unpreimport");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    return UBSE_OK;
}

static std::string eid_bytes_to_hex_dwords(const uint8_t *data, size_t len)
{
    std::ostringstream oss;
    oss << std::hex;
    const uint32_t constant_8 = 8; // 8字节

    uint64_t high = 0;
    for (int i = 0; i < constant_8; ++i) {                          // 8位
        high |= static_cast<uint64_t>(data[i]) << (i * constant_8); // 小端
    }

    uint64_t low = 0;
    for (int i = constant_8; i < len; ++i) {
        low |= static_cast<uint64_t>(data[i]) << ((i - constant_8) * constant_8); // 小端
    }

    oss << "0x" << low << ":0x" << high;
    return oss.str();
}

static void PrintObmmMemDesc(const obmm_mem_desc &obmmMemDesc)
{
    auto deid = eid_bytes_to_hex_dwords(obmmMemDesc.deid, UBSE_EID_LENGTH);
    auto seid = eid_bytes_to_hex_dwords(obmmMemDesc.seid, UBSE_EID_LENGTH);
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.scna=" << obmmMemDesc.scna;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.length=" << obmmMemDesc.length;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.dcna=" << obmmMemDesc.dcna;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.tokenid=" << obmmMemDesc.tokenid;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.addr=" << obmmMemDesc.addr;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.deid=" << deid;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.seid=" << seid;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.scna=" << obmmMemDesc.scna;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.priv_len=" << obmmMemDesc.priv_len;
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
mem_id RmObmmExecutor::ObmmExport(size_t size[MAX_NUMA_NODES], int arraySize, const ObmmOpParam &opParam,
                                  ubse_mem_obmm_mem_desc &desc)
{
    UBSE_LOG_DEBUG << MMI_LOG_INFO << OBMM_LOG_INFO << "Start to use Obmm Export interface, opParam is "
                   << opParam.toString() << ", size list is: ";
    for (int i = 0; i < MAX_NUMA_NODES; ++i) {
        if (size[i] != 0) {
            UBSE_LOG_DEBUG << MMI_LOG_INFO << OBMM_LOG_INFO << "numaid=" << i << ", size=" << size[i];
        }
    }
    auto obmmFlags = 0;
    if (opParam.borrowType == UbseBorrowType::FD_BORROW || opParam.borrowType == UbseBorrowType::SHARE_BORROW) {
        obmmFlags = obmmFlags | OBMM_EXPORT_FLAG_ALLOW_MMAP;
    }
    for (int i = 0; i < MAX_NUMA_NODES; ++i) {
        if (size[i] > 0) {
            UBSE_LOG_INFO << MMI_LOG_INFO << OBMM_LOG_INFO << "obmm_export numa=" << i << ", size=" << size[i]
                          << ", flag=" << obmmFlags;
        }
    }
    if (obmmExportFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmExportFunc is nullptr, please check.";
        return INVALID_MEM_ID;
    }
    auto obmmMemDesc = ConstructExportMemDesc(opParam.customMeta, opParam.privData);
    if (obmmMemDesc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ConstructExportMemDesc return null.";
        return INVALID_MEM_ID;
    }
    UbseSecurityModule::ModifyEffectiveCapabilities(overrideCap, true);
    mem_id memId = obmmExportFunc(size, obmmFlags, obmmMemDesc);
    UbseSecurityModule::ModifyEffectiveCapabilities(overrideCap, false);
    UBSE_LOG_INFO << MMI_LOG_INFO << OBMM_LOG_INFO << "ObmmExport returned! memid=" << memId
                  << ", obmmFlags=" << obmmFlags;
    PrintObmmMemDesc(*obmmMemDesc);
    if (memId == INVALID_MEM_ID) {
        char buf[STR_ERROR_BUF_SIZE] = {0};
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "ObmmExport error! memid="
                       << ", flag=" << obmmFlags << ", errno=" << errno << ", errMsg="
                       << RmCommonUtils::GetInstance().GetStrError(errno, buf, STR_ERROR_BUF_SIZE);
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        return memId;
    }
    CopyObmmMemDescValue(obmmMemDesc, desc);
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
    UBSE_LOG_DEBUG << MMI_LOG_INFO << OBMM_LOG_INFO << "Start to use Obmm Unexport interface, memid=" << id;
    unsigned long flags = 0;
    if (obmmUnexportFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmUnexportFunc is nullptr, please check.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseSecurityModule::ModifyEffectiveCapabilities(overrideCap, true);
    auto ret = obmmUnexportFunc(id, flags);
    UbseSecurityModule::ModifyEffectiveCapabilities(overrideCap, false);
    if (ret && errno != ENOENT) {
        char buf[STR_ERROR_BUF_SIZE] = {0};
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "ObmmUnExport error! memid=" << id << ", ret=" << ret
                       << ", flag=" << flags << ", errno=" << errno << ", errMsg="
                       << RmCommonUtils::GetInstance().GetStrError(errno, buf, STR_ERROR_BUF_SIZE);
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << OBMM_LOG_INFO << "obmm_unexport ok memid=" << id << ", flag=" << flags;
    return UBSE_OK;
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
        UBSE_LOG_INFO << MMI_LOG_INFO << "numa is" << *numa;
        if (*numa == 0) {
            return INVALID_MEM_ID;
        }
    }
    UBSE_LOG_DEBUG << MMI_LOG_INFO << OBMM_LOG_INFO << "Start to use Obmm Import interface, opParam is "
                   << opParam.toString();
    auto obmmFlags = 0;
    if (opParam.borrowType == UbseBorrowType::NUMA_BORROW || opParam.borrowType == UbseBorrowType::ADDR_BORROW) {
        obmmFlags = obmmFlags | OBMM_IMPORT_FLAG_NUMA_REMOTE;
    }
    if (opParam.borrowType == UbseBorrowType::FD_BORROW || opParam.borrowType == UbseBorrowType::SHARE_BORROW) {
        obmmFlags = obmmFlags | OBMM_IMPORT_FLAG_ALLOW_MMAP;
    }
    if (opParam.borrowType == UbseBorrowType::NUMA_BORROW && opParam.customMeta.decoderResult.staticHandle != 0) {
        obmmFlags = obmmFlags | OBMM_IMPORT_FLAG_PREIMPORT;
    }
    auto obmmMemDesc = ConstructImportMemDesc(opParam, desc);
    if (obmmMemDesc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ConstructImportMemDesc return null.";
        return INVALID_MEM_ID;
    }
    PrintObmmMemDesc(*obmmMemDesc);
    mem_id memid = INVALID_MEM_ID;
    if (obmmImportFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmImportFunc is nullptr, please check.";
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        return INVALID_MEM_ID;
    }
    UbseSecurityModule::ModifyEffectiveCapabilities(overrideCap, true);
    memid = obmmImportFunc(obmmMemDesc, obmmFlags, 0, numa);
    UbseSecurityModule::ModifyEffectiveCapabilities(overrideCap, false);
    if (memid == INVALID_MEM_ID) {
        char buf[STR_ERROR_BUF_SIZE] = {0};
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "ObmmImport error! memid=" << memid
                       << ", flag=" << obmmFlags << ", errno=" << errno << ", errMsg="
                       << RmCommonUtils::GetInstance().GetStrError(errno, buf, STR_ERROR_BUF_SIZE);
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        return memid;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << OBMM_LOG_INFO << " name=" <<  std::string(opParam.customMeta.name)
                  << ", opParam=" << opParam.toString() << ", obmm importMemid=" << memid << ", obmm exportMemid="
                  << opParam.customMeta.exportMemid << ", exportNodeId=" << opParam.customMeta.exportNodeId;
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
    UBSE_LOG_DEBUG << MMI_LOG_INFO << OBMM_LOG_INFO << "Start to use Obmm Unimport interface, mem_id=" << id;
    auto obmmFlags = 0;
    int ret;
    errno = 0;
    if (obmmUnimportFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmUnimportFunc is nullptr, please check.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseSecurityModule::ModifyEffectiveCapabilities(overrideCap, true);
    ret = obmmUnimportFunc(id, obmmFlags);
    UbseSecurityModule::ModifyEffectiveCapabilities(overrideCap, false);
    if (ret && errno != ENOENT) {
        char buf[STR_ERROR_BUF_SIZE] = {0};
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "obmm_unimport error! memid=" << id << ", ret=" << ret
                       << ", flag=" << obmmFlags << ", errno=" << errno << ", errMsg="
                       << RmCommonUtils::GetInstance().GetStrError(errno, buf, STR_ERROR_BUF_SIZE);
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << OBMM_LOG_INFO << "obmm_unimport memid=" << id << ", ret=" << ret;
    return UBSE_OK;
}

mem_id RmObmmExecutor::ObmmDevChangeUidGid(uint64_t memId, bool importMem, const ObmmOpParam &opParam)
{
    if (opParam.borrowType == UbseBorrowType::NUMA_BORROW || opParam.borrowType == UbseBorrowType::ADDR_BORROW) {
        return memId;
    }
    if (opParam.borrowType == UbseBorrowType::FD_BORROW && !importMem) {
        return memId;
    }
    if (!BaseObmmDevChangeUidGid(memId, opParam.uid, opParam.gid, opParam.mode)) {
        if (importMem) {
            ObmmUnImport(memId);
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Change uid error, unimport memid=" << memId;
        } else {
            ObmmUnExport(memId);
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Change uid error, unexport memid=" << memId;
        }
        return INVALID_MEM_ID;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "memid=" << memId << ", isimport=" << importMem
                  << ", opParam=" << opParam.toString();
    return memId;
}

std::vector<mem_id> RmObmmExecutor::ObmmImport(const std::vector<UbseMemObmmInfo> &desc, ObmmOpParam &opParam,
                                               UbseMemImportStatus &status, int *numa)
{
    if (desc.empty() || desc.size() != status.decoderResult.size()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The mem desc is invalid.";
        return {};
    }
    std::vector<mem_id> result;
    result.reserve(desc.size());
    for (int i = 0; i < desc.size(); i++) {
        opParam.customMeta.memidCount = desc.size();
        opParam.customMeta.exportMemid = desc[i].memId;
        opParam.customMeta.decoderResult = status.decoderResult[i];
        auto importMemid = ObmmImport(desc[i].desc, opParam, numa);
        if (importMemid == 0) {
            ObmmUnImport(result);
            UBSE_LOG_ERROR << MMI_LOG_INFO
                           << "OBMM import error, memIds=" << RmCommonUtils::GetInstance().MemToStr(result);
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
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "All memIds=" << RmCommonUtils::GetInstance().MemToStr(id)
                           << ", successfulList=" << RmCommonUtils::GetInstance().MemToStr(successfulList)
                           << ", errCode=" << ret;
            return ret;
        }
        successfulList.push_back(memId);
    }
    return UBSE_OK;
}

bool SplitObmmExportSize(const size_t size[MAX_NUMA_NODES], const size_t blockSize,
                         std::vector<std::array<size_t, MAX_NUMA_NODES>> &result)
{
    size_t blockNum[MAX_NUMA_NODES]{};
    size_t blockAll{0};
    if (blockSize == 0u) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Block size=0 is invalid.";
        return false;
    }
    for (int i = 0; i < MAX_NUMA_NODES; ++i) {
        bool isOverflow = false;
        blockNum[i] = (RmCommonUtils::GetInstance().SafeAdd(size[i], blockSize, isOverflow) - 1) / blockSize;
        if (isOverflow) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Overflow occurred during addition. size[" << i << "]=" << size[i]
                           << ", blockSize=" << blockSize;
            return false;
        }
        blockAll += blockNum[i];
    }
    result.clear();
    result.reserve(blockAll);
    for (int i = 0; i < blockAll; ++i) {
        result.push_back({});
    }
    int index = 0;
    for (int i = 0; i < MAX_NUMA_NODES; ++i) {
        for (int j = 0; j < blockNum[i]; ++j) {
            result[index++][i] = blockSize;
        }
    }
    return !result.empty();
}

std::vector<mem_id> RmObmmExecutor::ObmmExport(size_t size[MAX_NUMA_NODES], int arraySize, ObmmOpParam &opParam,
                                               std::vector<ubse_mem_obmm_mem_desc> &desc, uint64_t blockSize)
{
    std::vector<std::array<size_t, MAX_NUMA_NODES>> resultTemp;
    if (!SplitObmmExportSize(size, blockSize, resultTemp)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO
                       << "SplitObmmExportSize error, borrowTypeCode=" << static_cast<uint32_t>(opParam.borrowType)
                       << ", blockSize=" << blockSize;
        return {};
    }

    if (desc.empty()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The desc.size=" << desc.size();
        return {};
    }

    if (desc.size() != resultTemp.size()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The desc.size=" << desc.size()
                       << ", The resultTemp.size=" << resultTemp.size();
        return {};
    }
    std::vector<mem_id> result;
    result.reserve(resultTemp.size());
    for (int i = 0; i < resultTemp.size(); ++i) {
        opParam.customMeta.memidCount = resultTemp.size();
        auto id = ObmmExport(resultTemp[i].data(), MAX_NUMA_NODES, opParam, desc[i]);
        if (id == 0) {
            ObmmUnExport(result);
            UBSE_LOG_ERROR << MMI_LOG_INFO
                           << "OBMM export error, memIds=" << RmCommonUtils::GetInstance().MemToStr(result);
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
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "All memids=" << RmCommonUtils::GetInstance().MemToStr(id)
                           << ", successfulList=" << RmCommonUtils::GetInstance().MemToStr(successfulList)
                           << ", errCode=" << ret;
            return ret;
        }
        successfulList.push_back(memId);
    }
    return UBSE_OK;
}

UbseResult RmObmmExecutor::ObmmExportPid(ObmmPidExportParam &param, ubse_mem_obmm_mem_desc &desc,
                                         const UbseMemLocalObmmCustomMeta &customMeta, const UbMemPrivData &privData)
{
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "ObmmExportPid start";
    auto obmmMemDesc = ConstructExportMemDesc(customMeta, privData);
    if (obmmMemDesc == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto flag = 0;
    UBSE_LOG_INFO << MMI_LOG_INFO << "The pid=" << param.pid << ", va=" << reinterpret_cast<uint64_t>(param.va)
                  << ", size=" << param.size;
    UbseSecurityModule::ModifyEffectiveCapabilities(overrideCap, true);
    param.memid = obmmExportByPidFunc(param.pid, param.va, param.size, flag, obmmMemDesc);
    UbseSecurityModule::ModifyEffectiveCapabilities(overrideCap, false);
    if (param.memid == INVALID_MEM_ID) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "Export failed, memid=0. errno=" << errno
                       << ", errMsg: " << strerror(errno);
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    CopyObmmMemDescValue(obmmMemDesc, desc);
    PrintObmmMemDesc(*obmmMemDesc);
    if (UBSE_RESULT_FAIL(ubse::mmi::RmObmmDevRead::GetNuma(param.memid, param.exportNuma))) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "GetNuma by memid failed.";
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        return UBSE_MMI_OBMM_OP_FAILED;
    }

    UBSE_LOG_INFO << MMI_LOG_INFO << "obmm_export_useraddr success.";
    RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
    return UBSE_OK;
}

UbseResult RmObmmExecutor::ObmmQueryUBPaByMemId(uint64_t handle, unsigned long offset, unsigned long *pa)
{
    if (!pa) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << " pa is null.";
        return UBSE_ERROR_NULLPTR;
    }
    if (!obmmQueryPaByMemIdFunc) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "obmmQueryPaByMemIdFunc is null.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseSecurityModule::ModifyEffectiveCapabilities(overrideCap, true);
    auto ret = obmmQueryPaByMemIdFunc(handle, offset, pa);
    UbseSecurityModule::ModifyEffectiveCapabilities(overrideCap, false);
    if (ret != 0) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "obmmQueryPaByMemIdFunc error handle is " << handle << ".";
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    return UBSE_OK;
}

UbseResult RmObmmExecutor::ObmmPreImport(struct obmm_preimport_info *preimport_info, unsigned long flags)
{
    if (preimport_info == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The obmm_preimport_info is nullptr, error_code=" << UBSE_ERROR_INVAL;
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "Start to use Obmm PreImport interface, preimport_info is scna="
                  << preimport_info->scna << ", dcna=" << preimport_info->dcna << ", length="
                  << preimport_info->length;
    if (obmmPreImportFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The obmmPreImportFunc is nullptr, error_code=" << UBSE_ERROR_NULLPTR;
        return UBSE_ERROR_NULLPTR;
    }

    UbseSecurityModule::ModifyEffectiveCapabilities(overrideCap, true);
    auto ret = obmmPreImportFunc(preimport_info, flags);
    UbseSecurityModule::ModifyEffectiveCapabilities(overrideCap, false);

    if (ret == -1 && errno == EEXIST) {
        UBSE_LOG_INFO << MMI_LOG_INFO << "The cna connect has been preOnline, scna=" << preimport_info->scna
                      << ", dcna=" << preimport_info->dcna;
        return UBSE_OK;
    }
    if (ret != 0) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "obmmPreImportFailed, ret=" << ret << ", scna= " << preimport_info->scna
                       << ", dcna=" << preimport_info->dcna << ", errno=" << errno;
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "PreImport success, remote numa_id=" << preimport_info->numa_id << ", scna="
                  << preimport_info->scna;
    return UBSE_OK;
}
UbseResult RmObmmExecutor::ObmmUnPreImport(struct obmm_preimport_info *preimport_info, unsigned long flags)
{
    if (preimport_info == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The obmm_preimport_info is nullptr, error_code=" << UBSE_ERROR_INVAL;
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO
                  << "Start to use Obmm UnPreImport interface, preimport_info is scna=" << preimport_info->scna
                  << ", dcna=" << preimport_info->dcna << ", length=" << preimport_info->length;
    if (obmmUnPreImportFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The obmmPreImportFunc is nullptr, error_code=" << UBSE_ERROR_NULLPTR;
        return UBSE_ERROR_NULLPTR;
    }
    UbseSecurityModule::ModifyEffectiveCapabilities(overrideCap, true);
    auto ret = obmmUnPreImportFunc(preimport_info, flags);
    UbseSecurityModule::ModifyEffectiveCapabilities(overrideCap, false);
    if (ret != 0) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "obmmUnPreImportFailed, ret=" << ret << ", scna="
                       << preimport_info->scna << ", dcna=" << preimport_info->dcna;
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "UnPreImport success, remote numa_id=" << preimport_info->numa_id
                  << ", scna=" << preimport_info->scna;
    return UBSE_OK;
}

} // namespace ubse::mmi
