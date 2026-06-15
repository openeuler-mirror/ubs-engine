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

uint32_t GetPodId()
{
    return 0;
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
    return false;
}

static const ubse::election::GroupTopology *FindGroupByNodeId(
    const std::vector<ubse::election::GroupTopology> &groups, const std::string &nodeId,
    const ubse::election::GroupTopology *parentGroup,
    const ubse::election::GroupTopology *&matchedParentGroup)
{
    (void)groups;
    (void)nodeId;
    (void)parentGroup;
    matchedParentGroup = nullptr;
    return nullptr;
}

UbseResult GetClosHaTopology(ubse::election::HaTopologyInfo &topology)
{
    topology = {};
    return UBSE_OK;
}

UbseClosNodeRole DetectClosRole()
{
    return UbseClosNodeRole::UNKNOWN;
}

static UbseResult GetLocalMasterNodeId(std::string &prevNodeId)
{
    prevNodeId.clear();
    return UBSE_OK;
}

static UbseResult GetGlobalMasterNodeId(std::string &prevNodeId)
{
    prevNodeId.clear();
    return UBSE_OK;
}

static UbseResult GetParentGroupMasterNodeId(std::string &prevNodeId)
{
    prevNodeId.clear();
    return UBSE_OK;
}

UbseResult GetPrevReportNodeId(std::string &prevNodeId)
{
    prevNodeId.clear();
    return UBSE_OK;
}

UbseClosNodeRole GetClosRole()
{
    return UbseClosNodeRole::UNKNOWN;
}

bool IsCabinetMaster()
{
    return false;
}

bool IsPdMaster()
{
    return false;
}

bool IsGlobalMaster()
{
    return false;
}

uint32_t GetPmdMapping()
{
    return MAX_PERCENT;
}

static uint32_t blockSize = 0;

uint32_t GetBlockSize(UbseAllocator allocator)
{
    (void)allocator;
    return blockSize;
}

void GetCurNodeInfo(UbseNodeInfo &info)
{
    (void)info;
}

uint32_t IpToUint32(const std::string &ipStr, uint32_t &ip)
{
    (void)ipStr;
    ip = 0;
    return UBSE_OK;
}

std::string Uint32ToIp(uint32_t ip)
{
    (void)ip;
    return {};
}

static void AppendIpRange(const std::string &startIpStr, const std::string &endIpStr,
                                           std::vector<std::string> &result)
{
    (void)startIpStr;
    (void)endIpStr;
    (void)result;
}

std::vector<std::string> ParseIpList(const std::string &ipList)
{
    (void)ipList;
    return {};
}

uint32_t FindSameNetMask(std::string ipStr, std::string &localIp)
{
    (void)ipStr;
    localIp.clear();
    return UBSE_OK;
}
} // namespace ubse::nodeController