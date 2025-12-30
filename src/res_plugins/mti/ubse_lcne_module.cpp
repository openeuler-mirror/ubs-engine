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
#include <iostream>
#include <regex>
#include <shared_mutex>
#include <unordered_set>
#include "securec.h"

#include "ubse_event_module.h"
#include "ubse_http_module.h"
#include "ubse_thread_pool_module.h"
#include "src/res_plugins/mti/lcne/ubse_lcne_busInstance.h"
#include "src/res_plugins/mti/lcne/ubse_lcne_host_info.h"
#include "src/res_plugins/mti/lcne/ubse_lcne_node_info.h"
#include "src/res_plugins/mti/lcne/ubse_lcne_topology_client.h"
#include "src/res_plugins/mti/lcne/ubse_lcne_urma_eid.h"
#include "ubse_conf.h"
#include "ubse_conf_module.h"
#include "ubse_logger_module.h"
#include "ubse_str_util.h"
#include "ubse_net_util.h"

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
BASE_DYNAMIC_CREATE(UbseLcneModule, UbseTaskExecutorModule, UbseEventModule);
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_MTI_MID)

using UvsSetTopoInfo = uint32_t (*)(void *topo, uint32_t topNum);

UbseResult UbseLcneModule::GetLcneConf()
{
    // 读取lcne监听端口
    auto module = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "Failed to get config module";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    // 真实lcne端口
    std::string realLcnePortStr{realLcneDefaultPort}; // 默认为34256
    auto ret = module->GetConf<std::string>("ubse.ubfm", "lcne.server.port", realLcnePortStr);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to get the configuration for lcne.port. The default value for lcne.port will be used.";
        return UBSE_OK;
    }
    UBSE_LOG_DEBUG << "The value of the lcne.port field is " << realLcnePortStr;
    if (ConvertPortConfStrToInt(realLcnePortStr, LcneServer::realPort) != UBSE_OK) {
        LcneServer::realPort = DEFAULT_LCNE_SERVER_PORT;
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

UbseResult UbseLcneModule::Initialize()
{
    // Init阶段，向lcne设备模块获取信息
    auto ret = GetLcneConf();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Failed to get mock lcne port";
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

    return UBSE_OK;
}
UbseResult UbseLcneModule::GetLcneData()
{
    auto topoChangeRet = ubseLcneTopology.RegHttpHandler();
    auto topoRet = ubseLcneTopology.Start();
    auto urmaEidRet = UbseLcneUrmaEid::GetInstance().GetUrmaEid(allSocketComEid);
    auto busInstanceRet = UbseLcneBusInstance::GetInstance().QueryBusinstance(ubseLcneBusInstanceInfo);
    auto ioDieInfoRet = UbseLcneNodeInfo::GetGetInstance().QueryAllLcneIODieInfo(localBoardIOInfo);
    auto hostInfoRet = UbseLcneHostInfo::GetGetInstance().QueryLcneHostInfo(localBoardHostInfo);
    if (topoRet == UBSE_OK && topoChangeRet == UBSE_OK && urmaEidRet == UBSE_OK && busInstanceRet == UBSE_OK &&
        ioDieInfoRet == UBSE_OK && hostInfoRet == UBSE_OK) {
        return UBSE_OK;
    }
    return UBSE_ERROR;
}

void UbseLcneModule::UnInitialize() {}

UbseResult UbseLcneModule::Start()
{
    return UBSE_OK;
}

void UbseLcneModule::Stop() {}

UbseResult UbseLcneModule::GenerateBondingEid(const std::string &nodeId, unsigned char *bondingEid)
{
    uint32_t slotNumber = 0;
    UbseResult ret = ConvertStrToUint32(nodeId, slotNumber);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Failed to parse slotNumber, " << FormatRetCode(ret);
        return UBSE_ERROR_CLI_ARGS_FAILED;
    }
    // eid的后4个字节是从第12个字节开始的
    memcpy_s(bondingEid + 12, IPV6_SEGMENT_LENGTH, &slotNumber, IPV6_SEGMENT_LENGTH);
    return UBSE_OK;
}

