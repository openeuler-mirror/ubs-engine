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

#include "mp_smap_helper.h"
#include "ubse_def.h"
#include "ubse_logger.h"
#include "ubse_security.h"
#include "ubse_storage.h"
#include "mp_error.h"
#include "over_commit_ucache_strategy.h"

namespace mempooling::smap {
constexpr int SMAP_OK = 0;
constexpr int SMAP_ERROR = 1;

using namespace ubse::log;
using namespace mempooling::smap;
using namespace ubse::storage;
using namespace ubse::utils;

const int MpSmapHelper::smapParamErrorCode = -22;
const int MpSmapHelper::smapDealErrorCode = -9;
const int MpSmapHelper::smapApplyMemErrorCode = -12;
const int MpSmapHelper::smapTimeDoutErrorCode = -110;
const int MpSmapHelper::smapPermErrorCode = -1;   // SMAP处理异常错误码 Operation not permitted -1
const int MpSmapHelper::smapIOErrorCode = -5;     // SMAP处理异常错误码 I/O error -5
const int MpSmapHelper::smapRangeErrorCode = -34; // SMAP处理异常错误码 Math result not representable -34
const int MpSmapHelper::smapBadFNErrorCode = -9;  // SMAP处理异常错误码 Bad file number -9

const int MpSmapHelper::enableModeDisableNumaMig = 0;
const int MpSmapHelper::enableModeEnableNumaMig = 1;
const int MpSmapHelper::smapMigrateBackDefaultDestNid = -1;
const int MpSmapHelper::paStartEndRangeSize = 2;
static MpSmapHelper g_instance;
const size_t MpSmapHelper::setSmapRemoteNumaInfoMaxRetryCount = 3;
const uint32_t MpSmapHelper::setSmapRemoteNumaInfoMaxRetryInterval = 1; // 单位s
const int MpSmapHelper::SMAP_QUERY_PID_NUM = 40;
const int MpSmapHelper::SMAP_PARTIAL_SUCCESS = -3;

MpSmapHelper& MpSmapHelper::GetInstance()
{
    return g_instance;
}

MpResult MpSmapHelper::Init()
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Init start.";
    MpResult ret = SmapModule::Init();
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Failed to init smap module.";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapModule init success.";

    SmapInitFunc smapInitFunc = SmapModule::GetSmapInit();
    if (smapInitFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Smap init func is null.";
        return MEM_POOLING_ERROR;
    }
    int res =
        smapInitFunc(static_cast<const uint32_t>(MpConfiguration::GetInstance().GetPageType()), SmapModule::RackVmLog);
    if (res == SMAP_OK) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Smap init success.";
    } else if (res == -1) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Smap already initialized.";
    } else if (res < -1) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Smap init failed, ret = " << res << ".";
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Smap init failed.";
        return MEM_POOLING_ERROR;
    }

    // 读数据库并设置场景
    res = static_cast<int>(ReadAndSetRunMode());
    if (res != SMAP_OK) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Set RunMode failed.";
    }

    return MEM_POOLING_OK;
}

static void GetRunMode(const std::string& keyPrefix, const std::string& key, const UbseByteBuffer& buff, void* ctx)
{
    if (buff.len != 1 || buff.data == nullptr || ctx == nullptr) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Ctx or respData is null.";
        return;
    }
    int& runMode = *(static_cast<int*>(ctx));
    runMode = static_cast<int>(buff.data[0]);
    return;
}
static const std::string KEYPREFIX_SMAP = "mempooling";

// 读取数据库设置场景
MpResult MpSmapHelper::ReadAndSetRunMode()
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Entry ReadAndSetRunMode.";
    int runMode = -1;
    MpResult ret = UbseStorageQueryData("mempooling", "_runMode", &runMode, GetRunMode);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Failed to query runmode data.";
        return MEM_POOLING_ERROR;
    }
    if (runMode == -1) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] No runMode record.";
        return MEM_POOLING_ERROR;
    }
    // 如果查询到场景信息,则按照数据库中场景参数设置
    ret = MpSmapHelper::SmapMode(runMode);
    std::string runModeStr = "invalid";
    if (runMode == 0) {
        runModeStr = "water line";
    } else if (runMode == 1) {
        runModeStr = "mem fragment";
    }

    if (ret == MEM_POOLING_OK) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] SmapMode success, runMode is " << runModeStr << ".";
    } else {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] SmapMode failed, runMode is " << runModeStr << ".";
        return ret;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Exit ReadAndSetRunMode.";
    return MEM_POOLING_OK;
}

