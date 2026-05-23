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

#include "ubse_mem_controller.h"
#include <securec.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "ubse_com_module.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_mem_account.h"
#include "ubse_mem_controller_api_agent.h"
#include "ubse_mem_controller_module.h"
#include "ubse_mem_controller_query_api.h"
#include "ubse_mmi_interface.h"
#include "ubse_node_controller.h"
#include "api/ubse_mem_controller_helper.h"
#include "message/ubse_mem_opt_req_simpo.h"
#include "message/ubse_mem_opt_result_simpo.h"
#include "src/controllers/mem/mem_controller/debt/ubse_mem_debt_info.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::nodeController;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::mem::account;
using namespace ubse::election;
using namespace com;
using namespace ubse::mem::controller::message;

uint32_t UbseQueryResult(const std::string& name, UbseMemResult& result, UbseMemBorrowType borrowType)
{
    UbseRoleInfo currentInfo{};
    auto ret = UbseGetCurrentNodeInfo(currentInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get currentNode info failed, " << FormatRetCode(ret);
        return ret;
    }
    UbseRoleInfo masterInfo{};
    ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master info failed, " << FormatRetCode(ret);
        return ret;
    }

    SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP),
                        static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_GET_OPT_RES)};
    UbseMemOptReqSimpoPtr reqPtr = new (std::nothrow) UbseMemOptReqSimpo();
    if (reqPtr == nullptr) {
        UBSE_LOG_ERROR << "Allocate memory failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    reqPtr->SetOptRequest(name, currentInfo.nodeId, borrowType);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemOptResultSimpo();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Allocate memory failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }

    UbseContext& ubseContext = UbseContext::GetInstance();
    auto ubseComModule = ubseContext.GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "Communication module not init, " << FormatRetCode(UBSE_ERROR_MODULE_LOAD_FAILED);
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    ret = ubseComModule->RpcSend(sendParam, reqPtr, ubseResponsePtr, false);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Rpc sync send metric failed, " << FormatRetCode(ret);
        return ret;
    }

    auto ubseMemOptResultSimpoPtr = UbseBaseMessage::DeConvert<UbseMemOptResultSimpo>(ubseResponsePtr);
    result = ubseMemOptResultSimpoPtr->GetResp();
    return UBSE_OK;
}

// 辅助函数：检查节点是否在静态列表中
bool IsNodeInStaticList(const std::string& nodeId, const std::set<uint32_t>& staticNodeInfoList)

{
    try {
        auto id = std::stoull(nodeId);
        if (id > std::numeric_limits<uint32_t>::max()) {
            return false;
        }
        return staticNodeInfoList.find(static_cast<uint32_t>(id)) != staticNodeInfoList.end();
    } catch (const std::exception&) {
        return false;
    }
}

// 辅助函数：检查节点是否工作状态
bool IsNodeWorking(const std::string& nodeId,
                   const std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo>& nodeMap)
{
    auto it = nodeMap.find(nodeId);
    return it != nodeMap.end() && it->second.clusterState == UbseNodeClusterState::UBSE_NODE_WORKING;
}

// 辅助函数：获取借出节点ID
std::string GetLentNodeId(const UbseMemAlgoResult& algoResult)
{
    if (!algoResult.exportNumaInfos.empty()) {
        return algoResult.exportNumaInfos.front().nodeId;
    }
    return "";
}

// 辅助函数：获取借入节点ID
std::string GetBorrowNodeId(const UbseMemAlgoResult& algoResult)
{
    if (!algoResult.importNumaInfos.empty()) {
        return algoResult.importNumaInfos.front().nodeId;
    }
    return "";
}

