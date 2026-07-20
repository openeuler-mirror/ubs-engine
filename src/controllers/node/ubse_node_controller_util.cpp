/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "ubse_node_controller_util.h"

#include <cstring>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>

#include <optional>
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "securec.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election_module.h"
#include "ubse_error.h"
#include "ubse_lcne_module.h"
#include "ubse_logger_module.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_constants.h"
#include "ubse_mem_controller_addr_api.h"
#include "ubse_net_util.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_collector.h"
#include "ubse_node_mgr.h"
#include "ubse_str_util.h"
namespace ubse::nodeController {
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::mem::strategy;
using namespace ubse::mti;

UBSE_DEFINE_THIS_MODULE("ubse");

std::mutex UbseNodeControllerLockMgr::nodeControllerMutex_;
std::unordered_map<std::string, std::shared_ptr<std::shared_mutex>> UbseNodeControllerLockMgr::nodeControllerLocks_{};

bool ubEnable = false;

std::shared_ptr<std::shared_mutex> UbseNodeControllerLockMgr::GetLock(const std::string &nodeId)
{
    std::lock_guard<std::mutex> mapLock(nodeControllerMutex_);
    auto it = nodeControllerLocks_.find(nodeId);
    if (it != nodeControllerLocks_.end()) {
        return it->second;
    }
    auto lock = std::make_shared<std::shared_mutex>();
    nodeControllerLocks_.emplace(nodeId, lock);
    return lock;
}

void UbseNodeControllerLockMgr::WriteLock(const std::string &nodeId)
{
    GetLock(nodeId)->lock();
}

void UbseNodeControllerLockMgr::WriteUnLock(const std::string &nodeId)
{
    GetLock(nodeId)->unlock();
}

void UbseNodeControllerLockMgr::TryWriteLock(const std::string &nodeId)
{
    GetLock(nodeId)->try_lock();
}

void UbseNodeControllerLockMgr::ReadLock(const std::string &nodeId)
{
    GetLock(nodeId)->lock_shared();
}

void UbseNodeControllerLockMgr::ReadUnLock(const std::string &nodeId)
{
    GetLock(nodeId)->unlock_shared();
}

bool UbseNodeControllerLockMgr::TryReadLock(const std::string &nodeId)
{
    return GetLock(nodeId)->try_lock_shared();
}

UbseAllocator GetAllocator()
{
    std::string val;
    auto ret = GetUbseConf("obmm", "mempool_allocator", val);
    if (ret != UBSE_OK) {
        return UbseAllocator::BUDDY_HIGHMEM;
    }
    if (val == "hugetlb_pmd") {
        return UbseAllocator::HUGETLB_PMD;
    }
    if (val == "hugetlb_pud") {
        return UbseAllocator::HUGETLB_PUD;
    }
    return UbseAllocator::BUDDY_HIGHMEM;
}

uint32_t GetGroupId()
{
    auto currentNode = ubse::nodeMgr::GetCurrentNode();
    if (currentNode.groupId != 0) {
        return currentNode.groupId;
    }

    uint32_t groupId = 0;
    auto ret = GetUbseConf("ubse.node", "group_id", groupId);
    if (ret == UBSE_OK) {
        return groupId;
    }

    ret = GetUbseConf("ubse.node", "group_id", groupId);
    if (ret == UBSE_OK) {
        return groupId;
    }

    std::string groupIdStr;
    ret = GetUbseConf("ubse.node", "group_id", groupIdStr);
    if (ret != UBSE_OK) {
        ret = GetUbseConf("ubse.node", "group_id", groupIdStr);
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "read group id from node mgr and config failed, use default groupId=0";
        return 0;
    }

    uint32_t parsed = 0;
    if (ubse::utils::ConvertStrToUint32(groupIdStr, parsed) != UBSE_OK) {
        UBSE_LOG_WARN << "parse group id config failed, value=" << groupIdStr << ", use default groupId=0";
        return 0;
    }

    return parsed;
}

static bool IsGlobalMasterRole(ubse::election::RoleType role)
{
    return role == ubse::election::RoleType::MASTER;
}

static bool IsGlobalMasterRole(ubse::election::GlobalRoleType role)
{
    return role == ubse::election::GlobalRoleType::GLOBAL_MASTER;
}

static bool IsNodeInGroup(const ubse::election::GroupTopology &group, const std::string &nodeId)
{
    if (group.groupMasterId == nodeId || group.groupStandbyId == nodeId) {
        return true;
    }

    for (const auto &node : group.groupNodes) {
        if (node == nodeId) {
            return true;
        }
    }

    return false;
}

static const ubse::election::GroupTopology *FindGroupByNodeId(const std::vector<ubse::election::GroupTopology> &groups,
                                                              const std::string &nodeId,
                                                              const ubse::election::GroupTopology *parentGroup,
                                                              const ubse::election::GroupTopology *&matchedParentGroup)
{
    for (const auto &group : groups) {
        if (IsNodeInGroup(group, nodeId)) {
            matchedParentGroup = parentGroup;
            return &group;
        }
    }

    return nullptr;
}

UbseResult GetClosHaTopology(ubse::election::HaTopologyInfo &topology)
{
    auto module = UbseContext::GetInstance().GetModule<ubse::election::UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_WARN << "election module not load";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    topology = {};
    auto ret = module->GetCurNodeGlobalTopoInfo(topology);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "get ha topology info failed, " << FormatRetCode(ret);
        return ret;
    }

