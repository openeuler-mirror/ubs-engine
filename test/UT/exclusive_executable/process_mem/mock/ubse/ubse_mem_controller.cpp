/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>

#include "ubse_error.h"
#include "ubse_mem_controller.h"
#include "mock_control.h"

namespace ubse::mem::controller {

static std::vector<UbseNumaMemoryDebtInfo> g_mockDebtInfos;
static std::vector<UbseNumaMemoryImportDebtInfo> g_mockImportDebtInfos;
static uint32_t g_mockNumaCreateError = UBSE_OK;
static uint32_t g_mockNumaCreateErrorOnce = UBSE_OK;
static uint32_t g_mockNumaDeleteError = UBSE_OK;
static uint32_t g_mockNumaDeleteErrorOnce = UBSE_OK;
static uint32_t g_mockNumaDeleteCallCount = 0;
static uint32_t g_mockDebtInfoWithNodeError = UBSE_OK;
static std::vector<UbseNodeNumaInfo> g_mockNodeNumaInfos;
static int64_t g_mockNumaCreateNumaId = 3;
static uint32_t g_mockNumaCreateExportSlot = 7;
static uint64_t g_mockNumaCreateSize = 0;
static uint64_t g_mockLastNumaCreateOptSize = 0;
static std::string g_mockLastNumaCreateName;
static std::string g_mockLastNumaDeleteName;

static std::mutex g_createMutex;
static std::condition_variable g_createCv;
static bool g_createBlocked = false;
static bool g_createEntered = false;
static std::condition_variable g_createEnteredCv;

static std::mutex g_deleteMutex;
static std::condition_variable g_deleteCv;
static bool g_deleteBlocked = false;
static bool g_deleteEntered = false;
static std::condition_variable g_deleteEnteredCv;

void MockBlockNumaCreate()
{
    std::lock_guard<std::mutex> lock(g_createMutex);
    g_createBlocked = true;
    g_createEntered = false;
}

void MockReleaseNumaCreate()
{
    std::lock_guard<std::mutex> lock(g_createMutex);
    g_createBlocked = false;
    g_createCv.notify_all();
}

bool MockWaitNumaCreateEntered(int timeoutMs)
{
    std::unique_lock<std::mutex> lock(g_createMutex);
    g_createEnteredCv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [] { return g_createEntered; });
    return g_createEntered;
}

void MockBlockNumaDelete()
{
    std::lock_guard<std::mutex> lock(g_deleteMutex);
    g_deleteBlocked = true;
    g_deleteEntered = false;
}

void MockReleaseNumaDelete()
{
    std::lock_guard<std::mutex> lock(g_deleteMutex);
    g_deleteBlocked = false;
    g_deleteCv.notify_all();
}

bool MockWaitNumaDeleteEntered(int timeoutMs)
{
    std::unique_lock<std::mutex> lock(g_deleteMutex);
    g_deleteEnteredCv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [] { return g_deleteEntered; });
    return g_deleteEntered;
}

void MockSetNumaCreateDesc(int64_t numaId, uint32_t exportSlotId, uint64_t size)
{
    g_mockNumaCreateNumaId = numaId;
    g_mockNumaCreateExportSlot = exportSlotId;
    g_mockNumaCreateSize = size;
}
std::string MockGetLastNumaCreateName()
{
    return g_mockLastNumaCreateName;
}
uint64_t MockGetLastNumaCreateSize()
{
    return g_mockLastNumaCreateOptSize;
}

void MockSetDebtInfos(const std::vector<UbseNumaMemoryDebtInfo>& infos)
{
    g_mockDebtInfos = infos;
}
void MockSetImportDebtInfos(const std::vector<UbseNumaMemoryImportDebtInfo>& infos)
{
    g_mockImportDebtInfos = infos;
}
void MockClearDebtInfos()
{
    g_mockDebtInfos.clear();
    g_mockImportDebtInfos.clear();
}
void MockSetNumaCreateError(uint32_t err)
{
    g_mockNumaCreateError = err;
}
void MockSetNumaCreateErrorOnce(uint32_t err)
{
    g_mockNumaCreateErrorOnce = err;
}
void MockSetNumaDeleteError(uint32_t err)
{
    g_mockNumaDeleteError = err;
}
void MockSetNumaDeleteErrorOnce(uint32_t err)
{
    g_mockNumaDeleteErrorOnce = err;
}
uint32_t MockGetNumaDeleteCallCount()
{
    return g_mockNumaDeleteCallCount;
}
void MockSetDebtInfoWithNodeError(uint32_t err)
{
    g_mockDebtInfoWithNodeError = err;
}
void MockSetNodeNumaInfos(const std::vector<UbseNodeNumaInfo>& infos)
{
    g_mockNodeNumaInfos = infos;
}
std::string MockGetLastNumaDeleteName()
{
    return g_mockLastNumaDeleteName;
}

void MockAdjustNodeFree(uint64_t deltaBytes)
{
    if (g_mockNodeNumaInfos.empty()) {
        return;
    }
    for (auto& info : g_mockNodeNumaInfos) {
        info.memFree += deltaBytes;
    }
}
void MockResetAllErrors()
{
    g_mockDebtInfos.clear();
    g_mockImportDebtInfos.clear();
    g_mockNodeNumaInfos.clear();
    g_mockLastNumaCreateName.clear();
    g_mockLastNumaDeleteName.clear();
    g_mockNumaCreateError = UBSE_OK;
    g_mockNumaCreateErrorOnce = UBSE_OK;
    g_mockNumaDeleteError = UBSE_OK;
    g_mockNumaDeleteErrorOnce = UBSE_OK;
    g_mockNumaDeleteCallCount = 0;
    g_mockDebtInfoWithNodeError = UBSE_OK;
    g_mockNumaCreateNumaId = 3;
    g_mockNumaCreateExportSlot = 7;
    g_mockNumaCreateSize = 0;
    g_mockLastNumaCreateOptSize = 0;
    MockReleaseNumaCreate();
    MockReleaseNumaDelete();
}