// 辅助函数：处理NUMA导入对象
void ProcessNumaImportObj(const std::string& resourceId, const UbseMemNumaBorrowImportObj& numaImportObj,
                          const std::string& nodeId,
                          std::unordered_map<std::string, UbseNumaMemoryDebtInfo>& numaMemoryDebtInfoMap)
{
    if (numaImportObj.status.state != ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_IMPORT_SUCCESS) {
        return;
    }
    std::string borrowNodeId = GetBorrowNodeId(numaImportObj.algoResult);
    std::string lentNodeId = GetLentNodeId(numaImportObj.algoResult);
    if (!nodeId.empty() && nodeId != borrowNodeId && nodeId != lentNodeId) {
        return;
    }
    std::string resourceIdImportNodeId = resourceId + "_" + borrowNodeId;
    // 获取或创建账本信息
    auto [it, inserted] = numaMemoryDebtInfoMap.emplace(resourceIdImportNodeId, UbseNumaMemoryDebtInfo());
    if (inserted) {
        it->second.name = resourceId;
    }
    UbseNumaMemoryDebtInfo& debtInfo = it->second;
    debtInfo.borrowNodeId = borrowNodeId;
    debtInfo.lentNodeId = lentNodeId;
    // 处理借入信息
    debtInfo.borrowSocketIdList.clear();
    for (const auto& importNumaInfo : numaImportObj.algoResult.importNumaInfos) {
        debtInfo.borrowSocketIdList.emplace_back(importNumaInfo.socketId);
    }
    // 设置remoteNumaId
    if (!numaImportObj.status.importResults.empty()) {
        debtInfo.remoteNumaId = numaImportObj.status.importResults.front().numaId;
    }
    // 处理借入内存ID
    debtInfo.borrowMemId.clear();
    for (const auto& obmmInfo : numaImportObj.status.importResults) {
        debtInfo.borrowMemId.emplace_back(obmmInfo.memId);
    }
    // 处理借出信息
    debtInfo.lentSocketIdList.clear();
    debtInfo.lentNumaIdList.clear();
    debtInfo.lentNumaSizeList.clear();
    debtInfo.size = 0;
    for (const auto& exportNumaInfo : numaImportObj.algoResult.exportNumaInfos) {
        debtInfo.lentSocketIdList.emplace_back(exportNumaInfo.socketId);
        debtInfo.lentNumaIdList.emplace_back(exportNumaInfo.numaId);
        debtInfo.lentNumaSizeList.emplace_back(exportNumaInfo.size);
        debtInfo.size += exportNumaInfo.size;
    }
    // 处理借出内存ID
    debtInfo.lentMemId.clear();
    for (const auto& obmmInfo : numaImportObj.exportObmmInfo) {
        debtInfo.lentMemId.emplace_back(obmmInfo.memId);
    }
    // 处理用户私有数据
    if (memcpy_s(debtInfo.usrInfo, UBSE_MAX_USR_INFO_LEN, numaImportObj.req.usrInfo, UBSE_MAX_USR_INFO_LEN) !=
        UBSE_OK) {
        UBSE_LOG_ERROR << "MemCopy fail when copy usrInfo, numaImportObj name is " << numaImportObj.req.name;
    }
    debtInfo.uid = numaImportObj.req.udsInfo.uid;
    debtInfo.username = numaImportObj.req.udsInfo.username;
}

// 辅助函数：处理NUMA导出对象
void ProcessNumaExportObj(const std::string& resourceIdImportNodeId, const UbseMemNumaBorrowExportObj& numaExportObj,
                          const std::string& nodeId,
                          std::unordered_map<std::string, UbseNumaMemoryDebtInfo>& numaMemoryDebtInfoMap)
{
    if (numaExportObj.status.state != ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_EXPORT_SUCCESS) {
        return;
    }
    std::string borrowNodeId = GetBorrowNodeId(numaExportObj.algoResult);
    std::string lentNodeId = GetLentNodeId(numaExportObj.algoResult);
    if (!nodeId.empty() && nodeId != borrowNodeId && nodeId != lentNodeId) {
        return;
    }

    auto [it, inserted] = numaMemoryDebtInfoMap.emplace(resourceIdImportNodeId, UbseNumaMemoryDebtInfo());
    if (inserted) {
        it->second.name = numaExportObj.req.name;
    }

    UbseNumaMemoryDebtInfo& debtInfo = it->second;

    debtInfo.borrowNodeId = borrowNodeId;
    debtInfo.lentNodeId = lentNodeId;

    // 处理借入信息
    debtInfo.borrowSocketIdList.clear();

    for (const auto& importNumaInfo : numaExportObj.algoResult.importNumaInfos) {
        debtInfo.borrowSocketIdList.emplace_back(importNumaInfo.socketId);
    }

    // 处理借出信息
    debtInfo.lentSocketIdList.clear();
    debtInfo.lentNumaIdList.clear();
    debtInfo.lentNumaSizeList.clear();
    debtInfo.size = 0;

    for (const auto& exportNumaInfo : numaExportObj.algoResult.exportNumaInfos) {
        debtInfo.lentSocketIdList.emplace_back(exportNumaInfo.socketId);
        debtInfo.lentNumaIdList.emplace_back(exportNumaInfo.numaId);
        debtInfo.lentNumaSizeList.emplace_back(exportNumaInfo.size);
        debtInfo.size += exportNumaInfo.size;
    }

    // 处理借出内存ID
    debtInfo.lentMemId.clear();
    for (const auto& obmmInfo : numaExportObj.status.exportObmmInfo) {
        debtInfo.lentMemId.emplace_back(obmmInfo.memId);
    }
    // 处理用户私有数据
    if (memcpy_s(debtInfo.usrInfo, UBSE_MAX_USR_INFO_LEN, numaExportObj.req.usrInfo, UBSE_MAX_USR_INFO_LEN) !=
        UBSE_OK) {
        UBSE_LOG_ERROR << "MemCopy fail when copy usrInfo, numaExportObj name is " << numaExportObj.req.name;
    }
    debtInfo.uid = numaExportObj.req.udsInfo.uid;
    debtInfo.username = numaExportObj.req.udsInfo.username;
}

