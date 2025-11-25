/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "ubse_mem_controller_ledger.h"

#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_ledger_filter.h"
#include "ubse_mem_controller_module.h"
#include "ubse_mem_controller_msg.h"
#include "ubse_mem_scheduler.h"
#include "ubse_mem_utils.h"
namespace ubse::mem::controller {
using namespace ubse::mem::scheduler;
using namespace ubse::mem::controller;
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)

const uint32_t LEDGE_RETRY_DURATION = 2;

UbseResult LedgerHandler(const ubse::nodeController::UbseNodeInfo &node)
{
    if (node.clusterState != UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "nodeId=" << node.nodeId << "start ledger.";

    UbseResult ret = UBSE_OK;
    NodeMemDebtInfo masterDebtInfo{};
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    auto nodeId = node.nodeId;
    if (nodeMemDebtInfoMap.find(nodeId) != nodeMemDebtInfoMap.end()) {
        masterDebtInfo = nodeMemDebtInfoMap[nodeId];
    }
    // 获取对端节点的export
    NodeMemDebtInfo agentDebtInfo{};
    std::string currentNodeId = utils::GetCurNodeId();
    if (currentNodeId == node.nodeId) {
        UBSE_LOG_INFO << "current node ledger, use context";
        agentDebtInfo = masterDebtInfo;
    } else {
        // 账本采集失败时，无限重试，直到数据采集成功为止；若下一次对账周期此次还未采集成功，下一次smoothing直接退出，继续等待本次smoothing
        while (true) {
            ret = CollectLedge(node.nodeId, agentDebtInfo);
            if (ret == UBSE_OK) {
                break;
            }
            UBSE_LOG_WARN << "nodeId=" << node.nodeId << " collect ledge failed, will retry, " << FormatRetCode(ret);
            sleep(LEDGE_RETRY_DURATION);
        }
    }

    ret |= FdBorrowLedger(node.nodeId, masterDebtInfo, agentDebtInfo);
    ret |= NumaBorrowLedger(node.nodeId, masterDebtInfo, agentDebtInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << node.nodeId << " ledger failed, " << FormatRetCode(ret);
    } else {
        UBSE_LOG_INFO << "nodeId=" << node.nodeId << " ledger success.";
    }
    return ret;
}

UbseResult FdBorrowLedger(const std::string &nodeId, const NodeMemDebtInfo &masterDebtInfo,
                          const NodeMemDebtInfo &agentDebtInfo)
{
    std::vector<UbseMemFdBorrowExportObj> masterRunningExportObjs{};
    std::vector<UbseMemFdBorrowExportObj> masterNotRunningExportObjs{};
    std::vector<UbseMemFdBorrowExportObj> agentNotRunningExportObjs{};
    FilterRunningFdExport(TransFdExportList(masterDebtInfo.fdExportObjMap),
                          TransFdExportList(agentDebtInfo.fdExportObjMap), masterRunningExportObjs,
                          masterNotRunningExportObjs, agentNotRunningExportObjs);
    std::vector<UbseMemFdBorrowExportObj> masterDiffFdExports{};
    std::vector<UbseMemFdBorrowExportObj> agentDiffFdExports{};
    FilterFdDifferentExportSet(masterNotRunningExportObjs, agentNotRunningExportObjs, masterDiffFdExports,
                               agentDiffFdExports);

    auto bothExistsExportObj = FilterBothExistsFdExport(masterNotRunningExportObjs, agentNotRunningExportObjs);

    std::vector<UbseMemFdBorrowImportObj> masterRunningImportObjs{};
    std::vector<UbseMemFdBorrowImportObj> masterNotRunningImportObjs{};
    std::vector<UbseMemFdBorrowImportObj> agentNotRunningImportObjs{};
    FilterRunningFdImport(TransFdImportList(masterDebtInfo.fdImportObjMap),
                          TransFdImportList(agentDebtInfo.fdImportObjMap), masterRunningImportObjs,
                          masterNotRunningImportObjs, agentNotRunningImportObjs);

    std::vector<UbseMemFdBorrowImportObj> masterDiffFdImports{};
    std::vector<UbseMemFdBorrowImportObj> agentDiffFdImports{};
    FilterFdDifferentImportSet(masterNotRunningImportObjs, agentNotRunningImportObjs, masterDiffFdImports,
                               agentDiffFdImports);

    auto ret = MasterDiffFdExportHandler(nodeId, masterDiffFdExports);
    ret |= MasterDiffFdImportHandler(nodeId, masterDiffFdImports);
    ret |= BothFdExportHandler(nodeId, bothExistsExportObj);
    ret |= AgentDiffFdExportHandler(nodeId, agentDiffFdExports);
    ret |= AgentDiffFdImportHandler(nodeId, agentDiffFdImports);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << nodeId << " ledger fd borrow failed, " << FormatRetCode(ret);
    }
    return ret;
}

