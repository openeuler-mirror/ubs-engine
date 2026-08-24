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

#include "test_ubse_ssu_service_imp_fixture.h"
#include <cstring>
#include "ubse_ssu_adapter_impl.h"

namespace ubse::ssu::service::ut {

using namespace ubse::adapter_plugins::ssu::def;

// ============================================================================
// Mock C function and variable definitions
// ============================================================================

std::atomic<uint32_t> g_mockNextNsId{1};

std::unordered_map<std::string, uint32_t> g_deviceNsCounters;
std::mutex g_deviceNsMutex;

uint32_t GetNextNsIdForDevice(const DevAddrT &devAddr)
{
    std::string eid(reinterpret_cast<const char *>(devAddr.tgtEid.raw), EID_SIZE);
    std::lock_guard<std::mutex> lock(g_deviceNsMutex);
    auto &counter = g_deviceNsCounters[eid];
    if (counter == 0) {
        counter = 1;
    }
    return counter++;
}

void ResetDeviceNsCounters()
{
    std::lock_guard<std::mutex> lock(g_deviceNsMutex);
    g_deviceNsCounters.clear();
}

int MockCreateNamespace(const char *adminNqn, DevNamespaceInfoT *nsInfo)
{
    (void)adminNqn;
    if (nsInfo == nullptr || nsInfo->baseAttr.nsze == 0 || nsInfo->baseAttr.ncap == 0) {
        return -1;
    }
    nsInfo->namespaceId = GetNextNsIdForDevice(nsInfo->devAddr);
    nsInfo->usedBytes = 0;
    nsInfo->state = DevStatusT::DEV_ONLINE;
    for (uint32_t i = 0; i < GUID_SIZE; ++i) {
        nsInfo->guid[i] = static_cast<unsigned char>(nsInfo->namespaceId + i);
    }
    for (uint32_t i = 0; i < UUID_SIZE; ++i) {
        nsInfo->uuid[i] = static_cast<unsigned char>(nsInfo->namespaceId + i + 0x80);
    }
    std::string path = "/dev/nvme0n" + std::to_string(nsInfo->namespaceId);
    std::strncpy(nsInfo->devPath, path.c_str(), DEV_PATH_SIZE - 1);
    nsInfo->devPath[DEV_PATH_SIZE - 1] = '\0';
    return 0;
}

int MockDeleteNamespace(const char *adminNqn, DevNamespaceInfoT *nsInfo)
{
    (void)adminNqn;
    (void)nsInfo;
    return 0;
}

std::atomic<int> g_attachFailAfter{-1};
std::atomic<int> g_attachCallCount{0};

int MockAttachNamespace(const char *hostNqn, DevNamespaceInfoT *nsInfo)
{
    (void)hostNqn;
    (void)nsInfo;
    int call = g_attachCallCount.fetch_add(1);
    int failAfter = g_attachFailAfter.load();
    return (failAfter >= 0 && call >= failAfter) ? -1 : 0;
}

std::atomic<int> g_detachFailAfter{-1};
std::atomic<int> g_detachCallCount{0};

int MockDetachNamespace(const char *hostNqn, DevNamespaceInfoT *nsInfo)
{
    (void)hostNqn;
    (void)nsInfo;
    int call = g_detachCallCount.fetch_add(1);
    int failAfter = g_detachFailAfter.load();
    return (failAfter >= 0 && call >= failAfter) ? -1 : 0;
}

void ResetControllableMockState()
{
    g_attachFailAfter.store(-1);
    g_attachCallCount.store(0);
    g_detachFailAfter.store(-1);
    g_detachCallCount.store(0);
}

// ============================================================================
// Controllable AcquireDevInfo mock (for CacheMiss refresh tests)
// ============================================================================

std::atomic<bool> g_acquireDevInfoFail{false};
std::vector<UbseSsuDevNameSpace> g_acquireDevInfoNsList;

void ResetAcquireDevInfoMockState()
{
    g_acquireDevInfoFail.store(false);
    g_acquireDevInfoNsList.clear();
}

int ControllableAcquireDevInfo(const char *adminNqn, const DevAddrT *devList, int devCnt, DevInfoT *devInfoList)
{
    (void)adminNqn;
    if (g_acquireDevInfoFail.load()) {
        return -1;
    }
    for (int i = 0; i < devCnt; ++i) {
        auto &dev = devInfoList[i];
        std::memset(&dev, 0, sizeof(dev));
        dev.state = DevStatusT::DEV_ONLINE;
        dev.tnvmcap = UbseSsuServiceImpTestBase::TERABYTE;
        dev.unvmcap = UbseSsuServiceImpTestBase::TERABYTE;
        dev.cntlId = 1;
        std::strncpy(dev.devPath, "/dev/nvme0", sizeof(dev.devPath) - 1);
        std::strncpy(dev.sn, "MOCKSN00001", sizeof(dev.sn) - 1);
        std::strncpy(dev.mn, "MockSSU", sizeof(dev.mn) - 1);
        std::memcpy(dev.devAddr.tgtEid.raw, devList[i].tgtEid.raw, EID_SIZE);
        std::strncpy(dev.devAddr.subNqn, devList[i].subNqn, SUBNQN_SIZE - 1);

        // Populate namespaces from g_acquireDevInfoNsList (matching by eid)
        std::string eidStr(reinterpret_cast<const char *>(devList[i].tgtEid.raw), EID_SIZE);
        uint32_t nsIdx = 0;
        for (const auto &ns : g_acquireDevInfoNsList) {
            if (ns.subSystem.eid != eidStr || nsIdx >= MAX_NAMESPACES_PER_CTRL) {
                continue;
            }
            auto &devNs = dev.namespaces[nsIdx];
            std::memset(&devNs, 0, sizeof(devNs));
            devNs.namespaceId = ns.namespaceId;
            devNs.baseAttr.nsze = ns.nsze;
            devNs.baseAttr.ncap = ns.ncap;
            devNs.usedBytes = ns.nuse;
            devNs.state = DevStatusT::DEV_ONLINE;
            std::memcpy(devNs.guid, ns.guid.data(), std::min(ns.guid.size(), sizeof(devNs.guid)));
            std::memcpy(devNs.uuid, ns.uuid.data(), std::min(ns.uuid.size(), sizeof(devNs.uuid)));
            std::memcpy(devNs.userData, &ns.customData,
                        std::min(sizeof(ns.customData), sizeof(devNs.userData)));
            std::strncpy(devNs.devPath, "/dev/nvme0n1", sizeof(devNs.devPath) - 1);
            nsIdx++;
        }
        dev.nsCount = nsIdx;
    }
    return 0;
}

int MockAddNamespaceAllowHost(const char *adminNqn, DevNamespaceInfoT *nsInfo, const char *hostNqn)
{
    (void)adminNqn;
    (void)nsInfo;
    (void)hostNqn;
    return 0;
}

int MockRemoveNamespaceAllowHost(const char *adminNqn, DevNamespaceInfoT *nsInfo, const char *hostNqn)
{
    (void)adminNqn;
    (void)nsInfo;
    (void)hostNqn;
    return 0;
}

int MockGetNamespaceAllowHosts(const char *adminNqn, DevNamespaceInfoT *nsInfo, char ***list, uint32_t *count)
{
    (void)adminNqn;
    (void)nsInfo;
    if (list) {
        *list = nullptr;
    }
    if (count) {
        *count = 0;
    }
    return 0;
}

void MockFreeAllowHostsMem(char **list, uint32_t count)
{
    (void)count;
    if (list == nullptr) {
        return;
    }
    for (uint32_t i = 0; i < count; ++i) {
        if (list[i] != nullptr) {
            delete[] list[i];
        }
    }
    delete[] list;
}

// ============================================================================
// Controllable GetNamespaceAllowHosts mock (for FillAllowHostNqnList tests)
// ============================================================================

std::atomic<bool> g_allowHostsGetFail{false};
std::unordered_map<uint32_t, std::vector<std::string>> g_allowHostsMap;

void ResetAllowHostsMockState()
{
    g_allowHostsGetFail.store(false);
    g_allowHostsMap.clear();
}

int ControllableGetNamespaceAllowHosts(const char *adminNqn, DevNamespaceInfoT *nsInfo, char ***list,
                                       uint32_t *count)
{
    (void)adminNqn;
    if (g_allowHostsGetFail.load()) {
        return -1;
    }
    if (list == nullptr || count == nullptr || nsInfo == nullptr) {
        return -1;
    }
    *count = 0;
    *list = nullptr;
    auto it = g_allowHostsMap.find(nsInfo->namespaceId);
    if (it == g_allowHostsMap.end() || it->second.empty()) {
        return 0;
    }
    *list = new char *[it->second.size()];
    uint32_t idx = 0;
    for (const auto &nqn : it->second) {
        (*list)[idx] = new char[nqn.size() + 1];
        std::strncpy((*list)[idx], nqn.c_str(), nqn.size());
        (*list)[idx][nqn.size()] = '\0';
        ++idx;
    }
    *count = static_cast<uint32_t>(it->second.size());
    return 0;
}

int MockAcquireDevInfo(const char *adminNqn, const DevAddrT *devList, int devCnt, DevInfoT *devInfoList)
{
    (void)adminNqn;
    for (int i = 0; i < devCnt; ++i) {
        auto &dev = devInfoList[i];
        std::memset(&dev, 0, sizeof(dev));
        dev.state = DevStatusT::DEV_ONLINE;
        dev.nsCount = 0;
        dev.tnvmcap = 1099511627776ULL;
        dev.unvmcap = 1099511627776ULL;
        dev.cntlId = 1;
        std::strncpy(dev.devPath, "/dev/nvme0", sizeof(dev.devPath) - 1);
        std::strncpy(dev.sn, "MOCKSN00001", sizeof(dev.sn) - 1);
        std::strncpy(dev.mn, "MockSSU", sizeof(dev.mn) - 1);
        std::memcpy(dev.devAddr.tgtEid.raw, devList[i].tgtEid.raw, EID_SIZE);
        std::strncpy(dev.devAddr.subNqn, devList[i].subNqn, SUBNQN_SIZE - 1);
    }
    return 0;
}

void SetupAdapterFuncs()
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    // bypass DlOpenLib check
    impl.dlManager_.handle_ = reinterpret_cast<void *>(0x1);
    impl.createNamespace_ = MockCreateNamespace;
    impl.deleteNamespace_ = MockDeleteNamespace;
    impl.attachNamespace_ = MockAttachNamespace;
    impl.detachNamespace_ = MockDetachNamespace;
    impl.addNamespaceAllowHost_ = MockAddNamespaceAllowHost;
    impl.removeNamespaceAllowHost_ = MockRemoveNamespaceAllowHost;
    impl.getNamespaceAllowHosts_ = MockGetNamespaceAllowHosts;
    impl.freeAllowHostsMem_ = MockFreeAllowHostsMem;
    impl.acquireDevInfo_ = MockAcquireDevInfo;
}

