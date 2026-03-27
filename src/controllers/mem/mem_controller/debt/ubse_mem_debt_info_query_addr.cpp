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

#include "ubs_engine_topo.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_agent_task_manager.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_controller_query_api.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_node_controller_query_api.h"

namespace ubse::mem::controller::debt {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::election;
using namespace ubse::log;
uint32_t UbseMemAddrGet(const UbseMemDebtQueryRequest &request, UbseMemAddrDesc &desc)
{
    // 获取当前节点
    UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info, " << FormatRetCode(ret);
        return ret;
    }
    if (currentRoleInfo.nodeRole != ELECTION_ROLE_MASTER) {
        UBSE_LOG_ERROR << "current role is not master. nodeId: " << currentRoleInfo.nodeId;
        return UBSE_ERR_INTERNAL;
    }
    const std::string name = request.name;
    const std::string importNodeId = request.importNodeId;

    auto importObjPtr =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().GetResource(importNodeId, name);
    if (!importObjPtr) {
        UBSE_LOG_ERROR << "Failed to find no delete debt, name: " << name << "related node id: " << importNodeId;
        return UBSE_ERR_NOT_EXIST;
    }
    // 找到目标对象，提取信息
    desc.name = name;
    if (importObjPtr->status.importResults.empty()) {
        return UBSE_ERROR;
    }
    desc.numaId = importObjPtr->status.importResults[0].numaId;
    // 填充导入信息
    ubse::nodeController::def::UbseNode importNode;
    ubse::nodeController::UbseNodeGetByNodeId(importNodeId, importNode);
    desc.importNode.slotId = importNode.slotId;
    desc.importNode.hostName = importNode.hostName;
    desc.importNode.socketIdList.clear();
    for (int i = 0; i < UBS_TOPO_SOCKET_NUM; i++) {
        desc.importNode.socketIdList.push_back(static_cast<int16_t>(importNode.socketId[i]));
    }
    // 填充导出信息
    desc.lender.pid = importObjPtr->req.exportPid;
    desc.lender.slotId = static_cast<uint32_t>(std::stoul(importObjPtr->req.exportNodeId));
    desc.lender.socketId = importObjPtr->req.dstSocket;
    for (const auto &val : importObjPtr->req.exportAddrList) {
        desc.lender.vaLists.push_back({val.addr, val.size});
    }
    for (const auto &val : importObjPtr->algoResult.exportNumaInfos) {
        desc.size += val.size;
    }
    return UBSE_OK;
}

UbseMemResult GetAddrStageByObj(const std::string &name, const std::string &importNodeId)
{
    return GetStageByObj<UbseMemAddrBorrowImportObj, UbseMemAddrBorrowExportObj>(name, importNodeId);
}

UbseMemAddrBorrowExportObj UbseAddrExportObjGet(const std::string &nodeId, const std::string &name,
                                                const std::string &importNodeId, const bool isFromTaskManager)
{
    if (isFromTaskManager) {
        UbseMemAddrBorrowExportObj obj{};
        const auto ret = UbseMemAgentTaskManager::GetTaskObj(name, importNodeId, obj);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "name=" << name << ", importNodeId=" << importNodeId << " is not in task manager.";
        } else {
            return obj;
        }
    }

    const auto exportKey = GenerateExportObjKey(name, importNodeId);
    auto exportObjPtr =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().GetResourceByGlobalKey(exportKey);
    if (!exportObjPtr) {
        UBSE_LOG_WARN << "name=" << name << ", importNodeId=" << importNodeId << " is not in debt.";
        return {};
    }
    return *exportObjPtr;
}

UbseMemAddrBorrowImportObj UbseAddrImportObjGet(const std::string &nodeId, const std::string &name,
                                                const bool isFromTaskManager)
{
    if (isFromTaskManager) {
        UbseMemAddrBorrowImportObj obj{};
        const auto ret = UbseMemAgentTaskManager::GetTaskObj(name, "", obj);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "name=" << name << " is not in task manager.";
        } else {
            return obj;
        }
    }

    auto importObjPtr =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().GetResource(nodeId, name);
    if (!importObjPtr) {
        UBSE_LOG_WARN << "name=" << name << " is not in debt.";
        return {};
    }
    return *importObjPtr;
}
} // namespace ubse::mem::controller::debt