UbseResult UbseLcneModule::FillNodeComInfo()
{
    std::string &localNodeId = ubseLcneBusInstanceInfo.localNodeId;
    ubseNodeInfos_.clear();

    for (const auto &socketComEid : allSocketComEid) {
        std::string nodeId;
        std::string socketId;
        socketComEid.first.SplitDevName(nodeId, socketId);

        if (IsPrimaryEidExist(nodeId)) {
            continue;
        }

        unsigned char bondingEid[IPV6_BYTE_COUNT] = {0x42, 0x45, 0x49, 0x44}; // 前32位为BEID
        auto ret = GenerateBondingEid(nodeId, bondingEid);                    // BEID 0000 0000 [NodeId]000
        if (ret != UBSE_OK) {
            return ret;
        }

        std::string bondingEidString = BytesToIPv6String(bondingEid);

        if (nodeId == localNodeId) {
            ubseNodeInfo_.nodeId = localNodeId;
            ubseNodeInfo_.eid = bondingEidString;
            ubseNodeInfos_.insert(ubseNodeInfos_.begin(), ubseNodeInfo_);
        } else {
            ubseNodeInfos_.push_back({nodeId, bondingEidString});
        }
    }

    return UBSE_OK;
}

UbseResult UbseLcneModule::GetBondingEidByNodeId(std::string &bondingEid, const std::string &nodeId)
{
    for (MtiNodeInfo &nodeInfo : ubseNodeInfos_) {
        if (nodeId == nodeInfo.nodeId) {
            bondingEid = nodeInfo.eid;
            return UBSE_OK;
        }
    }
    return UBSE_ERROR;
}

const std::map<DevName, UbseLcneSocketInfo> UbseLcneModule::GetAllSocketComEid()
{
    return allSocketComEid;
}

const std::map<DevName, UbseLcneIODieInfo> UbseLcneModule::GetLocalBoardIOInfo()
{
    return localBoardIOInfo;
}

UbseResult UbseLcneModule::GetTopologyInfo(std::map<std::string, std::vector<IODieInfo>> &allNodeIOdieInfo)
{
    for (const auto &socketComEid : allSocketComEid) {
        IODieInfo ioDieInfo{};
        DevName devName(socketComEid.first);
        const std::string &primaryEid = socketComEid.second.primaryEid;

        UbseResult ret = ParseColonHexString(primaryEid, ioDieInfo.primaryEid);
        if (UBSE_RESULT_FAIL(ret)) {
            UBSE_LOG_ERROR << "Failed to parse primaryEid.";
            continue;
        }

        const std::map<std::string, UbseLcnePortInfo> &portEidList = socketComEid.second.portEidList;
        if (portEidList.size() > 9) { // 端口eid列表不能超过9个元素
            return UBSE_ERROR_INVAL;
        }

        ret = GetIoDiePortEid(devName, ioDieInfo, portEidList);
        if (UBSE_RESULT_FAIL(ret)) {
            UBSE_LOG_ERROR << "Failed to process port eid list.";
            return ret;
        }

        std::string nodeId;
        std::string socketId;
        socketComEid.first.SplitDevName(nodeId, socketId);
        ret |= ubse::utils::ConvertStrToInt(socketId, ioDieInfo.socketId);
        if (UBSE_RESULT_FAIL(ret)) {
            UBSE_LOG_ERROR << "Failed to parse ioDieInfo, devName=" << devName.devName;
            return UBSE_ERROR_CLI_ARGS_FAILED;
        }

        allNodeIOdieInfo[nodeId].push_back(ioDieInfo);
    }

    return UBSE_OK;
}

