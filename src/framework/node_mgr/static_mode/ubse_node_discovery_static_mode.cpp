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

#include "ubse_node_discovery_static_mode.h"

#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "adapter_plugins/mti/ubse_smbios.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_lcne_module.h"
#include "ubse_logger_module.h"
#include "ubse_node_mgr_def.h"
#include "ubse_str_util.h"
#include "ubse_node_static_info_mgr.h"

UBSE_DEFINE_THIS_MODULE("ubse");
namespace ubse::nodeMgr {
using namespace ubse::adapter_plugins::smbios;
using namespace ubse::adapter_plugins::mti;
using namespace ubse::log;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::common;
using namespace ubse::mti;
using namespace ubse::utils;

UbseResult UbseNodeDiscoveryStaticMode::Init()
{
    UbseResult ret = UBSE_OK;
    isClos_ = UbseSmbios::GetInstance().IsClosType();
    UbseNodeStaticInfo info{};
    ret = UbseNodeStaticInfoMgr::GetInstance().InitCurNodeInfo(info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "init cur node static info failed, " << FormatRetCode(ret);
        return ret;
    }
    UbseNodeStaticInfoMgr::GetInstance().SetCurrentNode(info);
    ret = GenerateClusterStaticInfo();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "init cluster static info failed, " << FormatRetCode(ret);
    }
    return ret;
}

UbseResult UbseNodeDiscoveryStaticMode::GenerateClusterStaticInfo()
{
    auto &nodeDiscovery = UbseNodeStaticInfoMgr::GetInstance();
    auto currentSuperNode = nodeDiscovery.GetCurrentNode();
    std::vector<UbseMtiNodeInfo> ubseNodeInfos;
    auto ret = UbseMtiInterface::GetInstance().GetClusterNodeInfoList(ubseNodeInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get all cluster node info failed, " << FormatRetCode(ret);
        return ret;
    }
    std::map<UbseMtiIouInfo, UbseMtiEidGroup> comUrmaInfoMap{};
    ret = UbseMtiInterface::GetInstance().GetMtiComEid(comUrmaInfoMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get all socket eid failed, " << FormatRetCode(ret);
        return ret;
    }
    for (const auto &socketComEid : comUrmaInfoMap) {
        std::string nodeId = socketComEid.first.slotId;
        std::string ubpuId = socketComEid.first.ubpuId;
        const auto it = std::find_if(ubseNodeInfos.begin(), ubseNodeInfos.end(),
                                     [&nodeId](const UbseMtiNodeInfo &info) { return info.nodeId == nodeId; });
        if (it == ubseNodeInfos.end()) {
            UBSE_LOG_WARN << "nodeId=" << nodeId << ", ubpuId=" << ubpuId
                          << ", not found in nodes info, will skip";
            continue;
        }
        UbseNodeStaticInfo info = nodeDiscovery.GetUbseNodeById(nodeId);
        if (info.nodeId.empty()) {
            info.nodeId = nodeId;
            info.superPodId = currentSuperNode.superPodId;
            info.groupId = currentSuperNode.groupId;
        }
        info.bonding0Eid = it->eid;
        info.feEidList[ubpuId] = socketComEid.second;
        nodeDiscovery.SetNodes({info});
        if (nodeId == currentSuperNode.nodeId) {
            nodeDiscovery.SetCurrentNode(info);
        }
    }
    if (isClos_) {
        // todo clos组网场景下,需要额外计算集群内其他节点的EID拓扑信息
    }
    return UBSE_OK;
}

void UbseNodeDiscoveryStaticMode::UnInit() {}
} // namespace ubse::nodeDiscovery