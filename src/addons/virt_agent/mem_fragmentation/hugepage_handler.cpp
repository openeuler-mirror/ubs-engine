/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "hugepage_handler.h"

#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <ubse_logger.h>
#include <ubse_security_module.h>

#include "hugepage_borrow_storage.h"
#include "mem_handler.h"
#include "vm_def.h"
#include "vm_file_util.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;
using namespace ubse::mem::controller;

VmResult HugePageHandler::ReconcileHugePages(const std::vector<uint16_t>& numaIds)
{
    return ReconcileHugePagesInternal(numaIds, false);
}

VmResult HugePageHandler::RecoverHugePages()
{
    return ReconcileHugePagesInternal({}, true);
}

VmResult HugePageHandler::AllocateHugePagesForBorrows(const std::vector<std::string>& borrowIds,
                                                      const std::vector<uint16_t>& numaIds, uint64_t pageSize)
{
    if (borrowIds.empty()) {
        UBSE_LOG_INFO << "No borrowId to allocate hugepages for, skip.";
        return VM_OK;
    }
    // These borrowIds cannot be persisted, rebuild the record table in memory and reuse the reconciliation, so that
    // the hugepages are calculated from the total unreturned borrowed size of each (numaId, pageSize), the same as
    // the normal reconciliation. Passing only the increment of this request would underestimate the target pages.
    std::unordered_map<std::string, uint64_t> borrowPageSizes;
    VmResult ret = HugePageBorrowStorage::GetAllBorrowPageSizes(borrowPageSizes);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Get hugepage borrow records failed, " << FormatRetCode(ret);
        return ret;
    }
    for (const auto& borrowId : borrowIds) {
        borrowPageSizes[borrowId] = pageSize;
    }
    return ReconcileWithRecords(borrowPageSizes, numaIds, false);
}

VmResult HugePageHandler::ReconcileHugePagesInternal(const std::vector<uint16_t>& numaIds, bool removeStaleRecords)
{
    // The record tells which hugepage size each borrowId is split into.
    std::unordered_map<std::string, uint64_t> borrowPageSizes;
    auto ret = HugePageBorrowStorage::GetAllBorrowPageSizes(borrowPageSizes);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Get hugepage borrow records failed, " << FormatRetCode(ret);
        return ret;
    }
    if (borrowPageSizes.empty()) {
        UBSE_LOG_INFO << "No hugepage borrow record, skip reconciliation.";
        return VM_OK;
    }
    return ReconcileWithRecords(borrowPageSizes, numaIds, removeStaleRecords);
}