// 辅助函数：处理所有账本信息
void ProcessDebtInfo(const NodeMemDebtInfoMap& memDebtInfoMap, const std::string& nodeId,
                     const std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo>& nodeMap,
                     std::vector<UbseNumaMemoryDebtInfo>& debtInfos)
{
    std::unordered_map<std::string, UbseNumaMemoryDebtInfo> numaMemoryDebtInfoMap; // key:resourceId_borrowNodeId

    // 遍历所有节点账本信息
    for (const auto& nodeDebtInfoPair : memDebtInfoMap) {
        const std::string& tmpNodeId = nodeDebtInfoPair.first;
        const auto& nodeDebtInfo = nodeDebtInfoPair.second;

        // 跳过非工作状态的节点
        if (!IsNodeWorking(tmpNodeId, nodeMap)) {
            continue;
        }

        // 处理导入对象
        for (const auto& numaImportObjPair : nodeDebtInfo.numaImportObjMap) {
            const std::string& resourceId = numaImportObjPair.first;
            const auto& numaImportObj = numaImportObjPair.second;
            ProcessNumaImportObj(resourceId, numaImportObj, nodeId, numaMemoryDebtInfoMap);
        }

        // 处理导出对象
        for (const auto& numaExportObjPair : nodeDebtInfo.numaExportObjMap) {
            const std::string& resourceIdImportNodeId = numaExportObjPair.first;
            const auto& numaExportObj = numaExportObjPair.second;
            ProcessNumaExportObj(resourceIdImportNodeId, numaExportObj, nodeId, numaMemoryDebtInfoMap);
        }
    }

    // 将结果转移到输出向量
    debtInfos.reserve(numaMemoryDebtInfoMap.size());
    for (const auto& debtInfoPair : numaMemoryDebtInfoMap) {
        debtInfos.emplace_back(debtInfoPair.second);
    }
}

// 辅助函数：检查对账状态
UbseResult CheckReconciliationStatus(const std::set<uint32_t>& staticNodeInfoList,
                                     const std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo>& nodeMap,
                                     const std::string& targetNodeId)
{
    if (!targetNodeId.empty() && IsNodeWorking(targetNodeId, nodeMap)) {
        return UBSE_OK;
    }
    for (const auto& nodeId : staticNodeInfoList) {
        std::string tmpNodeId = std::to_string(nodeId);
        if (tmpNodeId == targetNodeId) {
            continue;
        }

        auto it = nodeMap.find(tmpNodeId);
        if (it == nodeMap.end()) {
            UBSE_LOG_WARN << "Node=" << tmpNodeId << " does not exist in node controller.";
            return UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS; // 2:部分成功，节点不存在
        }

        if (it->second.clusterState != UbseNodeClusterState::UBSE_NODE_WORKING) {
            UBSE_LOG_WARN << "Node=" << tmpNodeId
                          << " does not working, clusterState=" << static_cast<uint32_t>(it->second.clusterState);
            return UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS; // 2:部分成功，节点不在工作状态
        }
    }
    return UBSE_OK;
}

// 辅助函数：检查对账状态,过滤
UbseResult CheckReconciliationStatusWithFault(
    const std::set<uint32_t>& staticNodeInfoList,
    const std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo>& nodeMap, const std::string& targetNodeId)
{
    if (!targetNodeId.empty() && IsNodeWorking(targetNodeId, nodeMap)) {
        return UBSE_OK;
    }
    for (const auto& nodeId : staticNodeInfoList) {
        std::string tmpNodeId = std::to_string(nodeId);
        if (tmpNodeId == targetNodeId) {
            continue;
        }

        auto it = nodeMap.find(tmpNodeId);
        if (it == nodeMap.end()) {
            UBSE_LOG_WARN << "Node=" << tmpNodeId << " does not exist in node controller.";
            return UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS; // 2:部分成功，节点不存在
        }

        if (it->second.clusterState == UbseNodeClusterState::UBSE_NODE_INIT ||
            it->second.clusterState == UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
            UBSE_LOG_WARN << "Node=" << tmpNodeId
                          << " does not working, clusterState=" << static_cast<uint32_t>(it->second.clusterState);
            return UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS; // 2:部分成功，节点不在工作状态
        }
    }
    return UBSE_OK;
}

