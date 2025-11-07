/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "ubse_node_controller_util.h"
#include "ubse_error.h"
#include "ubse_context.h"
#include "ubse_logger_module.h"
#include "ubse_conf_module.h"
#include "ubse_net_util.h"
#include "ubse_str_util.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_collector.h"
#include "securec.h"
#include "ubse_str_util.h"

namespace ubse::nodeController {
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::log;
using namespace ubse::utils;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_NODE_CONTROLLER_MID)

std::mutex UbseNodeControllerLockMgr::nodeControllerMutex;
std::unordered_map<std::string, std::shared_ptr<std::shared_mutex>> UbseNodeControllerLockMgr::nodeControllerLocks{};

std::shared_ptr<std::shared_mutex> UbseNodeControllerLockMgr::GetLock(const std::string &nodeId)
{
    std::lock_guard<std::mutex> mapLock(nodeControllerMutex);
    auto it = nodeControllerLocks.find(nodeId);
    if (it != nodeControllerLocks.end()) {
        return it->second;
    }
    auto lock = std::make_shared<std::shared_mutex>();
    nodeControllerLocks.emplace(nodeId, lock);
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

void UbseNodeControllerLockMgr::TryReadLock(const std::string &nodeId)
{
    GetLock(nodeId)->try_lock_shared();
}

bool IsUBEnable()
{
    auto ubseConfModule = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (ubseConfModule == nullptr) {
        UBSE_LOG_ERROR << "Get config info failed";
        return true;
    }
    bool ubEnable = true;
    std::string ipList;
    auto ret = ubseConfModule->GetConf<std::string>("ubse.rpc", "cluster.ipList", ipList);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "Unable to get ub config, use default urma, " << FormatRetCode(ret);
        ubEnable = true;
    } else {
        ubEnable = false;
    }
    return ubEnable;
}

std::unordered_map<std::string, std::string> GetClusterIpListFromConf()
{
    std::unordered_map<std::string, std::string> ipMap;
    std::vector<std::string> ips;
    auto ubseConfModule = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (ubseConfModule == nullptr) {
        UBSE_LOG_ERROR << "Get config info failed";
        return ipMap;
    }
    std::string defaultVal;
    auto ret = ubseConfModule->GetConf<std::string>("ubse.rpc", "cluster.ipList", defaultVal);
    if (ret != UBSE_OK || defaultVal.empty()) {
        UBSE_LOG_WARN << "Unable to get cluster.ipList config," << FormatRetCode(ret) << " ,use default tcp";
        return ipMap;
    }
    std::vector<std::string> ipRangeVec;
    ubse::utils::Split(defaultVal, ",", ipRangeVec);
    for (auto& range : ipRangeVec) {
        if (range.find('-') != std::string::npos) {
            UbseNetUtil::ParseIpRangeToList(range, ips);
        } else if (UbseNetUtil::ValidIpv4Addr(range) || UbseNetUtil::ValidIpv6Addr(range)) {
            ips.emplace_back(range);
        } else {
            UBSE_LOG_WARN << "Invalid ip range:" << range;
        }
    }
    std::sort(ips.begin(), ips.end());
    for (int i = 0; i < ips.size(); ++i) {
        UBSE_LOG_INFO << "Put ip " << ips[i] << ", id " << i;
        ipMap.emplace(ips[i], std::to_string(i + 1));
    }
    return ipMap;
}

void GetCurNodeInfo(UbseNodeInfo& info)
{
    MtiNodeInfo ubseNodeInfo{};
    auto module = UbseContext::GetInstance().GetModule<UbseLcneModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "lcne module not init";
        return;
    }
    auto ret = module->UbseGetLocalNodeInfo(ubseNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get local node info failed, " << FormatRetCode(ret);
        return;
    }
    UBSE_LOG_INFO << "collect node id=" << ubseNodeInfo.nodeId;
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
    if (!IsUBEnable()) {
        ret = CollectIpList(info);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Get local ip list failed, " << FormatRetCode(ret);
            return;
        }
        auto ipMap = GetClusterIpListFromConf();
        for (auto ip : info.ipList) {
            std::string ipStr;
            if (ip.type == UbseIpType::UBSE_IP_V4) {
                ipStr = UbseNetUtil::Ipv4ArrToString(ip.ipv4.addr);
            } else {
                ipStr = UbseNetUtil::Ipv6ArrToString(ip.ipv6.addr);
            }
            UBSE_LOG_INFO << "Ip str is " << ipStr;
            auto it = ipMap.find(ipStr);
            if (it == ipMap.end()) {
                continue;
            }
            info.nodeId = it->second;
            info.comIp = it->first;
            return;
        }
        UBSE_LOG_ERROR << "Get local node id failed";
    }
}

std::vector<UbseNodeInfo> GetStaticNodeInfoFromConf()
{
    std::vector<UbseNodeInfo> nodeInfos{};
    if (!IsUBEnable()) {
        auto allIps = GetClusterIpListFromConf();
        if (allIps.empty()) {
            return nodeInfos;
        }
        for (auto& ip : allIps) {
            UbseNodeInfo ubseNodeInfo;
            ubseNodeInfo.nodeId = ip.second;
            ubseNodeInfo.comIp = ip.first;
            nodeInfos.push_back(ubseNodeInfo);
        }
        return nodeInfos;
    }
    return nodeInfos;
}
} // namespace ubse::nodeController