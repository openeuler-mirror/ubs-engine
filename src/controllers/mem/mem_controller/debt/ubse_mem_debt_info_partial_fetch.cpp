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
#include "ubse_mem_debt_info_partial_fetch.h"
#include <set>
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_debt_info.h"
#include "ubse_serial_util.h"

namespace ubse::mem::controller::debt {
using namespace ubse::common::def;
using namespace ubse::mem::controller::message;
UBSE_DEFINE_THIS_MODULE("ubse");

constexpr uint64_t BYTES_PER_MEGABYTE = 1024 * 1024;

uint64_t ConvertSizeToMB(const uint64_t value)
{
    return value / BYTES_PER_MEGABYTE;
}

const std::set<UbseMemState> allowedStates = {
    UbseMemState::UBSE_MEM_IMPORT_RUNNING,    UbseMemState::UBSE_MEM_EXPORT_RUNNING,
    UbseMemState::UBSE_MEM_IMPORT_SUCCESS,    UbseMemState::UBSE_MEM_EXPORT_SUCCESS,
    UbseMemState::UBSE_MEM_IMPORT_DESTROYING, UbseMemState::UBSE_MEM_EXPORT_DESTROYING
};

enum class AccoutObjType {
    SHM_EXPORT,
    SHM_IMPORT,
    NUMA_EXPORT,
    NUMA_IMPORT,
    FD_EXPORT,
    FD_IMPORT,
    ADDR_EXPORT,
    ADDR_IMPORT
};

template <typename ObjType>
bool ShouldRecordObject(const AccountType &type, AccoutObjType accountObjType, const DebtFetchInfo &debtFetchInfo,
    const ObjType &it)
{
    if (!debtFetchInfo.name.empty() && debtFetchInfo.name != it->second.req.name) {
        return false;
    }

    if (allowedStates.find(it->second.status.state) == allowedStates.end()) {
        UBSE_LOG_WARN << "The status type of " << it->second.req.name << " is " << it->second.status.state <<
            ", and no statistics will be collected.";
        return false;
    }

    bool needToRecord = false;
    if (accountObjType == AccoutObjType::SHM_EXPORT || accountObjType == AccoutObjType::SHM_IMPORT) {
        needToRecord = (!it->second.algoResult.exportNumaInfos.empty());
    } else {
        needToRecord =
            (!it->second.algoResult.importNumaInfos.empty() && !it->second.algoResult.exportNumaInfos.empty());
    }

    if (!needToRecord) {
        UBSE_LOG_WARN << "The import/export of type " << AccountTypeUtil::AccountTypeToString(type) << " (" <<
            it->second.req.name << ") does not exist.";
        return false;
    }
    return true;
}

template <typename ObjType>
void BuildFlatDebtInfo(const AccountType &type, AccoutObjType accountObjType, const DebtFetchInfo &debtFetchInfo,
    const ObjType &it, FlatDebtInformation &flatDebtInfo)
{
    flatDebtInfo.name = it->second.req.name;
    flatDebtInfo.type = type;
    flatDebtInfo.status = static_cast<uint32_t>(it->second.status.state);
    flatDebtInfo.lendId = it->second.algoResult.exportNumaInfos.begin()->nodeId;
    if (accountObjType == AccoutObjType::SHM_EXPORT) {
        // nothing to do
    } else if (accountObjType == AccoutObjType::SHM_IMPORT) {
        flatDebtInfo.importId = debtFetchInfo.nodeId;
    } else {
        flatDebtInfo.importId = it->second.algoResult.importNumaInfos.begin()->nodeId;
    }
    for (const auto &item : it->second.algoResult.exportNumaInfos) {
        flatDebtInfo.numaLendInfos.push_back({ item.socketId, item.numaId, ConvertSizeToMB(item.size) });
    }
}

template <typename ObjType>
void HandleImportResults(AccoutObjType accountObjType, const ObjType &it, FlatDebtInformation &flatDebtInfo)
{
    if (accountObjType == AccoutObjType::NUMA_IMPORT) {
        flatDebtInfo.handle = it->second.status.importResults.empty() ?
            "" :
            std::to_string(it->second.status.importResults.begin()->numaId);
    } else if (accountObjType == AccoutObjType::FD_IMPORT || accountObjType == AccoutObjType::SHM_IMPORT) {
        std::ostringstream oss;
        for (const auto &item : it->second.status.importResults) {
            if (!oss.str().empty()) {
                oss << ",";
            }
            oss << item.memId;
        }
        flatDebtInfo.handle = oss.str();
    }
}

template <typename T, typename = void> struct HasImportResultsInStruct : std::false_type {};
template <typename T>
struct HasImportResultsInStruct<T, std::void_t<decltype(std::declval<T>().status.importResults)>> : std::true_type {};
template <typename ObjMapType>
void CollectSingleAccountMap(const ObjMapType &objs, const AccountType &type, const DebtFetchInfo &debtFetchInfo,
    AccoutObjType accountObjType, PartialFetchRes &partialFetchRes)
{
    if (debtFetchInfo.pageSize <= NO_0) {
        UBSE_LOG_ERROR << "An error is logged when the pageSize in debtFetchInfo is less than or equal to zero.";
        return;
    }
    int totalItems = static_cast<int>(objs.size());
    int totalPages = (totalItems + debtFetchInfo.pageSize - NO_1) / debtFetchInfo.pageSize;
    UBSE_LOG_DEBUG << "totalPages: " << totalPages << ", pageNum: " << debtFetchInfo.pageNum;
    if (debtFetchInfo.pageNum < NO_1 || debtFetchInfo.pageNum > totalPages) {
        return;
    }
    auto it = objs.begin();
    std::advance(it, (debtFetchInfo.pageNum - NO_1) * debtFetchInfo.pageSize);
    if (!partialFetchRes.NeedToContinue) {
        partialFetchRes.NeedToContinue = (debtFetchInfo.pageNum < totalPages);
    }

    for (int i = 0; i < debtFetchInfo.pageSize && it != objs.end(); ++i, ++it) {
        if (!ShouldRecordObject(type, accountObjType, debtFetchInfo, it)) {
            continue;
        }
        FlatDebtInformation flatDebtInfo{};
        BuildFlatDebtInfo(type, accountObjType, debtFetchInfo, it, flatDebtInfo);
        if constexpr (HasImportResultsInStruct<decltype(it->second)>::value) {
            HandleImportResults(accountObjType, it, flatDebtInfo);
        }
        partialFetchRes.flatDebt.push_back(flatDebtInfo);
        UBSE_LOG_DEBUG << PartialFetchRes::toString(partialFetchRes);
    }
}

void HandleExportType(const DebtFetchInfo &debtFetchInfo, PartialFetchRes &partialFetchRes,
    const decltype(nodeMemDebtInfoMap)::iterator &it)
{
    if (debtFetchInfo.borrowType == AccountType::NUMA || debtFetchInfo.borrowType == AccountType::INIT) {
        CollectSingleAccountMap(it->second.numaExportObjMap, AccountType::NUMA, debtFetchInfo,
            AccoutObjType::NUMA_EXPORT, partialFetchRes);
    }
    if (debtFetchInfo.borrowType == AccountType::FD || debtFetchInfo.borrowType == AccountType::INIT) {
        CollectSingleAccountMap(it->second.fdExportObjMap, AccountType::FD, debtFetchInfo, AccoutObjType::FD_EXPORT,
            partialFetchRes);
    }
    if (debtFetchInfo.borrowType == AccountType::SHM || debtFetchInfo.borrowType == AccountType::INIT) {
        CollectSingleAccountMap(it->second.shareExportObjMap, AccountType::SHM, debtFetchInfo,
            AccoutObjType::SHM_EXPORT, partialFetchRes);
    }
    if (debtFetchInfo.borrowType == AccountType::ADDR || debtFetchInfo.borrowType == AccountType::INIT) {
        CollectSingleAccountMap(it->second.addrExportObjMap, AccountType::ADDR, debtFetchInfo,
            AccoutObjType::ADDR_EXPORT, partialFetchRes);
    }
}

void HandleImportType(const DebtFetchInfo &debtFetchInfo, PartialFetchRes &partialFetchRes,
    const decltype(nodeMemDebtInfoMap)::iterator &it)
{
    if (debtFetchInfo.borrowType == AccountType::NUMA || debtFetchInfo.borrowType == AccountType::INIT) {
        CollectSingleAccountMap(it->second.numaImportObjMap, AccountType::NUMA, debtFetchInfo,
            AccoutObjType::NUMA_IMPORT, partialFetchRes);
    }
    if (debtFetchInfo.borrowType == AccountType::FD || debtFetchInfo.borrowType == AccountType::INIT) {
        CollectSingleAccountMap(it->second.fdImportObjMap, AccountType::FD, debtFetchInfo, AccoutObjType::FD_IMPORT,
            partialFetchRes);
    }
    if (debtFetchInfo.borrowType == AccountType::SHM || debtFetchInfo.borrowType == AccountType::INIT) {
        CollectSingleAccountMap(it->second.shareImportObjMap, AccountType::SHM, debtFetchInfo,
            AccoutObjType::SHM_IMPORT, partialFetchRes);
    }
    if (debtFetchInfo.borrowType == AccountType::ADDR || debtFetchInfo.borrowType == AccountType::INIT) {
        CollectSingleAccountMap(it->second.addrImportObjMap, AccountType::ADDR, debtFetchInfo,
            AccoutObjType::ADDR_IMPORT, partialFetchRes);
    }
}

UbseResult FetchDebtInfoByTypeAndPage(const DebtFetchInfo &debtFetchInfo, PartialFetchRes &partialFetchRes)
{
    mapLock.LockRead();
    auto it = nodeMemDebtInfoMap.find(debtFetchInfo.nodeId);
    if (it == nodeMemDebtInfoMap.end()) {
        mapLock.UnLock();
        UBSE_LOG_INFO << "The debt of this node " << debtFetchInfo.nodeId << " does not exist.";
        return UBSE_OK;
    }

    if (debtFetchInfo.type == DebtFetchType::EXPORT) {
        HandleExportType(debtFetchInfo, partialFetchRes, it);
    } else if (debtFetchInfo.type == DebtFetchType::IMPORT) {
        HandleImportType(debtFetchInfo, partialFetchRes, it);
    } else {
        mapLock.UnLock();
        UBSE_LOG_ERROR << "The fetch type of debt is unknown.";
        return UBSE_ERROR_INVAL;
    }
    mapLock.UnLock();
    return UBSE_OK;
}

ubse::common::def::UbseResult ValidateDebtFetchInfo(DebtFetchInfo debtFetchInfo)
{
    // Check if nodeId is all digits and length not more than 10
    if (debtFetchInfo.nodeId.length() > NO_10 || debtFetchInfo.nodeId.empty()) {
        UBSE_LOG_ERROR << "nodeId length exceeds 10 or is empty.";
        return UBSE_ERROR;
    }
    if (!std::all_of(debtFetchInfo.nodeId.begin(), debtFetchInfo.nodeId.end(), ::isdigit)) {
        UBSE_LOG_ERROR << "nodeId contains non-digit characters.";
        return UBSE_ERROR;
    }

    // Check if pageNum and pageSize are valid
    if (debtFetchInfo.pageNum < 0 || debtFetchInfo.pageSize < 0) {
        UBSE_LOG_ERROR << "pageNum or pageSize is negative.";
        return UBSE_ERROR;
    }

    if (debtFetchInfo.pageSize > EACH_PAGE_SIZE) {
        UBSE_LOG_ERROR << "pageSize exceeds the maximum allowed value " << EACH_PAGE_SIZE << ".";
        return UBSE_ERROR;
    }

    // Prevent overflow in pageNum * pageSize
    const int maxInt = std::numeric_limits<int>::max();
    if (debtFetchInfo.pageNum > 0 && debtFetchInfo.pageSize > 0 &&
        debtFetchInfo.pageNum > maxInt / debtFetchInfo.pageSize) {
        UBSE_LOG_ERROR << "pageNum * pageSize may cause integer overflow.";
        return UBSE_ERROR;
    }

    if (ubse::mem::controller::message::DebtFetchInfo::DebtFetchTypeToString(debtFetchInfo.type) == "UNKNOWN") {
        UBSE_LOG_WARN << "Invalid DebtFetchType: UNKNOWN";
        return UBSE_ERROR;
    }
    std::string error = {};
    if (!debtFetchInfo.CheckFilter(error)) {
        UBSE_LOG_ERROR << error;
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}
} // namespace ubse::mem::controller::debt
