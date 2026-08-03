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

#ifndef TEST_UBSE_URMA_CONTROLLER_DEF_H
#define TEST_UBSE_URMA_CONTROLLER_DEF_H

#include "ubse_node_com_urma_collector.h"
#include "ubse_node_controller.h"
#include "ubse_thread_pool_module.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_controller_rpc.h"
#include "ubse_urma_uvs_module.h"

namespace ubse::urmaController {
using ubse::urma::EidGroup;
using ubse::urma::UbseFeInfo;
using ubse::urma::UbseUrmaUvsAggrDev;
extern std::shared_ptr<UbseFeInfo> GetUrmaVfeFromEidGroup(EidGroup& eidGroup);
extern void SetUrmaInfoState(const std::string& urmaDevEid, bool isActive, const std::string& nodeId);
extern std::string GetUrmaDevEidByUrmaName(const std::string& urmaName);
extern bool IsUrmaBondingActivated(const std::string& urmaName);
extern UbseResult RecoverOneUrmaDeviceForOneNode(const std::string& nodeId, UbseUrmaUvsAggrDev& dev);
extern std::atomic<uint32_t> g_asyncHandlerCnt;
UrmaDevState ConvertUint32ToBondingState(uint32_t val);
UbseResult GetCurNodeIdAndMasterNodeId(std::string& curNodeId, std::string& masterNodeId);
UbseResult UbseUrmaAsyncBrocastUrmaInfo();
UbseResult ReportUrmaNodeInfoToMaster(const std::string& nodeId);
void UbseUrmaBandwidthInit(const std::string& nodeId);
UbseResult QueryUrmaInfoFromMaster(const ubse::election::UbseRoleInfo& roleInfo,
                                   std::vector<std::string>& updateNodeIds);
UbseResult DoUpdateUrmaInfos(std::vector<std::string> updateNodeIds);
UbseResult ForwardActiveReqToSpecifyNode(const std::string& nodeId, const ubse::message::UbseBaseMessagePtr& request,
                                         const ubse::message::UbseBaseMessagePtr& response);
UbseResult PostUpdateUrmaInfosTask(const std::map<std::string, uint64_t>& urmaInfoTimestamps);
UbseResult UbseUrmaAsyncNotifyOneNodeUrmaInfoChange(const std::string& notifyNodeId);
UbseResult BrocastUrmaInfoTask(const std::string& nodeId);
} // namespace ubse::urmaController

namespace ubse::urmaController::ut {
inline ubse::urma::UbseUrmaInfo MakeBacking(const std::string& devEid, const std::string& firstFeEid,
                                            const std::string& secondFeEid, uint64_t hwResId = 1)
{
    ubse::urma::UbseUrmaInfo info{};
    info.urmaDevEid = devEid;
    info.urmaDevType = ubse::urma::UrmaDevType::UNIQUE;
    info.state = ubse::urma::UrmaDevState::UNKNOWN;
    info.hwResId = hwResId;
    auto firstFe = std::make_shared<ubse::urma::UbseFeInfo>();
    auto secondFe = std::make_shared<ubse::urma::UbseFeInfo>();
    info.eidGroups.push_back({firstFeEid, {}, std::move(firstFe)});
    info.eidGroups.push_back({secondFeEid, {}, std::move(secondFe)});
    return info;
}

inline void InsertBacking(const std::string& name, ubse::urma::UbseUrmaInfo info, const std::string& nodeId = "1")
{
    auto& manager = UbseUrmaControllerManager::GetInstance();
    ubse::utils::WriteLocker<ubse::utils::ReadWriteLock> writeLock(&manager.rwLock);
    manager.nodeInfos[nodeId].nodeId = nodeId;
    manager.nodeInfos[nodeId].urmaList[name] = std::move(info);
}

} // namespace ubse::urmaController::ut

namespace ubse::urma {
inline bool operator==(const UbseFeInfo& a, const UbseFeInfo& b)
{
    return a.name == b.name && a.slotId == b.slotId && a.ubpuId == b.ubpuId && a.iouId == b.iouId &&
           a.entityId == b.entityId && a.fetype == b.fetype;
}
inline bool operator==(const EidGroup& a, const EidGroup& b)
{
    if (a.primaryEid != b.primaryEid || a.portEids != b.portEids)
        return false;
    if (a.feInfo == nullptr && b.feInfo == nullptr)
        return true;
    if (a.feInfo != nullptr && b.feInfo != nullptr)
        return *a.feInfo == *b.feInfo;
    return false;
}
inline bool operator==(const UbseUrmaInfo& a, const UbseUrmaInfo& b)
{
    return a.subPath == b.subPath && a.urmaDevEid == b.urmaDevEid && a.eidGroups == b.eidGroups &&
           a.urmaDevType == b.urmaDevType && a.state == b.state && a.hwResId == b.hwResId;
}
inline bool operator==(const UbseUrmaUvsFe& a, const UbseUrmaUvsFe& b)
{
    return a.ubpuId == b.ubpuId && a.entityId == b.entityId && a.primaryEid == b.primaryEid;
}
inline bool operator==(const UbseUrmaUvsAggrDev& a, const UbseUrmaUvsAggrDev& b)
{
    return a.urmaDevEid == b.urmaDevEid && a.feList == b.feList;
}
inline bool operator==(const UbseUrmaUvsNodeInfo& a, const UbseUrmaUvsNodeInfo& b)
{
    return a.nodeId == b.nodeId && a.devList == b.devList;
}
inline bool operator==(const UbseUrmaNodeInfo& a, const UbseUrmaNodeInfo& b)
{
    return a.nodeId == b.nodeId && a.urmaList == b.urmaList && a.updateTimeStamp == b.updateTimeStamp;
}
} // namespace ubse::urma

#endif // TEST_UBSE_URMA_CONTROLLER_DEF_H
