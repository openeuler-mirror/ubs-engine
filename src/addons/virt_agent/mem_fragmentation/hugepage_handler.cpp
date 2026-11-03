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

#include <fstream>

#include <unistd.h>

#include <ubse_com.h>
#include <ubse_logger.h>

#include "vm_def.h"
#include "vm_file_util.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;

VmResult HugePageHandler::SetHugePages(const uint64_t& numaId, const uint64_t& borrowedSize, const uint64_t& pageSize)
{
    UBSE_LOG_INFO << "Set hugepages start, numaId=" << numaId << ", pageSize=" << pageSize << "kB.";

    // Directly execute the set hugepages operation on this node
    HugePageInfo hugePageInfo{numaId, borrowedSize, pageSize};
    auto ret = AllocateHugePages(hugePageInfo);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Set hugepages failed, numaId=" << numaId << ", pageSize=" << pageSize << "kB. "
                       << FormatRetCode(ret);
        return VM_ERROR;
    }
    UBSE_LOG_INFO << "Set hugepages successfully, numaId=" << numaId << ", pageSize=" << pageSize << "kB.";
    return ret;
}

VmResult HugePageHandler::AllocateHugePages(const HugePageInfo& pageInfo)
{
    UBSE_LOG_INFO << "Allocate hugePages start, numaId=" << pageInfo.numaId << ", borrowSizes=" << pageInfo.borrowedSize
                  << ", pageSize=" << pageInfo.pageSize << "kB";
    std::string filePath;
    VmResult ret = GetHugePageCanonicalPath(std::to_string(pageInfo.numaId), pageInfo.pageSize, filePath);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "GetHugePageCanonicalPath failed, pageSize=" << pageInfo.pageSize << "kB, "
                       << FormatRetCode(ret);
        return VM_ERROR;
    }
    uint64_t originalHugePages;
    ret = GetCurrentHugePages(filePath, originalHugePages);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Get originalHugePages failed, pageSize=" << pageInfo.pageSize << "kB, "
                       << FormatRetCode(ret);
        return VM_ERROR;
    }
    UBSE_LOG_INFO << "NumaId=" << pageInfo.numaId << ", originalHugePages=" << originalHugePages
                  << ", pageSize=" << pageInfo.pageSize << "kB";
    // Calculate the size of the final large page allocation using pageSize as the unit (round up if not divisible).
    const uint64_t pageSizeBytes = pageInfo.pageSize * 1024;
    const uint64_t hugePages =
        pageInfo.borrowedSize / pageSizeBytes + ((pageInfo.borrowedSize % pageSizeBytes == 0) ? 0 : 1);
    if (pageInfo.borrowedSize % pageSizeBytes != 0) {
        UBSE_LOG_WARN << "Borrowed size is not aligned to page size, rounding up, numaId=" << pageInfo.numaId
                      << ", borrowedSize=" << pageInfo.borrowedSize << ", pageSize=" << pageInfo.pageSize
                      << "kB, targetHugePages=" << hugePages;
    }
    for (size_t count = 0; count < MAX_WRITE_HUGE_PAGE_RETRY_TIME; ++count) {
        ret = RewriteHugePages(filePath, hugePages);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "RewriteHugePages failed, pageSize=" << pageInfo.pageSize
                           << "kB, targetHugePages=" << hugePages << ", " << FormatRetCode(ret);
            return VM_ERROR;
        }
        ret = GetCurrentHugePages(filePath, originalHugePages);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "Get originalHugePages failed, pageSize=" << pageInfo.pageSize << "kB, "
                           << FormatRetCode(ret);
            return VM_ERROR;
        }
        if (originalHugePages != hugePages) {
            UBSE_LOG_WARN << "Allocate hugePages not enough, one last try, numaId=" << pageInfo.numaId
                          << ", pageSize=" << pageInfo.pageSize << "kB, targetHugePages=" << hugePages
                          << ", currentHugePages=" << originalHugePages;
            sleep(WRITE_HUGE_PAGE_RETRY_INTERVAL);
            continue;
        }
        UBSE_LOG_INFO << "Allocate hugePages successed, numaId=" << pageInfo.numaId
                      << ", pageSize=" << pageInfo.pageSize << "kB, targetHugePages=" << hugePages;
        return VM_OK;
    }
    UBSE_LOG_INFO << "Allocate hugePages failed, numaId=" << pageInfo.numaId << ", pageSize=" << pageInfo.pageSize
                  << "kB, targetHugePages=" << hugePages;
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