VmResult HugePageHandler::ReconcileWithRecords(const std::unordered_map<std::string, uint64_t>& borrowPageSizes,
                                               const std::vector<uint16_t>& numaIds, bool removeStaleRecords)
{
    // The borrow ledger only contains the borrowIds that are not returned yet.
    std::vector<UbseNumaMemoryImportDebtInfo> debtInfos;
    VmResult ret = MemHandler::GetBorrowDebtInfos(debtInfos);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Get borrow debt info failed, " << FormatRetCode(ret);
        return ret;
    }
    // (numaId, pageSize) -> total borrowed bytes that should be split into this page size.
    std::map<std::pair<uint64_t, uint64_t>, uint64_t> targetSizeMap;
    std::unordered_set<std::string> unreturnedBorrowIds;
    for (const auto& debtInfo : debtInfos) {
        // The ledger only contains the borrowIds that are not returned yet, so any borrowId in the ledger must keep
        // its record even if the debt entry is incomplete or the record cannot be found temporarily.
        unreturnedBorrowIds.insert(debtInfo.name);
        if (debtInfo.remoteNumaId < 0 || debtInfo.size == 0) {
            continue;
        }
        const auto recordIter = borrowPageSizes.find(debtInfo.name);
        if (recordIter == borrowPageSizes.end()) {
            UBSE_LOG_WARN << "No hugepage record for borrowId=" << debtInfo.name << ", size=" << debtInfo.size
                          << ", skip it.";
            continue;
        }
        const auto numaId = static_cast<uint16_t>(debtInfo.remoteNumaId);
        if (!numaIds.empty() && std::find(numaIds.begin(), numaIds.end(), numaId) == numaIds.end()) {
            continue;
        }
        auto& targetSize = targetSizeMap[{numaId, recordIter->second}];
        if (targetSize > std::numeric_limits<uint64_t>::max() - debtInfo.size) {
            UBSE_LOG_ERROR << "NumaId=" << numaId << " borrowed size exceeds the limit.";
            return VM_ERROR;
        }
        targetSize += debtInfo.size;
    }
    if (removeStaleRecords) {
        if (debtInfos.empty()) {
            // An empty ledger may mean the borrow data has not been loaded yet. Dropping the records in that case
            // would lose the hugepage size of the borrowed memory permanently, so keep them.
            UBSE_LOG_WARN << "Borrow ledger is empty, keep the hugepage records.";
        } else {
            std::vector<std::string> returnedBorrowIds;
            for (const auto& borrowPageSize : borrowPageSizes) {
                if (unreturnedBorrowIds.count(borrowPageSize.first) == 0) {
                    returnedBorrowIds.emplace_back(borrowPageSize.first);
                }
            }
            if (!returnedBorrowIds.empty()) {
                UBSE_LOG_INFO << "Remove returned borrow records, count=" << returnedBorrowIds.size();
                ret = HugePageBorrowStorage::DeleteBorrowPageSizes(returnedBorrowIds);
                if (ret != VM_OK) {
                    UBSE_LOG_WARN << "Remove returned borrow records failed, " << FormatRetCode(ret);
                }
            }
        }
    }
    if (targetSizeMap.empty()) {
        UBSE_LOG_INFO << "No hugepage needs to be set.";
        return VM_OK;
    }
    ubse::security::CapabilityGuard overrideGuard({CAP_DAC_OVERRIDE});
    if (!overrideGuard.IsEnabled()) {
        UBSE_LOG_ERROR << "Failed to enable CAP_DAC_OVERRIDE.";
        return VM_ERROR;
    }
    for (const auto& [key, size] : targetSizeMap) {
        HugePageInfo pageInfo{key.first, size, key.second};
        ret = AllocateHugePages(pageInfo);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "Allocate hugepages failed, numaId=" << key.first << ", pageSize=" << key.second
                           << "kB, size=" << size << ", " << FormatRetCode(ret);
            return ret;
        }
    }
    UBSE_LOG_INFO << "Reconcile hugepages end, numaCount=" << targetSizeMap.size();
    return VM_OK;
}

VmResult HugePageHandler::AllocateHugePages(const HugePageInfo& pageInfo)
{
    UBSE_LOG_INFO << "Allocate hugePages start, numaId=" << pageInfo.numaId
                  << ", borrowedSize=" << pageInfo.borrowedSize << ", pageSize=" << pageInfo.pageSize << "kB";
    std::string filePath;
    VmResult ret = GetHugePageCanonicalPath(std::to_string(pageInfo.numaId), pageInfo.pageSize, filePath);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "GetHugePageCanonicalPath failed, pageSize=" << pageInfo.pageSize << "kB, "
                       << FormatRetCode(ret);
        return VM_ERROR;
    }
    // Calculate how many hugepages of this page size the borrowed memory needs (round up if not divisible).
    const uint64_t pageSizeBytes = pageInfo.pageSize * 1024;
    const uint64_t targetHugePages =
        pageInfo.borrowedSize / pageSizeBytes + ((pageInfo.borrowedSize % pageSizeBytes == 0) ? 0 : 1);
    if (pageInfo.borrowedSize % pageSizeBytes != 0) {
        UBSE_LOG_WARN << "Borrowed size is not aligned to page size, rounding up, numaId=" << pageInfo.numaId
                      << ", borrowedSize=" << pageInfo.borrowedSize << ", pageSize=" << pageInfo.pageSize
                      << "kB, targetHugePages=" << targetHugePages;
    }
    if (targetHugePages == 0) {
        UBSE_LOG_INFO << "No hugePages need to be allocated, numaId=" << pageInfo.numaId
                      << ", pageSize=" << pageInfo.pageSize << "kB";
        return VM_OK;
    }
    uint64_t currentHugePages = 0;
    ret = GetCurrentHugePages(filePath, currentHugePages);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Get currentHugePages failed, pageSize=" << pageInfo.pageSize << "kB, " << FormatRetCode(ret);
        return VM_ERROR;
    }
    if (currentHugePages >= targetHugePages) {
        UBSE_LOG_INFO << "HugePages already enough, numaId=" << pageInfo.numaId << ", pageSize=" << pageInfo.pageSize
                      << "kB, targetHugePages=" << targetHugePages << ", currentHugePages=" << currentHugePages;
        return VM_OK;
    }
    for (size_t count = 0; count < MAX_WRITE_HUGE_PAGE_RETRY_TIME; ++count) {
        ret = RewriteHugePages(filePath, targetHugePages);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "RewriteHugePages failed, pageSize=" << pageInfo.pageSize
                           << "kB, targetHugePages=" << targetHugePages << ", " << FormatRetCode(ret);
            return VM_ERROR;
        }
        ret = GetCurrentHugePages(filePath, currentHugePages);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "Get currentHugePages failed, pageSize=" << pageInfo.pageSize << "kB, "
                           << FormatRetCode(ret);
            return VM_ERROR;
        }
        if (currentHugePages < targetHugePages) {
            UBSE_LOG_WARN << "Allocate hugePages not enough, one last try, numaId=" << pageInfo.numaId
                          << ", pageSize=" << pageInfo.pageSize << "kB, targetHugePages=" << targetHugePages
                          << ", currentHugePages=" << currentHugePages;
            sleep(WRITE_HUGE_PAGE_RETRY_INTERVAL);
            continue;
        }
        UBSE_LOG_INFO << "Allocate hugePages successed, numaId=" << pageInfo.numaId
                      << ", pageSize=" << pageInfo.pageSize << "kB, targetHugePages=" << targetHugePages;
        return VM_OK;
    }
    // The huge pages are not enough after retries, which may be caused by memory fragmentation.
    UBSE_LOG_ERROR << "Allocate hugePages not enough, numaId=" << pageInfo.numaId << ", pageSize=" << pageInfo.pageSize
                   << "kB, targetHugePages=" << targetHugePages << ", currentHugePages=" << currentHugePages;
    return VM_ERROR;
}

