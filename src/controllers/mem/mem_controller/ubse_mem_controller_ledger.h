/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#ifndef UBSE_MANAGER_UBSE_MEM_CONTROLLER_LEDGER_H
#define UBSE_MANAGER_UBSE_MEM_CONTROLLER_LEDGER_H

#include "ubse_common_def.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mmi_interface.h"
#include "ubse_node_controller.h"


namespace ubse::mem::controller {
using namespace ubse::common::def;
using namespace ubse::nodeController;
using namespace ubse::adapter_plugins::mmi;
using UbseMemShareBorrowExportObjMap =
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>>>;

UbseResult LedgerHandler(const ubse::nodeController::UbseNodeInfo &node);

UbseResult FdBorrowLedger(const std::string &nodeId, const NodeMemDebtInfo &masterDebtInfo,
                          const NodeMemDebtInfo &agentDebtInfo,
                          const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap);

UbseResult NumaBorrowLedger(const std::string &nodeId, const NodeMemDebtInfo &masterDebtInfo,
                            const NodeMemDebtInfo &agentDebtInfo,
                            const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap);

UbseResult AddrBorrowLedger(const std::string &nodeId, const NodeMemDebtInfo &masterDebtInfo,
                            const NodeMemDebtInfo &agentDebtInfo,
                            const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap);

UbseResult ShmBorrowLedger(const std::string &nodeId, const NodeMemDebtInfo &masterDebtInfo,
                           const NodeMemDebtInfo &agentDebtInfo);

UbseResult MasterDiffFdExportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowExportObj> objs);

UbseResult MasterDiffFdImportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowImportObj> objs);

UbseResult MasterDiffNumaExportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowExportObj> objs);

UbseResult MasterDiffNumaImportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowImportObj> objs);

UbseResult MasterDiffAddrExportHandler(const std::string &nodeId, std::vector<UbseMemAddrBorrowExportObj> objs);

UbseResult MasterDiffAddrImportHandler(const std::string &nodeId, std::vector<UbseMemAddrBorrowImportObj> objs);

UbseResult MasterDiffShareExportHandler(const std::string &nodeId, std::vector<UbseMemShareBorrowExportObj> objs);

UbseResult MasterDiffShareImportHandler(const std::string &nodeId, std::vector<UbseMemShareBorrowImportObj> objs);

bool IsMasterExitsRunningFdImportObj(const NodeMemDebtInfoMap &nodeMemDebtInfoMap, const std::string &importNodeId,
                                     const std::string &name);

bool IsMasterExitsRunningNumaImportObj(const NodeMemDebtInfoMap &nodeMemDebtInfoMap, const std::string &importNodeId,
                                       const std::string &name);

bool IsMasterExitsRunningAddrImportObj(const NodeMemDebtInfoMap& nodeMemDebtInfoMap, const std::string &importNodeId,
                                       const std::string &name);

UbseResult BothFdExportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowExportObj> objs,
                               const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap);

UbseResult BothNumaExportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowExportObj> objs,
                                 const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap);

UbseResult BothAddrExportHandler(const std::string &nodeId, std::vector<UbseMemAddrBorrowExportObj> objs,
                                 const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap);

UbseResult AgentDiffFdExportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowExportObj> objs,
                                    const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap);

UbseResult AgentDiffFdImportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowImportObj> objs);

UbseResult AgentDiffNumaExportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowExportObj> objs,
                                      const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap);

UbseResult AgentDiffNumaImportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowImportObj> objs);

UbseResult AgentDiffAddrExportHandler(const std::string &nodeId, std::vector<UbseMemAddrBorrowExportObj> objs,
                                      const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap);

UbseResult AgentDiffAddrImportHandler(const std::string &nodeId, std::vector<UbseMemAddrBorrowImportObj> objs);

UbseResult AgentDiffShareExportHandler(const std::string &nodeId, std::vector<UbseMemShareBorrowExportObj> objs);

UbseResult AgentDiffShareImportHandler(const std::string &nodeId, std::vector<UbseMemShareBorrowImportObj> objs);

UbseResult MasterRunningFdExportHandler(const std::string &nodeId,
                                        const std::vector<UbseMemFdBorrowExportObj> &masterRunningObjs);

UbseResult MasterRunningFdImportHandler(const std::string &nodeId,
                                        const std::vector<UbseMemFdBorrowImportObj> &masterRunningObjs);

UbseResult MasterRunningNumaExportHandler(const std::string &nodeId,
                                          const std::vector<UbseMemNumaBorrowExportObj> &masterRunningObjs);

UbseResult MasterRunningNumaImportHandler(const std::string &nodeId,
                                          const std::vector<UbseMemNumaBorrowImportObj> &masterRunningObjs);


UbseResult MasterRunningAddrExportHandler(const std::string &nodeId,
                                          const std::vector<UbseMemAddrBorrowExportObj> &masterRunningObjs);

UbseResult MasterRunningAddrImportHandler(const std::string &nodeId,
                                          const std::vector<UbseMemAddrBorrowImportObj> &masterRunningObjs);

UbseResult MasterRunningShareExportHandler(const std::string &nodeId,
                                           const std::vector<UbseMemShareBorrowExportObj> &masterRunningObjs);