// 设置场景(水线--0;碎片--1)，数据持久化
MpResult MpSmapHelper::SetRunModeAndWrite(int runMode)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Entry SetRunModeAndWrite.";
    // 设置场景
    MpResult ret = MpSmapHelper::SmapMode(runMode);
    std::string runModeStr = "invalid";
    if (runMode == 0) {
        runModeStr = "water line";
    } else if (runMode == 1) {
        runModeStr = "mem fragment";
    }

    if (ret == MEM_POOLING_OK) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] SmapMode success, runMode is " << runModeStr << ".";
    } else {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] SmapMode failed, runMode is " << runModeStr << ".";
        return ret;
    }
    // 碎片场景数据持久化
    UbseByteBuffer buffer;
    buffer.len = 1;
    buffer.data = new (std::nothrow) uint8_t[buffer.len];
    if (buffer.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] Failed to allocate memory, size=" << buffer.len << ".";
        return MEM_POOLING_ERROR;
    }
    buffer.data[0] = static_cast<uint8_t>(runMode);
    ret = UbseStoragePutData("mempooling", "_runMode", &buffer);
    delete[] buffer.data;
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] UbseStoragePutData failed.";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Exit SetRunModeAndWrite.";
    return MEM_POOLING_OK;
}

void MpSmapHelper::VmSmapClose()
{
    SmapModule::CloseSmapHandle();
}

int MpSmapHelper::QueryVMFreqArray(int pidIn, uint16_t* dataIn, uint32_t lengthIn, uint32_t& lengthOut, int dataSource)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Start QueryVMFreqArray.";
    SmapQueryVmFreqFunc smapQueryVmFreqFunc = SmapModule::GetSmapQueryVmFreq();
    if (smapQueryVmFreqFunc == nullptr) {
        return MEM_POOLING_ERROR;
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Cd VM mem freq array.";
    int ret = smapQueryVmFreqFunc(pidIn, dataIn, lengthIn, lengthOut, dataSource);
    if (ret == SMAP_OK) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Call smapQueryVmFreqFunc success.";
    } else if (ret < 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Call smapQueryVmFreqFunc failed.";
        if (ret == MEM_POOLING_ERROR_SIGN_INT) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Init smapQueryVmFreqFunc failed.";
        }
        if (ret == smapParamErrorCode) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Param of smapQueryVmFreqFunc error.";
        }
        return MEM_POOLING_ERROR;
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] End QueryVMFreqArray.";
    return MEM_POOLING_OK;
}

MpResult MpSmapHelper::SmapMode(int runMode)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SetSmapRunMode start.";
    SetSmapRunModeFunc setSmapRunModeFunc = SmapModule::GetSetSmapRunModeFunc();
    if (setSmapRunModeFunc == nullptr) {
        return MEM_POOLING_ERROR;
    }

    int ret = setSmapRunModeFunc(runMode);
    if (ret == SMAP_OK) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SetSmapRunMode success.";
    } else if (ret < 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SetSmapRunMode failed.";
        if (ret == MEM_POOLING_ERROR_SIGN_INT) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SetSmapRunMode init failed.";
            return MEM_POOLING_SMAP_NOT_INIT_ERROR;
        }
        if (ret == smapParamErrorCode) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SetSmapRunMode param error.";
            return MEM_POOLING_SMAP_PARAM_ERROR;
        }
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SetSmapRunMode end.";
    return MEM_POOLING_OK;
}

MpResult MpSmapHelper::GetHugePageCanonicalPath(const std::string& remoteNumaId, std::string& filePath)
{
    filePath = HUGEPAGES_PATH_HEAD + remoteNumaId + HUGEPAGES_PATH_TAIL;
    if (!UbseFileUtil::CanonicalPath(filePath)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] HugepagesPath is invalid.";
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] HugepagesPath is invalid, filePath=" << filePath << ".";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult MpSmapHelper::TryAllocateHugePagesOnce(const std::string& filePath, uint64_t targetHugePages)
{
    auto res = ubse::security::ChangeOverrideCapability(true);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Change override capability failed.";
        return MEM_POOLING_ERROR;
    }
    MpResult ret = RewriteHugePages(filePath, targetHugePages);
    (void)ubse::security::ChangeOverrideCapability(false);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] RewriteHugePages failed, target=" << targetHugePages << ", ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult MpSmapHelper::AllocateHugePagesWithRetry(uint64_t numaId, uint64_t borrowSize)
{
    const int MAX_RETRY = 100;
    int retryCnt = 0;

    std::string filePath;
    MpResult ret = GetHugePageCanonicalPath(std::to_string(numaId), filePath);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] GetHugePageCanonicalPath failed.";
        return MEM_POOLING_ERROR;
    }

    uint64_t originalHugePages = 0;
    ret = GetOriginalHugePages(filePath, originalHugePages);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] GetOriginalHugePages failed.";
        return MEM_POOLING_ERROR;
    }

    uint64_t addPages = borrowSize / (2 * 1024 * 1024);
    uint64_t targetHugePages = originalHugePages + addPages;

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MpSmapHelper] numaId=" << numaId << ", originalHugePages=" << originalHugePages
        << ", addPages=" << addPages << ", targetHugePages=" << targetHugePages << ".";

    do {
        ret = TryAllocateHugePagesOnce(filePath, targetHugePages);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpSmapHelper] RewriteHugePages failed at retry=" << retryCnt << ", numaId=" << numaId << ".";
            retryCnt++;
            continue;
        }

        // 再次读取实际 hugepages
        uint64_t realHugePages = 0;
        ret = GetOriginalHugePages(filePath, realHugePages);
        if (ret == MEM_POOLING_OK && realHugePages >= targetHugePages) {
            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpSmapHelper] Allocate hugepages success, numaId=" << numaId << ", realHugePages=" << realHugePages
                << ", targetHugePages=" << targetHugePages << ", retryCnt=" << retryCnt << ".";
            return MEM_POOLING_OK;
        }

        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] HugePages not reached target, numaId=" << numaId << ", realHugePages=" << realHugePages
            << ", targetHugePages=" << targetHugePages << ", retryCnt=" << retryCnt << ".";

        retryCnt++;
    } while (retryCnt < MAX_RETRY);

    UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MpSmapHelper] AllocateHugePages final failed after " << MAX_RETRY << " retries, numaId=" << numaId
        << ", targetHugePages=" << targetHugePages << ".";

    return MEM_POOLING_ERROR;
}