// ============================================================================
// UbseSsuServiceImpTestBase implementation
// ============================================================================

void UbseSsuServiceImpTestBase::SetUp()
{
    CleanupLedger();
    ResetCollectorCache();
    ResetControllableMockState();
    ResetAcquireDevInfoMockState();
    ResetStatusUpdateMockState();

    // Default: UbseGetRole returns Master, other election APIs succeed
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Master));
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
}

void UbseSsuServiceImpTestBase::TearDown()
{
    GlobalMockObject::verify();
    CleanupLedger();
    ResetCollectorCache();
}

// -----------------------------------------------------------------------
// Role mock helpers
// -----------------------------------------------------------------------

uint32_t UbseSsuServiceImpTestBase::MockGetRole_Master(std::string &role)
{
    role = ELECTION_ROLE_MASTER;
    return UBSE_OK;
}

uint32_t UbseSsuServiceImpTestBase::MockGetRole_Agent(std::string &role)
{
    role = ELECTION_ROLE_AGENT;
    return UBSE_OK;
}

uint32_t UbseSsuServiceImpTestBase::MockGetRole_Standby(std::string &role)
{
    role = ELECTION_ROLE_STANDBY;
    return UBSE_OK;
}

uint32_t UbseSsuServiceImpTestBase::MockGetRole_Unsupported(std::string &role)
{
    role = "unknown_role";
    return UBSE_OK;
}