// 主函数
UbseResult UbseGetNumaMemDebtInfoWithNode(const std::string& nodeId, std::vector<UbseNumaMemoryDebtInfo>& debtInfos)
{
    UBSE_LOG_INFO << "The UbseGetNumaMemDebtInfoWithNode method begins execution, nodeId" << nodeId;
    // 参数校验
    if (nodeId.empty()) {
        UBSE_LOG_ERROR << "Invalid argument, nodeId is empty!";
        return UBSE_ERR_INVALID_ARG;
    }

    // 获取节点信息
    std::set<uint32_t> staticNodeInfoList = UbseNodeController::GetInstance().UbseGetAllDeployedNode();
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeMap =
        UbseNodeController::GetInstance().GetAllNodes();

    // 检查节点是否存在
    if (!IsNodeInStaticList(nodeId, staticNodeInfoList)) {
        UBSE_LOG_ERROR << "Invalid argument, nodeId does not exist static node list! nodeId=" << nodeId;
        return UBSE_ERR_INVALID_ARG;
    }

    // 获取账本信息
    NodeMemDebtInfoMap memDebtInfoMap;
    uint32_t ret = UbseGetMemDebtInfo("", memDebtInfoMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "The UbseGetMemDebtInfo method call failed.";
        return UBSE_ERR_INTERNAL;
    }

    // 处理账本信息
    ProcessDebtInfo(memDebtInfoMap, nodeId, nodeMap, debtInfos);

    // 检查对账完成状态
    auto retCode = CheckReconciliationStatusWithFault(staticNodeInfoList, nodeMap, nodeId);
    UBSE_LOG_INFO << "The UbseGetNumaMemDebtInfoWithNode method has been executed end, return code=" << retCode;
    return retCode;
}

UbseResult UbseGetNumaMemDebtInfo(std::vector<UbseNumaMemoryDebtInfo>& debtInfos)
{
    // 获取节点信息
    std::set<uint32_t> staticNodeInfoList = UbseNodeController::GetInstance().UbseGetAllDeployedNode();
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeMap =
        UbseNodeController::GetInstance().GetAllNodes();

    // 获取账本信息
    NodeMemDebtInfoMap memDebtInfoMap;
    uint32_t ret = UbseGetMemDebtInfo("", memDebtInfoMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "The UbseGetMemDebtInfo method call failed.";
        return UBSE_ERR_INTERNAL;
    }

    // 处理账本信息
    ProcessDebtInfo(memDebtInfoMap, "", nodeMap, debtInfos);

    // 检查对账完成状态
    auto retCode = CheckReconciliationStatus(staticNodeInfoList, nodeMap, "");
    UBSE_LOG_INFO << "The UbseGetNumaMemDebtInfo method has been executed end, return code="
                  << static_cast<uint32_t>(retCode);
    return retCode;
}

UbseResult ConvertImportDebtInfo(const std::pair<const std::string, UbseMemNumaBorrowImportObj>& numaImportObjPair,
                                 UbseNumaMemoryImportDebtInfo& debtInfo)
{
    // 处理导入对象
    const auto& numaImportObj = numaImportObjPair.second;
    if (numaImportObj.status.state != ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_IMPORT_SUCCESS) {
        return UBSE_ERROR;
    }
    std::string borrowNodeId = GetBorrowNodeId(numaImportObj.algoResult);
    debtInfo.name = numaImportObjPair.first;
    debtInfo.borrowNodeId = borrowNodeId;
    // 处理借入信息
    debtInfo.borrowSocketIdList.clear();
    for (const auto& importNumaInfo : numaImportObj.algoResult.importNumaInfos) {
        debtInfo.borrowSocketIdList.emplace_back(importNumaInfo.socketId);
    }
    // 处理借用内存大小
    debtInfo.size = 0;
    for (const auto& exportNumaInfo : numaImportObj.algoResult.exportNumaInfos) {
        debtInfo.size += exportNumaInfo.size;
    }
    // 处理用户私有数据
    if (memcpy_s(debtInfo.usrInfo, UBSE_MAX_USR_INFO_LEN, numaImportObj.req.usrInfo, UBSE_MAX_USR_INFO_LEN) !=
        UBSE_OK) {
        UBSE_LOG_ERROR << "MemCopy fail when copy usrInfo, numaImportObj name is " << numaImportObj.req.name;
    }
    // 设置remoteNumaId
    if (!numaImportObj.status.importResults.empty()) {
        debtInfo.remoteNumaId = numaImportObj.status.importResults.front().numaId;
    }
    return UBSE_OK;
}

UbseResult UbseGetNumaMemImportDebtInfoWithLocalNode(std::vector<UbseNumaMemoryImportDebtInfo>& debtInfos)
{
    UBSE_LOG_INFO << "The UbseGetNumaMemDebtInfoWithLocalNodeImport method begins execution";
    ubse::nodeController::UbseNodeInfo curNode = UbseNodeController::GetInstance().GetCurNode();
    if (curNode.localState == UbseNodeLocalState::UBSE_NODE_RESTORE) {
        UBSE_LOG_WARN << "current node is restoring from obmm.";
        return UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS;
    }
    NodeMemDebtInfo nodeDebtInfo = GetNodeMemDebtInfoById(curNode.nodeId);
    for (const auto& numaImportObjPair : nodeDebtInfo.numaImportObjMap) {
        UbseNumaMemoryImportDebtInfo debtInfo;
        if (ConvertImportDebtInfo(numaImportObjPair, debtInfo) != UBSE_OK) {
            continue;
        }
        debtInfos.emplace_back(debtInfo);
    }
    UBSE_LOG_INFO << "The UbseGetNumaMemDebtInfoWithLocalNodeImport method has been executed end.";
    return UBSE_OK;
}

