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

#include "ubse_mti_interface_default.h"
#include <securec.h>
#include "ubse_context.h"
#include "ubse_lcne_module.h"
#include "ubse_str_util.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"
#include "lcne/ubse_lcne_decoder_entry.h"
#include "lcne/ubse_lcne_decoder_handle.h"
#include "lcne/ubse_lcne_ets.h"
#include "lcne/ubse_lcne_fe_eid.h"
#include "lcne/ubse_lcne_qos.h"
namespace ubse::adapter_plugins::mti {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace common::def;
using namespace ubse::mti;
using namespace context;
using namespace ubse::lcne;
using namespace ubse::adapter_plugins::mti::mami;
void SwapNodeInfo(UbseMtiNodeInfo& distNodeInfo, const MtiNodeInfo& srcNodeInfo)
{
    distNodeInfo.eid = srcNodeInfo.eid;
    distNodeInfo.nodeId = srcNodeInfo.nodeId;
}
void SwapNodeInfoList(std::vector<UbseMtiNodeInfo>& distNodeInfoList, std::vector<MtiNodeInfo>& srcNodeInfoList)
{
    for (auto& nodeInfo : srcNodeInfoList) {
        UbseMtiNodeInfo ubseNodeInfo;
        SwapNodeInfo(ubseNodeInfo, nodeInfo);
        distNodeInfoList.push_back(ubseNodeInfo);
    }
}
UbseResult UbseMtiInterfaceDefault::GetLocalNodeInfo(UbseMtiNodeInfo& nodeInfo)
{
    auto module = UbseContext::GetInstance().GetModule<ubse::mti::UbseLcneModule>();
    if (module == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    MtiNodeInfo tmpNodeInfo{};
    auto ret = module->UbseGetLocalNodeInfo(tmpNodeInfo);
    if (ret != UBSE_OK) {
        return ret;
    }
    SwapNodeInfo(nodeInfo, tmpNodeInfo);
    return UBSE_OK;
}

UbseResult UbseMtiInterfaceDefault::GetClusterNodeInfoList(std::vector<UbseMtiNodeInfo>& nodeInfoList)
{
    auto module = UbseContext::GetInstance().GetModule<UbseLcneModule>();
    if (module == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    std::vector<MtiNodeInfo> nodeIdList{};
    auto ret = module->UbseGetAllNodeInfos(nodeIdList);
    if (ret != UBSE_OK) {
        return ret;
    }
    SwapNodeInfoList(nodeInfoList, nodeIdList);
    return UBSE_OK;
}

UbseResult UbseMtiInterfaceDefault::GetClusterCpuTopo(UbseMtiCpuTopoInfoMap& topo)
{
    auto module = UbseContext::GetInstance().GetModule<UbseLcneModule>();
    if (module == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    UbseDevTopology devTopology{};
    auto ret = module->UbseGetDevTopology(devTopology);
    if (ret != UBSE_OK) {
        return ret;
    }
    std::map<UbseDevName, UbseMtiEidGroup> allSocketComEid = module->GetAllSocketComEid();
    std::map<UbseDevName, UbseLcneIODieInfo> localBoardIOInfo = module->GetLocalBoardIOInfo();
    for (const auto& [devName, devicInfoPair] : devTopology) {
        std::string devNodeId, socketId;
        devName.SplitDevName(devNodeId, socketId);
        UbseMtiCpuTopoInfo info{};
        auto conver_ret_first = utils::ConvertStrToUint32(devicInfoPair.first.slotId, info.slotId);
        auto conver_ret_last = utils::ConvertStrToUint32(socketId, info.socketId);
        if (conver_ret_first != UBSE_OK || conver_ret_last != UBSE_OK) {
            UBSE_LOG_ERROR << "convert str failed, "
                           << "dev.second.first.slotId=" << devicInfoPair.first.slotId << ", socketId=" << socketId;
            return UBSE_ERROR;
        }
        info.primaryEid = allSocketComEid[devName].primaryEid;
        info.chipId = devicInfoPair.first.chipId;
        info.cardId = devicInfoPair.first.cardId;
        info.busNodeCna = devicInfoPair.first.busNodeCna;
        info.eid = localBoardIOInfo[devName].ubControllerEid; // LCNE获取时能保证key存在
        info.guid = localBoardIOInfo[devName].guid;           // LCNE获取时能保证key存在
        info.portInfos = devicInfoPair.second;
        for (auto& portInfo : info.portInfos) {
            portInfo.second.urmaEid = allSocketComEid[devName].portEids[portInfo.second.portId];
        }
        topo[devName] = info;
    }
    return UBSE_OK;
}

UbseResult UbseMtiInterfaceDefault::GetLocalIp(std::string& localIp)
{
    auto module = UbseContext::GetInstance().GetModule<UbseLcneModule>();
    if (module == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    localIp = module->GetClusterLocalIp();
    return UBSE_OK;
}

UbseResult UbseMtiInterfaceDefault::GetClusterIpList(std::vector<std::string>& ipList)
{
    auto module = UbseContext::GetInstance().GetModule<UbseLcneModule>();
    if (module == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    ipList = std::move(module->GetClusterIpList());
    return UBSE_OK;
}
UbseResult UbseMtiInterfaceDefault::AddDecoderEntry(const mami::UbseMamiMemImportInfo& importInfo,
                                                    mami::UbseMamiMemImportResult& importResult,
                                                    const UbseDecoderTrustRingData& trustRingData)
{
    return UbseLcneDecoderEntry::AddDecoderEntry(importInfo, importResult, trustRingData);
}

UbseResult UbseMtiInterfaceDefault::DeleteDecoderEntry(const mami::UbseMamiMemWithdraw& drawInfo)
{
    return lcne::UbseLcneDecoderEntry::DeleteDecoderEntry(drawInfo);
}

UbseResult UbseMtiInterfaceDefault::InvalidateDecoderEntry(const mami::UbseMamiMemWithdraw& drawInfo)
{
    return lcne::UbseLcneDecoderEntry::InvalidateDecoderEntry(drawInfo);
}

UbseResult UbseMtiInterfaceDefault::GetAllMemHandles(const mami::UbseMamiMemHandleQueryInfo& queryInfo,
                                                     std::vector<mami::UbseMamiMemHandleValue>& handleValues)
{
    return lcne::UbseLcneDecoderHandle::GetInstance().GetAllMemHandles(queryInfo, handleValues);
}

UbseResult UbseMtiInterfaceDefault::GetAllSocketComEid(std::map<UbseDevName, UbseMtiEidGroup>& socketInfoMap)
{
    auto module = UbseContext::GetInstance().GetModule<UbseLcneModule>();
    if (module == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    socketInfoMap = module->GetAllSocketComEid();
    return UBSE_OK;
}

UbseResult UbseMtiInterfaceDefault::UbseGetFeEid(UbseMtiIouInfo iouInfo, std::vector<UbseMtiFeInfo> &allFeInfos)
{
    return lcne::UbseLcneFeEid::GetInstance().GetFeEid(iouInfo, allFeInfos);
}

UbseResult UbseMtiInterfaceDefault::UbseCreateQosProfile(UbseMtiQosProfile ubseLcneQosProfile)
{
    return lcne::UbseLcneQos::GetInstance().CreateQosProfile(ubseLcneQosProfile);
}

UbseResult UbseMtiInterfaceDefault::UbseDeleteQosProfile(std::string profileName)
{
    return lcne::UbseLcneQos::GetInstance().DeleteQosProfile(profileName);
}

UbseResult UbseMtiInterfaceDefault::UbseQueryQosProfile(std::string profileName, UbseMtiQosProfile& ubseLcneQosProfile)
{
    return lcne::UbseLcneQos::GetInstance().QueryQosProfile(profileName, ubseLcneQosProfile);
}

UbseResult UbseMtiInterfaceDefault::UbseApplyVfeQos(UbseMtiFeInfo ubseFeInfo, std::string profileName)
{
    return lcne::UbseLcneQos::GetInstance().ApplyVfeQos(ubseFeInfo, profileName);
}

UbseResult UbseMtiInterfaceDefault::UbseDeleteVfeQos(UbseMtiFeInfo ubseFeInfo)
{
    return lcne::UbseLcneQos::GetInstance().DeleteVfeQos(ubseFeInfo);
}

UbseResult UbseMtiInterfaceDefault::UbseQueryVfeQos(UbseMtiFeInfo ubseFeInfo, std::string& profileName)
{
    return lcne::UbseLcneQos::GetInstance().DeleteVfeQos(ubseFeInfo);
}

UbseResult UbseMtiInterfaceDefault::UbseCreateEtsProfile(const UbseMtiEtsProfile &etsProfile)
{
    return lcne::UbseLcneEts::GetInstance().CreateEtsProfile(etsProfile);
}

UbseResult UbseMtiInterfaceDefault::UbseAddEtsVlsToProfile(const std::string &profileName,
                                                         const std::vector<UbseEtsVl> &vls)
{
    return lcne::UbseLcneEts::GetInstance().AddEtsVlsToProfile(profileName, vls);
}

UbseResult UbseMtiInterfaceDefault::UbseAddEtsPriorityGroupsToProfile(
    const std::string &profileName, const std::vector<UbseEtsPriorityGroup> &priorityGroups)
{
    return lcne::UbseLcneEts::GetInstance().AddEtsPriorityGroupsToProfile(profileName, priorityGroups);
}

UbseResult UbseMtiInterfaceDefault::UbseDeleteEtsProfile(const std::string &profileName)
{
    return lcne::UbseLcneEts::GetInstance().DeleteEtsProfile(profileName);
}

UbseResult UbseMtiInterfaceDefault::UbseRemoveEtsVlsFromProfile(const std::string &profileName)
{
    return lcne::UbseLcneEts::GetInstance().RemoveEtsVlsFromProfile(profileName);
}

UbseResult UbseMtiInterfaceDefault::UbseRemoveEtsPriorityGroupsFromProfile(const std::string &profileName)
{
    return lcne::UbseLcneEts::GetInstance().RemoveEtsPriorityGroupsFromProfile(profileName);
}

UbseResult UbseMtiInterfaceDefault::UbseQueryEtsProfile(const std::string &profileName, UbseMtiEtsProfile &etsProfile)
{
    return lcne::UbseLcneEts::GetInstance().QueryEtsProfile(profileName, etsProfile);
}
} // namespace ubse::adapter_plugins::mti