MpResult MpSmapHelper::AllocateHugePages(std::vector<uint64_t>& remoteNumaIds, std::vector<uint64_t>& borrowSizes)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Allocate hugePages start.";
    std::unordered_map<uint64_t, uint64_t> map;
    size_t size = remoteNumaIds.size();
    for (size_t i = 0; i < size; ++i) {
        if (map.find(remoteNumaIds[i]) != map.end()) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpSmapHelper] Update remoteNumaId:" << remoteNumaIds[i] << ", borrowSizes: " << borrowSizes[i]
                << ".";
            map[remoteNumaIds[i]] += borrowSizes[i];
        } else {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpSmapHelper] Add remoteNumaId:" << remoteNumaIds[i] << ", borrowSizes: " << borrowSizes[i] << ".";
            map[remoteNumaIds[i]] = borrowSizes[i];
        }
    }
    for (const auto& pair : map) {
        MpResult ret = AllocateHugePagesWithRetry(pair.first, pair.second);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpSmapHelper] AllocateHugePagesWithRetry failed for numaId=" << pair.first << ", ret=" << ret
                << ".";
            return MEM_POOLING_ERROR;
        }
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Allocate hugePages end.";
    return MEM_POOLING_OK;
}

MpResult MpSmapHelper::ReleaseHugePages(std::vector<uint64_t> &remoteNumaIds, std::vector<uint64_t> &borrowSizes)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Release hugePages start.";

    std::unordered_map<uint64_t, uint64_t> map;
    size_t size = remoteNumaIds.size();
    for (size_t i = 0; i < size; ++i) {
        if (map.find(remoteNumaIds[i]) != map.end()) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpSmapHelper] Update remoteNumaId:" << remoteNumaIds[i]
                << ", release borrowSize: " << borrowSizes[i] << ".";

            map[remoteNumaIds[i]] += borrowSizes[i];
        } else {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpSmapHelper] Add remoteNumaId:" << remoteNumaIds[i]
                << ", release borrowSize: " << borrowSizes[i] << ".";

            map[remoteNumaIds[i]] = borrowSizes[i];
        }
    }

    for (const auto &pair : map) {
        MpResult ret = ReleaseHugePagesWithRetry(pair.first, pair.second);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpSmapHelper] ReleaseHugePagesWithRetry failed for numaId="
                << pair.first << ", ret=" << ret << ".";
            return MEM_POOLING_ERROR;
        }
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Release hugePages end.";
    return MEM_POOLING_OK;
}

MpResult MpSmapHelper::ReleaseHugePagesWithRetry(uint64_t numaId, uint64_t borrowSize)
{
    const int MAX_RETRY = 100;
    int retryCnt = 0;
    std::string filePath;

    MpResult ret = GetHugePageCanonicalPath(std::to_string(numaId), filePath);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] GetHugePageCanonicalPath failed.";
        return MEM_POOLING_ERROR;
    }

    uint64_t originalHugePages = 0;
    ret = GetOriginalHugePages(filePath, originalHugePages);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] GetOriginalHugePages failed.";
        return MEM_POOLING_ERROR;
    }

    uint64_t releasePages = borrowSize / (2 * 1024 * 1024);
    uint64_t targetHugePages = 0;

    if (originalHugePages > releasePages) {
        targetHugePages = originalHugePages - releasePages;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MpSmapHelper] numaId=" << numaId << ", originalHugePages=" << originalHugePages
        << ", releasePages=" << releasePages << ", targetHugePages=" << targetHugePages << ".";

    do {
        ret = TryAllocateHugePagesOnce(filePath, targetHugePages);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] RewriteHugePages failed at retry="
                << retryCnt << ", numaId=" << numaId << ".";
            retryCnt++;
            continue;
        }

        uint64_t realHugePages = 0;
        ret = GetOriginalHugePages(filePath, realHugePages);
        if (ret == MEM_POOLING_OK &&
            realHugePages <= targetHugePages) {
            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpSmapHelper] Release hugepages success, numaId=" << numaId << ", realHugePages="
                << realHugePages << ", targetHugePages=" << targetHugePages << ", retryCnt=" << retryCnt << ".";
            return MEM_POOLING_OK;
        }

        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] HugePages not reached target, numaId=" << numaId << ", realHugePages="
            << realHugePages << ", targetHugePages=" << targetHugePages << ", retryCnt=" << retryCnt << ".";
        retryCnt++;
    } while (retryCnt < MAX_RETRY);

    UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MpSmapHelper] ReleaseHugePages final failed after " << MAX_RETRY << " retries, numaId=" << numaId
        << ", targetHugePages=" << targetHugePages << ".";

    return MEM_POOLING_ERROR;
}

