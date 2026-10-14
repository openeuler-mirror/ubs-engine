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

#include "ssu_virt_it_mock_adapter.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <unordered_map>

#include "ubse_ssu_adapter_impl.h"
#include "ubse_ssu_utils.h"

namespace ubse::ssu::virt::it {

using namespace ubse::adapter_plugins::ssu::def;
using ubse::ssu::utils::StrToUuid;

namespace {
constexpr uint64_t TERABYTE = 1099511627776ULL;
constexpr uint32_t EID_STR_SIZE = 16;

std::mutex g_itDeviceNsMutex;
std::unordered_map<std::string, uint32_t> g_itDeviceNsCounters;
} // namespace

std::atomic<int> g_itAttachCallCount{0};
std::atomic<int> g_itDetachCallCount{0};
std::vector<UbseSsuDevNameSpace> g_itAcquireNsList;

namespace {
uint32_t GetNextNsIdForDevice(const DevAddrT &devAddr)
{
    std::string eid(reinterpret_cast<const char *>(devAddr.tgtEid.raw), EID_STR_SIZE);
    std::lock_guard<std::mutex> lock(g_itDeviceNsMutex);
    auto &counter = g_itDeviceNsCounters[eid];
    if (counter == 0) {
        counter = 1;
    }
    return counter++;
}

int ItCreateNamespace(const char *adminNqn, DevNamespaceInfoT *nsInfo)
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

int ItDeleteNamespace(const char *adminNqn, DevNamespaceInfoT *nsInfo)
{
    (void)adminNqn;
    (void)nsInfo;
    return 0;
}

int ItAttachNamespace(const char *hostNqn, DevNamespaceInfoT *nsInfo)
{
    (void)hostNqn;
    (void)nsInfo;
    g_itAttachCallCount.fetch_add(1);
    return 0;
}

int ItDetachNamespace(const char *hostNqn, DevNamespaceInfoT *nsInfo)
{
    (void)hostNqn;
    (void)nsInfo;
    g_itDetachCallCount.fetch_add(1);
    return 0;
}

int ItAddNamespaceAllowHost(const char *adminNqn, DevNamespaceInfoT *nsInfo, const char *hostNqn)
{
    (void)adminNqn;
    (void)nsInfo;
    (void)hostNqn;
    return 0;
}

int ItRemoveNamespaceAllowHost(const char *adminNqn, DevNamespaceInfoT *nsInfo, const char *hostNqn)
{
    (void)adminNqn;
    (void)nsInfo;
    (void)hostNqn;
    return 0;
}

int ItGetNamespaceAllowHosts(const char *adminNqn, DevNamespaceInfoT *nsInfo, char ***list, uint32_t *count)
{
    (void)adminNqn;
    (void)nsInfo;
    if (list != nullptr) {
        *list = nullptr;
    }
    if (count != nullptr) {
        *count = 0;
    }
    return 0;
}

void ItFreeAllowHostsMem(char **list, uint32_t count)
{
    (void)count;
    if (list == nullptr) {
        return;
    }
    for (uint32_t i = 0; i < count; ++i) {
        delete[] list[i];
    }
    delete[] list;
}

// 按 eid 填充设备信息与 namespace(与 UT ControllableAcquireDevInfo 同一约定)
int ItAcquireDevInfo(const char *adminNqn, const DevAddrT *devList, int devCnt, DevInfoT *devInfoList)
{
    (void)adminNqn;
    for (int i = 0; i < devCnt; ++i) {
        auto &dev = devInfoList[i];
        std::memset(&dev, 0, sizeof(dev));
        dev.state = DevStatusT::DEV_ONLINE;
        dev.tnvmcap = TERABYTE;
        dev.unvmcap = TERABYTE;
        dev.cntlId = 1;
        std::strncpy(dev.devPath, "/dev/nvme0", sizeof(dev.devPath) - 1);
        std::strncpy(dev.sn, "ITSN00001", sizeof(dev.sn) - 1);
        std::strncpy(dev.mn, "ItMockSSU", sizeof(dev.mn) - 1);
        std::memcpy(dev.devAddr.tgtEid.raw, devList[i].tgtEid.raw, EID_SIZE);
        std::strncpy(dev.devAddr.subNqn, devList[i].subNqn, SUBNQN_SIZE - 1);

        std::string eidStr(reinterpret_cast<const char *>(devList[i].tgtEid.raw), EID_STR_SIZE);
        uint32_t nsIdx = 0;
        for (const auto &ns : g_itAcquireNsList) {
            if (ns.subSystem.eid != eidStr || nsIdx >= MAX_NAMESPACES_PER_CTRL) {
                continue;
            }
            auto &devNs = dev.namespaces[nsIdx];
            std::memset(&devNs, 0, sizeof(devNs));
            devNs.namespaceId = ns.namespaceId;
            devNs.baseAttr.nsze = ns.nsze;
            devNs.baseAttr.ncap = ns.ncap;
            // ns.nuse 为 LBA 数量,DevNamespaceInfoT.usedBytes 为字节,需乘 LBA 大小换算
            // flbas=1 对应 4K,否则 512B
            uint64_t lbaSize = (ns.nsOptions.flbas == 1) ? 4096 : 512;
            devNs.usedBytes = ns.nuse * lbaSize;
            devNs.state = DevStatusT::DEV_ONLINE;
            std::memcpy(devNs.guid, ns.guid.data(), std::min(ns.guid.size(), sizeof(devNs.guid)));
            std::memcpy(devNs.uuid, ns.uuid.data(), std::min(ns.uuid.size(), sizeof(devNs.uuid)));
            std::memcpy(devNs.userData, &ns.customData, std::min(sizeof(ns.customData), sizeof(devNs.userData)));
            std::strncpy(devNs.devPath, ns.nsDevPath.c_str(), sizeof(devNs.devPath) - 1);
            nsIdx++;
        }
        dev.nsCount = nsIdx;
    }
    return 0;
}
} // namespace

void SetupItAdapterFuncs()
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    // 绕过 DlOpenLib 检查(占位句柄,Teardown 时释放)
    impl.dlManager_.handle_ = reinterpret_cast<void *>(0x1);
    impl.createNamespace_ = ItCreateNamespace;
    impl.deleteNamespace_ = ItDeleteNamespace;
    impl.attachNamespace_ = ItAttachNamespace;
    impl.detachNamespace_ = ItDetachNamespace;
    impl.addNamespaceAllowHost_ = ItAddNamespaceAllowHost;
    impl.removeNamespaceAllowHost_ = ItRemoveNamespaceAllowHost;
    impl.getNamespaceAllowHosts_ = ItGetNamespaceAllowHosts;
    impl.freeAllowHostsMem_ = ItFreeAllowHostsMem;
    impl.acquireDevInfo_ = ItAcquireDevInfo;
}