UbseResult NumaBorrowLedger(const std::string &nodeId, const NodeMemDebtInfo &masterDebtInfo,
                            const NodeMemDebtInfo &agentDebtInfo)
{
    std::vector<UbseMemNumaBorrowExportObj> masterRunningExportObjs{};
    std::vector<UbseMemNumaBorrowExportObj> masterNotRunningExportObjs{};
    std::vector<UbseMemNumaBorrowExportObj> agentNotRunningExportObjs{};
    FilterRunningNumaExport(TransNumaExportList(masterDebtInfo.numaExportObjMap),
                            TransNumaExportList(agentDebtInfo.numaExportObjMap), masterRunningExportObjs,
                            masterNotRunningExportObjs, agentNotRunningExportObjs);
    std::vector<UbseMemNumaBorrowExportObj> masterDiffNumaExports{};
    std::vector<UbseMemNumaBorrowExportObj> agentDiffNumaExports{};
    FilterNumaDifferentExportSet(masterNotRunningExportObjs, agentNotRunningExportObjs, masterDiffNumaExports,
                                 agentDiffNumaExports);

    auto bothExistsExportObj = FilterBothExistsNumaExport(masterNotRunningExportObjs, agentNotRunningExportObjs);

    std::vector<UbseMemNumaBorrowImportObj> masterRunningImportObjs{};
    std::vector<UbseMemNumaBorrowImportObj> masterNotRunningImportObjs{};
    std::vector<UbseMemNumaBorrowImportObj> agentNotRunningImportObjs{};
    FilterRunningNumaImport(TransNumaImportList(masterDebtInfo.numaImportObjMap),
                            TransNumaImportList(agentDebtInfo.numaImportObjMap), masterRunningImportObjs,
                            masterNotRunningImportObjs, agentNotRunningImportObjs);

    std::vector<UbseMemNumaBorrowImportObj> masterDiffNumaImports{};
    std::vector<UbseMemNumaBorrowImportObj> agentDiffNumaImports{};
    FilterNumaDifferentImportSet(masterNotRunningImportObjs, agentNotRunningImportObjs, masterDiffNumaImports,
                                 agentDiffNumaImports);

    auto ret = MasterDiffNumaExportHandler(nodeId, masterDiffNumaExports);
    ret |= MasterDiffNumaImportHandler(nodeId, masterDiffNumaImports);
    ret |= BothNumaExportHandler(nodeId, bothExistsExportObj);
    ret |= AgentDiffNumaExportHandler(nodeId, agentDiffNumaExports);
    ret |= AgentDiffNumaImportHandler(nodeId, agentDiffNumaImports);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << nodeId << " ledger numa borrow failed, " << FormatRetCode(ret);
    }
    return ret;
}

UbseResult MasterDiffFdExportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowExportObj> objs)
{
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff fd export name=" << obj.req.name
                    << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff fd export name=" << obj.req.name << ", start to destroy";
        obj.status.state = UbseMemState::UBSE_MEM_EXPORT_DESTROYED;
        AddFdExport(obj);
    }
    return UBSE_OK;
}

UbseResult MasterDiffFdImportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowImportObj> objs)
{
    // 若master存在 单导出，agent不存在单导出
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff fd import name=" << obj.req.name
                    << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff fd import name=" << obj.req.name << ", start to destroy";
        obj.status.state = UbseMemState::UBSE_MEM_IMPORT_DESTROYED;
        AddFdImport(obj);
    }
    return UBSE_OK;
}

UbseResult MasterDiffNumaExportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowExportObj> objs)
{
    // 若master存在 单导出，agent不存在单导出
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff numa export name=" << obj.req.name
                    << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff numa export name=" << obj.req.name
                    << ", start to destroy";
        obj.status.state = UbseMemState::UBSE_MEM_EXPORT_DESTROYED;
        AddNumaExport(obj);
    }
    return UBSE_OK;
}

UbseResult MasterDiffNumaImportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowImportObj> objs)
{
    // 若master存在 单导出，agent不存在单导出
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff numa import name=" << obj.req.name
                    << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff numa import name=" << obj.req.name
                    << ", start to destroy";
        obj.status.state = UbseMemState::UBSE_MEM_IMPORT_DESTROYED;
        AddNumaImport(obj);
    }
    return UBSE_OK;
}