void MpSmapHelper::RollBackHugePagesIfNeeded(bool hugePageAllocated,
                                             std::vector<uint64_t> &remoteNumaIds,
                                             std::vector<uint64_t> &borrowSizes)
{
    if (!hugePageAllocated) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Do not need to release.";
        return;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Start to execute release.";
    MpResult releaseRet = MpSmapHelper::GetInstance().ReleaseHugePages(remoteNumaIds, borrowSizes);
    if (releaseRet != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] ReleaseHugePages failed after VmsMigrate failed.";
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MpSmapHelper] ReleaseHugePages success after VmsMigrate failed.";
}

MpResult MpSmapHelper::RewriteHugePages(const std::string &realPath, uint64_t targetHugePages)
{
    // 按2M为计算单位,计算最后分配大页的大小
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MpSmapHelper] Param targetHugePages: " << targetHugePages << ".";
    std::ofstream outputFile(realPath, std::ios::out);
    if (!outputFile.is_open()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] Failed to open file, realPath: " << realPath << ".";
        return MEM_POOLING_ERROR;
    }
    outputFile << targetHugePages << std::endl;
    outputFile.close();
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MpSmapHelper] Allocate HugePages = " << targetHugePages << ".";
    return MEM_POOLING_OK;
}

MpResult MpSmapHelper::GetOriginalHugePages(const std::string& realPath, uint64_t& originalHugePages)
{
    std::ifstream inputFile(realPath);
    if (!inputFile.is_open()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] Failed to open inputFile, realPath: " << realPath << ".";
        return MEM_POOLING_ERROR;
    }
    std::string line;
    (void)std::getline(inputFile, line);
    inputFile.close();
    std::istringstream iss(line);
    if (!(iss >> originalHugePages)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Failed to parse integer from file.";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult MpSmapHelper::SmapMigrateRemoteNuma(MigrateNumaMsg msg)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapMigrateRemoteNuma start.";
    SmapMigrateRemoteNumaFunc smapMigrateRemoteNumaFunc = SmapModule::GetSmapMigrateRemoteNumaFunc();
    if (smapMigrateRemoteNumaFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Ptr smapMigrateRemoteNumaFunc == nullptr.";
        return MEM_POOLING_ERROR;
    }
    int ret = smapMigrateRemoteNumaFunc(&msg);
    if (ret != SMAP_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] SmapMigrateRemoteNumaFunc failed " << ret << ".";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapMigrateRemoteNuma succeed.";
    return MEM_POOLING_OK;
}

MpResult MpSmapHelper::GetVmRatioOnFaultNumaBySmap(const int16_t faultNumaId,
                                                   std::unordered_map<pid_t, smap::ProcessPayload>& processPayloadMap)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "GetVmRatioOnFaultNumaBySmap start.";
    const auto smapQueryProcessConfig = mempooling::smap::SmapModule::GetSmapGetRemoteProcessesFunc();
    if (smapQueryProcessConfig == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[Rebalance] smapQueryProcessConfig is null.";
        return MEM_POOLING_ERROR;
    }
    smap::ProcessPayload processPayload[MpSmapHelper::SMAP_QUERY_PID_NUM];
    int retLen = 0;
    // retLen长度不会超过MP_SMAP_QUERY_PID_NUM，不会越界
    auto ret = static_cast<MpResult>(
        smapQueryProcessConfig(faultNumaId, processPayload, MpSmapHelper::SMAP_QUERY_PID_NUM, &retLen));
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Query config failed for NUMA=" << faultNumaId << ".";
        return MEM_POOLING_ERROR;
    }

    // 遍历 processPayload 数组，填充 map
    for (int i = 0; i < MpSmapHelper::SMAP_QUERY_PID_NUM; ++i) {
        const smap::ProcessPayload& payload = processPayload[i];
        processPayloadMap[payload.pid] = payload; // 以 pid 为键插入到 map 中
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "GetVmRatioOnFaultNumaBySmap end.";
    return MEM_POOLING_OK;
}