    if (topology.currentNode.nodeId.empty()) {
        UBSE_LOG_WARN << "get ha topology info failed, current node id is empty";
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

UbseClosNodeRole DetectClosRole()
{
    ubse::election::HaTopologyInfo topology{};
    auto ret = GetClosHaTopology(topology);
    if (ret != UBSE_OK) {
        return UbseClosNodeRole::UNKNOWN;
    }

    if (IsGlobalMasterRole(topology.currentNode.globalRole)) {
        return UbseClosNodeRole::GLOBAL_MASTER;
    }

    if (topology.currentNode.groupRole != ubse::election::RoleType::MASTER) {
        return UbseClosNodeRole::CABINET_AGENT;
    }

    if (topology.currentGroup.groupId.empty()) {
        UBSE_LOG_WARN << "current group is empty, nodeId="
                      << topology.currentNode.nodeId;
        return UbseClosNodeRole::UNKNOWN;
    }

    if (topology.currentGroup.isManagingGroup) {
        return UbseClosNodeRole::PD_MASTER;
    }

    return UbseClosNodeRole::CABINET_MASTER;
}

static UbseResult GetLocalMasterNodeId(std::string &prevNodeId)
{
    auto module = UbseContext::GetInstance().GetModule<ubse::election::UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "election module not load";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    ubse::election::Node localMasterNode{};
    auto ret = module->GetLocalMasterNode(localMasterNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "get local master node failed, " << FormatRetCode(ret);
        return ret;
    }

    if (localMasterNode.id.empty()) {
        UBSE_LOG_WARN << "local master node id is empty";
        return UBSE_ERROR;
    }

    prevNodeId = localMasterNode.id;
    return UBSE_OK;
}

static UbseResult GetGlobalMasterNodeId(std::string &prevNodeId)
{
    auto module = UbseContext::GetInstance().GetModule<ubse::election::UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "election module not load";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    ubse::election::Node globalMasterNode{};
    auto ret = module->UbseGetMasterNode(globalMasterNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "get global master node failed, " << FormatRetCode(ret);
        return ret;
    }

    if (globalMasterNode.id.empty()) {
        UBSE_LOG_WARN << "global master node id is empty";
        return UBSE_ERROR;
    }

    prevNodeId = globalMasterNode.id;
    return UBSE_OK;
}

static UbseResult GetParentGroupMasterNodeId(std::string &prevNodeId)
{
    ubse::election::HaTopologyInfo topology{};
    auto ret = GetClosHaTopology(topology);
    if (ret != UBSE_OK) {
        return ret;
    }

    if (topology.currentGroup.isManagingGroup) {
        UBSE_LOG_WARN << "current group is managing group, no parent group, groupId="
                      << topology.currentGroup.groupId;
        return UBSE_ERROR;
    }

    const ubse::election::GroupTopology *parentGroup = nullptr;
    for (const auto &group : topology.groups) {
        if (!group.isManagingGroup || group.groupMasterId.empty()) {
            continue;
        }

        if (parentGroup != nullptr && parentGroup->groupId != group.groupId) {
            UBSE_LOG_WARN << "multiple parent managing groups found, currentGroupId="
                          << topology.currentGroup.groupId;
            return UBSE_ERROR;
        }

        parentGroup = &group;
    }

    if (parentGroup == nullptr) {
        UBSE_LOG_WARN << "parent managing group not found, currentGroupId="
                      << topology.currentGroup.groupId
                      << ", nodeId=" << topology.currentNode.nodeId;
        return UBSE_ERROR;
    }

    prevNodeId = parentGroup->groupMasterId;
    return UBSE_OK;
}

UbseResult GetPrevReportNodeId(std::string &prevNodeId)
{
    prevNodeId.clear();

    auto role = GetClosRole();
    if (role == UbseClosNodeRole::UNKNOWN) {
        UBSE_LOG_WARN << "clos role unknown";
        return UBSE_ERROR;
    }

    if (role == UbseClosNodeRole::GLOBAL_MASTER) {
        UBSE_LOG_INFO << "global master has no prev report node";
        return UBSE_ERROR;
    }

    if (role == UbseClosNodeRole::CABINET_AGENT) {
        return GetLocalMasterNodeId(prevNodeId);
    }

    if (role == UbseClosNodeRole::PD_MASTER) {
        return GetGlobalMasterNodeId(prevNodeId);
    }

    return GetParentGroupMasterNodeId(prevNodeId);
}

UbseClosNodeRole GetClosRole()
{
    return DetectClosRole();
}

bool IsCabinetMaster()
{
    return GetClosRole() == UbseClosNodeRole::CABINET_MASTER;
}

bool IsPdMaster()
{
    return GetClosRole() == UbseClosNodeRole::PD_MASTER;
}

bool IsGlobalMaster()
{
    return GetClosRole() == UbseClosNodeRole::GLOBAL_MASTER;
}

uint32_t GetPmdMapping()
{
    uint32_t val;
    auto ret = GetUbseConf("os", "pmd_mapping", val);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "read pmd mapping config failed, section=os, key=pmd_mapping";
        return MAX_PERCENT;
    }
    if (val > MAX_PERCENT) {
        UBSE_LOG_WARN << "read pmd mapping config invalid, section=os, key=pmd_mapping";
        return MAX_PERCENT;
    }
    return val;
}

static uint32_t blockSize = 0;

uint32_t GetBlockSize(UbseAllocator allocator)
{
    if (blockSize != 0) {
        return blockSize;
    }
    std::string value;
    auto ret = GetUbseConf("ubse.memory", "obmm.memory.block.size", value);
    if (ret == UBSE_OK && !value.empty()) {
        uint32_t parsed = 0;
        if (ubse::utils::ConvertStrToUint32(value, parsed) == UBSE_OK && parsed >= MB_4M && parsed <= MB_4096M &&
            parsed % ubse::common::def::NO_2 == 0) {
            blockSize = parsed;
            return blockSize;
        }
    }
    if (allocator == UbseAllocator::HUGETLB_PUD) {
        blockSize = BLOCK_1G;
        return blockSize;
    }
    std::string osPageSize;
    ret = GetUbseConf("os", "page_size", osPageSize);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get os page_size type failed, Use default value 4096";
        osPageSize = PAGE_SIZE_4K;
    }
    if (osPageSize == PAGE_SIZE_64K) {
        blockSize = BLOCK_512M;
    } else if (osPageSize == PAGE_SIZE_4K) {
        blockSize = BLOCK_128M;
    } else {
        UBSE_LOG_WARN << "Get os page_size type is invalid, Use default value 4096";
        blockSize = BLOCK_128M;
    }
    return blockSize;
}

