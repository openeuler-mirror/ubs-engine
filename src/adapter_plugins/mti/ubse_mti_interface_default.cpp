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
namespace ubse::adapter_plugins::mti {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace common::def;
using namespace ubse::mti;
using namespace context;
using namespace ubse::lcne;
using namespace ubse::adapter_plugins::mti::mami;
void SwapNodeInfo(UbseMtiNodeInfo& distNodeInfo, const UbseMtiNodeInfo& srcNodeInfo)
{
    distNodeInfo.eid = srcNodeInfo.eid;
    distNodeInfo.nodeId = srcNodeInfo.nodeId;
}
void SwapNodeInfoList(std::vector<UbseMtiNodeInfo>& distNodeInfoList, std::vector<UbseMtiNodeInfo>& srcNodeInfoList)
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
    UbseMtiNodeInfo tmpNodeInfo{};
    auto ret = module->UbseGetLocalNodeInfo(tmpNodeInfo);
    if (ret != UBSE_OK) {
        return ret;
    }
    SwapNodeInfo(nodeInfo, tmpNodeInfo);
    return UBSE_OK;
}

UbseResult UbseMtiInterfaceDefault::GetCurNodeTopo(UbseDevTopology& topo)
{
    auto module = UbseContext::GetInstance().GetModule<ubse::mti::UbseLcneModule>();
    if (module == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto ret = module->UbseGetDevTopology(topo);
    if (ret != UBSE_OK) {
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseMtiInterfaceDefault::GetClusterNodeInfoList(std::vector<UbseMtiNodeInfo>& nodeInfoList)
{
    auto module = UbseContext::GetInstance().GetModule<UbseLcneModule>();
    if (module == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    std::vector<UbseMtiNodeInfo> nodeIdList{};
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
    std::map<UbseMtiIouInfo, UbseMtiEidGroup> allSocketComEid = module->GetMtiComEid();
    std::map<UbseMtiIouInfo, UbseLcneIODieInfo> localBoardIOInfo = module->GetLocalBoardIOInfo();
    for (const auto& [devName, devicInfoPair] : devTopology) {
        UbseMtiIouInfo iouInfo(devicInfoPair.first.slotId, devicInfoPair.first.chipId, devicInfoPair.first.cardId);
        UbseMtiCpuTopoInfo info{};
        if (utils::ConvertStrToUint32(iouInfo.slotId, info.nodeId) != UBSE_OK) {
            UBSE_LOG_ERROR << "convert str failed, slotId = " << iouInfo.slotId;
            return UBSE_ERROR;
        }
        info.primaryEid = allSocketComEid[iouInfo].primaryEid;
        info.chipId = devicInfoPair.first.chipId;
        info.cardId = devicInfoPair.first.cardId;
        info.busNodeCna = devicInfoPair.first.busNodeCna;
        info.eid = localBoardIOInfo[iouInfo].ubControllerEid; // LCNE获取时能保证key存在
        info.guid = localBoardIOInfo[iouInfo].guid;           // LCNE获取时能保证key存在
        info.portInfos = devicInfoPair.second;
        for (auto& portInfo : info.portInfos) {
            portInfo.second.urmaEid = allSocketComEid[iouInfo].portEids[portInfo.second.portId];
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

UbseResult UbseMtiInterfaceDefault::GetMtiComEid(std::map<UbseMtiIouInfo, UbseMtiEidGroup>& comUrmaInfoMap)
{
    auto module = UbseContext::GetInstance().GetModule<UbseLcneModule>();
    if (module == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    comUrmaInfoMap = module->GetMtiComEid();
    return UBSE_OK;
}

UbseResult UbseMtiInterfaceDefault::UbseGetFeEid(UbseMtiIouInfo iouInfo, std::vector<UbseMtiFeInfo>& allFeInfos)
{
    auto module = UbseContext::GetInstance().GetModule<UbseLcneModule>();
    if (module == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    iouInfo.slotId = module->GetCurSlotId();
    return lcne::UbseLcneFeEid::GetInstance().GetFeEid(iouInfo, allFeInfos);
}

UbseResult UbseMtiInterfaceDefault::UbseCreateEtsProfile(const UbseMtiEtsProfile& etsProfile)
{
    return lcne::UbseLcneEts::GetInstance().CreateEtsProfile(etsProfile);
}

UbseResult UbseMtiInterfaceDefault::UbseAddEtsVlsToProfile(const std::string& profileName,
                                                           const std::vector<UbseEtsVl>& vls)
{
    return lcne::UbseLcneEts::GetInstance().AddEtsVlsToProfile(profileName, vls);
}

UbseResult UbseMtiInterfaceDefault::UbseAddEtsPriorityGroupsToProfile(
    const std::string& profileName, const std::vector<UbseEtsPriorityGroup>& priorityGroups)
{
    return lcne::UbseLcneEts::GetInstance().AddEtsPriorityGroupsToProfile(profileName, priorityGroups);
}

UbseResult UbseMtiInterfaceDefault::UbseAddEtsVlsAndPriorityGroupsToProfile(
    const std::string& profileName, const std::vector<UbseEtsVl>& vls,
    const std::vector<UbseEtsPriorityGroup>& priorityGroups)
{
    return lcne::UbseLcneEts::GetInstance().AddEtsVlsAndPriorityGroupsToProfile(profileName, vls, priorityGroups);
}

UbseResult UbseMtiInterfaceDefault::UbseDeleteEtsProfile(const std::string& profileName)
{
    return lcne::UbseLcneEts::GetInstance().DeleteEtsProfile(profileName);
}

UbseResult UbseMtiInterfaceDefault::UbseRemoveEtsVlsFromProfile(const std::string& profileName)
{
    return lcne::UbseLcneEts::GetInstance().RemoveEtsVlsFromProfile(profileName);
}

UbseResult UbseMtiInterfaceDefault::UbseRemoveEtsPriorityGroupsFromProfile(const std::string& profileName)
{
    return lcne::UbseLcneEts::GetInstance().RemoveEtsPriorityGroupsFromProfile(profileName);
}

UbseResult UbseMtiInterfaceDefault::UbseQueryEtsProfile(const std::string& profileName, UbseMtiEtsProfile& etsProfile)
{
    return lcne::UbseLcneEts::GetInstance().QueryEtsProfile(profileName, etsProfile);
}

UbseResult UbseMtiInterfaceDefault::UbseQueryAllEtsProfiles(std::vector<UbseMtiEtsProfile>& etsProfiles)
{
    return lcne::UbseLcneEts::GetInstance().QueryAllEtsProfiles(etsProfiles);
}

UbseResult UbseMtiInterfaceDefault::UbseApplyEtsProfileToInterface(const std::string& interfaceName,
                                                                   const std::string& profileName)
{
    return lcne::UbseLcneEts::GetInstance().ApplyEtsProfileToInterface(interfaceName, profileName);
}

UbseResult UbseMtiInterfaceDefault::UbseRemoveEtsProfileFromInterface(const std::string& interfaceName)
{
    return lcne::UbseLcneEts::GetInstance().RemoveEtsProfileFromInterface(interfaceName);
}

UbseResult UbseMtiInterfaceDefault::UbseQueryAllInterfaceEtsProfile(
    std::vector<UbseMtiInterfaceEtsApplication>& applications)
{
    return lcne::UbseLcneEts::GetInstance().QueryAllInterfaceEtsProfile(applications);
}

UbseResult UbseMtiInterfaceDefault::UbseQueryInterfaceEtsProfile(const std::string& interfaceName,
                                                                 std::string& profileName)
{
    return lcne::UbseLcneEts::GetInstance().QueryInterfaceEtsProfile(interfaceName, profileName);
}

} // namespace ubse::adapter_plugins::mti