UbseResult BothFdExportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowExportObj> objs)
{
    UbseResult ret = UBSE_OK;
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " both fd export name=" << obj.req.name
                    << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }

        if (obj.algoResult.importNumaInfos.size() == 0) {
            UBSE_LOG_WARN << "nodeId=" << nodeId << " both fd export=" << obj.req.name << " has no import numa info";
            DeleteFdExport(obj);
            return UBSE_OK;
        }
        std::string remoteImportNodeId = obj.algoResult.importNumaInfos[0].nodeId;
        // 查询导入对象是否存在
        UbseMemFdBorrowImportObj importObj{};
        ret = QueryFdImportObj(remoteImportNodeId, obj.req.name, importObj);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "req name=" << obj.req.name << " query import nodeId=" << remoteImportNodeId
                        << " failed, " << FormatRetCode(ret);
            // 查询失败，跳过本次账本，等待下次处理
            continue;
        }
        if (!importObj.req.name.empty()) {
            UBSE_LOG_INFO << "nodeId=" << nodeId << " both fd export name=" << obj.req.name
                        << " has import obj, state=" << importObj.status.state;
            if (importObj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED) {
                UBSE_LOG_INFO << "nodeId=" << nodeId << " both fd export name=" << obj.req.name
                            << " peer import obj, state=destroyed start to destroy";
                DeleteFdExport(obj);
                return UBSE_OK;
            }
        } else {
            if (!IsMasterExitsRunningFdImportObj(GetNodeMemDebtInfoMap(), remoteImportNodeId, obj.req.name)) {
                UBSE_LOG_INFO << "nodeId=" << nodeId << " both fd export name=" << obj.req.name
                            << " context not exists running peer import obj record, start to destroy";
                DeleteFdExport(obj);
            }
        }
    }
    return UBSE_OK;
}

UbseResult BothNumaExportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowExportObj> objs)
{
    UbseResult ret = UBSE_OK;
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " both numa export name=" << obj.req.name
                    << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }

        if (obj.algoResult.importNumaInfos.size() == 0) {
            UBSE_LOG_WARN << "nodeId=" << nodeId << " both numa export=" << obj.req.name << " has no import numa info";
            DeleteNumaExport(obj);
            return UBSE_OK;
        }
        std::string remoteImportNodeId = obj.algoResult.importNumaInfos[0].nodeId;
        // 查询导入对象是否存在
        UbseMemNumaBorrowImportObj importObj{};
        ret = QueryNumaImportObj(remoteImportNodeId, obj.req.name, importObj);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "req name=" << obj.req.name << " query import nodeId=" << remoteImportNodeId
                        << " failed, " << FormatRetCode(ret);
            continue;
        }
        if (!importObj.req.name.empty()) {
            UBSE_LOG_INFO << "nodeId=" << nodeId << " both numa export name=" << obj.req.name
                        << " has import obj, state=" << importObj.status.state;
            if (importObj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED) {
                UBSE_LOG_INFO << "nodeId=" << nodeId << " both numa export name=" << obj.req.name
                            << " peer import obj, state=destroyed start to destroy";
                DeleteNumaExport(obj);
                return UBSE_OK;
            }
        } else {
            if (!IsMasterExitsRunningNumaImportObj(nodeMemDebtInfoMap, remoteImportNodeId, obj.req.name)) {
                UBSE_LOG_INFO << "nodeId=" << nodeId << " both numa export name=" << obj.req.name
                            << " context not exists running peer import obj record, start to destroy";
                DeleteNumaExport(obj);
            }
        }
    }
    return UBSE_OK;
}

bool IsMasterExitsRunningFdImportObj(NodeMemDebtInfoMap nodeMemDebtInfoMap, const std::string &importNodeId,
                                     const std::string &name)
{
    if (nodeMemDebtInfoMap.find(importNodeId) != nodeMemDebtInfoMap.end()) {
        if (nodeMemDebtInfoMap[importNodeId].fdImportObjMap.find(name) !=
            nodeMemDebtInfoMap[importNodeId].fdImportObjMap.end()) {
            // 在对账export去导入端查询import节点，对端不存在后，若master侧存在running状态的导入账本，不删除
            auto obj = nodeMemDebtInfoMap[importNodeId].fdImportObjMap[name];
            if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_RUNNING) {
                UBSE_LOG_INFO << "master node ctx exists running fd importNodeId=" << importNodeId << ", name=" << name
                            << "state=" << static_cast<uint32_t>(obj.status.state);
                return true;
            }
            UBSE_LOG_INFO << "master node ctx exists other state fd importNodeId=" << importNodeId << ", name=" << name
                        << "state=" << static_cast<uint32_t>(obj.status.state);
            return false;
        }
    }
    UBSE_LOG_INFO << "master node ctx not exists fd importNodeId=" << importNodeId << ", name=" << name;
    return false;
}