void GetCurNodeInfo(UbseNodeInfo &info)
{
    adapter_plugins::mti::UbseMtiNodeInfo ubseNodeInfo{};
    auto ret = adapter_plugins::mti::UbseMtiInterface::GetInstance().GetLocalNodeInfo(ubseNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get local node info failed, " << FormatRetCode(ret);
        return;
    }
    info.nodeId = ubseNodeInfo.nodeId;
    auto cpyRet = strcpy_s(info.bondingEid, sizeof(info.bondingEid), ubseNodeInfo.eid.c_str());
    if (cpyRet != EOK) {
        UBSE_LOG_ERROR << "cpy eid failed, ErrorCode=" << cpyRet;
        return;
    }
    ret = ConvertStrToUint32(ubseNodeInfo.nodeId, info.slotId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Convert slotId failed, " << FormatRetCode(ret);
        return;
    }

    // 只在初始化时获取并存储guid
    if (info.guid.empty()) {
        auto lcneModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::mti::UbseLcneModule>();
        if (lcneModule != nullptr) {
            UbseLcneOSInfo lcneOsInfo = lcneModule->GetUbseLcneOSInfo();
            info.guid = lcneOsInfo.guid;
            if (info.guid.empty()) {
                UBSE_LOG_WARN << "GUID is empty from LcneOSInfo";
            }
        } else {
            UBSE_LOG_ERROR << "Failed to get UbseLcneModule instance";
            info.guid = "";
        }
    } else {
        UBSE_LOG_DEBUG << "Using cached GUID: " << info.guid << " for node: " << info.nodeId;
    }

    info.allocator = GetAllocator();
    // pud场景1G大页全用于内存借用，pmdMapping为100%
    info.pmdMapping = (info.allocator == UbseAllocator::HUGETLB_PUD) ? MAX_PERCENT : GetPmdMapping();
    info.blockSize = GetBlockSize(info.allocator);
    if (info.allocator == UbseAllocator::HUGETLB_PMD && info.pmdMapping != MAX_PERCENT) {
        UBSE_LOG_WARN << "hugetlb_pmd requires pmd mapping 100%, current=" << info.pmdMapping;
        return;
    }
    if (info.allocator == UbseAllocator::BUDDY_HIGHMEM && info.pmdMapping == MAX_PERCENT) {
        UBSE_LOG_WARN << "buddy_highmem not expected at 100%, continue if Ok";
    }
    info.groupId = GetGroupId();
}