MpResult MpSmapHelper::SmapMigratePidRemoteNumaHelper(pid_t* pidArr, int len, int srcNid, int destNid)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapMigratePidRemoteNumaHelper start.";

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MpSmapHelper] Input remoteNuma, srcNid = " << srcNid << ", destNid = " << destNid << ".";

    MigrateEscapeMsg msg;
    msg.count = len;

    std::unordered_map<pid_t, smap::ProcessPayload> processPayloadMap;
    auto ret = GetVmRatioOnFaultNumaBySmap(srcNid, processPayloadMap);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] GetVmRatioOnFaultNumaBySmap failed.";
        return MEM_POOLING_ERROR;
    }

    int runMode = -1;

    auto sceneType = MpConfiguration::GetInstance().GetSceneType();
    if (sceneType == MpSceneType::CONTAINER_SCENE) {
        runMode = 0;
    } else {
        ret = UbseStorageQueryData("mempooling", "_runMode", &runMode, GetRunMode);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Failed to query runmode data.";
            return MEM_POOLING_ERROR;
        }
        if (runMode == -1) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] No runMode record.";
            return MEM_POOLING_ERROR;
        }
    }
    for (int i = 0; i < msg.count; i++) {
        msg.payload[i].pid = pidArr[i];
        msg.payload[i].srcNid = srcNid;
        msg.payload[i].destNid = destNid;

        if (processPayloadMap.count(pidArr[i]) <= 0) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpSmapHelper] The pid=" << pidArr[i] << " not in smap config.";
            return MEM_POOLING_ERROR;
        }

        msg.payload[i].ratio = processPayloadMap[pidArr[i]].ratio;
        msg.payload[i].memSize = processPayloadMap[pidArr[i]].memSize;
        msg.payload[i].migrateMode = static_cast<MigrateMode>(runMode);
    }

    ret = SmapMigratePidMultiRemoteNumaHelper(msg);
    if (ret != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapMigratePidRemoteNumaHelper end.";
    return MEM_POOLING_OK;
}

MpResult MpSmapHelper::SmapMigratePidMultiRemoteNumaHelper(MigrateEscapeMsg& msg)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapMigratePidMultiRemoteNumaHelper start.";

    SmapMigratePidRemoteNumaFunc smapMigratePidRemoteNumaFunc = SmapModule::GetSmapMigratePidRemoteNumaFunc();
    if (smapMigratePidRemoteNumaFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] Ptr smapMigratePidRemoteNumaFunc == nullptr.";
        return MEM_POOLING_ERROR;
    }

    int ret = smapMigratePidRemoteNumaFunc(&msg);
    if (ret == SMAP_OK) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapMigratePidRemoteNumaHelper succeed.";
    } else if (ret == smapPermErrorCode) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] Operation not permitted. ret: " << ret << ".";
        return MEM_POOLING_ERROR;
    } else if (ret == smapParamErrorCode) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Invalid argument. ret: " << ret << ".";
        return MEM_POOLING_ERROR;
    } else if (ret == smapTimeDoutErrorCode) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Connection timed out. ret: " << ret << ".";
        return MEM_POOLING_ERROR;
    } else if (ret == smapIOErrorCode) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] I/O error. ret: " << ret << ".";
        return MEM_POOLING_ERROR;
    } else {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] SmapMigratePidRemoteNumaHelper error. ret: " << ret << ".";
        return MEM_POOLING_ERROR;
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapMigratePidMultiRemoteNumaHelper end.";
    return MEM_POOLING_OK;
}

MpResult MpSmapHelper::SmapMigratePidMultiRemoteNumaHelperWithRetry(MigrateEscapeMsg& msg)
{
    constexpr int kMaxRetry = 3;
    constexpr auto kRetryInterval = std::chrono::seconds(1);

    MpResult ret = MEM_POOLING_ERROR;

    for (int i = 0; i < kMaxRetry; ++i) {
        ret = MpSmapHelper::SmapMigratePidMultiRemoteNumaHelper(msg);
        if (ret == MEM_POOLING_OK) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpSmapHelper] SmapMigratePidRemoteNumaHelper succeed.";
            return MEM_POOLING_OK;
        }

        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] SmapMigratePidRemoteNumaHelper failed, retry " << (i + 1) << "/" << kMaxRetry
            << ", ret=" << ret;

        if (i < kMaxRetry - 1) {
            std::this_thread::sleep_for(kRetryInterval);
        }
    }

    return ret;
}

