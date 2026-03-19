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

#include "ubse_mem_debt_info_query.h"

#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_debt_info.h"
#include "ubse_node_controller.h"

namespace ubse::mem::controller::debt {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::election;

inline uint64_t BytesToMB(uint64_t bytes)
{
    constexpr uint64_t BYTE_TO_MB = 1024ULL * 1024ULL;
    return bytes / (BYTE_TO_MB);
}

struct FdCollectorPolicy {
    static const UbseMemFdExportObjMap &GetExportMap(const NodeMemDebtInfo &debtInfo)
    {
        return debtInfo.fdExportObjMap; // 访问fd类型的导出映射
    }
    static const UbseMemFdImportObjMap &GetImportMap(const NodeMemDebtInfo &debtInfo)
    {
        return debtInfo.fdImportObjMap; // 访问fd类型的导入映射
    }
};

struct NumaCollectorPolicy {
    static const UbseMemNumaExportObjMap &GetExportMap(const NodeMemDebtInfo &debtInfo)
    {
        return debtInfo.numaExportObjMap; // 访问numa类型的导出映射
    }
    static const UbseMemNumaImportObjMap &GetImportMap(const NodeMemDebtInfo &debtInfo)
    {
        return debtInfo.numaImportObjMap; // 访问numa类型的导入映射
    }
};

struct AddrCollectorPolicy {
    static const UbseMemAddrExportObjMap &GetExportMap(const NodeMemDebtInfo &debtInfo)
    {
        return debtInfo.addrExportObjMap; // 访问addr类型的导出映射
    }
    static const UbseMemAddrImportObjMap &GetImportMap(const NodeMemDebtInfo &debtInfo)
    {
        return debtInfo.addrImportObjMap; // 访问addr类型的导入映射
    }
};

/* *
 * 通用模板：按 Export 遍历并匹配 Import，累加 size 到 nodeBorrows
 * @tparam Policy 定义所有类型差异
 * @param debtInfoMap 全量账本
 * @param nodeBorrows 统计借用数据
 */
template <typename CollectorPolicy>
void CollectGenericImportSize(const NodeMemDebtInfoMap &debtInfoMap,
                              std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> &nodeBorrows)
{
    for (const auto &[exportNodeId, debtInfo] : debtInfoMap) {
        const auto &exportMap = CollectorPolicy::GetExportMap(debtInfo);
        for (const auto &[_, exportObj] : exportMap) {
            // 过滤借用未成功账本
            if (exportObj.status.state != UBSE_MEM_EXPORT_SUCCESS) {
                continue;
            }
            // 查借入账本是否存在
            const auto &importNodeId = exportObj.req.importNodeId;
            const auto importDebtInfoIt = debtInfoMap.find(importNodeId);
            if (importDebtInfoIt == debtInfoMap.end()) {
                continue;
            }
            const auto &importMap = CollectorPolicy::GetImportMap(importDebtInfoIt->second);
            auto importObjectIt = importMap.find(exportObj.req.name);
            if (importObjectIt == importMap.end()) {
                continue;
            }
            auto importObj = importObjectIt->second;
            // 过滤借用未成功账本
            if (importObj.status.state != UBSE_MEM_IMPORT_SUCCESS) {
                continue;
            }
            // 累加
            uint64_t size = 0;
            for (const auto &numaInfo : importObj.algoResult.exportNumaInfos) {
                size += numaInfo.size;
            }
            nodeBorrows[exportNodeId][importNodeId] += BytesToMB(size);
        }
    }
}

uint32_t UbseMemNodeBorrowQuery(std::vector<UbseNodeBorrowInfo> &nodeBorrowInfo)
{
    // 获取当前节点
    UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info, " << FormatRetCode(ret);
        return ret;
    }
    if (currentRoleInfo.nodeRole != ELECTION_ROLE_MASTER) {
        UBSE_LOG_ERROR << "current role is not master. nodeId: " << currentRoleInfo.nodeId;
        return UBSE_ERROR;
    }
    // 聚集fd numa addr类型借用size
    auto debtInfoMap = GetNodeMemDebtInfoMap();
    std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> nodeBorrowSizeMap;
    CollectGenericImportSize<FdCollectorPolicy>(debtInfoMap, nodeBorrowSizeMap);
    CollectGenericImportSize<NumaCollectorPolicy>(debtInfoMap, nodeBorrowSizeMap);
    CollectGenericImportSize<AddrCollectorPolicy>(debtInfoMap, nodeBorrowSizeMap);
    // build resp
    auto nodeInfos = nodeController::UbseNodeController::GetInstance().GetAllNodes();
    for (const auto &[exportNodeId, importNodeInfo] : nodeBorrowSizeMap) {
        for (const auto &[importNodeId, size] : importNodeInfo) {
            if (nodeInfos.find(importNodeId) == nodeInfos.end() || nodeInfos.find(exportNodeId) == nodeInfos.end()) {
                UBSE_LOG_WARN << "The importNode or exportNode does not exist, importNodeId=" << importNodeId
                              << " , exportNodeId=" << exportNodeId;
                continue;
            }
            nodeBorrowInfo.push_back({nodeInfos[importNodeId].slotId, nodeInfos[importNodeId].hostName,
                                      nodeInfos[exportNodeId].slotId, nodeInfos[exportNodeId].hostName, size});
        }
    }
    return UBSE_OK;
}
} // namespace ubse::mem::controller::debt