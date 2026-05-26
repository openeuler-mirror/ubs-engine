/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_lcne_module.h"
#include <dlfcn.h>
#include <cstdint>
#include <iostream>
#include <shared_mutex>
#include <unordered_set>
#include "securec.h"

#include "ubse_event_module.h"
#include "ubse_http_module.h"
#include "ubse_thread_pool_module.h"
#include "src/adapter_plugins/mti/lcne/ubse_lcne_busInstance.h"
#include "src/adapter_plugins/mti/lcne/ubse_lcne_host_info.h"
#include "src/adapter_plugins/mti/lcne/ubse_lcne_node_info.h"
#include "src/adapter_plugins/mti/lcne/ubse_lcne_urma_eid.h"
#include "src/adapter_plugins/mti/lcne/ubse_lcne_fe_eid.h"
#include "ubse_conf.h"
#include "ubse_conf_module.h"
#include "ubse_logger_module.h"
#include "ubse_smbios.h"
#include "ubse_str_util.h"
#include "ubse_mti_eid_util.h"
#include "ubse_net_util.h"
#include "adapter_plugins/mti/ubse_smbios.h"

namespace ubse::mti {
using namespace ubse::module;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::task_executor;
using namespace ubse::event;
using namespace ubse::http;
using namespace ubse::lcne;
using namespace ubse::utils;
using namespace ubse::log;
using namespace adapter_plugins::mti;
using namespace ubse::adapter_plugins::smbios;
BASE_DYNAMIC_CREATE(UbseLcneModule, UbseTaskExecutorModule, UbseEventModule, UbseHttpModule);
UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseLcneModule::GetLcneConf()
{
    // 读取lcne监听端口
    auto module = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "Failed to get config module";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    // 真实lcne端口
    std::string realLcnePortStr{realUbfmDefaultPort}; // 默认为34256
    auto ret = module->GetConf<std::string>("ubse.ubfm", "ubm.server.port", realLcnePortStr);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to get the configuration for lcne.port. Uds protocol will be used.";
        LcneServer::isTcpServer = false;
        return UBSE_OK;
    }
    UBSE_LOG_DEBUG << "The value of the lcne.port field is " << realLcnePortStr;
    LcneServer::isTcpServer = true;
    if (ConvertPortConfStrToInt(realLcnePortStr, LcneServer::realPort) != UBSE_OK) {
        LcneServer::realPort = DEFAULT_UBM_SERVER_PORT;
        UBSE_LOG_WARN << "The default value for port will be used.";
    };
    return UBSE_OK;
}

UbseResult UbseLcneModule::ConvertPortConfStrToInt(const std::string &portStr, int &port)
{
    if (ConvertStrToInt(portStr, port) != UBSE_OK) {
        UBSE_LOG_ERROR << "The value of portStr " << portStr << " is can not convert to int.";
        return UBSE_ERROR;
    };
    if (port < 1024 || port > 65535) { // 配置校验，要求限定范围在[1024.65535]
        UBSE_LOG_ERROR << "The value of port " << port << " is out of range.";
        return UBSE_ERROR;
    }
    UBSE_LOG_DEBUG << "Now the port used by the ubse is " << port;
    return UBSE_OK;
}