VmResult HugePageHandler::GetHugePageDirName(const uint64_t& pageSize, std::string& dirName)
{
    if (pageSize == HUGE_PAGE_SIZE_2M_KB) {
        dirName = "/hugepages/hugepages-2048kB/nr_hugepages";
        return VM_OK;
    }
    if (pageSize == HUGE_PAGE_SIZE_1G_KB) {
        dirName = "/hugepages/hugepages-1048576kB/nr_hugepages";
        return VM_OK;
    }
    UBSE_LOG_ERROR << "Invalid pageSize: " << pageSize << "kB, supported values are 2048 and 1048576 (kB).";
    return VM_ERROR;
}

VmResult HugePageHandler::GetHugePageCanonicalPath(const std::string& numaId, const uint64_t& pageSize,
                                                   std::string& filePath)
{
    if (!std::all_of(begin(numaId), end(numaId), [](char c) { return std::isdigit(c); })) {
        UBSE_LOG_ERROR << "numaId must be all digit.";
        return VM_ERROR;
    }
    std::string pathTail;
    VmResult ret = GetHugePageDirName(pageSize, pathTail);
    if (ret != VM_OK) {
        return ret;
    }
    filePath = HUGEPAGES_PATH_HEAD + numaId + pathTail;
    if (!VmFileUtil::CanonicalPath(filePath)) {
        UBSE_LOG_ERROR << "HugePage path is invalid, pageSize=" << pageSize << "kB, filePath: " << filePath;
        return VM_ERROR;
    }
    return VM_OK;
}

VmResult HugePageHandler::RewriteHugePages(const std::string& realPath, const uint64_t& targetHugePages)
{
    std::ofstream outputFile(realPath, std::ios::out);
    if (!outputFile.is_open()) {
        UBSE_LOG_ERROR << "Failed to open file, realPath: " << realPath;
        return VM_ERROR;
    }
    outputFile << targetHugePages << std::endl;
    outputFile.close();
    UBSE_LOG_INFO << "Allocate HugePages=" << targetHugePages;
    return VM_OK;
}

VmResult HugePageHandler::GetCurrentHugePages(const std::string& realPath, uint64_t& currentHugePages)
{
    std::ifstream inputFile(realPath);
    if (!inputFile.is_open()) {
        UBSE_LOG_ERROR << "Failed to open inputFile, realPath: " << realPath;
        return VM_ERROR;
    }
    std::string line;
    std::getline(inputFile, line);
    inputFile.close();
    std::istringstream iss(line);
    if (!(iss >> currentHugePages)) {
        UBSE_LOG_ERROR << "Failed to parse integer from file.";
        return VM_ERROR;
    }
    return VM_OK;
}
} // namespace vm