uint32_t IpToUint32(const std::string &ipStr, uint32_t &ip)
{
    in_addr addr;
    if (inet_pton(AF_INET, ipStr.c_str(), &addr) <= 0) {
        UBSE_LOG_ERROR << "parse ip " << ipStr << " to uint32 failed.";
        return UBSE_ERROR;
    }
    ip = ntohl(addr.s_addr);
    return UBSE_OK;
}

std::string Uint32ToIp(uint32_t ip)
{
    in_addr addr;
    addr.s_addr = htonl(ip);
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, buf, sizeof(buf));
    return std::string(buf);
}

static void AppendIpRange(const std::string &startIpStr, const std::string &endIpStr,
                          std::vector<std::string> &result)
{
    uint32_t startIp = 0;
    uint32_t endIp = 0;

    UbseResult ret = IpToUint32(startIpStr, startIp);
    ret |= IpToUint32(endIpStr, endIp);
    if (ret != UBSE_OK || startIp == 0 || endIp < startIp) {
        return;
    }

    for (uint32_t ip = startIp;; ++ip) {
        result.push_back(Uint32ToIp(ip));
        if (ip == endIp) {
            break;
        }
    }
}

std::vector<std::string> ParseIpList(const std::string &ipList)
{
    std::vector<std::string> result;
    std::stringstream ss(ipList);
    std::string token;

    while (std::getline(ss, token, ',')) {
        size_t dashPos = token.find('-');
        if (dashPos == std::string::npos) {
            result.push_back(token);
            continue;
        }

        std::string startIpStr = token.substr(0, dashPos);
        std::string endIpStr = token.substr(dashPos + 1);
        AppendIpRange(startIpStr, endIpStr, result);
    }
    return result;
}

uint32_t FindSameNetMask(std::string ipStr, std::string &localIp)
{
    uint32_t rootIp = 0;
    auto ret = IpToUint32(ipStr, rootIp);
    if (ret != UBSE_OK) {
        return ret;
    }

    struct ifaddrs *ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == -1) {
        UBSE_LOG_ERROR << "get interface address failed";
        return UBSE_ERROR;
    }

    for (ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if (ifa->ifa_flags & IFF_LOOPBACK) {
            continue;
        }
        if (!(ifa->ifa_flags & IFF_UP)) {
            continue;
        }
        // 获取子网掩码
        if (!ifa->ifa_netmask) {
            continue;
        }

        struct sockaddr_in addr {};
        if (memcpy_s(&addr, sizeof(addr), ifa->ifa_addr, sizeof(addr)) != EOK) {
            UBSE_LOG_WARN << "copy interface address failed";
            continue;
        }

        struct sockaddr_in mask {};
        if (memcpy_s(&mask, sizeof(mask), ifa->ifa_netmask, sizeof(mask)) != EOK) {
            UBSE_LOG_WARN << "copy interface netmask failed";
            continue;
        }

        uint32_t ip = ntohl(addr.sin_addr.s_addr);
        uint32_t netmask = ntohl(mask.sin_addr.s_addr);
        uint32_t localNet = ip & netmask;
        uint32_t rootNet = rootIp & netmask;

        if (localNet == rootNet) {
            uint32_t hostBits = ~netmask;
            if ((ip & hostBits) != 0 && (ip & hostBits) != hostBits) {
                localIp = Uint32ToIp(ip);
                freeifaddrs(ifaddr);
                return UBSE_OK;
            }
        }
    }

    freeifaddrs(ifaddr);
    UBSE_LOG_ERROR << "current node and root node are not on the same network plane";
    return UBSE_ERROR;
}
} // namespace ubse::nodeController