void UbseLcneModule::UpdateClusterIpListAndLocalIp()
{
    auto ubseConfModule = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (ubseConfModule == nullptr) {
        UBSE_LOG_ERROR << "Get config info failed";
        return;
    }
    std::string defaultVal;
    auto ret = ubseConfModule->GetConf<std::string>("ubse.rpc", "cluster.ipList", defaultVal);
    if (ret != UBSE_OK || defaultVal.empty()) {
        UBSE_LOG_WARN << "Unable to get cluster.ipList config, " << FormatRetCode(ret) << ", use default tcp.";
        return;
    }
    std::vector<std::string> ipRangeVec;
    std::vector<std::string> ips;
    Split(defaultVal, ",", ipRangeVec);
    for (auto &range : ipRangeVec) {
        if (range.find('-') != std::string::npos) {
            UbseNetUtil::ParseIpRangeToList(range, ips);
        } else if (UbseNetUtil::ValidIpv4Addr(range) || UbseNetUtil::ValidIpv6Addr(range)) {
            ips.emplace_back(range);
        } else {
            UBSE_LOG_WARN << "Invalid ip range:" << range;
        }
    }
    std::sort(ips.begin(), ips.end());
    std::vector<std::string> localIps{};
    ret = UbseNetUtil::GetIpInfo(localIps);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Collect ip list failed, " << FormatRetCode(ret);
        return;
    }
    for (auto &ip : localIps) {
        if (auto it = std::find(ips.begin(), ips.end(), ip); it != ips.end()) {
            localIp = *it;
        }
    }
    if (localIp.empty()) {
        UBSE_LOG_ERROR << "Get local ip failed.";
        return;
    }
    clusterIpList = ips;
}

UbseResult UbseLcneModule::Initialize()
{
    // Init阶段，向lcne设备模块获取信息
    auto ret = GetLcneConf();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Failed to get mock lcne port.";
        return UBSE_ERROR;
    }
    std::unique_lock<std::shared_mutex> lock(rw_mutex);
    while (!g_globalStop.load()) {
        if (GetLcneData() == UBSE_OK) {
            UBSE_LOG_INFO << "[MTI] get lcne device info succeed.";
            break;
        };
        // 等待3秒循环访问
        if (g_globalCv.wait_for(lock, std::chrono::seconds(3), [] { return g_globalStop.load(); })) {
            std::cout << "Stopped in the process of continuously obtaining lcne device information." << std::endl;
            break;
        }
    }
    // 填充通信信息
    ret = FillNodeComInfo();
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "[MTI] Failed to generate bonding eid.";
        return UBSE_ERROR;
    }
    UpdateClusterIpListAndLocalIp();
    return UBSE_OK;
}
UbseResult UbseLcneModule::GetLcneData()
{
    auto topoChangeRet = ubseLcneTopology.RegHttpHandler();
    auto topoRet = ubseLcneTopology.Start();
    auto busInstanceRet = UbseLcneBusInstance::GetInstance().QueryBusinstance(ubseLcneBusInstanceInfo);
    auto ioDieInfoRet = UbseLcneNodeInfo::GetGetInstance().QueryAllLcneIODieInfo(localBoardIOInfo);
    auto hostInfoRet = UbseLcneHostInfo::GetGetInstance().QueryLcneHostInfo(localBoardHostInfo);
    if (topoRet == UBSE_OK && topoChangeRet == UBSE_OK && busInstanceRet == UBSE_OK &&
        ioDieInfoRet == UBSE_OK && hostInfoRet == UBSE_OK) {
        return GetComUrmaEid();
    }
    return UBSE_ERROR;
}

void UbseLcneModule::UnInitialize() {}

UbseResult UbseLcneModule::Start()
{
    return UBSE_OK;
}

void UbseLcneModule::Stop() {}

UbseResult UbseLcneModule::GetComUrmaEid()
{
    UbseResult ret;
    if (!UbseSmbios::GetInstance().IsClosType()) {
        ret = UbseLcneUrmaEid::GetInstance().GetUrmaEid(allSocketComEid);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[MTI] Failed to get UrmaEid from static.";
            return ret;
        }
    } else {
        if (localBoardIOInfo.size() != NO_2) {
            UBSE_LOG_ERROR << "[MTI] Failed to get iou info, iou size=" << localBoardIOInfo.size();
            return UBSE_ERROR;
        }
        for (auto &dev : localBoardIOInfo) {
            UbseMtiIouInfo iou(dev.first.slotId, dev.first.ubpuId, dev.first.iouId);
            UbseMtiEidGroup fe;
            if (UbseLcneFeEid::GetInstance().GetComUrmaEidClos(iou, fe) != UBSE_OK) {
                UBSE_LOG_ERROR << "[MTI] Failed to get UrmaEid from local board.";
                return UBSE_ERROR;
            }
            allSocketComEid[iou] = fe;
            UBSE_LOG_INFO << "[MTI] allSocketComEid ubpu=" << dev.first.ubpuId << ", entity=" << fe.entityId
                           << ", primaryEid=" << fe.primaryEid << ", portEids.size=" << fe.portEids.size();
        }
    }
    return UBSE_OK;
}