int MpSmapHelper::SmapEnableProcessMigrateHelper(pid_t* pidArr, int len, int enable, int flags)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapEnableProcessMigrateHelper start.";

    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = SmapModule::GetSmapEnableProcessMigrateFunc();
    if (smapEnableProcessMigrateFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] Ptr smapEnableProcessMigrateFunc == nullptr.";
        return MEM_POOLING_ERROR;
    }

    int ret = smapEnableProcessMigrateFunc(pidArr, len, enable, flags);
    if (ret == SMAP_OK) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapEnableProcessMigrateHelper succeed.";
    } else if (ret == smapPermErrorCode) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] Operation not permitted. ret: " << ret << ".";
        return MEM_POOLING_ERROR;
    } else if (ret == smapParamErrorCode) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Invalid argument. ret: " << ret << ".";
        return MEM_POOLING_ERROR;
    } else if (ret == smapApplyMemErrorCode) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Out of memory. ret: " << ret << ".";
        return MEM_POOLING_ERROR;
    } else if (ret == smapRangeErrorCode) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] The smapRangeErrorCode. ret: " << ret << ".";
        return MEM_POOLING_ERROR;
    } else if (ret == smapBadFNErrorCode) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Bad file number. ret: " << ret << ".";
        return MEM_POOLING_ERROR;
    } else {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] SmapEnableProcessMigrateHelper error. ret: " << ret << ".";
        return MEM_POOLING_ERROR;
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapEnableProcessMigrateHelper end.";
    return MEM_POOLING_OK;
}
MpResult MpSmapHelper::SetSmapRemoteNumaInfo(
    const int16_t& srcNumaId, const std::vector<over_commit::MemBorrowInfoWithSrc>& memBorrowInfosWithSrc)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SetSmapRemoteNumaInfo start.";
    const SetSmapRemoteNumaInfoFunc setSmapRemoteNumaInfo = SmapModule::GetSetSmapRemoteNumaInfo();
    if (setSmapRemoteNumaInfo == nullptr) {
        return MEM_POOLING_ERROR;
    }
    MpResult ret{};
    auto borrowNuma = srcNumaId;
    std::unordered_map<uint16_t, uint64_t> memBorrowInfoMap;
    for (const auto& [srcNid, presentNumaId, borrowSize] : memBorrowInfosWithSrc) {
        if (borrowNuma != -1 && srcNid != static_cast<uint64_t>(borrowNuma)) {
            continue;
        }
        if (memBorrowInfoMap.find(presentNumaId) == memBorrowInfoMap.end()) {
            memBorrowInfoMap[presentNumaId] = borrowSize;
        } else {
            memBorrowInfoMap[presentNumaId] += borrowSize;
        }
    }
    for (const auto& [fst, snd] : memBorrowInfoMap) {
        RemoteNumaInfo remoteNumaInfo = {.srcNid = srcNumaId, .destNid = fst, .size = snd >> over_commit::KB2MB};
        remoteNumaInfo.size *= (1 - over_commit::GetLocalUcacheUsageRatio());
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] RemoteNumaInfo: " << remoteNumaInfo.ToString() << ".";
        size_t i = 0;
        for (; i < setSmapRemoteNumaInfoMaxRetryCount; ++i) {
            auto smapRet = setSmapRemoteNumaInfo(&remoteNumaInfo);
            if (smapRet != SMAP_OK) {
                UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[MpSmapHelper] Set failed, retry_id=" << i << ". " << remoteNumaInfo.ToString() << ".";
                sleep(setSmapRemoteNumaInfoMaxRetryInterval);
            } else {
                break;
            }
        }
        if (i == setSmapRemoteNumaInfoMaxRetryCount) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpSmapHelper] Smap error, set smap remote numa info over max retry count.";
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpSmapHelper] Current max retry count ret=" << ret << ".";
            return MEM_POOLING_ERROR;
        }
    }
    return MEM_POOLING_OK;
}

MigrateOutMsg MpSmapHelper::GetMigrateOutMsgInOverCommitMultiNuma(
    const std::vector<over_commit::MemMigrateResult>& memMigrateResults, const uint16_t ratio)
{
    MigrateOutMsg migrateOutMsg{};

    // 1、按pid分组
    std::unordered_map<pid_t, std::vector<size_t>> grouped;
    for (size_t i = 0; i < memMigrateResults.size(); ++i) {
        grouped[memMigrateResults[i].pid].push_back(i);
    }

    migrateOutMsg.count = static_cast<int>(grouped.size());
    if (migrateOutMsg.count > MAX_NR_MIGOUT_MP) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] Invalid migrate-out count, expected max_size=" << MAX_NR_MIGOUT_MP
            << ", actual_size=" << migrateOutMsg.count << ".";
        migrateOutMsg.count = 0;
        return migrateOutMsg;
    }

    // 2、 填充payload
    int payloadIdx = 0;
    for (const auto& [pid, idxVec] : grouped) {
        auto& payload = migrateOutMsg.payload[payloadIdx];
        payload.srcNid = -1;
        payload.pid = pid;
        payload.count = std::min<int>(idxVec.size(), REMOTE_NUMA_NUM);

        for (int innerIdx = 0; innerIdx < payload.count; ++innerIdx) {
            const auto& r = memMigrateResults[idxVec[innerIdx]];
            auto& inner = payload.inner[innerIdx];
            inner.destNid = r.remoteNumaId;
            inner.memSize = 0;
            inner.ratio = r.maxRatio;
            inner.migrateMode = MIG_RATIO_MODE;

            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpSmapHelper] pid=" << pid << ", destNid=" << inner.destNid << ", ratio=" << inner.ratio
                << ", memSize=" << inner.memSize << ", mode=" << inner.migrateMode << ".";
        }

        payloadIdx++;
    }

    return migrateOutMsg;
}

