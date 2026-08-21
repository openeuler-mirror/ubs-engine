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

#ifndef TEST_UBSE_SSU_SERVICE_IMP_FIXTURE_H
#define TEST_UBSE_SSU_SERVICE_IMP_FIXTURE_H

#include <gtest/gtest.h>
#include <atomic>
#include <memory>
#include <mockcpp/mockcpp.hpp>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "debt/ubse_ssu_debt_ledger.h"
#include "framework/misc/ubse_future_mgr.h"
#include "ubse_com.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_ssu_adapter_impl.h"
#include "ubse_ssu_adapter_interface.h"
#include "ubse_ssu_def.h"
#include "ubse_ssu_service_imp.h"
#include "ubse_ssu_utils.h"
#include "message/ubse_ssu_attach_detach_verify_msg.h"
#include "message/ubse_ssu_status_update_msg.h"
#include "message/ubse_ssu_sync_resp_msg.h"

namespace ubse::ssu::service::ut {

using namespace ubse::adapter_plugins::ssu::def;
using namespace ubse::election;
using namespace ubse::ssu::debt;
using namespace ubse::ssu::utils;
using namespace ubse::misc::future;
using namespace ubse::com;
using namespace ubse::ssu::message;

class UbseSsuServiceImpTestBase : public testing::Test {
public:
    UbseSsuServiceImpTestBase() = default;

    void SetUp() override;
    void TearDown() override;

protected:
    UbseSsuServiceImp &service_ = UbseSsuServiceImp::GetInstance();

    // -----------------------------------------------------------------------
    // Role mock helpers
    // -----------------------------------------------------------------------
    static uint32_t MockGetRole_Master(std::string &role);
    static uint32_t MockGetRole_Agent(std::string &role);
    static uint32_t MockGetRole_Standby(std::string &role);
    static uint32_t MockGetRole_Unsupported(std::string &role);

    // -----------------------------------------------------------------------
    // Config mock helper (UbseGetStr)
    // -----------------------------------------------------------------------
    static uint32_t MockUbseGetStr(const std::string &section, const std::string &configKey,
                                   std::string &configVal);

    // -----------------------------------------------------------------------
    // Request builders
    // -----------------------------------------------------------------------
    static UbseSsuAllocSpaceReq MakeAllocReq(const std::string &name = "test_alloc", uint64_t nsSize = 4096,
                                             uint32_t nsNum = 2,
                                             UbseSsuLBAFormat lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512,
                                             UbseSsuAllocStrategy strategy = UbseSsuAllocStrategy::LINEAR,
                                             const std::string &tenant = "");
    static UbseSsuSpaceReq MakeSpaceReq(const std::string &name = "test_space", const std::string &nqn = "",
                                        const UbseSsuAllocIdentityInfo &identity = {});
    static UbseSsuAllocIdentityInfo MakeIdentity(const std::string &userName = "test_user", uid_t uid = 100);

    // -----------------------------------------------------------------------
    // Device builders
    // -----------------------------------------------------------------------
    static UbseSsuDevInfo MakeDev(const std::string &eid, const std::string &subNqn, uint64_t totalBytes = TERABYTE,
                                  uint64_t usedBytes = 0, uint32_t nsCount = 0,
                                  UbseSsuState state = UbseSsuState::ONLINE);

    // Populate collector cache (uses -fno-access-control to access private members)
    static void PopulateCollectorCache(const std::vector<UbseSsuDevInfo> &devices);

    // -----------------------------------------------------------------------
    // Ledger helpers
    // -----------------------------------------------------------------------
    static void PutLedgerEntry(const std::string &name, UbseSsuNsState state, const UbseSsuAllocResult &result = {});
    static bool LedgerEntryExists(const std::string &name);

    // ========================================================================
    // Collector cache helpers
    // ========================================================================

    // Create a UbseSsuDevNameSpace for the collector cache.
    // nsze is in bytes; it is converted to LBA count (÷512) internally.
    static UbseSsuDevNameSpace MakeNsForCache(const std::string &eid, const std::string &subNqn, uint32_t nsId,
                                              uint64_t nsze = 4096,
                                              const std::string &guid = std::string(GUID_SIZE, '\xAB'),
                                              const std::string &uuid = std::string(UUID_SIZE, '\xAB'),
                                              uid_t uid = 100,
                                              const std::string &userName = "test_user",
                                              const std::string &defaultNqn = "nqn.default");

    // Add a namespace to a specific device in the collector cache.
    // If the device (by eid) does not exist, creates a new device entry.
    static void AddNsToCollectorCache(const UbseSsuDevNameSpace &ns);

    // Build a UbseSsuNameSpaceInfo from a UbseSsuDevNameSpace (for ledger entries)
    static UbseSsuNameSpaceInfo MakeNameSpaceInfo(const UbseSsuDevNameSpace &ns, uint64_t nsSize = 4096);