UbseResult UbseMemNumaCreateWithLender(const std::string& name, const UbseMemBorrower& borrower,
                                       const std::vector<UbseMemNumaLender>& lenders,
                                       uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN], UbseMemNumaDesc& desc)
{
    // 参数校验
    auto ret = UbseMemCreateWithLenderReqIsValid(name, borrower, lenders);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 请求转换
    UbseMemNumaBorrowReq numaBorrowReq;
    UbseMemOperationResp resp;
    ret = ConvertUbseMemNumaCreateWithLenderReq(name, borrower, lenders, usrInfo, numaBorrowReq);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 调用内部ubse_mem_controller_api_agent.h接口
    ubse::mem::controller::agent::UbseMemNumaBorrow(numaBorrowReq, resp);
    if (resp.errorCode != UBSE_OK) {
        UBSE_LOG_INFO << "numa create with lender failed, return code=" << resp.errorCode;
        return resp.errorCode;
    }
    int32_t retCode = ubse::mem::controller::UbseMemNumaGetWithImportNode(resp.name, borrower.nodeId, desc);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemNumaGet, ErrorCode=" << retCode;
    }
    return resp.errorCode;
}

UbseResult UbseMemNumaCreate(const std::string& name, const UbseMemBorrower& borrower, const UbseMemNumaCreateOpt& opt,
                             UbseMemNumaDesc& desc)
{
    // 参数校验
    auto ret = UbseMemCreateReqIsValid(name, borrower, opt);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 请求转换
    UbseMemNumaBorrowReq numaBorrowReq;
    UbseMemOperationResp resp;
    ret = ConvertUbseMemNumaCreateReq(name, borrower, opt, numaBorrowReq);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 调用内部ubse_mem_controller_api_agent.h接口
    ubse::mem::controller::agent::UbseMemNumaBorrow(numaBorrowReq, resp);
    if (resp.errorCode != UBSE_OK) {
        UBSE_LOG_INFO << "numa create failed, return code=" << resp.errorCode;
        return resp.errorCode;
    }
    int32_t retCode = ubse::mem::controller::UbseMemNumaGetWithImportNode(resp.name, borrower.nodeId, desc);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemNumaGet, ErrorCode=" << retCode;
    }
    return resp.errorCode;
}

UbseResult UbseMemNumaCreateWithCandidate(const std::string& name, const UbseMemBorrower& borrower,
                                          const UbseMemNumaCandidateOpt& opt, UbseMemNumaDesc& desc)
{
    // 参数校验
    auto ret = UbseMemCreateWithCandidateReqIsValid(name, borrower, opt);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 请求转换
    UbseMemNumaBorrowReq numaBorrowReq;
    UbseMemOperationResp resp;
    ret = ConvertUbseMemNumaCreateWithCandidateReq(name, borrower, opt, numaBorrowReq);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 调用内部ubse_mem_controller_api_agent.h接口
    ubse::mem::controller::agent::UbseMemNumaBorrow(numaBorrowReq, resp);
    if (resp.errorCode != UBSE_OK) {
        UBSE_LOG_ERROR << "numa create with candidate failed, return code=" << resp.errorCode;
        return resp.errorCode;
    }
    int32_t retCode = ubse::mem::controller::UbseMemNumaGetWithImportNode(resp.name, borrower.nodeId, desc);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemNumaGet, ErrorCode=" << retCode;
    }
    return resp.errorCode;
}

UbseResult UbseMemNumaDelete(const std::string& name, const UbseMemBorrower& borrower)
{
    // 参数校验
    auto ret = UbseMemDeleteReqIsValid(name, borrower);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 请求转换
    UbseMemReturnReq returnReq;
    UbseMemOperationResp resp;
    ConvertUbseMemDeleteReq(name, borrower, returnReq);
    // 调用内部ubse_mem_controller_api_agent.h接口
    ubse::mem::controller::agent::UbseMemReturn(returnReq, MemOperationType::NUMA_RETURN, resp);
    if (resp.errorCode != UBSE_OK) {
        UBSE_LOG_INFO << "numa delete failed, return code=" << resp.errorCode;
        return resp.errorCode;
    }
    return resp.errorCode;
}

UbseResult UbseMemAddrCreate(const std::string& name, const UbseMemBorrower& borrower,
                             const UbseMemProcessLender& lender, uint32_t flag, UbseMemAddrDesc& desc)
{
    // 参数校验
    auto ret = UbseMemAddrCreateReqIsValid(name, borrower, lender);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 请求转换
    UbseMemAddrBorrowReq addrBorrowReq;
    UbseMemOperationResp resp;
    ConvertUbseMemAddrCreateReq(name, borrower, lender, flag, addrBorrowReq);
    // 调用内部ubse_mem_controller_api_agent.h接口
    ubse::mem::controller::agent::UbseMemAddrBorrow(addrBorrowReq, resp);
    if (resp.errorCode != UBSE_OK) {
        UBSE_LOG_INFO << "addr create failed, return code=" << resp.errorCode;
        return resp.errorCode;
    }
    int32_t retCode = ubse::mem::controller::UbseMemAddrGet(resp.name, borrower.nodeId, desc);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemAddrGet, ErrorCode=" << retCode;
    }
    return resp.errorCode;
}