MigrateOutMsg MpSmapHelper::GetMigrateOutMsgInOverCommit(
    const std::vector<over_commit::MemMigrateResult>& memMigrateResults, const uint16_t ratio)
{
    MigrateOutMsg migrateOutMsg{};
    migrateOutMsg.count = static_cast<int>(memMigrateResults.size());

    if (migrateOutMsg.count > MAX_NR_MIGOUT_MP) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] Invalid migrate-out count, expected max_size= " << MAX_NR_MIGOUT_MP
            << ", actual_size=" << migrateOutMsg.count << ".";
        migrateOutMsg.count = 0;
        return migrateOutMsg;
    }
    int indexOfMigrateOutPayloadInner = 0; // 当前版本仅支持单远端numa
    for (int i = 0; i < migrateOutMsg.count; i++) {
        migrateOutMsg.payload[i].count = 1; // // 当前版本仅支持单远端numa
        migrateOutMsg.payload[i].pid = memMigrateResults[i].pid;
        migrateOutMsg.payload[i].inner[indexOfMigrateOutPayloadInner].destNid = memMigrateResults[i].remoteNumaId;
        migrateOutMsg.payload[i].inner[indexOfMigrateOutPayloadInner].ratio = memMigrateResults[i].maxRatio;
        migrateOutMsg.payload[i].inner[indexOfMigrateOutPayloadInner].memSize = 0;
        migrateOutMsg.payload[i].inner[indexOfMigrateOutPayloadInner].migrateMode = MIG_RATIO_MODE;
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_NAME)
            << "[MpSmapHelper] Pid=" << memMigrateResults[i].pid << ", destNid=" << memMigrateResults[i].remoteNumaId
            << ", ratio=" << migrateOutMsg.payload[i].inner[indexOfMigrateOutPayloadInner].ratio
            << ", memSize=" << migrateOutMsg.payload[i].inner[indexOfMigrateOutPayloadInner].memSize
            << ", migrateMode=" << migrateOutMsg.payload[i].inner[indexOfMigrateOutPayloadInner].migrateMode << ".";
    }
    return migrateOutMsg;
}

MpResult MpSmapHelper::MigrateOutInOverCommit(const std::vector<over_commit::MemMigrateResult>& memMigrateResults,
                                              const uint16_t ratio)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] MigrateOutInOverCommit start.";
    const SmapMigrateOutFunc smapMigrateOutFunc = SmapModule::GetSmapMigrateOut();
    if (smapMigrateOutFunc == nullptr) {
        return MEM_POOLING_ERROR;
    };

    MigrateOutMsg migrateOutMsg;
    if (MpConfiguration::GetInstance().GetMpSceneType() == MpSceneType::VIRTUAL_SCENE &&
        MpConfiguration::GetInstance().GetMultiNumaScene() == true) {
        // 多numa场景+虚机场景 迁移策略生成
        migrateOutMsg = GetMigrateOutMsgInOverCommitMultiNuma(memMigrateResults, ratio);
    } else {
        // 单numa场景 迁移策略生成
        migrateOutMsg = GetMigrateOutMsgInOverCommit(memMigrateResults, ratio);
    }

    const auto ret = smapMigrateOutFunc(&migrateOutMsg, static_cast<int>(MpConfiguration::GetInstance().GetPageType()));
    if (ret == SMAP_PARTIAL_SUCCESS) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] MigrateOutInOverCommit Partial success.";
        return MEM_POOLING_SMAP_PARTIAL_SUCCESS;
    }
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] MigrateOutInOverCommit failed, ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

int MpSmapHelper::SmapAddProcessTrackingHelper(const std::vector<pid_t>& pidVec,
                                               const std::vector<uint32_t>& scanTimeVec, int scanType,
                                               const std::vector<uint32_t>& durationVec)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapAddProcessTrackingHelper start.";
    const SmapAddProcessTrackingFunc smapAddProcessTrackingFunc = SmapModule::GetSmapAddProcessTrackingFunc();
    if (smapAddProcessTrackingFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Failed to get function symbol.";
        return MEM_POOLING_ERROR;
    };

    if (pidVec.size() != scanTimeVec.size() || pidVec.size() != durationVec.size()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Input array lengths do not match.";
        return MEM_POOLING_ERROR;
    }

    // 获取指针
    pid_t* pidArr = const_cast<pid_t*>(pidVec.data());
    uint32_t* scanTimeArr = const_cast<uint32_t*>(scanTimeVec.data());
    uint32_t* durationArr = const_cast<uint32_t*>(durationVec.data());
    int len = static_cast<int>(pidVec.size());
    int ret = smapAddProcessTrackingFunc(pidArr, scanTimeArr, durationArr, len, scanType);
    if (ret != SMAP_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Failed to add process tracking to Smap.";
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] The error code = " << ret << ".";
    } else {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] Successfully added process tracking to Smap.";
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapAddProcessTrackingHelper end.";
    return ret;
}

int MpSmapHelper::SmapRemoveProcessTrackingHelper(const std::vector<pid_t>& pidVec, int flags)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapRemoveProcessTrackingHelper start.";
    const SmapRemoveProcessTrackingFunc smapRemoveProcessTrackingFunc = SmapModule::GetSmapRemoveProcessTrackingFunc();
    if (smapRemoveProcessTrackingFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Failed to get function symbol.";
        return MEM_POOLING_ERROR;
    };

    pid_t* pidArr = const_cast<pid_t*>(pidVec.data());
    int len = static_cast<int>(pidVec.size());

    int ret = smapRemoveProcessTrackingFunc(pidArr, len, flags);
    if (ret != SMAP_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] Failed to remove process tracking to Smap.";
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] The error code = " << ret << ".";
    } else {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] Successfully removed process tracking to Smap.";
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapRemoveProcessTrackingHelper end.";
    return ret;
}