    static constexpr uint64_t TERABYTE = 1099511627776ULL;
    static constexpr uint64_t GIGABYTE = 1073741824ULL;

private:
    static void CleanupLedger();
    static void ResetCollectorCache();
};

// ============================================================================
// Adapter Mock C Functions
// Shared across all SSU service test files.
// ============================================================================
extern std::atomic<uint32_t> g_mockNextNsId;

extern std::unordered_map<std::string, uint32_t> g_deviceNsCounters;
extern std::mutex g_deviceNsMutex;

uint32_t GetNextNsIdForDevice(const DevAddrT &devAddr);
void ResetDeviceNsCounters();

int MockCreateNamespace(const char *adminNqn, DevNamespaceInfoT *nsInfo);
int MockDeleteNamespace(const char *adminNqn, DevNamespaceInfoT *nsInfo);
int MockAttachNamespace(const char *hostNqn, DevNamespaceInfoT *nsInfo);
int MockDetachNamespace(const char *hostNqn, DevNamespaceInfoT *nsInfo);
int MockAddNamespaceAllowHost(const char *adminNqn, DevNamespaceInfoT *nsInfo, const char *hostNqn);
int MockRemoveNamespaceAllowHost(const char *adminNqn, DevNamespaceInfoT *nsInfo, const char *hostNqn);
int MockGetNamespaceAllowHosts(const char *adminNqn, DevNamespaceInfoT *nsInfo, char ***list, uint32_t *count);
void MockFreeAllowHostsMem(char **list, uint32_t count);
int MockAcquireDevInfo(const char *adminNqn, const DevAddrT *devList, int devCnt, DevInfoT *devInfoList);

void SetupAdapterFuncs();

// ============================================================================
// Controllable Attach/Detach mock state
// Tests can set g_attachFailAfter/g_detachFailAfter to inject failures.
// ============================================================================
extern std::atomic<int> g_attachFailAfter;
extern std::atomic<int> g_attachCallCount;
extern std::atomic<int> g_detachFailAfter;
extern std::atomic<int> g_detachCallCount;

void ResetControllableMockState();

// ============================================================================
// Controllable AcquireDevInfo mock state (for CacheMiss refresh tests)
// Tests can set g_acquireDevInfoFail=true to make acquireDevInfo_ fail,
// or populate g_acquireDevInfoNsList to return namespace data on refresh.
// ============================================================================
extern std::atomic<bool> g_acquireDevInfoFail;
extern std::vector<UbseSsuDevNameSpace> g_acquireDevInfoNsList;

int ControllableAcquireDevInfo(const char *adminNqn, const DevAddrT *devList, int devCnt, DevInfoT *devInfoList);
void ResetAcquireDevInfoMockState();

// ============================================================================
// Controllable GetNamespaceAllowHosts mock state (for FillAllowHostNqnList tests)
// Tests can set g_allowHostsGetFail=true to make getNamespaceAllowHosts_ fail,
// or populate g_allowHostsMap[nsId] to return a specific NQN list.
// ============================================================================
extern std::atomic<bool> g_allowHostsGetFail;
extern std::unordered_map<uint32_t, std::vector<std::string>> g_allowHostsMap;

int ControllableGetNamespaceAllowHosts(const char *adminNqn, DevNamespaceInfoT *nsInfo, char ***list,
                                       uint32_t *count);
void ResetAllowHostsMockState();

// ============================================================================
// RPC mock helpers for Agent path tests
// Shared across AttachDetach / Linear / Striped test files.
// ============================================================================
extern std::vector<UbseSsuNameSpaceInfo> g_mockNsList;
extern std::vector<UbseSsuNsVerifyInfo> g_mockNsVerifyList;
extern uint32_t g_mockVerifyErrorCode;
extern std::string g_mockVerifyDevName; // verify响应中的聚合块设备名（账本权威值）

// Mock callback for UbseRpcEndpoint::UbseRpcSend — member-function style.
uint32_t MockVerifyRpcSend(UbseRpcEndpoint *self, const std::string &targetNodeId,
                           const UbseRpcMessage &req, UbseRpcMessage &resp);

// Setup agent role + RPC success path (verify sends MockVerifyRpcSend response).
void SetupAgentRoleAndRpcSuccess(const std::vector<UbseSsuNameSpaceInfo> &nameSpaceList = {},
                                 const std::vector<UbseSsuNsVerifyInfo> &nsVerifyList = {},
                                 uint32_t verifyErrorCode = UBSE_OK);

// Setup agent role + RPC failure path (UbseRpcSend returns UBSE_ERROR).
void SetupAgentRoleAndRpcFail();

// ============================================================================
// Status Update (agent → master) mock tracking
// After each agent-path Attach/Detach/Linear/Striped call, these globals record
// whether and how SendStatusUpdate was called via the shared UbseRpcSend mock.
// Reset before use with ResetStatusUpdateMockState() or manually.
// ============================================================================
extern std::string g_lastStatusUpdateName;
extern UbseSsuNsState g_lastStatusUpdateState;
extern uint32_t g_statusUpdateCallCount;
extern uint32_t g_mockStatusUpdateResult;  // errorCode the mock returns to agent

void ResetStatusUpdateMockState();

// Build a UbseSsuNameSpaceInfo from raw fields (for agent mock RPC response).
UbseSsuNameSpaceInfo MakeNsAgentInfo(const std::string &eid, const std::string &nqn,
                                     uint32_t nsId, const std::string &devPath);

// Build a UbseSsuNsVerifyInfo from a guid (for agent mock RPC response).
UbseSsuNsVerifyInfo MakeNsVerifyInfo(const std::string &guid);

// Setup mock for CreateBlockDevice to return success (needed for Linear/Striped agent attach success tests).
// Uses UbseSsuAdapterImpl since MOCKER_CPP_VIRTUAL requires the concrete instance for vtable hooking.
void MockCreateBlockDeviceSuccess();

} // namespace ubse::ssu::service::ut

#endif // TEST_UBSE_SSU_SERVICE_IMP_FIXTURE_H