// -----------------------------------------------------------------------
// Config mock helper
// -----------------------------------------------------------------------

uint32_t UbseSsuServiceImpTestBase::MockUbseGetStr(const std::string &section, const std::string &configKey,
                                                   std::string &configVal)
{
    (void)section;
    (void)configKey;
    configVal = "nqn.2024-01.org.nvmexpress:uuid:mck-admin-nqn";
    return UBSE_OK;
}

// -----------------------------------------------------------------------
// Request builders
// -----------------------------------------------------------------------

UbseSsuAllocSpaceReq UbseSsuServiceImpTestBase::MakeAllocReq(const std::string &name, uint64_t nsSize, uint32_t nsNum,
                                                             UbseSsuLBAFormat lbaFormat, UbseSsuAllocStrategy strategy,
                                                             const std::string &tenant)
{
    UbseSsuAllocSpaceReq req;
    req.name = name;
    req.nsSize = nsSize;
    req.nsNum = nsNum;
    req.lbaFormat = lbaFormat;
    req.strategy = strategy;
    req.tenant = tenant;
    return req;
}

UbseSsuSpaceReq UbseSsuServiceImpTestBase::MakeSpaceReq(const std::string &name, const std::string &nqn,
                                                        const UbseSsuAllocIdentityInfo &identity)
{
    UbseSsuSpaceReq req;
    req.name = name;
    req.nqn = nqn;
    req.identity = identity;
    return req;
}