void ResetItAdapterMockState()
{
    g_itAttachCallCount.store(0);
    g_itDetachCallCount.store(0);
    g_itAcquireNsList.clear();
    std::lock_guard<std::mutex> lock(g_itDeviceNsMutex);
    g_itDeviceNsCounters.clear();
}

void ReleaseItAdapterDlHandle()
{
    UbseSsuAdapterImpl::GetInstance().dlManager_.handle_ = nullptr;
}

UbseSsuNameSpaceInfo RegisterAdapterNs(const std::string &eid, const std::string &subNqn, uint32_t nsId,
                                       const std::string &uuidByte, uid_t uid, const std::string &userName)
{
    // guid/uuid 均为 16 字节二进制;uuid 用 uuidByte 区分不同 namespace
    UbseSsuDevNameSpace ns{};
    ns.namespaceId = nsId;
    ns.subSystem.eid = eid;
    ns.subSystem.subNqn = subNqn;
    ns.subSystem.jettyId = 1;
    ns.guid = std::string(GUID_SIZE, '\xAB');
    ns.uuid = std::string(UUID_SIZE, '\xAB');
    // 用 uuidByte 逐字节覆盖,保证不同 namespace 的 uuid 互不相同
    for (uint32_t i = 0; i < UUID_SIZE; ++i) {
        ns.uuid[i] = uuidByte[i % uuidByte.size()];
    }
    ns.nsDevPath = "/dev/nvme0n" + std::to_string(nsId);
    ns.nsze = 4096; // 4096 LBAs
    ns.ncap = 4096;
    ns.nsOptions.flbas = 0;
    ns.nuse = 0;
    ns.customData.uid = uid;
    std::strncpy(ns.customData.userName, userName.c_str(), sizeof(ns.customData.userName) - 1);
    ns.customData.userName[sizeof(ns.customData.userName) - 1] = '\0';
    std::strncpy(ns.customData.defaultNqn, "nqn.default", sizeof(ns.customData.defaultNqn) - 1);
    ns.customData.defaultNqn[sizeof(ns.customData.defaultNqn) - 1] = '\0';

    g_itAcquireNsList.push_back(ns);

    // 构造与适配器可见 namespace 对齐的 UbseSsuNameSpaceInfo(nsUuid 与 ns.uuid 经 StrToUuid 转换后一致)
    UbseSsuNameSpaceInfo info;
    info.tgtEid = ns.subSystem.eid;
    info.tgtNqn = ns.subSystem.subNqn;
    info.namespaceId = ns.namespaceId;
    info.nsUuid = StrToUuid(ns.uuid);
    info.nsDevPath = ns.nsDevPath;
    info.nsSize = 4096 * 512; // 4096 LBA × 512B
    info.lbaFormat = ubse::plugin::service::ssu::UbseSsuLBAFormat::LBA_FORMAT_512;
    return info;
}

UbseSsuConnectInfo MakeItConnectInfo(const std::string &name, const std::string &tgtEid)
{
    UbseSsuConnectInfo ci{};
    ci.srcEid = "src_" + name;
    ci.tgtEid = tgtEid;
    ci.tgtNqn = "nqn.target." + name;
    ci.hostNqn = "nqn.host." + name;
    ci.nsUuid = "uuid-" + name;
    ci.nsId = 1;
    return ci;
}

} // namespace ubse::ssu::virt::it