UbseResult UbseMemAddrDelete(const std::string& name, const UbseMemBorrower& borrower)
{
    // 参数校验
    auto ret = UbseMemDeleteReqIsValid(name, borrower);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 请求转换
    UbseMemReturnReq returnReq;
    UbseMemOperationResp resp;
    ConvertUbseMemDeleteReq(name, borrower, returnReq);
    // 调用内部ubse_mem_controller_api_agent.h接口
    ubse::mem::controller::agent::UbseMemReturn(returnReq, MemOperationType::ADDR_RETURN, resp);
    if (resp.errorCode != UBSE_OK) {
        UBSE_LOG_INFO << "addr delete failed, return code=" << resp.errorCode;
        return resp.errorCode;
    }
    return UBSE_OK;
}

UbseResult GetNodeNumaInfoFromAccountAndSort(std::vector<ubse::mem::account::UbseNumaNodeInfo>& numaNodeInfos)
{
    auto ret = ubse::mem::account::UbseAllNumaInfo(numaNodeInfos);
    if (ret != 0) {
        UBSE_LOG_ERROR << "get numa node info failed, code=" << std::to_string(ret);
        return UBSE_ERR_INTERNAL;
    }
    auto sortFunction = [](const ubse::mem::account::UbseNumaNodeInfo& numa1,
                           const ubse::mem::account::UbseNumaNodeInfo& numa2) {
        if (numa1.nodeId == numa2.nodeId) {
            return numa1.numaId < numa2.numaId;
        }
        return numa1.nodeId < numa2.nodeId;
    };
    std::sort(numaNodeInfos.begin(), numaNodeInfos.end(), sortFunction);
    return UBSE_OK;
}

void ConvertNumaNodeInfo(std::vector<UbseNodeNumaInfo>& numaNodeInfoList,
                         const std::vector<ubse::mem::account::UbseNumaNodeInfo>& numaNodeInfos,
                         const std::string& nodeId)
{
    for (auto& numaInfo : numaNodeInfos) {
        if (nodeId != numaInfo.nodeId && !nodeId.empty()) {
            continue;
        }
        UbseNodeNumaInfo tmpNodeNumaInfo;
        tmpNodeNumaInfo.hostName = numaInfo.hostName;
        tmpNodeNumaInfo.nodeId = numaInfo.nodeId;
        tmpNodeNumaInfo.socketId = numaInfo.socketId;
        tmpNodeNumaInfo.numaId = numaInfo.numaId;
        tmpNodeNumaInfo.mReservedMemRatio = numaInfo.mReservedMemRatio;
        tmpNodeNumaInfo.memTotal = numaInfo.mMemTotal;
        tmpNodeNumaInfo.memFree = numaInfo.mMemFree;
        tmpNodeNumaInfo.nrHugepages = numaInfo.nrHugepages;
        tmpNodeNumaInfo.freeHugepages = numaInfo.freeHugepages;
        tmpNodeNumaInfo.usedHugepages = numaInfo.nrHugepages - numaInfo.freeHugepages;
        tmpNodeNumaInfo.timestamp = numaInfo.mTimestamp;
        tmpNodeNumaInfo.memLent = numaInfo.mMemLent;
        tmpNodeNumaInfo.memShared = numaInfo.mMemShared;
        numaNodeInfoList.push_back(std::move(tmpNodeNumaInfo));
    }
}

UbseResult UbseGetAllNodeNumaInfo(std::vector<UbseNodeNumaInfo>& numaNodeInfoList)
{
    if (!numaNodeInfoList.empty()) {
        UBSE_LOG_ERROR << "Get numaNodeInfoList size=" << numaNodeInfoList.size();
        return UBSE_ERR_INVALID_ARG;
    }
    std::vector<ubse::mem::account::UbseNumaNodeInfo> numaNodeInfos;
    auto ret = GetNodeNumaInfoFromAccountAndSort(numaNodeInfos);
    ConvertNumaNodeInfo(numaNodeInfoList, numaNodeInfos, "");
    return ret;
}