UbseSsuAllocIdentityInfo UbseSsuServiceImpTestBase::MakeIdentity(const std::string &userName, uid_t uid)
{
    UbseSsuAllocIdentityInfo identity;
    identity.userName = userName;
    identity.uid = uid;
    return identity;
}

// -----------------------------------------------------------------------
// Device builders
// -----------------------------------------------------------------------

UbseSsuDevInfo UbseSsuServiceImpTestBase::MakeDev(const std::string &eid, const std::string &subNqn,
                                                  uint64_t totalBytes, uint64_t usedBytes, uint32_t nsCount,
                                                  UbseSsuState state)
{
    UbseSsuDevInfo dev;
    dev.subSystem.eid = eid;
    dev.subSystem.subNqn = subNqn;
    dev.totalBytes = totalBytes;
    dev.usedBytes = usedBytes;
    dev.state = state;
    for (uint32_t i = 0; i < nsCount; ++i) {
        UbseSsuDevNameSpace ns;
        ns.namespaceId = i + 1;
        ns.subSystem = dev.subSystem;
        dev.nameSpaces.push_back(ns);
    }
    return dev;
}

void UbseSsuServiceImpTestBase::PopulateCollectorCache(const std::vector<UbseSsuDevInfo> &devices)
{
    auto &svc = UbseSsuServiceImp::GetInstance();
    svc.collector_.cachedDevMap_.clear();
    for (const auto &dev : devices) {
        svc.collector_.cachedDevMap_[dev.subSystem.eid] = std::make_shared<const UbseSsuDevInfo>(dev);
    }
}

// -----------------------------------------------------------------------
// Ledger helpers
// -----------------------------------------------------------------------

void UbseSsuServiceImpTestBase::PutLedgerEntry(const std::string &name, UbseSsuNsState state,
                                               const UbseSsuAllocResult &result)
{
    auto entry = std::make_shared<UbseSsuLedgerEntry>();
    entry->name = name;
    entry->state = state;
    entry->allocResult = result;
    UbseSsuDebtLedger::GetInstance().Put(name, entry);
}

bool UbseSsuServiceImpTestBase::LedgerEntryExists(const std::string &name)
{
    return UbseSsuDebtLedger::GetInstance().Get(name) != nullptr;
}

// ========================================================================
// Collector cache helpers
// ========================================================================

// Create a UbseSsuDevNameSpace for the collector cache
UbseSsuDevNameSpace UbseSsuServiceImpTestBase::MakeNsForCache(const std::string &eid, const std::string &subNqn,
                                                              uint32_t nsId, uint64_t nsze, const std::string &guid,
                                                              const std::string &uuid, uid_t uid,
                                                              const std::string &userName,
                                                              const std::string &defaultNqn)
{
    UbseSsuDevNameSpace ns;
    ns.namespaceId = nsId;
    ns.subSystem.eid = eid;
    ns.subSystem.subNqn = subNqn;
    ns.guid = guid;
    ns.uuid = uuid;
    ns.nsDevPath = "/dev/nvme0n" + std::to_string(nsId);
    ns.nsze = nsze / 512;
    ns.ncap = nsze / 512;
    ns.nsOptions.flbas = 0;
    ns.customData.uid = uid;
    std::strncpy(ns.customData.userName, userName.c_str(), sizeof(ns.customData.userName) - 1);
    std::strncpy(ns.customData.defaultNqn, defaultNqn.c_str(), sizeof(ns.customData.defaultNqn) - 1);
    ns.subSystem.jettyId = 1;
    return ns;
}

