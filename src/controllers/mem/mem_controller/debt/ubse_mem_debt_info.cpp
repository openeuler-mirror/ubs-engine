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

#include "ubse_mem_debt_info.h"
#include "ubse_election.h"
#include "ubse_node.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_module.h"
#include "ubse_context.h"
#include "ubse_logger.h"
#include "ubse_error.h"
#include "ubse_com_module.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::election;
using namespace ubse::context;
using namespace ubse::com;
using namespace message;
using namespace ubse::nodeController;

ReadWriteLock mapLock;
NodeMemDebtInfoMap nodeMemDebtInfoMap{};
template <class ObjMap>
void FilterNoStateObjMap(ObjMap& objMap, const UbseMemState& state)
{
    for (auto iterator = objMap.begin(); iterator != objMap.end();) {
        if (iterator->second.status.state == state) {
            iterator = objMap.erase(iterator);
            continue;
        }
        ++iterator;
    }
}

void FilterNoDeleteDebt(NodeMemDebtInfo& debtInfo)
{
    FilterNoStateObjMap<UbseMemFdImportObjMap>(debtInfo.fdImportObjMap, UbseMemState::UBSE_MEM_IMPORT_DESTROYED);
    FilterNoStateObjMap<UbseMemFdExportObjMap>(debtInfo.fdExportObjMap, UbseMemState::UBSE_MEM_EXPORT_DESTROYED);
    FilterNoStateObjMap<UbseMemNumaImportObjMap>(debtInfo.numaImportObjMap, UbseMemState::UBSE_MEM_IMPORT_DESTROYED);
    FilterNoStateObjMap<UbseMemNumaExportObjMap>(debtInfo.numaExportObjMap, UbseMemState::UBSE_MEM_EXPORT_DESTROYED);
    FilterNoStateObjMap<UbseMemShareImportObjMap>(debtInfo.shareImportObjMap, UbseMemState::UBSE_MEM_IMPORT_DESTROYED);
    FilterNoStateObjMap<UbseMemShareExportObjMap>(debtInfo.shareExportObjMap, UbseMemState::UBSE_MEM_EXPORT_DESTROYED);
    FilterNoStateObjMap<UbseMemAddrImportObjMap>(debtInfo.addrImportObjMap, UbseMemState::UBSE_MEM_IMPORT_DESTROYED);
    FilterNoStateObjMap<UbseMemAddrExportObjMap>(debtInfo.addrExportObjMap, UbseMemState::UBSE_MEM_EXPORT_DESTROYED);
}
NodeMemDebtInfoMap GetNodeMemDebtInfoMap()
{
    mapLock.LockRead();
    NodeMemDebtInfoMap map = nodeMemDebtInfoMap;
    mapLock.UnLock();
    return std::move(map);
}

UbseMemShareBorrowExportObj GetNodeMemShareExpDebtInfoMap(const std::string &nodeId, const std::string &name)
{
    UbseMemShareBorrowExportObj obj{};
    mapLock.LockRead();
    for (auto &[nodeId_, debtInfo] : nodeMemDebtInfoMap) {
        if (!nodeId.empty() && nodeId != nodeId_) {
            continue;
        }
        if (debtInfo.shareExportObjMap.find(name) != debtInfo.shareExportObjMap.end()) {
            obj = debtInfo.shareExportObjMap[name];
            break;
        }
    }
    mapLock.UnLock();
    return obj;
}

std::vector<UbseMemShareBorrowImportObj> GetNodeMemShareImportDebtInfoMap(const std::string &nodeId,
                                                                          const std::string &name)
{
    std::vector<UbseMemShareBorrowImportObj> objs;
    mapLock.LockRead();
    for (auto &[nodeId_, debtInfo] : nodeMemDebtInfoMap) {
        if (!nodeId.empty() && nodeId != nodeId_) {
            continue;
        }
        if (debtInfo.shareImportObjMap.find(name) != debtInfo.shareImportObjMap.end()) {
            objs.emplace_back(debtInfo.shareImportObjMap[name]);
        }
    }
    mapLock.UnLock();
    return objs;
}

NodeMemDebtInfoMap GetNoDeletedNodeMemDebtInfoMap()
{
    auto debtMap = GetNodeMemDebtInfoMap();
    for (auto [nodeId, debtInfo] : debtMap) {
        FilterNoDeleteDebt(debtInfo);
        debtMap[nodeId] = debtInfo;
    }
    return std::move(debtMap);
}

NodeMemDebtInfo GetNodeMemDebtInfoById(const std::string& nodeId)
{
    mapLock.LockRead();
    auto it = nodeMemDebtInfoMap.find(nodeId);
    if (it == nodeMemDebtInfoMap.end()) {
        mapLock.UnLock();
        return {};
    }
    NodeMemDebtInfo debtInfo = it->second;
    mapLock.UnLock();
    return debtInfo;
}

NodeMemDebtInfo GetNoDeletedNodeMemDebtInfoById(const std::string& nodeId)
{
    auto debtInfo = GetNodeMemDebtInfoById(nodeId);
    FilterNoDeleteDebt(debtInfo);
    return debtInfo;
}

void ClearNodeDebtInfoMap()
{
    mapLock.LockWrite();
    if (nodeMemDebtInfoMap.empty()) {
        mapLock.UnLock();
        return;
    }
    auto curNodeId = UbseNodeController::GetInstance().GetCurrentNodeId();
    for (auto it = nodeMemDebtInfoMap.begin(); it != nodeMemDebtInfoMap.end();) {
        if (it->first != curNodeId) {
            UBSE_LOG_INFO << "Clean obj map, nodeId=" << it->first << "curNodeId=" << curNodeId;
            // erase 返回下一个有效的 iterator
            it = nodeMemDebtInfoMap.erase(it);
        } else {
            ++it;
        }
    }
    mapLock.UnLock();
}
}  // namespace ubse::mem::controller