UbseResult UbseGetNodeNumaInfoByNodeId(const std::string& nodeId, std::vector<UbseNodeNumaInfo>& numaNodeInfoList)
{
    if (nodeId.empty() || !numaNodeInfoList.empty()) {
        UBSE_LOG_ERROR << "Get nodeId=" << nodeId << ", numaNodeInfoList size=" << numaNodeInfoList.size();
        return UBSE_ERR_INVALID_ARG;
    }
    std::vector<ubse::mem::account::UbseNumaNodeInfo> numaNodeInfos;
    auto ret = GetNodeNumaInfoFromAccountAndSort(numaNodeInfos);
    if (ret != UBSE_OK) {
        return ret;
    }
    ConvertNumaNodeInfo(numaNodeInfoList, numaNodeInfos, nodeId);
    if (numaNodeInfoList.empty()) {
        UBSE_LOG_ERROR << "Get numaNodeInfoList is empty";
        return UBSE_ERR_INVALID_ARG;
    }
    return ret;
}

UbseResult UbseMemDebtCircleCheck(const std::string& srcNodeId, const std::string& dstNodeId, bool& isCircle)
{
    if (srcNodeId == dstNodeId) {
        UBSE_LOG_WARN << "The source Node ID is same with destination node ID, ID=" << srcNodeId;
        return UBSE_ERR_INVALID_ARG; // 错误码：参数不合法
    }
    // 获得numa静态信息和账本动态信息
    std::vector<NumaStaticInfo> numaInfo;
    std::vector<LedgerDymaticInfo> ledgerInfo;
    uint32_t ret = GetMemInfoFromInner(numaInfo, ledgerInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get information from inner module failed, code=" << std::to_string(ret);
        return UBSE_MEMCONTROLLER_ERROR_GET_INFO_FAIL;
    }
    isCircle = false;
    for (const auto& singleLedgerInfo : ledgerInfo) {
        if (singleLedgerInfo.type == LedgerType::SHARE) {
            continue;
        }
        std::string borrowNodeId = singleLedgerInfo.borrowNodeId.empty() ? "" : singleLedgerInfo.borrowNodeId.front();
        if (srcNodeId == singleLedgerInfo.lentNodeId || dstNodeId == borrowNodeId) {
            isCircle = true;
            return UBSE_OK;
        }
    }
    return UBSE_OK;
}

// 辅助函数：处理addr导入对象
void ProcessAddrImportObj(const std::string& resourceId, const UbseMemAddrBorrowImportObj& addrBorrowImportObj,
                          const std::string& nodeId,
                          std::unordered_map<std::string, UbseMemAddrDesc>& addrMemoryDebtInfoMap)
{
    if (addrBorrowImportObj.status.state != ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_IMPORT_SUCCESS) {
        return;
    }
    std::string borrowNodeId = addrBorrowImportObj.req.importNodeId;
    std::string lentNodeId = addrBorrowImportObj.req.exportNodeId;
    if (!nodeId.empty() && nodeId != borrowNodeId && nodeId != lentNodeId) {
        return;
    }
    std::string resourceIdImportNodeId = resourceId + "_" + borrowNodeId;
    // 获取或创建账本信息
    auto [it, inserted] = addrMemoryDebtInfoMap.emplace(resourceIdImportNodeId, UbseMemAddrDesc());
    if (inserted) {
        it->second.name = resourceId;
    }
    UbseMemAddrDesc& debtInfo = it->second;
    debtInfo.numaId = addrBorrowImportObj.status.importResults[0].numaId;
    // 处理借入信息
    debtInfo.importNode.slotId = static_cast<uint32_t>(std::stoul(borrowNodeId));
    debtInfo.importNode.socketIdList.clear();
    for (const auto& importNumaInfo : addrBorrowImportObj.algoResult.importNumaInfos) {
        debtInfo.importNode.socketIdList.emplace_back(static_cast<int16_t>(importNumaInfo.socketId));
    }
    // 处理借出信息
    debtInfo.lender.slotId = static_cast<uint32_t>(std::stoul(lentNodeId));
    debtInfo.lender.socketId = addrBorrowImportObj.req.dstSocket;
    debtInfo.lender.pid = addrBorrowImportObj.req.exportPid;
    for (auto val : addrBorrowImportObj.req.exportAddrList) {
        debtInfo.lender.vaLists.push_back({val.addr, val.size});
    }
    debtInfo.size = 0;
    for (const auto& exportNumaInfo : addrBorrowImportObj.algoResult.exportNumaInfos) {
        debtInfo.size += exportNumaInfo.size;
    }
}

