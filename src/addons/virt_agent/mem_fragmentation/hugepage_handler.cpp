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
#include <fstream>
#include <ubse_com.h>
#include <ubse_logger.h>

#include "vm_file_util.h"
#include "vm_def.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("vm_plugin");
using namespace ubse::log;

VmResult HugePageHandler::SetHugePages(const uint64_t &numaId, const uint64_t &borrowedSize)
{
    UBSE_LOG_INFO << "Set hugepages start.";

    // Directly execute the set hugepages operation on this node
    HugePageInfo hugePageInfo{numaId, borrowedSize};
    auto ret = AllocateHugePages(hugePageInfo);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Set hugepages failed. " << FormatRetCode(ret);
        return VM_ERROR;
    }
    UBSE_LOG_INFO << "Set hugepages successfully.";
    return ret;
}

VmResult HugePageHandler::AllocateHugePages(const HugePageInfo &pageInfo)
{
    UBSE_LOG_INFO << "Allocate hugePages start, numaId=" << pageInfo.numaId
                  << ", borrowSizes=" << pageInfo.borrowedSize;
    std::string filePath;
    VmResult ret = GetHugePageCanonicalPath(std::to_string(pageInfo.numaId), filePath);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "GetHugePageCanonicalPath failed," << FormatRetCode(ret);
        return VM_ERROR;
    }
    uint64_t originalHugePages;
    ret = GetCurrentHugePages(filePath, originalHugePages);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Get originalHugePages failed," << FormatRetCode(ret);
        return VM_ERROR;
    }
    UBSE_LOG_INFO << "NumaId=" << pageInfo.numaId << ", originalHugePages=" << originalHugePages;
    // Calculate the size of the final large page allocation using 2M as the unit (round up if not divisible).
    const uint64_t hugePages = (pageInfo.borrowedSize + (2 * MB_TO_BYTES - 1)) / (2 * MB_TO_BYTES);
    for (size_t count = 0; count < MAX_WRITE_HUGE_PAGE_RETRY_TIME; ++count) {
        ret = RewriteHugePages(filePath, hugePages);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "RewriteHugePages failed, " << FormatRetCode(ret);
            return VM_ERROR;
        }
        ret = GetCurrentHugePages(filePath, originalHugePages);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "Get originalHugePages failed," << FormatRetCode(ret);
            return VM_ERROR;
        }
        if (originalHugePages != hugePages) {
            UBSE_LOG_WARN << "Allocate hugePages not enough, one last try.";
            sleep(WRITE_HUGE_PAGE_RETRY_INTERVAL);
            continue;
        }
        UBSE_LOG_INFO << "Allocate hugePages successed.";
        return VM_OK;
    }
    UBSE_LOG_INFO << "Allocate hugePages failed.";
    return VM_ERROR;
}

VmResult HugePageHandler::GetHugePageCanonicalPath(const std::string &numaId, std::string &filePath)
{
    if (!std::all_of(begin(numaId), end(numaId), [](char c) { return std::isdigit(c); })) {
        UBSE_LOG_ERROR << "numaId must be all digit.";
        return VM_ERROR;
    }
    filePath = HUGEPAGES_PATH_HEAD + numaId + HUGEPAGES_PATH_TAIL;
    if (!VmFileUtil::CanonicalPath(filePath)) {
        UBSE_LOG_ERROR << "HugePage path is invalid, filePath: " << filePath;
        return VM_ERROR;
    }
    return VM_OK;
}

VmResult HugePageHandler::RewriteHugePages(const std::string &realPath, const uint64_t &targetHugePages)
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

VmResult HugePageHandler::GetCurrentHugePages(const std::string &realPath, uint64_t &currentHugePages)
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