MpResult MpSmapHelper::SmapMigrateBack(MigrateBackMsg& migrateBackMsg)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapMigrateBack start.";
    SmapMigrateBackFunc smapMigrateBackFunc = SmapModule::GetSmapMigrateBackFunc();
    if (smapMigrateBackFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Ptr smapMigrateBackFunc == nullptr.";
        return MEM_POOLING_ERROR;
    }
    int ret = smapMigrateBackFunc(&migrateBackMsg);
    if (ret != SMAP_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapMigrateBackFunc failed " << ret << ".";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapMigrateBack succeed.";
    return MEM_POOLING_OK;
}

MpResult MpSmapHelper::SmapEnableNuma(EnableNodeMsg& enableMsg)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapEnableNuma start.";
    SmapEnableNodeFunc smapEnableNodeFunc = SmapModule::GetSmapEnableNodeFunc();
    if (smapEnableNodeFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Ptr smapEnableNodeFunc == nullptr.";
        return MEM_POOLING_ERROR;
    }
    int ret = smapEnableNodeFunc(&enableMsg);
    if (ret != SMAP_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapEnableNodeFunc failed " << ret << ".";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] SmapEnableNuma succeed.";
    return MEM_POOLING_OK;
}

MpResult MpSmapHelper::SmapGetBackResult(uint64_t taskId, uint16_t& ret)
{
    ret = 0;
    std::string fileHead = "/sys/kernel/debug/smap/mb_";
    std::string fileEnd = std::to_string(taskId);
    std::string result = fileHead + fileEnd;

    auto res = ubse::security::ChangeOverrideCapability(true);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Change override capability failed.";
        return MEM_POOLING_ERROR;
    }

    std::ifstream file(result);

    if (!file.is_open()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Can not open file, file name: " << result;
        return MEM_POOLING_ERROR;
    }

    std::string line;
    if (std::getline(file, line)) {
        if (std::stoul(line) >= MB_TASK_BUTT) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Get smap back result is invaild.";
            file.close();
            (void)ubse::security::ChangeOverrideCapability(false);
            return MEM_POOLING_ERROR;
        } else {
            ret = std::stoul(line);
            file.close();
            (void)ubse::security::ChangeOverrideCapability(false);
            return MEM_POOLING_OK;
        }
    } else {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapHelper] Get file result fail, file name: " << result;
        file.close();
        (void)ubse::security::ChangeOverrideCapability(false);
        return MEM_POOLING_ERROR;
    }
}

MpResult MpSmapHelper::GetLocalSmapBackResult(uint64_t taskId)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] GetLocalSmapBackResult start.";

    uint16_t smapBackRet = MB_TASK_BUTT;
    uint32_t retryCount = 0;
    bool isRetry;
    do {
        isRetry = false;
        uint32_t ret = SmapGetBackResult(taskId, smapBackRet);
        if (ret != MEM_POOLING_OK || smapBackRet >= MB_TASK_ERR) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpSmapHelper] Smap back result is fail, ret=" << smapBackRet << ".";
            return MEM_POOLING_ERROR;
        }
        if (smapBackRet == MB_TASK_DONE) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Get smap back result is OK.";
            return MEM_POOLING_OK;
        }
        if (smapBackRet == MB_TASK_CREATED || smapBackRet == MB_TASK_WAITING) {
            if (retryCount < RETRY_MAX_COUNT) {
                isRetry = true;
                retryCount++;
                sleep(GET_RET_TIME);
            }
        }
    } while (isRetry);

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MpSmapHelper] Get smap result is timeout, retry_time=" << RETRY_MAX_COUNT * GET_RET_TIME << "s.";

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] GetLocalSmapBackResult succeed.";

    return MEM_POOLING_ERROR;
}

MpResult MpSmapHelper::SmapQueryProcessConfigHelper(int nid, std::vector<ProcessPayload>& processPayloadList)
{
    const SmapQueryProcessConfigFunc smapQueryProcessConfigFunc = SmapModule::GetSmapQueryProcessConfigFunc();
    if (smapQueryProcessConfigFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[RmrsSmapHelper] Failed to get function symbol.";
        return MEM_POOLING_ERROR;
    }

    ProcessPayload payloadArr[SMAP_QUERY_PID_NUM];
    int realLen = 0;
    int res = smapQueryProcessConfigFunc(nid, payloadArr, SMAP_QUERY_PID_NUM, &realLen);
    if (res != SMAP_OK || realLen < 0 || realLen > SMAP_QUERY_PID_NUM) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[RmrsSmapHelper] SmapQueryProcessConfig error." << nid << " " << realLen << " " << res;
        return MEM_POOLING_ERROR;
    }
    for (int i = 0; i < realLen; i++) {
        processPayloadList.push_back(payloadArr[i]);
    }
    return MEM_POOLING_OK;
}

} // namespace mempooling::smap