// Add a namespace to a specific device in the collector cache.
// If the device (by eid) does not exist, creates a new device entry.
void UbseSsuServiceImpTestBase::AddNsToCollectorCache(const UbseSsuDevNameSpace &ns)
{
    auto &svc = UbseSsuServiceImp::GetInstance();
    // cachedDevMap_ is std::unordered_map<std::string, std::shared_ptr<const UbseSsuDevInfo>>
    // We need a mutable copy to add the namespace
    auto it = svc.collector_.cachedDevMap_.find(ns.subSystem.eid);
    if (it == svc.collector_.cachedDevMap_.end()) {
        auto dev = std::make_shared<UbseSsuDevInfo>();
        dev->subSystem = ns.subSystem;
        dev->totalBytes = TERABYTE;
        dev->state = UbseSsuState::ONLINE;
        dev->nameSpaces.push_back(ns);
        svc.collector_.cachedDevMap_[ns.subSystem.eid] = std::const_pointer_cast<const UbseSsuDevInfo>(dev);
    } else {
        // Cannot modify const UbseSsuDevInfo through shared_ptr<const>, need to recreate
        auto mutableDev = std::make_shared<UbseSsuDevInfo>(*it->second);
        mutableDev->nameSpaces.push_back(ns);
        svc.collector_.cachedDevMap_[ns.subSystem.eid] = std::const_pointer_cast<const UbseSsuDevInfo>(mutableDev);
    }
}

// Build a UbseSsuNameSpaceInfo from a UbseSsuDevNameSpace (for ledger entries)
UbseSsuNameSpaceInfo UbseSsuServiceImpTestBase::MakeNameSpaceInfo(const UbseSsuDevNameSpace &ns, uint64_t nsSize)
{
    UbseSsuNameSpaceInfo info;
    info.tgtEid = ns.subSystem.eid;
    info.tgtNqn = ns.subSystem.subNqn;
    info.namespaceId = ns.namespaceId;
    info.nsUuid = ns.uuid;
    info.nsDevPath = ns.nsDevPath;
    info.nsSize = nsSize;
    info.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;
    return info;
}

// -----------------------------------------------------------------------
// Private helpers
// -----------------------------------------------------------------------

void UbseSsuServiceImpTestBase::CleanupLedger()
{
    auto &ledger = UbseSsuDebtLedger::GetInstance();
    auto entries = ledger.GetAll();
    for (const auto &entry : entries) {
        ledger.Remove(entry->name);
    }
}

void UbseSsuServiceImpTestBase::ResetCollectorCache()
{
    auto &svc = UbseSsuServiceImp::GetInstance();
    svc.collector_.cachedDevMap_.clear();
}

// ============================================================================
// RPC mock helpers for Agent path tests
// ============================================================================

std::vector<UbseSsuNameSpaceInfo> g_mockNsList;
std::vector<UbseSsuNsVerifyInfo> g_mockNsVerifyList;
uint32_t g_mockVerifyErrorCode{UBSE_OK};
std::string g_mockVerifyDevName;

// Status update tracking globals
std::string g_lastStatusUpdateName;
UbseSsuNsState g_lastStatusUpdateState{UbseSsuNsState::IDLE};
uint32_t g_statusUpdateCallCount{0};
uint32_t g_mockStatusUpdateResult{UBSE_OK};

void ResetStatusUpdateMockState()
{
    g_lastStatusUpdateName.clear();
    g_lastStatusUpdateState = UbseSsuNsState::IDLE;
    g_statusUpdateCallCount = 0;
    g_mockStatusUpdateResult = UBSE_OK;
    g_mockVerifyDevName.clear();
}

