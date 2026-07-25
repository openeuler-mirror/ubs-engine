/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "page_file_helper.h"
#include "ubse_security.h"

namespace mempooling::over_commit {
using namespace ubse::log;
using namespace ubse::utils;

std::string PageFileHelper::HUGEPAGES_PATH_HEAD = "/sys/devices/system/node/node";
std::string PageFileHelper::HUGEPAGES_PATH_TAIL = "/hugepages/hugepages-2048kB/nr_hugepages";
const int RETRYCOUNT = 5;

MpResult PageFileHelper::GetHugePageCanonicalPath(const std::string& remoteNumaId, std::string& filePath)
{
    filePath = HUGEPAGES_PATH_HEAD + remoteNumaId + HUGEPAGES_PATH_TAIL;
    if (!UbseFileUtil::CanonicalPath(filePath)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit] HugepagesPath is invalid.";
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit] HugepagesPath is invalid, filePath=" << filePath << ".";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult PageFileHelper::RewriteHugePagesWithRetry(const std::string& filePath, uint64_t& originalHugePages,
                                                   const uint16_t fst, const uint64_t snd, const int retryCount)
{
    int count = 0;
    auto res = ubse::security::ChangeOverrideCapability(true);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit] Change override capability failed.";
        return MEM_POOLING_ERROR;
    }
    do {
        auto ret = RewriteHugePages(filePath, snd);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit] RewriteHugePages failed, ret=" << ret << ".";
            (void)ubse::security::ChangeOverrideCapability(false);
            return MEM_POOLING_ERROR;
        }
        ret = GetOriginalHugePages(filePath, originalHugePages);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit] GetOriginalHugePages After RewriteHugePages failed, ret=" << ret << ".";
            (void)ubse::security::ChangeOverrideCapability(false);
            return MEM_POOLING_ERROR;
        }
        count++;
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit] SetHugePage=" << snd / KB22MB << ", GetHugePage=" << originalHugePages
            << ", count=" << count << ".";
    } while (count < retryCount && originalHugePages != snd / KB22MB);
    if (count >= retryCount && originalHugePages != snd / KB22MB) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit] SetHugePage do not match GetHugePage after retry=" << retryCount << ".";
        (void)ubse::security::ChangeOverrideCapability(false);
        return MEM_POOLING_ERROR;
    }
    (void)ubse::security::ChangeOverrideCapability(false);
    return MEM_POOLING_OK;
}

MpResult PageFileHelper::AllocateHugePages(const std::vector<MemBorrowInfoWithSrc>& memBorrowInfoWithSrcs)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit] Allocate hugePages start.";
    std::unordered_map<uint16_t, uint64_t> memBorrowInfoMap;
    for (const auto& [srcNumaId, presentNumaId, borrowSize] : memBorrowInfoWithSrcs) {
        if (memBorrowInfoMap.find(presentNumaId) == memBorrowInfoMap.end()) {
            memBorrowInfoMap[presentNumaId] = borrowSize;
        } else {
            memBorrowInfoMap[presentNumaId] += borrowSize;
        }
    }
    for (const auto& [fst, snd] : memBorrowInfoMap) {
        std::string filePath;
        VmResult ret = GetHugePageCanonicalPath(std::to_string(fst), filePath);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit] GetHugePageCanonicalPath failed, ret=" << ret << ".";
            return MEM_POOLING_ERROR;
        }
        uint64_t originalHugePages;
        ret = GetOriginalHugePages(filePath, originalHugePages);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit] GetOriginalHugePages failed, ret=" << ret << ".";
            return MEM_POOLING_ERROR;
        }
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit] RemoteNumaId=" << fst << ", OriginalHugePages=" << originalHugePages << ".";
        ret = RewriteHugePagesWithRetry(filePath, originalHugePages, fst, snd, RETRYCOUNT);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit] RewriteHugePages failed, ret=" << ret << ".";
            return MEM_POOLING_ERROR;
        }
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit] Allocate hugePages end.";
    return MEM_POOLING_OK;
}

MpResult PageFileHelper::GetOriginalHugePages(const std::string& filePath, uint64_t& originalHugePages)
{
    std::ifstream inputFile(filePath);
    if (!inputFile.is_open()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit] Failed to open inputFile.";
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit] Failed to open inputFile, realPath=" << filePath << ".";
        return MEM_POOLING_ERROR;
    }
    std::string line;
    std::getline(inputFile, line);
    inputFile.close();
    if (std::istringstream iss(line); !(iss >> originalHugePages)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit] Failed to parse integer from file.";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult PageFileHelper::RewriteHugePages(const std::string& realPath, const uint64_t borrowSize)
{
    // 按2M为计算单位,计算最后分配大页的大小
    const uint64_t nrHugePages = borrowSize / KB22MB;
    std::ofstream outputFile(realPath, std::ios::out);
    if (!outputFile.is_open()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit] Failed to open file.";
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit] Failed to open file, realPath=" << realPath << ".";
        return MEM_POOLING_ERROR;
    }
    outputFile << nrHugePages << std::endl;
    outputFile.close();
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit] Allocate HugePages=" << nrHugePages << ".";
    return MEM_POOLING_OK;
}
} // namespace mempooling::over_commit