// 辅助函数：处理addr导出对象
void ProcessAddrExportObj(const std::string& resourceIdImportNodeId,
                          const UbseMemAddrBorrowExportObj& addrBorrowExportObj, const std::string& nodeId,
                          std::unordered_map<std::string, UbseMemAddrDesc>& addrMemoryDebtInfoMap)
{
    if (addrBorrowExportObj.status.state != ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_EXPORT_SUCCESS) {
        return;
    }
    std::string borrowNodeId = addrBorrowExportObj.req.importNodeId;
    std::string lentNodeId = addrBorrowExportObj.req.exportNodeId;
    if (!nodeId.empty() && nodeId != borrowNodeId && nodeId != lentNodeId) {
        return;
    }
    auto [it, inserted] = addrMemoryDebtInfoMap.emplace(resourceIdImportNodeId, UbseMemAddrDesc());
    if (inserted) {
        it->second.name = addrBorrowExportObj.req.name;
    }
    UbseMemAddrDesc& debtInfo = it->second;
    // 处理借入信息
    debtInfo.importNode.slotId = static_cast<uint32_t>(std::stoul(borrowNodeId));
    debtInfo.importNode.socketIdList.clear();
    for (const auto& importNumaInfo : addrBorrowExportObj.algoResult.importNumaInfos) {
        debtInfo.importNode.socketIdList.emplace_back(static_cast<int16_t>(importNumaInfo.socketId));
    }
    // 处理借出信息
    debtInfo.lender.slotId = static_cast<uint32_t>(std::stoul(lentNodeId));
    debtInfo.lender.socketId = addrBorrowExportObj.req.dstSocket;
    debtInfo.lender.pid = addrBorrowExportObj.req.exportPid;
    for (auto val : addrBorrowExportObj.req.exportAddrList) {
        debtInfo.lender.vaLists.push_back({val.addr, val.size});
    }
    debtInfo.size = 0;
    for (const auto& exportNumaInfo : addrBorrowExportObj.algoResult.exportNumaInfos) {
        debtInfo.size += exportNumaInfo.size;
    }
}

// 辅助函数：处理所有账本信息
void ProcessDebtInfoForAddr(const NodeMemDebtInfoMap& memDebtInfoMap, const std::string& nodeId,
                            const std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo>& nodeMap,
                            std::vector<UbseMemAddrDesc>& debtInfos)
{
    std::unordered_map<std::string, UbseMemAddrDesc> addrMemoryDebtInfoMap; // key:resourceId_borrowNodeId

    // 遍历所有节点账本信息
    for (const auto& nodeDebtInfoPair : memDebtInfoMap) {
        const std::string& tmpNodeId = nodeDebtInfoPair.first;
        const auto& nodeDebtInfo = nodeDebtInfoPair.second;

        // 跳过非工作状态的节点
        if (!IsNodeWorking(tmpNodeId, nodeMap)) {
            continue;
        }
        // 处理导入对象
        for (const auto& addrImportObjMap : nodeDebtInfo.addrImportObjMap) {
            const std::string& resourceId = addrImportObjMap.first;
            const auto& addrBorrowImportObj = addrImportObjMap.second;
            ProcessAddrImportObj(resourceId, addrBorrowImportObj, nodeId, addrMemoryDebtInfoMap);
        }
        // 处理导出对象
        for (const auto& addrExportObjMap : nodeDebtInfo.addrExportObjMap) {
            const std::string& resourceIdImportNodeId = addrExportObjMap.first;
            const auto& addrBorrowExportObj = addrExportObjMap.second;
            ProcessAddrExportObj(resourceIdImportNodeId, addrBorrowExportObj, nodeId, addrMemoryDebtInfoMap);
        }
    }
    // 将结果转移到输出向量
    debtInfos.reserve(addrMemoryDebtInfoMap.size());
    for (const auto& debtInfoPair : addrMemoryDebtInfoMap) {
        debtInfos.emplace_back(debtInfoPair.second);
    }
}

UbseResult UbseGetAddrMemDebtInfoWithNode(const std::string& nodeId, std::vector<UbseMemAddrDesc>& debtInfos)
{
    UBSE_LOG_INFO << "The UbseGetAddrMemDebtInfoWithNode method begins execution, nodeId" << nodeId;
    // 参数校验
    if (nodeId.empty()) {
        UBSE_LOG_ERROR << "Invalid argument, nodeId is empty!";
        return UBSE_ERR_INVALID_ARG;
    }

    // 获取节点信息
    std::set<uint32_t> staticNodeInfoList = UbseNodeController::GetInstance().UbseGetAllDeployedNode();
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeMap =
        UbseNodeController::GetInstance().GetAllNodes();

    // 检查节点是否存在
    if (!IsNodeInStaticList(nodeId, staticNodeInfoList)) {
        UBSE_LOG_ERROR << "Invalid argument, nodeId does not exist static node list! nodeId=" << nodeId;
        return UBSE_ERR_INVALID_ARG;
    }

    // 获取账本信息
    NodeMemDebtInfoMap memDebtInfoMap;
    uint32_t ret = UbseGetMemDebtInfo("", memDebtInfoMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "The UbseGetMemDebtInfo method call failed.";
        return UBSE_ERR_INTERNAL;
    }

    // 处理账本信息
    ProcessDebtInfoForAddr(memDebtInfoMap, nodeId, nodeMap, debtInfos);

    // 检查对账完成状态
    UbseResult retCode = CheckReconciliationStatus(staticNodeInfoList, nodeMap, nodeId);
    UBSE_LOG_INFO << "The UbseGetAddrMemDebtInfoWithNode method has been executed end, return code=" << retCode;
    return retCode;
}
} // namespace ubse::mem::controller