UbseResult UbseLcneModule::GetIoDiePortEid(const DevName &devName, IODieInfo &ioDieInfo,
                                           const std::map<std::string, UbseLcnePortInfo> &portEidList)
{
    uint32_t index = 0;
    for (const auto &portEid : portEidList) {
        const std::string &urmaEid = portEid.second.urmaEid;
        UbseResult ret = ParseColonHexString(urmaEid, ioDieInfo.portEid[index]);
        if (UBSE_RESULT_FAIL(ret)) {
            UBSE_LOG_ERROR << "Failed to parse eid, eid=" << urmaEid;
            return UBSE_ERROR_INVAL;
        }

        DevTopology devTopology{};
        ubseLcneTopology.UbseGetDevTopology(devTopology);

        auto devTopologyIter = devTopology.find(devName);
        if (devTopologyIter == devTopology.end()) {
            ++index;
            continue;
        }
        UbseDevPortName ubseDevPortName{portEid.first};
        std::unordered_map<UbseDevPortName, UbsePortInfo, DevPortNameHash> &ubseDevPorts =
            devTopologyIter->second.second;
        auto ubseDevPortIter = ubseDevPorts.find(ubseDevPortName);
        if (ubseDevPortIter == ubseDevPorts.end()) {
            UBSE_LOG_ERROR << "The corresponding port name was not found portName=" << ubseDevPortName.devPortName;
            return UBSE_ERROR_INVAL;
        }

        UbsePortInfo &ubsePortInfo = ubseDevPortIter->second;
        if (ubsePortInfo.portStatus == PortStatus::DOWN) {
            ++index;
            continue;
        }

        DevName remoteDevName(ubsePortInfo.remoteSlotId, ubsePortInfo.remoteChipId);
        auto socketComEidIter = allSocketComEid.find(remoteDevName);
        if (socketComEidIter == allSocketComEid.end()) {
            UBSE_LOG_ERROR << "The corresponding device name was not found , devName=" << remoteDevName.devName;
            return UBSE_ERROR_INVAL;
        }

        UbseLcneSocketInfo &remoteSocketComEid = socketComEidIter->second;
        auto portEidIter = remoteSocketComEid.portEidList.find(ubsePortInfo.remotePortId);
        if (portEidIter == remoteSocketComEid.portEidList.end()) {
            UBSE_LOG_ERROR << "The corresponding port id was not found, portId=" << ubsePortInfo.remotePortId;
            return UBSE_ERROR_INVAL;
        }

        ret = ParseColonHexString(portEidIter->second.urmaEid, ioDieInfo.peerPortEid[index]);
        if (UBSE_RESULT_FAIL(ret)) {
            UBSE_LOG_ERROR << "Failed to parse peer port eid, eid=" << portEidIter->second.urmaEid;
            return UBSE_ERROR_INVAL;
        }
        ++index;
    }
    return UBSE_OK;
}

UbseResult UbseLcneModule::SetUvsComInfo()
{
    std::map<std::string, std::vector<IODieInfo>> allNodeIOdieInfo;
    auto ret = GetTopologyInfo(allNodeIOdieInfo);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Get topology info failed, ErrorCode=" << ret;
        return ret;
    }

    uint32_t size = allNodeIOdieInfo.size();
    std::unique_ptr<TopoInfo[]> topoArray = std::make_unique<TopoInfo[]>(size);

    ret = FillTopoArray(allNodeIOdieInfo, topoArray);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to FillTopoArray";
        return ret;
    }

    void *handle = dlopen("/usr/lib64/libtpsa.so", RTLD_LAZY);
    if (handle == nullptr) {
        UBSE_LOG_WARN << "dlopen uvs.so failed";
        return UBSE_ERROR_NOENT;
    }
    auto uvsSetTopoInfo = (UvsSetTopoInfo)dlsym(handle, "uvs_set_topo_info");
    if (uvsSetTopoInfo == nullptr) {
        UBSE_LOG_ERROR << "Failed to find symbol 'uvs_set_topo_info'";
        dlclose(handle);
        return UBSE_ERROR_NULLPTR;
    }

    ret = uvsSetTopoInfo(topoArray.get(), size);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Uvs failed to set topology information, ErrorCode=" << ret;
        dlclose(handle);
        return ret;
    }
    dlclose(handle);

    return UBSE_OK;
}
UbseResult UbseLcneModule::FillTopoArray(std::map<std::string, std::vector<IODieInfo>> &allNodeIOdieInfo,
                                         std::unique_ptr<TopoInfo[]> &topoArray)
{
    uint32_t index = 0;
    for (const auto &nodeIOdieInfo : allNodeIOdieInfo) {
        const std::string &nodeId = nodeIOdieInfo.first;
        TopoInfo &topoInfo = topoArray[index];

        // 填充bondingEid
        std::string bondingEid;
        auto ret = GetBondingEidByNodeId(bondingEid, nodeId);
        if (UBSE_RESULT_FAIL(ret)) {
            UBSE_LOG_WARN << "Cannot find the BondingEid corresponding to the node ID.";
            return UBSE_ERROR_INVAL;
        }
        ret = ParseColonHexString(bondingEid, topoInfo.bondingEid);
        if (UBSE_RESULT_FAIL(ret)) {
            UBSE_LOG_WARN << "Failed to parse ioDieInfo, bondingEid=" << bondingEid;
            return UBSE_ERROR_INVAL;
        }

        // 填充ioDieInfo
        uint32_t maxIoDieSize = 2;
        uint32_t ioDieSize = nodeIOdieInfo.second.size();
        ioDieSize = std::min(maxIoDieSize, ioDieSize);
        for (uint32_t i = 0; i < ioDieSize; i++) {
            topoInfo.ioDieInfo[i] = nodeIOdieInfo.second[i];
        }

        // 填充isCurNode
        topoInfo.isCurNode = (nodeId == ubseLcneBusInstanceInfo.localNodeId);

        ++index;
    }
    return UBSE_OK;
}

