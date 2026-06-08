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

#include "ubse_mem_account.h"
#include <securec.h>
#include "ubse_json_util.h"
#include "ubse_node_controller.h"

#include "ubse_common_def.h"
#include "ubse_conf.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mmi_interface.h"
#include "ubse_mem_controller_query_api.h"
namespace ubse::mem::account {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::common::def;
using namespace ubse::utils;
using namespace ubse::adapter_plugins::mmi;
const std::string MEM_CONFING_SECTION_NAME = "ubse.memory";
const std::string POOL_MEMORY_RATIO = "system.pool.memory.ratio";
/* **************************************** */
/* 账本查询先调用内部模块构建中间结构体            */
/* 而后再用中间结构体构建接口出参                 */
/* **************************************** */

using UbseBorrowLendPair = std::pair<uint64_t, uint64_t>;                   // borrow, lend
using UbseNumaLedgerInfo = std::unordered_map<uint32_t, UbseBorrowLendPair>; // numaId:<borrow, lend>
using UbseNumaSharedMem = std::unordered_map<uint32_t, uint64_t>;           // numaId: mMemSharedSize

static std::string GetPrefixBeforeLastUnderscore(const std::string &str)
{
    size_t pos = str.rfind('_');
    if (pos != std::string::npos) {
        return str.substr(0, pos);
    }
    return str; // 或者返回空字符串，根据需求决定
}

/**
* @brief 根据内部模块的节点信息填写numa静态信息
* @param nodeInfos [in] 内部模块的节点信息
* @param numaInfo [out] 中间numa信息
*/
static void AssignNumaInfo(const std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> &nodeInfos,
                           std::vector<NumaStaticInfo> &numaInfo)
{
    for (const auto &[_, nodeInfo] : nodeInfos) {
        for (const auto &[_, singleNumaInfo] : nodeInfo.numaInfos) {
            NumaStaticInfo tmpNumaInfo;
            tmpNumaInfo.nodeId = singleNumaInfo.location.nodeId;
            tmpNumaInfo.soketId = singleNumaInfo.socketId;
            tmpNumaInfo.numaId = singleNumaInfo.location.numaId;
            tmpNumaInfo.hostName = nodeInfo.hostName;
            tmpNumaInfo.cpuList = singleNumaInfo.bindCore;
            tmpNumaInfo.totalMemSize = singleNumaInfo.size * NO_1024;
            tmpNumaInfo.freeMemSize = singleNumaInfo.freeSize * NO_1024;
            tmpNumaInfo.nrHugePages = singleNumaInfo.nr_hugepages_2M;
            tmpNumaInfo.freeHugePages = singleNumaInfo.free_hugepages_2M;
            tmpNumaInfo.timeStamp = singleNumaInfo.timestamp;
            tmpNumaInfo.ratio = nodeInfo.pmdMapping;
            numaInfo.emplace_back(tmpNumaInfo);
        }
    }
}

/**
* @brief 通过内部import对象填写中间账本信息
* @param numaInfos [in] 内部numa账本信息
* @param ledgerInfo [out] 中间账本信息
*/
static void FormNumaInfoUsingImportObj(const std::vector<ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo> &numaInfos,
                                       LedgerDymaticInfo &ledgerInfo)
{
    if (numaInfos.empty()) {
        return;
    }
    std::string nodeId = numaInfos.front().nodeId;
    ledgerInfo.borrowNodeId.emplace_back(nodeId);
    for (const auto &numaDebt : numaInfos) {
        ledgerInfo.borrowSocketIdList[nodeId].emplace_back(numaDebt.socketId);
        ledgerInfo.borrowNumaIdList[nodeId].emplace_back(numaDebt.numaId);
        ledgerInfo.borrowNumaSizeList[nodeId].emplace_back(numaDebt.size);
    }
}

/**
* @brief 通过内部export对象填写中间账本信息
* @param numaInfos [in] 内部numa账本信息
* @param ledgerInfo [out] 中间账本信息
*/
static void FormNumaInfoUsingExportObj(const std::vector<ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo> &numaInfos,
                                       LedgerDymaticInfo &ledgerInfo)
{
    if (numaInfos.empty()) {
        return;
    }
    std::string nodeId = numaInfos.front().nodeId;
    ledgerInfo.lentNodeId = nodeId;
    for (const auto &numaDebt : numaInfos) {
        ledgerInfo.lentSocketIdList.emplace_back(numaDebt.socketId);
        ledgerInfo.lentNumaIdList.emplace_back(numaDebt.numaId);
        ledgerInfo.lentNumaSizeList.emplace_back(numaDebt.size);
    }
}

/**
* @brief 使用导入内部importBaseObj构建中间账本信息
* @param importBaseObj [in] 内部账本基类
* @param resourceIdNew [in] 资源id，name_borrowNodeId
* @param type [in] 借用类型
* @param midLedgerInfo [out] 中间账本信息
*/

static void FormImportLedger(const UbseMemBorrowImportBaseObj &importBaseObj,
                             const std::string &resourceIdNew, const LedgerType type,
                             std::unordered_map<std::string, LedgerDymaticInfo> &midLedgerInfo)
{
    if (importBaseObj.algoResult.importNumaInfos.empty()) {
        return;
    }
    if (importBaseObj.status.state != ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_IMPORT_SUCCESS) {
        return;
    }
    if (midLedgerInfo.find(resourceIdNew) == midLedgerInfo.end()) {
        LedgerDymaticInfo singleLedgerInfo{};
        singleLedgerInfo.resourceId = GetPrefixBeforeLastUnderscore(resourceIdNew);
        singleLedgerInfo.type = type;
        midLedgerInfo[resourceIdNew] = singleLedgerInfo;
    }
    FormNumaInfoUsingImportObj(importBaseObj.algoResult.importNumaInfos, midLedgerInfo[resourceIdNew]);
    std::string borrowNodeId = importBaseObj.algoResult.importNumaInfos.front().nodeId;
    for (const auto &obmmInfo : importBaseObj.status.importResults) {
        midLedgerInfo[resourceIdNew].borrowMemId[borrowNodeId].emplace_back(obmmInfo.memId);
    }
}

/**
* @brief 使用Fd的Import对象补充中间账本，下同
* @param fdImportObjMap [in] fd的import账本
* @param midLedgerInfo [out] 中间账本
*/
static void FormImportFdLedger(const UbseMemFdImportObjMap &fdImportObjMap,
                               std::unordered_map<std::string, LedgerDymaticInfo> &midLedgerInfo)
{
    for (const auto &[resourceId, fdImportObj] : fdImportObjMap) {
        FormImportLedger(fdImportObj, resourceId, LedgerType::FD, midLedgerInfo);
    }
}

/**
* @brief 将内部obmm描述符转换为中间结构
* @param input [in] 内部obmm描述
* @param output [out] 中间结构
*/
static void AssignObmmDes(const ubse_mem_obmm_mem_desc &input, ObmmDesc &output)
{
#ifdef UB_ENVIRONMENT
    output.addr = input.addr;
    output.length = input.length;
    output.seid = input.seid;
    output.deid = input.deid;
    output.tokenId = input.tokenid;
    output.scna = input.scna;
    output.dcna = input.dcna;
    output.marId = input.marId;
#endif
}

/**
* @brief 使用内部exportBaseObj构建中间的账本信息
* @param exportBaseObj [in] 内部账本基类
* @param resourceIdNew [in] 资源id，name_borrowNodeId
* @param midLedgerInfo [out] 中间账本信息
*/
static void FormExportLedger(const UbseMemBorrowExportBaseObj &exportBaseObj,
                             const std::string &resourceIdNew, LedgerType type,
                             std::unordered_map<std::string, LedgerDymaticInfo> &midLedgerInfo)
{
    if (exportBaseObj.status.state != ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_EXPORT_SUCCESS) {
        return;
    }
    if (midLedgerInfo.find(resourceIdNew) == midLedgerInfo.end()) {
        LedgerDymaticInfo singleLedgerInfo{};
        singleLedgerInfo.resourceId = GetPrefixBeforeLastUnderscore(resourceIdNew);
        singleLedgerInfo.type = type;
        midLedgerInfo[resourceIdNew] = singleLedgerInfo;
    }
    if (exportBaseObj.algoResult.exportNumaInfos.empty()) {
        return;
    }
    std::string nodeId = exportBaseObj.algoResult.exportNumaInfos.front().nodeId;
    midLedgerInfo[resourceIdNew].lentNodeId = nodeId;
    FormNumaInfoUsingExportObj(exportBaseObj.algoResult.exportNumaInfos, midLedgerInfo[resourceIdNew]);
    for (const auto &obmmInfo : exportBaseObj.status.exportObmmInfo) {
        midLedgerInfo[resourceIdNew].lentMemId.emplace_back(obmmInfo.memId);
    }
    for (const auto &obmmInfo : exportBaseObj.status.exportObmmInfo) {
        ObmmDesc tmpObmmInfo{};
        AssignObmmDes(obmmInfo.desc, tmpObmmInfo);
        midLedgerInfo[resourceIdNew].obmmDesc.emplace_back(tmpObmmInfo);
    }
}

/**
* @brief 使用Fd的Export对象补充中间账本，下同
* @param fdExportObjMap [in] fd的export账本
* @param midLedgerInfo [out] 中间账本
*/
static void FormExportFdLedger(const UbseMemFdExportObjMap &fdExportObjMap,
                               std::unordered_map<std::string, LedgerDymaticInfo> &midLedgerInfo)
{
    for (const auto &[resourceId, fdExportObj] : fdExportObjMap) {
        FormExportLedger(fdExportObj, resourceId, LedgerType::FD, midLedgerInfo);
    }
}

/**
* @brief 将内部的fd借用转换为中间账本结构体，下同
* @param fdImportObjMap [in] 内部fd导入信息
* @param fdExportObjMap [in] 内部fd导出信息
* @param ledgerInfo [out] 中间账本结构体
*/
static void AssignFdLedger(const NodeMemDebtInfoMap &debtInfoMap,
                           std::vector<LedgerDymaticInfo> &ledgerInfo)
{
    UbseMemFdImportObjMap fdImportObjMap;
    UbseMemFdExportObjMap fdExportObjMap;
    for (const auto &[nodeId, nodeDebtInfo] : debtInfoMap) {
        for (const auto &[resourceId, fdImportObj] : nodeDebtInfo.fdImportObjMap) {
            std::string resourceIdNew = resourceId;
            resourceIdNew += "_";
            resourceIdNew += nodeId;
            fdImportObjMap[resourceIdNew] = fdImportObj;
        }
        for (const auto &[resourceId, fdExportObj] : nodeDebtInfo.fdExportObjMap) {
            fdExportObjMap[resourceId] = fdExportObj;
        }
    }
    std::unordered_map<std::string, LedgerDymaticInfo> midLedgerInfo; // resourceId : ledgerInfo
    // 非共享账本成功借用要求import的状态机转换为success，与export无关，所以依赖import对象查找export对象
    // 通过import账本构建中间账本的借入部分
    FormImportFdLedger(fdImportObjMap, midLedgerInfo);
    // 通过export构建中间账本的借出部分
    FormExportFdLedger(fdExportObjMap, midLedgerInfo);
    // 将map转换为vector
    for (const auto &[_, value] : midLedgerInfo) {
        ledgerInfo.emplace_back(value);
    }
}

static void FormImportNumaLedger(const UbseMemNumaImportObjMap &numaImportObjMap,
                                 std::unordered_map<std::string, LedgerDymaticInfo> &midLedgerInfo)
{
    for (const auto &[resourceId, numaImportObj] : numaImportObjMap) {
        LedgerType type = numaImportObj.req.udsInfo.pid > 0 ? LedgerType::APP : LedgerType::WATER;
        FormImportLedger(numaImportObj, resourceId, type, midLedgerInfo);
        if (midLedgerInfo.find(resourceId) != midLedgerInfo.end()) {
            if (numaImportObj.status.importResults.empty()) {
                continue;
            }
            midLedgerInfo[resourceId].remoteNumaId = numaImportObj.status.importResults[0].numaId;
            int16_t tempNumaId{0};
            if (memcpy_s(&tempNumaId, sizeof(tempNumaId), numaImportObj.req.usrInfo, sizeof(tempNumaId)) != EOK) {
                UBSE_LOG_ERROR << "memcpy_s failed.";
            }
            midLedgerInfo[resourceId].borrowNumaId = tempNumaId;
        }
    }
}

static void FormExportNumaLedger(const UbseMemNumaExportObjMap &numaExportObjMap,
                                 std::unordered_map<std::string, LedgerDymaticInfo> &midledgerInfo)
{
    for (const auto &[resourceId, numaExportObj] : numaExportObjMap) {
        LedgerType type = numaExportObj.req.udsInfo.pid > 0 ? LedgerType::APP : LedgerType::WATER;
        FormExportLedger(numaExportObj, resourceId, type, midledgerInfo);
        if (midledgerInfo.find(resourceId) != midledgerInfo.end()) {
            int16_t tempNumaId{0};
            if (memcpy_s(&tempNumaId, sizeof(tempNumaId), numaExportObj.req.usrInfo, sizeof(tempNumaId)) != EOK) {
                UBSE_LOG_ERROR << "memcpy_s failed.";
            }
            midledgerInfo[resourceId].borrowNumaId = tempNumaId;
        }
    }
}

static void AssignNumaLedger(const NodeMemDebtInfoMap &debtInfoMap,
                             std::vector<LedgerDymaticInfo> &ledgerInfos)
{
    UbseMemNumaImportObjMap numaImportObjMap;
    UbseMemNumaExportObjMap numaExportObjMap;
    for (const auto &[nodeId, nodeDebtInfo] : debtInfoMap) {
        for (const auto &[resourceId, numaImportObj] : nodeDebtInfo.numaImportObjMap) {
            std::string resourceIdNew = resourceId;
            resourceIdNew += "_";
            resourceIdNew += nodeId;
            numaImportObjMap[resourceIdNew] = numaImportObj;
        }
        for (const auto &[resourceId, numaExportObj] : nodeDebtInfo.numaExportObjMap) {
            numaExportObjMap[resourceId] = numaExportObj;
        }
    }
    std::unordered_map<std::string, LedgerDymaticInfo> midLedgerInfos;
    // 非共享账本成功借用要求import的状态机转换为success，与export无关，所以依赖import对象查找export对象
    // 通过import账本构建中间账本的借入部分
    FormImportNumaLedger(numaImportObjMap, midLedgerInfos);
    // 通过export构建中间账本的借出部分
    FormExportNumaLedger(numaExportObjMap, midLedgerInfos);
    // 将map转换为vector
    for (const auto &[_, value] : midLedgerInfos) {
        ledgerInfos.emplace_back(value);
    }
}

static void FormImportAddrLedger(const UbseMemAddrImportObjMap &addrImportObjMap,
                                 std::unordered_map<std::string, LedgerDymaticInfo> &midLedgerInfo)
{
    for (const auto &[resourceId, addrImportObj] : addrImportObjMap) {
        FormImportLedger(addrImportObj, resourceId, LedgerType::ADDR, midLedgerInfo);
        if (midLedgerInfo.find(resourceId) != midLedgerInfo.end()) {
            if (addrImportObj.status.importResults.empty()) {
                continue;
            }
            midLedgerInfo[resourceId].remoteNumaId = addrImportObj.status.importResults[0].numaId;
        }
    }
}

static void FormExportAddrLedger(const UbseMemAddrExportObjMap &addrExportObjMap,
                                 std::unordered_map<std::string, LedgerDymaticInfo> &midLedgerInfo)
{
    for (const auto &[resourceId, addrExportObj] : addrExportObjMap) {
        FormExportLedger(addrExportObj, resourceId, LedgerType::ADDR, midLedgerInfo);
    }
}

static void AssignAddrLedger(const NodeMemDebtInfoMap &debtInfoMap,
                             std::vector<LedgerDymaticInfo> &ledgerInfos)
{
    UbseMemAddrImportObjMap addrImportObjMap;
    UbseMemAddrExportObjMap addrExportObjMap;
    for (const auto &[nodeId, nodeDebtInfo] : debtInfoMap) {
        for (const auto &[resourceId, addrImportObj] : nodeDebtInfo.addrImportObjMap) {
            std::string resourceIdNew = resourceId;
            resourceIdNew += "_";
            resourceIdNew += nodeId;
            addrImportObjMap[resourceIdNew] = addrImportObj;
        }
        for (const auto &[resourceId, addrExportObj] : nodeDebtInfo.addrExportObjMap) {
            addrExportObjMap[resourceId] = addrExportObj;
        }
    }
    std::unordered_map<std::string, LedgerDymaticInfo> midLedgerInfos;
    // 非共享账本成功借用要求import的状态机转换为success，与export无关，所以依赖import对象查找export对象
    // 通过import账本构建中间账本的借入部分
    FormImportAddrLedger(addrImportObjMap, midLedgerInfos);
    // 通过export构建中间账本的借出部分
    FormExportAddrLedger(addrExportObjMap, midLedgerInfos);
    // 将map转换为vector
    for (const auto &[_, value] : midLedgerInfos) {
        ledgerInfos.emplace_back(value);
    }
}

static void FormShmImportLedger(const UbseMemBorrowImportBaseObj &importBaseObj,
                                const std::string &resourceId, const std::string &nodeId,
                                std::unordered_map<std::string, LedgerDymaticInfo> &midLedgerInfo)
{
    if (importBaseObj.status.state != ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_IMPORT_SUCCESS) {
        return;
    }
    if (midLedgerInfo.find(resourceId) == midLedgerInfo.end()) {
        LedgerDymaticInfo singleLedgerInfo{};
        singleLedgerInfo.resourceId = resourceId;
        singleLedgerInfo.type = LedgerType::SHARE;
        midLedgerInfo[resourceId] = singleLedgerInfo;
    }
    FormNumaInfoUsingImportObj(importBaseObj.algoResult.importNumaInfos, midLedgerInfo[resourceId]);
    for (const auto &obmmInfo : importBaseObj.status.importResults) {
        midLedgerInfo[resourceId].borrowMemId[nodeId].emplace_back(obmmInfo.memId);
    }
}

static void FormImportShareLedger(
    const std::unordered_map<std::string, std::vector<UbseMemShareBorrowImportObj>> &shareImportObjMap,
    std::unordered_map<std::string, LedgerDymaticInfo> &midLedgerInfos)
{
    for (const auto &[resourceId, shareImportObjs] : shareImportObjMap) {
        for (const auto &shareImportObj : shareImportObjs) {
            FormShmImportLedger(shareImportObj, resourceId, shareImportObj.importNodeId, midLedgerInfos);
        }
    }
}

static void FormExportShareLedger(const UbseMemShareExportObjMap &shareExportObjMap,
                                  std::unordered_map<std::string, LedgerDymaticInfo> &midLedgerInfos)
{
    for (const auto &[resourceId, shareExportObj] : shareExportObjMap) {
        if (shareExportObj.status.state != ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_EXPORT_SUCCESS) {
            continue;
        }
        if (midLedgerInfos.find(resourceId) == midLedgerInfos.end()) {
            LedgerDymaticInfo singleLedgerInfo{};
            singleLedgerInfo.resourceId = resourceId;
            singleLedgerInfo.type = LedgerType::SHARE;
            midLedgerInfos[resourceId] = singleLedgerInfo;
        }
        std::string nodeId = shareExportObj.algoResult.exportNumaInfos.empty() ?
                                 "" :
                                 shareExportObj.algoResult.exportNumaInfos.front().nodeId;
        midLedgerInfos[resourceId].lentNodeId = nodeId;
        FormNumaInfoUsingExportObj(shareExportObj.algoResult.exportNumaInfos, midLedgerInfos[resourceId]);
        for (const auto &obmmInfo : shareExportObj.status.exportObmmInfo) {
            midLedgerInfos[resourceId].lentMemId.emplace_back(obmmInfo.memId);
        }
        for (const auto &obmmInfo : shareExportObj.status.exportObmmInfo) {
            ObmmDesc tmpObmmInfo{};
            AssignObmmDes(obmmInfo.desc, tmpObmmInfo);
            midLedgerInfos[resourceId].obmmDesc.emplace_back(tmpObmmInfo);
        }
    }
}

static void AssignShareLedger(const NodeMemDebtInfoMap &debtInfoMap,
                              std::vector<LedgerDymaticInfo> &ledgerInfos)
{
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowImportObj>> shareImportObjMap;
    UbseMemShareExportObjMap shareExportObjMap;
    for (const auto &[_, nodeDebtInfo] : debtInfoMap) {
        for (const auto &[resourceId, shareImportObj] : nodeDebtInfo.shareImportObjMap) {
            shareImportObjMap[resourceId].emplace_back(shareImportObj);
        }
        for (const auto &[resourceId, shareExportObj] : nodeDebtInfo.shareExportObjMap) {
            shareExportObjMap[resourceId] = shareExportObj;
        }
    }
    std::unordered_map<std::string, LedgerDymaticInfo> midLedgerInfo; // resourceId : ledgerInfo
    // 通过export构建中间账本的借出部分
    FormExportShareLedger(shareExportObjMap, midLedgerInfo);
    // 共享内存单方面成功就视为成功
    // 通过import账本构建中间账本的借入部分
    FormImportShareLedger(shareImportObjMap, midLedgerInfo);
    // 将map转换为vector
    for (const auto &[_, value] : midLedgerInfo) {
        ledgerInfos.emplace_back(value);
    }
}

static void FormExportMemId(const std::vector<ubse::adapter_plugins::mmi::UbseMemObmmInfo> &obmms,
                            LedgerDymaticInfo &ledger)
{
    for (const auto &obmm : obmms) {
        ledger.lentMemId.emplace_back(obmm.memId);
    }
}

static void RefillLedger(const NodeMemDebtInfoMap &debtInfoMap,
                         std::vector<LedgerDymaticInfo> &ledgerInfos)
{
    for (auto &ledgerInfo : ledgerInfos) {
        if (!ledgerInfo.lentNodeId.empty() || ledgerInfo.borrowNodeId.empty()) {
            continue;
        }
        std::string nodeId = ledgerInfo.borrowNodeId.front();
        if (debtInfoMap.find(nodeId) == debtInfoMap.end()) {
            continue;
        }
        if (ledgerInfo.type == LedgerType::APP || ledgerInfo.type == LedgerType::WATER) {
            auto itNuma = debtInfoMap.at(nodeId).numaImportObjMap.find(ledgerInfo.resourceId);
            if (itNuma == debtInfoMap.at(nodeId).numaImportObjMap.end()) {
                continue;
            }
            FormNumaInfoUsingExportObj(itNuma->second.algoResult.exportNumaInfos, ledgerInfo);
            FormExportMemId(itNuma->second.exportObmmInfo, ledgerInfo);
        }
        if (ledgerInfo.type == LedgerType::FD) {
            auto itFd = debtInfoMap.at(nodeId).fdImportObjMap.find(ledgerInfo.resourceId);
            if (itFd == debtInfoMap.at(nodeId).fdImportObjMap.end()) {
                continue;
            }
            FormNumaInfoUsingExportObj(itFd->second.algoResult.exportNumaInfos, ledgerInfo);
            FormExportMemId(itFd->second.exportObmmInfo, ledgerInfo);
        }
        if (ledgerInfo.type == LedgerType::SHARE) {
            auto itShare = debtInfoMap.at(nodeId).shareImportObjMap.find(ledgerInfo.resourceId);
            if (itShare == debtInfoMap.at(nodeId).shareImportObjMap.end()) {
                continue;
            }
            FormNumaInfoUsingExportObj(itShare->second.algoResult.exportNumaInfos, ledgerInfo);
            FormExportMemId(itShare->second.exportObmmInfo, ledgerInfo);
        }
        if (ledgerInfo.type == LedgerType::ADDR) {
            auto itAddr = debtInfoMap.at(nodeId).addrImportObjMap.find(ledgerInfo.resourceId);
            if (itAddr == debtInfoMap.at(nodeId).addrImportObjMap.end()) {
                continue;
            }
            FormNumaInfoUsingExportObj(itAddr->second.algoResult.exportNumaInfos, ledgerInfo);
            FormExportMemId(itAddr->second.exportObmmInfo, ledgerInfo);
        }
    }
}

/**
* @brief 从内部模块获取账本信息并填入中间账本结构体
* @param debtInfoMap [in] 内部模块的账本信息
* @param ledgerInfo [out] 中间账本信息
* @return
*/
static void GetledgerInfoFromInner(const NodeMemDebtInfoMap &debtInfoMap,
                                   std::vector<LedgerDymaticInfo> &ledgerInfo)
{
    AssignNumaLedger(debtInfoMap, ledgerInfo);
    AssignFdLedger(debtInfoMap, ledgerInfo);
    AssignAddrLedger(debtInfoMap, ledgerInfo);
    AssignShareLedger(debtInfoMap, ledgerInfo);
    RefillLedger(debtInfoMap, ledgerInfo);
}

/**
* @brief 调用内部模块获得numa静态信息和账本动态信息
* @param numaInfo [out] numa静态信息
* @param ledgerInfo [out] 账本动态信息
*/
uint32_t GetMemInfoFromInner(std::vector<NumaStaticInfo> &numaInfo, std::vector<LedgerDymaticInfo> &ledgerInfo)
{
    // 从内部模块获取numa静态信息和账本动态信息
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos =
        ubse::nodeController::UbseNodeController::GetInstance().GetAllNodes();
    NodeMemDebtInfoMap debtInfoMap;
    uint32_t ret = controller::UbseGetMemDebtInfoFromMaster("", debtInfoMap);
    if (ret != 0) {
        // 由下层模块打日志
        return ret;
    }
    // 构建中间numa静态信息
    AssignNumaInfo(nodeInfos, numaInfo);
    // 构建账本中间信息
    GetledgerInfoFromInner(debtInfoMap, ledgerInfo);
    return 0;
}

static void FillBorrowedLentInfoList(const std::string &nodeId, const std::vector<LedgerDymaticInfo> &ledgerInfo,
                                     UbseBorrowedLentInfoList &outList);

static void FillSingleLedgerInfo(const UbseNodeBorrowLentInfo &singleLedgerInfo,
                                 std::unordered_map<std::string, UbseNumaLedgerInfo> &numaLedger)
{
    for (const auto &borrowItem : singleLedgerInfo.borrowedItem) {
        // 没有对应节点的情况下创建空信息
        if (numaLedger.find(singleLedgerInfo.nodeId) == numaLedger.end()) {
            UbseNumaLedgerInfo numaLedgerInfo{};
            numaLedger[singleLedgerInfo.nodeId] = numaLedgerInfo;
        }
        // 填写借入信息
        numaLedger[singleLedgerInfo.nodeId][borrowItem.numaId].first += borrowItem.size;
    }
    for (const auto &lentItem : singleLedgerInfo.lentItem) {
        // 没有对应节点的情况下创建空信息
        if (numaLedger.find(singleLedgerInfo.nodeId) == numaLedger.end()) {
            UbseNumaLedgerInfo numaLedgerInfo{};
            numaLedger[singleLedgerInfo.nodeId] = numaLedgerInfo;
        }
        // 填写借出信息
        numaLedger[singleLedgerInfo.nodeId][lentItem.numaId].second += lentItem.size;
    }
}

/**
* @brief 从借入借出信息中得到每个numa的借入借出信息
* @param ledgerInfo [in]借入借出信息
* @param numaLedger [out] nodeId : { numaId : {borrowSize, lendSize}}
*/
static void GetLedgerFromBorrowLendInfo(const std::vector<LedgerDymaticInfo> &ledgerInfo,
                                        std::unordered_map<std::string, UbseNumaLedgerInfo> &numaLedger)
{
    // 获取账本信息
    UbseBorrowedLentInfoList numaLedgerList;
    FillBorrowedLentInfoList("", ledgerInfo, numaLedgerList);
    // 填写numa的账本信息
    for (const auto &singleLedgerInfo : numaLedgerList) {
        FillSingleLedgerInfo(singleLedgerInfo, numaLedger);
    }
}

/**
* @brief 通过全量账本信息填写共享内存信息
* @param ledgerInfo [in] 全量账本信息
* @param sharedMem [out] 共享内存信息 {nodeId: {numaId: size}}
*/
static void GetNumaShareSize(const std::vector<LedgerDymaticInfo> &ledgerInfo,
                             std::unordered_map<std::string, UbseNumaSharedMem> &sharedMem)
{
    for (const auto &ledger : ledgerInfo) {
        if (ledger.type != LedgerType::SHARE) {
            continue;
        }
        for (size_t i = 0; i < ledger.lentNumaIdList.size(); ++i) {
            sharedMem[ledger.lentNodeId][ledger.lentNumaIdList[i]] += ledger.lentNumaSizeList[i];
        }
    }
}

uint32_t UbseAllNumaInfo(std::vector<UbseNumaNodeInfo> &numaNodeInfoList)
{
    // 获得静态numa信息和动态账本信息
    std::vector<NumaStaticInfo> numaInfo;
    std::vector<LedgerDymaticInfo> ledgerInfo;
    uint32_t ret = GetMemInfoFromInner(numaInfo, ledgerInfo);
    if (ret != 0) {
        UBSE_LOG_ERROR << "get information from inner module failed, code=" << std::to_string(ret);
        return ret;
    }
    // 获得节点-numa级的账本
    std::unordered_map<std::string, UbseNumaLedgerInfo> numaledger{};
    GetLedgerFromBorrowLendInfo(ledgerInfo, numaledger);
    // 获得共享内存大小
    std::unordered_map<std::string, UbseNumaSharedMem> sharedMem{};
    GetNumaShareSize(ledgerInfo, sharedMem);
    // 填写numa信息
    for (const auto &numa : numaInfo) {
        UbseNumaLedgerInfo numaLedgerInfo{};
        UbseNumaNodeInfo numaRes{};
        numaRes.nodeId = numa.nodeId;
        numaRes.hostName = numa.hostName;
        numaRes.numaId = numa.numaId;
        numaRes.socketId = numa.soketId;
        numaRes.mCpuList = std::vector<uint16_t>(numa.cpuList);
        numaRes.mReservedMemRatio = numa.ratio;
        numaRes.mMemTotal = numa.totalMemSize;
        numaRes.mMemFree = numa.freeMemSize;
        numaRes.nrHugepages = numa.nrHugePages;
        numaRes.freeHugepages = numa.freeHugePages;
        numaRes.mTimestamp = numa.timeStamp;
        uint64_t borrowSize = 0, lendSize = 0;
        if (numaledger.find(numa.nodeId) != numaledger.end() &&
            numaledger[numa.nodeId].find(numa.numaId) != numaledger[numa.nodeId].end()) {
            borrowSize = numaledger[numa.nodeId][numa.numaId].first;
            lendSize = numaledger[numa.nodeId][numa.numaId].second;
        }
        numaRes.mMemBorrowed = borrowSize;
        numaRes.mMemLent = lendSize;
        numaRes.mMemShared = sharedMem.find(numa.nodeId) != sharedMem.end() &&
                                     sharedMem[numa.nodeId].find(numa.numaId) != sharedMem[numa.nodeId].end() ?
                                 sharedMem[numa.nodeId][numa.numaId] :
                                 0;
        numaNodeInfoList.emplace_back(numaRes);
    }
    return 0;
}

/**
* @brief 提取所需的obmm描述构建字符串数组
* @param obmmDescs [in] obmm描述信息
* @param res [out] 描述信息字符串
*/
static void GetObmmDescVecStr(const std::vector<ObmmDesc> &obmmDescs, std::string &res)
{
    std::vector<std::string> jsonVec;
    if (!UbseJsonUtil::ConvertVector2JsonStr(jsonVec, res)) {
        UBSE_LOG_ERROR << "OBMM information form failed,";
        res = "[]";
    }
}

/**
* @brief 判断nodeId是否为目标nodeId。当nodeId为空的时候返回真；当nodeId不为空的时候，与目标nodeId相同为真
* @param nodeId [in] 待判断nodeId
* @param targetNode [out] 目标nodeId
* @return bool
*/
static bool IsNodeTarget(const std::string &nodeId, const std::string &targetNode)
{
    return nodeId.empty() || nodeId == targetNode;
}

static void FillBorrowAccount(const LedgerDymaticInfo &ledger, const std::string &nodeId, UbseBorrowAccountMap &outList)
{
    std::string borrowNodeId = ledger.borrowNodeId.empty() ? "" : ledger.borrowNodeId.front();
    if (IsNodeTarget(nodeId, borrowNodeId)) {
        // 如果资源id不在结果中则创建空账本
        std::string resourceIdNew = ledger.resourceId;
        resourceIdNew += "_";
        resourceIdNew += borrowNodeId;
        resourceIdNew += "_" + std::to_string(static_cast<int>(ledger.type));
        if (outList.find(resourceIdNew) == outList.end()) {
            UbseMemoryBorrowInfo borrowInfo{};
            borrowInfo.name = ledger.resourceId;
            borrowInfo.type = ledger.type;
            outList[resourceIdNew] = borrowInfo;
        }
        // 填写借入账本
        if (!ledger.borrowNodeId.empty()) {
            outList[resourceIdNew].borrowNodeId = borrowNodeId;
        }
        if (!ledger.borrowSocketIdList.empty()) {
            outList[resourceIdNew].borrowSocketIdList = ledger.borrowSocketIdList.begin()->second;
        }
        if (!ledger.borrowNumaIdList.empty()) {
            outList[resourceIdNew].borrowNumaIdList = ledger.borrowNumaIdList.begin()->second;
        }
        if (!ledger.borrowNumaSizeList.empty()) {
            outList[resourceIdNew].borrowNumaSizeList = ledger.borrowNumaSizeList.begin()->second;
        }
        if (!ledger.borrowMemId.empty()) {
            outList[resourceIdNew].borrowMemId = ledger.borrowMemId.begin()->second;
        }
        outList[resourceIdNew].remoteNumaId = ledger.remoteNumaId;
        outList[resourceIdNew].borrowNumaId = ledger.borrowNumaId;
    }
}

static void FillLentAccount(const LedgerDymaticInfo &ledger, const std::string nodeId, UbseBorrowAccountMap &outList)
{
    std::string borrowNodeId = ledger.borrowNodeId.empty() ? "" : ledger.borrowNodeId.front();
    if (IsNodeTarget(nodeId, ledger.lentNodeId)) {
        // 如果资源id不在结果中则创建空信息
        std::string resourceIdNew = ledger.resourceId;
        resourceIdNew += "_";
        resourceIdNew += borrowNodeId;
        resourceIdNew += "_" + std::to_string(static_cast<int>(ledger.type));
        if (outList.find(resourceIdNew) == outList.end()) {
            UbseMemoryBorrowInfo borrowInfo{};
            borrowInfo.name = ledger.resourceId;
            outList[resourceIdNew] = borrowInfo;
        }
        // 填写借出账本
        outList[resourceIdNew].lentNodeId = ledger.lentNodeId;
        outList[resourceIdNew].lentSocketIdList = ledger.lentSocketIdList;
        outList[resourceIdNew].lentNumaIdList = ledger.lentNumaIdList;
        outList[resourceIdNew].lentNumaSizeList = ledger.lentNumaSizeList;
        outList[resourceIdNew].lentMemId = ledger.lentMemId;
        outList[resourceIdNew].size = 0;
        for (const auto &numaSize : ledger.lentNumaSizeList) {
            outList[resourceIdNew].size += numaSize;
        }
        std::string obmmDesString;
        GetObmmDescVecStr(ledger.obmmDesc, obmmDesString);
        outList[resourceIdNew].lentObmmDesc = obmmDesString;
    }
}

/**
* @brief 使用中间账本信息构造目标账本信息
* @param nodeId [in] 当空是查询所有账本信息，当不为空的时候查询该节点所存在的账本
* @param ledgerInfo [in] 中间账本信息
* @param outList [out] 目标账本信息
*/
static void FillBorrowedAccountMap(const std::string &nodeId, const std::vector<LedgerDymaticInfo> &ledgerInfo,
                                   UbseBorrowAccountMap &outList)
{
    for (const auto &ledger : ledgerInfo) {
        // 普通借用不包括共享借用
        if (ledger.type == LedgerType::SHARE) {
            continue;
        }
        // 填写借入账本信息
        FillBorrowAccount(ledger, nodeId, outList);
        // 填写借出账本
        FillLentAccount(ledger, nodeId, outList);
    }
}

uint32_t UbseAllBorrowAccountInfo(const std::string &nodeId, UbseBorrowAccountMap &accountMap)
{
    // 获取numa静态信息和账本动态信息
    std::vector<NumaStaticInfo> numaInfo;
    std::vector<LedgerDymaticInfo> ledgerInfo;
    uint32_t ret = GetMemInfoFromInner(numaInfo, ledgerInfo);
    if (ret != 0) {
        UBSE_LOG_ERROR << "get information from inner module failed, code=" << std::to_string(ret);
        return ret;
    }
    // 填写账本结果
    FillBorrowedAccountMap(nodeId, ledgerInfo, accountMap);
    return UBSE_OK;
}

uint32_t UbseAllShmAccountInfo(UbseShmAccountMap &outMap)
{
    // 获取numa静态信息和账本动态信息
    std::vector<NumaStaticInfo> numaInfo;
    std::vector<LedgerDymaticInfo> ledgerInfo;
    uint32_t ret = GetMemInfoFromInner(numaInfo, ledgerInfo);
    if (ret != 0) {
        UBSE_LOG_ERROR << "Get information from inner module failed, code=" << std::to_string(ret);
        return ret;
    }
    // 填写共享账本信息
    for (const auto &singleLedger : ledgerInfo) {
        if (singleLedger.type != LedgerType::SHARE) {
            continue;
        }
        UbseShmAccountInfo shmAccountInfo{};
        shmAccountInfo.exportMemId = singleLedger.lentMemId;
        shmAccountInfo.exportNode = singleLedger.lentNodeId;
        shmAccountInfo.importMap = std::unordered_map<std::string, std::vector<uint64_t>>(singleLedger.borrowMemId);
        shmAccountInfo.size = 0;
        for (const auto &lentNumaSize : singleLedger.lentNumaSizeList) {
            shmAccountInfo.size += lentNumaSize;
        }
        outMap[singleLedger.resourceId] = shmAccountInfo;
    }
    return 0;
}

// 辅助类型定义 exportNodeId,importNodeId,numaId
using BorrowedLentKey = std::tuple<std::string, std::string, uint32_t>;
const int EXPORT_ID_KEY = 0;
const int IMPORT_ID_KEY = 1;
const int NUMA_ID_KEY = 2;

// 定义一个辅助结构用于累加
struct AccumulatedItem {
    uint64_t memory{0}; // 累加的内存大小，单位字节
};

// 自定义哈希函数
struct BorrowedLentKeyHash {
    std::size_t operator()(const BorrowedLentKey &k) const
    {
        return std::hash<std::string>()(std::get<EXPORT_ID_KEY>(k)) ^
               std::hash<std::string>()(std::get<IMPORT_ID_KEY>(k)) ^
               (std::hash<unsigned int>()(std::get<NUMA_ID_KEY>(k)) << 1);
    }
};

// 自定义相等比较函数
struct BorrowedLentKeyEqual {
    bool operator()(const BorrowedLentKey &lhs, const BorrowedLentKey &rhs) const
    {
        return lhs == rhs;
    }
};

// 使用自定义哈希函数和相等比较函数的unordered_map
using AccumulatedMap = std::unordered_map<BorrowedLentKey, AccumulatedItem, BorrowedLentKeyHash, BorrowedLentKeyEqual>;
using BorrowedLentInfoMap = std::unordered_map<std::string, UbseNodeBorrowLentInfo>;

void AccumulateLentInfo(const UbseBorrowAccountMap &accountMap, AccumulatedMap &lentMap)
{
    for (const auto &[_, memoryBorrowInfo] : accountMap) {
        for (size_t i = 0; i < memoryBorrowInfo.lentNumaIdList.size(); ++i) {
            BorrowedLentKey key = std::make_tuple(memoryBorrowInfo.lentNodeId, memoryBorrowInfo.borrowNodeId,
                                                  memoryBorrowInfo.lentNumaIdList[i]);
            if (lentMap[key].memory > UINT32_MAX - memoryBorrowInfo.lentNumaSizeList[i]) {
                break;
            }
            lentMap[key].memory += memoryBorrowInfo.lentNumaSizeList[i];
        }
    }
}

void AccumulateBorrowedInfo(const UbseBorrowAccountMap &accountMap, AccumulatedMap &borrowedMap)
{
    for (const auto &[_, memoryBorrowInfo] : accountMap) {
        for (size_t i = 0; i < memoryBorrowInfo.borrowNumaIdList.size(); ++i) {
            BorrowedLentKey key = std::make_tuple(memoryBorrowInfo.lentNodeId, memoryBorrowInfo.borrowNodeId,
                                                  memoryBorrowInfo.borrowNumaIdList[i]);
            borrowedMap[key].memory += memoryBorrowInfo.borrowNumaSizeList[i];
        }
    }
}

void BuildInfoMap(AccumulatedMap &dataMap, BorrowedLentInfoMap &infoMap, bool isLending)
{
    for (const auto &[key, item] : dataMap) {
        const auto &importNodeId = std::get<IMPORT_ID_KEY>(key);
        const auto &exportNodeId = std::get<EXPORT_ID_KEY>(key);
        uint32_t numaId = std::get<NUMA_ID_KEY>(key);
        auto &info = infoMap[isLending ? exportNodeId : importNodeId];

        UbseBorrowLentItem entry;
        entry.nodeId = isLending ? importNodeId : exportNodeId;
        entry.numaId = numaId;
        entry.size = item.memory;

        isLending ? info.lentItem.push_back(entry) : info.borrowedItem.push_back(entry);
        info.nodeId = isLending ? exportNodeId : importNodeId;
    }
}

void FillBorrowedLentInfoList(const std::string &nodeId, const std::vector<LedgerDymaticInfo> &ledgerInfo,
                              UbseBorrowedLentInfoList &outList)
{
    UbseBorrowAccountMap accountMap;
    FillBorrowedAccountMap("", ledgerInfo, accountMap);
    AccumulatedMap borrowedMap;
    AccumulatedMap lentMap;
    BorrowedLentInfoMap infoMap;

    AccumulateBorrowedInfo(accountMap, borrowedMap);
    AccumulateLentInfo(accountMap, lentMap);

    BuildInfoMap(borrowedMap, infoMap, false); // false for borrowed
    BuildInfoMap(lentMap, infoMap, true);      // true for lent

    for (auto &[_nodeId, info] : infoMap) {
        if (nodeId.empty() || nodeId == _nodeId) {
            outList.push_back(std::move(info));
        }
    }
}

uint32_t UbseGetBorrowedLentInfo(const std::string &nodeId, UbseBorrowedLentInfoList &outList)
{
    // 获得numa静态信息和账本动态信息
    std::vector<NumaStaticInfo> numaInfo;
    std::vector<LedgerDymaticInfo> ledgerInfo;
    uint32_t ret = GetMemInfoFromInner(numaInfo, ledgerInfo);
    if (ret != 0) {
        UBSE_LOG_ERROR << "Get information from inner module failed, code=" << std::to_string(ret);
        return ret;
    }
    // 填写numa借用信息
    FillBorrowedLentInfoList(nodeId, ledgerInfo, outList);
    return UBSE_OK;
}
} // namespace ubse::mem::account