UbseResult UbseLcneModule::FillNodeComInfo()
{
    ubseNodeInfos_.clear();
    if (UbseSmbios::GetInstance().IsClosType()) {
        uint32_t serverIdx = 0;
        auto ret = adapter_plugins::smbios::UbseSmbios::GetInstance().GetServerIdx(serverIdx);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to get super node basic info, " << FormatRetCode(ret);
            return ret;
        }
        ubseNodeInfo_.nodeId = std::to_string(serverIdx + 1);
        ubseNodeInfo_.eid = utils::GenerateUrmaDevEid(0, serverIdx + 1, 0, 0);
        ubseNodeInfos_.insert(ubseNodeInfos_.begin(), ubseNodeInfo_);
    } else {
        std::string &localNodeId = ubseLcneBusInstanceInfo.localNodeId;
        for (const auto &[iou, socketComEid] : allSocketComEid) {
            if (IsPrimaryEidExist(iou.slotId)) {
                continue;
            }
            uint32_t slotId = 0;
            if (ConvertStrToUint32(iou.slotId, slotId) != UBSE_OK) {
                UBSE_LOG_ERROR << "Convert slotId failed, " << FormatRetCode(UBSE_ERROR);
                return UBSE_ERROR;
            }
            std::string bondingEidString = utils::GenerateUrmaDevEid(0, slotId, 0, 0);
            if (iou.slotId == localNodeId) {
                ubseNodeInfo_.nodeId = localNodeId;
                ubseNodeInfo_.eid = bondingEidString;
                ubseNodeInfos_.insert(ubseNodeInfos_.begin(), ubseNodeInfo_);
            } else {
                ubseNodeInfos_.push_back({iou.slotId, bondingEidString});
            }
        }
    }
    return UBSE_OK;
}

const std::map<UbseMtiIouInfo, UbseMtiEidGroup> UbseLcneModule::GetMtiComEid()
{
    return allSocketComEid;
}

const std::map<UbseMtiIouInfo, UbseLcneIODieInfo> UbseLcneModule::GetLocalBoardIOInfo()
{
    return localBoardIOInfo;
}

bool UbseLcneModule::IsPrimaryEidExist(const std::string &nodeId)
{
    for (const auto &nodeInfo : ubseNodeInfos_) {
        if (nodeId == nodeInfo.nodeId) {
            return true;
        }
    }
    return false;
}

UbseResult UbseLcneModule::UbseGetLocalNodeInfo(UbseMtiNodeInfo &ubseNodeInfo)
{
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    ubseNodeInfo = ubseNodeInfo_;
    return UBSE_OK;
}

UbseResult UbseLcneModule::UbseGetAllNodeInfos(std::vector<UbseMtiNodeInfo> &ubseNodeInfos)
{
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    ubseNodeInfos = ubseNodeInfos_;
    return UBSE_OK;
}

UbseResult UbseLcneModule::UbseGetDevTopology(UbseDevTopology &devTopology)
{
    return ubseLcneTopology.UbseGetDevTopology(devTopology);
}

UbseLcneOSInfo UbseLcneModule::GetUbseLcneOSInfo()
{
    return localBoardHostInfo;
}

std::vector<std::string> UbseLcneModule::GetClusterIpList()
{
    return clusterIpList;
}

std::string UbseLcneModule::GetClusterLocalIp()
{
    return localIp;
}
} // namespace ubse::mti