UbseResult UbseLcneModule::ParseColonHexString(const std::string &input, char outBytes[IPV6_BYTE_COUNT])
{
    if (input.size() != IPV6_FULL_FORMAT_LENGTH) {
        return UBSE_ERROR;
    }

    // 将 char* 转换为 unsigned char* 以匹配 sscanf 的格式要求
    auto *uOut = reinterpret_cast<unsigned char *>(outBytes);

    int scanned = sscanf_s(input.c_str(),
                           "%2hhx%2hhx:"
                           "%2hhx%2hhx:"
                           "%2hhx%2hhx:"
                           "%2hhx%2hhx:"
                           "%2hhx%2hhx:"
                           "%2hhx%2hhx:"
                           "%2hhx%2hhx:"
                           "%2hhx%2hhx",
                           &uOut[0], &uOut[1], &uOut[2], &uOut[3], &uOut[4], &uOut[5], &uOut[6], &uOut[7], &uOut[8],
                           &uOut[9], &uOut[10], &uOut[11], &uOut[12], &uOut[13], &uOut[14], &uOut[15]);

    return scanned == IPV6_BYTE_COUNT ? UBSE_OK : UBSE_ERROR;
}

std::string UbseLcneModule::BytesToIPv6String(const unsigned char inBytes[IPV6_BYTE_COUNT])
{
    std::array<char, IPV6_FULL_FORMAT_LENGTH + 1> buffer{};

    snprintf_s(buffer.data(), buffer.size(), IPV6_FULL_FORMAT_LENGTH,
               "%02x%02x:"
               "%02x%02x:"
               "%02x%02x:"
               "%02x%02x:"
               "%02x%02x:"
               "%02x%02x:"
               "%02x%02x:"
               "%02x%02x",
               inBytes[NO_0], inBytes[NO_1], inBytes[NO_2], inBytes[NO_3], inBytes[NO_4], inBytes[NO_5], inBytes[NO_6],
               inBytes[NO_7], inBytes[NO_8], inBytes[NO_9], inBytes[NO_10], inBytes[NO_11], inBytes[NO_12],
               inBytes[NO_13], inBytes[NO_14], inBytes[NO_15]);

    return buffer.data();
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

UbseResult UbseLcneModule::UbseGetLocalNodeInfo(MtiNodeInfo &ubseNodeInfo)
{
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    ubseNodeInfo = ubseNodeInfo_;
    return UBSE_OK;
}

UbseResult UbseLcneModule::UbseGetAllNodeInfos(std::vector<MtiNodeInfo> &ubseNodeInfos)
{
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    ubseNodeInfos = ubseNodeInfos_;
    return UBSE_OK;
}

UbseResult UbseLcneModule::UbseGetDevTopology(DevTopology &devTopology)
{
    return ubseLcneTopology.UbseGetDevTopology(devTopology);
}

UbseLcneOSInfo UbseLcneModule::GetUbseLcneOSInfo()
{
    return localBoardHostInfo;
}

std::unordered_map<std::string, std::string> UbseLcneModule::GetClusterIpListFromConf()
{
    std::unordered_map<std::string, std::string> ipMap;
    std::vector<std::string> ips;

    std::string defaultVal;
    auto ret = UbseGetStr("ubse.rpc", "cluster.ipList", defaultVal);
    if (ret != UBSE_OK || defaultVal.empty()) {
        UBSE_LOG_WARN << "Unable to get cluster.ipList config," << FormatRetCode(ret) << " ,use default tcp";
        return ipMap;
    }
    std::vector<std::string> ipRangeVec;
    ubse::utils::Split(defaultVal, ",", ipRangeVec);
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
    for (int i = 0; i < ips.size(); ++i) {
        UBSE_LOG_INFO << "Put ip " << ips[i] << ", id " << i;
        ipMap.emplace(ips[i], std::to_string(i + 1));
    }
    return ipMap;
}
} // namespace ubse::mti