UbseResult AgentDiffFdExportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowExportObj> objs)
{
    UbseResult ret = UBSE_OK;
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff fd export name=" << obj.req.name
                    << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        AddFdExport(obj);

        if (obj.algoResult.importNumaInfos.size() == 0) {
            UBSE_LOG_WARN << "nodeId=" << nodeId << " agent diff fd export=" << obj.req.name
                        << " has no import numa info";
            DeleteFdExport(obj);
            return UBSE_OK;
        }
        std::string remoteImportNodeId = obj.algoResult.importNumaInfos[0].nodeId;
        // 查询导入对象是否存在
        UbseMemFdBorrowImportObj importObj{};
        ret = QueryFdImportObj(remoteImportNodeId, obj.req.name, importObj);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "req name=" << obj.req.name << " query import nodeId=" << remoteImportNodeId
                        << " query import obj"<< " failed, " << FormatRetCode(ret);
            continue;
        }
        if (importObj.req.name.empty()) {
            UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff fd export name=" << obj.req.name
                        << " has no import obj, start to destroy";
            DeleteFdExport(obj);
            return UBSE_OK;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff fd export name=" << obj.req.name
                    << " has import obj, state=" << importObj.status.state;
        if (importObj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED) {
            UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff fd export name=" << obj.req.name
                        << " peer import obj destroyed, start to destroy";
            DeleteFdExport(obj);
            return UBSE_OK;
        }
    }
    return UBSE_OK;
}

UbseResult AgentDiffFdImportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowImportObj> objs)
{
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff fd import name=" << obj.req.name
                    << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff fd import name=" << obj.req.name << " start to restore";
        AddFdImport(obj);
    }
    return UBSE_OK;
}

bool IsMasterExitsRunningNumaImportObj(NodeMemDebtInfoMap nodeMemDebtInfoMap, const std::string &importNodeId,
                                       const std::string &name)
{
    if (nodeMemDebtInfoMap.find(importNodeId) != nodeMemDebtInfoMap.end()) {
        if (nodeMemDebtInfoMap[importNodeId].numaImportObjMap.find(name) !=
            nodeMemDebtInfoMap[importNodeId].numaImportObjMap.end()) {
            // 在对账export去导入端查询import节点，对端不存在后，若master侧存在running状态的导入账本，不删除
            auto obj = nodeMemDebtInfoMap[importNodeId].numaImportObjMap[name];
            if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_RUNNING) {
                UBSE_LOG_INFO << "master node ctx exists running numa importNodeId=" << importNodeId << ", name="
                            << name << "state=" << static_cast<uint32_t>(obj.status.state);
                return true;
            }
            UBSE_LOG_INFO << "master node ctx exists other state numa importNodeId=" << importNodeId << ", name="
                        << name << "state=" << static_cast<uint32_t>(obj.status.state);
            return false;
        }
    }
    UBSE_LOG_INFO << "master node ctx not exists numa importNodeId=" << importNodeId << ", name=" << name;
    return false;
}

UbseResult AgentDiffNumaExportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowExportObj> objs)
{
    UbseResult ret = UBSE_OK;
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff numa export name=" << obj.req.name
                    << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        AddNumaExport(obj);

        if (obj.algoResult.importNumaInfos.size() == 0) {
            UBSE_LOG_WARN << "nodeId=" << nodeId << " agent diff numa export=" << obj.req.name
                        << " has no import numa info";
            DeleteNumaExport(obj);
            return UBSE_OK;
        }
        std::string remoteImportNodeId = obj.algoResult.importNumaInfos[0].nodeId;
        // 查询导入对象是否存在
        UbseMemNumaBorrowImportObj importObj{};
        ret = QueryNumaImportObj(remoteImportNodeId, obj.req.name, importObj);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "req name=" << obj.req.name << " query import nodeId=" << remoteImportNodeId
                        << " failed, " << FormatRetCode(ret);
            continue;
        }
        if (importObj.req.name.empty()) {
            UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff numa export name=" << obj.req.name
                        << " has no import obj, start to destroy";
            DeleteNumaExport(obj);
            return UBSE_OK;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff numa export name=" << obj.req.name
                    << " has import obj, state=" << importObj.status.state;
        if (importObj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED) {
            UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff numa export name=" << obj.req.name
                        << " peer import obj destroyed, start to destroy";
            DeleteNumaExport(obj);
            return UBSE_OK;
        }
    }
    return UBSE_OK;
}

UbseResult AgentDiffNumaImportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowImportObj> objs)
{
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff numa import name=" << obj.req.name
                    << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff numa import name=" << obj.req.name << " start to restore";
        AddNumaImport(obj);
    }
    return UBSE_OK;
}
} // namespace ubse::mem::controller