uint32_t MockVerifyRpcSend(UbseRpcEndpoint *self, const std::string &targetNodeId,
                           const UbseRpcMessage &req, UbseRpcMessage &resp)
{
    (void)self;
    (void)targetNodeId;

    // Dispatch by actual message type via RTTI.
    // Verify requests (VerifyAttachDetachIdentityViaRpc) use UbseSsuAttachDetachVerifyReqMsg,
    // while status update requests (SendStatusUpdate) use UbseSsuStatusReqMsg.
    const auto *verifyReq = dynamic_cast<const UbseSsuAttachDetachVerifyReqMsg *>(&req);
    if (verifyReq != nullptr) {
        // Build verify response (same as before)
        UbseSsuAttachDetachVerifyResp respData;
        respData.errorCode = g_mockVerifyErrorCode;
        respData.nameSpaceList = g_mockNsList;
        respData.nsVerifyList = g_mockNsVerifyList;
        respData.devName = g_mockVerifyDevName;
        UbseSsuAttachDetachVerifyRespMsg populatedMsg(respData);
        std::unique_ptr<uint8_t[]> buffer;
        uint32_t bufferSize = 0;
        if (populatedMsg.Serialize(buffer, bufferSize) != UBSE_OK || buffer == nullptr) {
            return UBSE_ERROR;
        }
        return resp.Deserialize(buffer.get(), bufferSize);
    }

    // Otherwise, treat as status update request
    const auto *statusReq = dynamic_cast<const UbseSsuStatusReqMsg *>(&req);
    if (statusReq != nullptr) {
        const auto &statusUpdate = statusReq->GetStatusUpdateReq();
        g_lastStatusUpdateName = statusUpdate.requestName;
        g_lastStatusUpdateState = statusUpdate.state;
        g_statusUpdateCallCount++;
        UbseSsuSyncRespMsg rspMsg(g_mockStatusUpdateResult);
        std::unique_ptr<uint8_t[]> buffer;
        uint32_t bufferSize = 0;
        if (rspMsg.Serialize(buffer, bufferSize) != UBSE_OK) {
            return UBSE_ERROR;
        }
        return resp.Deserialize(buffer.get(), bufferSize);
    }

    // Unknown message type
    return UBSE_ERROR;
}

void SetupAgentRoleAndRpcSuccess(
    const std::vector<UbseSsuNameSpaceInfo> &nameSpaceList,
    const std::vector<UbseSsuNsVerifyInfo> &nsVerifyList,
    uint32_t verifyErrorCode)
{
    g_mockNsList = nameSpaceList;
    g_mockNsVerifyList = nsVerifyList;
    g_mockVerifyErrorCode = verifyErrorCode;

    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(UbseSsuServiceImpTestBase::MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseGetCurrentNodeInfo).reset();
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseRpcEndpointFactory::GetRpcEndpoint).reset();
    MOCKER(&UbseRpcEndpointFactory::GetRpcEndpoint).stubs().will(returnValue(std::make_shared<UbseRpcEndpoint>()));
    MOCKER_CPP(&UbseRpcEndpoint::UbseRpcSend).reset();
    MOCKER_CPP(&UbseRpcEndpoint::UbseRpcSend).stubs().will(invoke(MockVerifyRpcSend));
}

void SetupAgentRoleAndRpcFail()
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(UbseSsuServiceImpTestBase::MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseGetCurrentNodeInfo).reset();
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseRpcEndpointFactory::GetRpcEndpoint).reset();
    MOCKER(&UbseRpcEndpointFactory::GetRpcEndpoint).stubs().will(returnValue(std::make_shared<UbseRpcEndpoint>()));
    MOCKER_CPP(&UbseRpcEndpoint::UbseRpcSend).reset();
    MOCKER_CPP(&UbseRpcEndpoint::UbseRpcSend).stubs().will(returnValue(UBSE_ERROR));
}

UbseSsuNameSpaceInfo MakeNsAgentInfo(const std::string &eid, const std::string &nqn,
                                     uint32_t nsId, const std::string &devPath)
{
    UbseSsuNameSpaceInfo info;
    info.tgtEid = eid;
    info.tgtNqn = nqn;
    info.namespaceId = nsId;
    info.nsDevPath = devPath;
    info.nsSize = 4096;
    info.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;
    return info;
}

UbseSsuNsVerifyInfo MakeNsVerifyInfo(const std::string &guid)
{
    UbseSsuNsVerifyInfo info;
    info.guid = guid;
    info.jettyId = 1;
    info.defaultNqn = "nqn.default";
    return info;
}

void MockCreateBlockDeviceSuccess()
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::CreateBlockDevice)
        .stubs()
        .will(returnValue(UBSE_OK));
}

} // namespace ubse::ssu::service::ut