UbseResult UbseGetNumaMemDebtInfoWithNode(const std::string& nodeId, std::vector<UbseNumaMemoryDebtInfo>& debtInfos)
{
    if (g_mockDebtInfoWithNodeError != UBSE_OK) {
        return g_mockDebtInfoWithNodeError;
    }
    if (!g_mockDebtInfos.empty()) {
        debtInfos = g_mockDebtInfos;
    }
    return UBSE_OK;
}

UbseResult UbseMemNumaDelete(const std::string& name, const UbseMemBorrower& borrower)
{
    ++g_mockNumaDeleteCallCount;
    g_mockLastNumaDeleteName = name;
    if (g_mockNumaDeleteErrorOnce != UBSE_OK) {
        UbseResult onceErr = g_mockNumaDeleteErrorOnce;
        g_mockNumaDeleteErrorOnce = UBSE_OK;
        return onceErr;
    }
    {
        std::unique_lock<std::mutex> lock(g_deleteMutex);
        g_deleteEntered = true;
        g_deleteEnteredCv.notify_all();
        g_deleteCv.wait(lock, [] { return !g_deleteBlocked; });
    }
    return g_mockNumaDeleteError;
}

UbseResult UbseMemNumaCreate(const std::string& name, const UbseMemBorrower& borrower, const UbseMemNumaCreateOpt& opt,
                             UbseMemNumaDesc& desc)
{
    if (g_mockNumaCreateErrorOnce != UBSE_OK) {
        UbseResult onceErr = g_mockNumaCreateErrorOnce;
        g_mockNumaCreateErrorOnce = UBSE_OK;
        return onceErr;
    }
    if (g_mockNumaCreateError != UBSE_OK) {
        return g_mockNumaCreateError;
    }
    g_mockLastNumaCreateName = name;
    g_mockLastNumaCreateOptSize = opt.size;
    {
        std::unique_lock<std::mutex> lock(g_createMutex);
        g_createEntered = true;
        g_createEnteredCv.notify_all();
        g_createCv.wait(lock, [] { return !g_createBlocked; });
    }
    desc.name = name;
    desc.numaId = g_mockNumaCreateNumaId;
    desc.exportNode.slotId = g_mockNumaCreateExportSlot;
    desc.size = (g_mockNumaCreateSize != 0) ? g_mockNumaCreateSize : opt.size;
    memcpy(desc.usrInfo, opt.usrInfo, UBSE_MAX_USR_INFO_LEN);
    MockAdjustNodeFree(opt.size);
    return UBSE_OK;
}

UbseResult UbseMemNumaCreateWithCandidate(const std::string& name, const UbseMemBorrower& borrower,
                                          const UbseMemNumaCandidateOpt& opt, UbseMemNumaDesc& desc)
{
    return g_mockNumaCreateError;
}

UbseResult UbseGetNumaMemDebtInfo(std::vector<UbseNumaMemoryDebtInfo>& debtInfos)
{
    return UBSE_OK;
}

UbseResult UbseGetAllNodeNumaInfo(std::vector<UbseNodeNumaInfo>& numaNodeInfoList)
{
    return UBSE_OK;
}

uint32_t UbseQueryResult(const std::string& name, UbseMemResult& result, UbseMemBorrowType borrowType)
{
    return 0;
}

UbseResult UbseGetNumaMemImportDebtInfoWithLocalNode(std::vector<UbseNumaMemoryImportDebtInfo>& debtInfos)
{
    if (!g_mockImportDebtInfos.empty()) {
        debtInfos = g_mockImportDebtInfos;
        return UBSE_OK;
    }
    return UBSE_OK;
}

UbseResult UbseMemNumaCreateWithLender(const std::string& name, const UbseMemBorrower& borrower,
                                       const std::vector<UbseMemNumaLender>& lenders,
                                       uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN], UbseMemNumaDesc& desc)
{
    return UBSE_OK;
}

UbseResult UbseMemAddrCreate(const std::string& name, const UbseMemBorrower& borrower,
                             const UbseMemProcessLender& lender, uint32_t flag, UbseMemAddrDesc& desc)
{
    return UBSE_OK;
}

UbseResult UbseMemAddrDelete(const std::string& name, const UbseMemBorrower& borrower)
{
    return UBSE_OK;
}

UbseResult UbseMemDebtCircleCheck(const std::string& srcNodeId, const std::string& dstNodeId, bool& isCircle)
{
    isCircle = false;
    return UBSE_OK;
}

UbseResult UbseGetNodeNumaInfoByNodeId(const std::string& nodeId, std::vector<UbseNodeNumaInfo>& numaNodeInfoList)
{
    if (!g_mockNodeNumaInfos.empty()) {
        numaNodeInfoList = g_mockNodeNumaInfos;
    }
    return UBSE_OK;
}

UbseResult UbseGetAddrMemDebtInfoWithNode(const std::string& nodeId, std::vector<UbseMemAddrDesc>& debtInfos)
{
    return UBSE_OK;
}
} // namespace ubse::mem::controller