UbseResult MasterRunningShareImportHandler(const std::string &nodeId,
                                           const std::vector<UbseMemShareBorrowImportObj> &masterRunningObjs);

std::unordered_map<std::string, std::unordered_map<std::string, size_t>> collectExportInfo(
    const NodeMemDebtInfoMap &debtMap);

void incrementRefCount(std::unordered_map<std::string, std::unordered_map<std::string, size_t>> &refCountMap,
                       const std::string &name, const std::string &baseNodeId);

void updateRefCount(std::unordered_map<std::string, std::unordered_map<std::string, size_t>> &refCountMap,
                    const NodeMemDebtInfoMap &debtMap);

std::unordered_map<std::string, std::unordered_map<std::string, size_t>> CountShareMemoryRefCount(
    const NodeMemDebtInfoMap &debtMap);

bool IsAllRefCountZero(const std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> &baseMap,
                       const std::unordered_map<std::string, std::unordered_map<std::string, size_t>> &refCountMap,
                       const std::string &name);

std::vector<UbseMemShareBorrowExportObj> CollectAllExportObjs(
    const std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> &baseMap);

void CleanAnonymousObjs(const std::vector<UbseMemShareBorrowExportObj> &allObjs,
                        std::vector<UbseMemShareBorrowExportObj> &toClean);

std::vector<UbseMemShareBorrowExportObj> FilterRemainingObjs(const std::vector<UbseMemShareBorrowExportObj> &allObjs,
                                                             const std::vector<UbseMemShareBorrowExportObj> &toClean);

void CleanExtraObjs(const std::vector<UbseMemShareBorrowExportObj> &remainingObjs,
                    std::vector<UbseMemShareBorrowExportObj> &toClean);

bool IsObjInVector(const UbseMemShareBorrowExportObj &obj, const std::vector<UbseMemShareBorrowExportObj> &vec);

void CleanRefCountZeroObjs(const std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> &baseMap,
                           const std::unordered_map<std::string, std::unordered_map<std::string, size_t>> &refCountMap,
                           const std::string &name, std::vector<UbseMemShareBorrowExportObj> &toClean);

void CheckAndCleanMultiBaseNode(
    const std::string &name, const std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> &baseMap,
    const std::unordered_map<std::string, std::unordered_map<std::string, size_t>> &refCountMap,
    std::vector<UbseMemShareBorrowExportObj> &toClean);

void CheckAndCleanSingleBaseNode(
    const std::string &name, const std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> &baseMap,
    const std::unordered_map<std::string, std::unordered_map<std::string, size_t>> &refCountMap,
    std::vector<UbseMemShareBorrowExportObj> &toClean);

void ExecuteShareMemoryClean(const std::vector<UbseMemShareBorrowExportObj> &toClean);

UbseMemShareBorrowExportObjMap CollectExportObjsByBaseNode(const NodeMemDebtInfoMap &debtMap);

std::vector<UbseMemShareBorrowExportObj> GetIntersection(const std::vector<UbseMemShareBorrowExportObj> &toClean1,
                                                         const std::vector<UbseMemShareBorrowExportObj> &toClean2);

bool CompareUbseMemShareBorrowExportObj(const UbseMemShareBorrowExportObj &obj1,
                                        const UbseMemShareBorrowExportObj &obj2);

void ProcessCurrentCleanList(const NodeMemDebtInfoMap &debtMap, std::vector<UbseMemShareBorrowExportObj> &toClean);

bool CleanShmTimer(int sleep_seconds);

void HandleClean(const std::vector<UbseMemShareBorrowExportObj> &originalToClean);

void CleanShmZeroImportHandler();

std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> GetExportObjsByBaseNode(
    const NodeMemDebtInfoMap &debtMap, const std::string &name);

std::vector<std::pair<std::string, size_t>> CollectBaseRefCount(
    const std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> &baseToObjs,
    const std::unordered_map<std::string, std::unordered_map<std::string, size_t>> &refCountMap,
    const std::string &name);

std::string FindMaxRefCountBaseNode(const std::vector<std::pair<std::string, size_t>> &baseRefCount);

UbseMemShareBorrowExportObj GetExportObjByBaseNode(
    const std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> &baseToObjs,
    const std::string &baseNodeId);

bool IsImportObjMatch(const UbseMemShareBorrowImportObj &importObj, const std::string &name,
                      const std::string &baseNodeId);

std::vector<UbseMemShareBorrowImportObj> CollectImportObjsFromNode(
    const std::unordered_map<std::string, UbseMemShareBorrowImportObj> &importMap, const std::string &name,
    const std::string &baseNodeId);

std::vector<UbseMemShareBorrowImportObj> GetImportObjsByBaseNode(const NodeMemDebtInfoMap &debtMap,
                                                                 const std::string &name,
                                                                 const std::string &baseNodeId);

std::vector<UbseMemShareBorrowImportObj> GetAllImportObjsByName(const NodeMemDebtInfoMap &debtMap,
                                                                const std::string &name);

UbseResult AgentInvalidateImportDebt(const std::string &name, UbseMemBorrowType type);

std::pair<UbseMemShareBorrowExportObj, std::vector<UbseMemShareBorrowImportObj>> GetMaxRefCountExportObj(
    const std::string &name);
} // namespace ubse::mem::controller

#endif // UBSE_MANAGER_UBSE_MEM_CONTROLLER_LEDGER_H
