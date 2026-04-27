#ifndef TEST_UBSE_URMA_CONTROLLER_DEF_H
#define TEST_UBSE_URMA_CONTROLLER_DEF_H

#include "ubse_node_controller.h"
#include "ubse_thread_pool_module.h"
#include "ubse_urma_uvs_module.h"
#include "ubse_node_com_urma_collector.h"
#include "ubse_urma_controller_rpc.h"
#include "ubse_urma_controller_manager.h"

namespace ubse::urmaController {
extern std::shared_ptr<UbseFeInfo> GetUrmaVfeFromEidGroup(EidGroup &eidGroup);
extern void SetUrmaInfoState(const std::string &urmaDevEid, bool isActive, const std::string &nodeId);
extern std::string GetUrmaDevEidByUrmaName(const std::string &urmaName);
extern bool IsUrmaBondingActivated(const std::string &urmaName);
extern UbseResult RecoverOneUrmaDeviceForOneNode(const std::string &nodeId, UbseUrmaUvsAggrDev &dev);
extern std::atomic<uint32_t> g_asyncHandlerCnt;
UrmaDevState ConvertUint32ToBondingState(uint32_t val);
UbseResult GetCurNodeIdAndMasterNodeId(std::string &curNodeId, std::string &masterNodeId);
UbseResult UbseUrmaAsyncBrocastUrmaInfo();
UbseResult ReportUrmaNodeInfoToMaster(const std::string &nodeId);
void UbseUrmaBandwidthInit(const std::string &nodeId);
UbseResult QueryUrmaInfoFromMaster(const ubse::election::UbseRoleInfo &roleInfo, std::vector<std::string> &updateNodeIds);
UbseResult DoUpdateUrmaInfos(std::vector<std::string> updateNodeIds);
UbseResult ForwardActiveReqToSpecifyNode(const std::string &nodeId, const ubse::message::UbseBaseMessagePtr &request,
                                            const ubse::message::UbseBaseMessagePtr &response);
UbseResult PostUpdateUrmaInfosTask(const std::map<std::string, uint64_t> &urmaInfoTimestamps);
UbseResult UbseUrmaAsyncNotifyOneNodeUrmaInfoChange(const std::string &notifyNodeId);
UbseResult BrocastUrmaInfoTask(const std::string &nodeId);
}

namespace ubse::urma {
inline bool operator==(const UrmaQosProfile &a, const UrmaQosProfile &b)
{
    return a.profileName == b.profileName && a.minBandWidth == b.minBandWidth && a.maxBandWidth == b.maxBandWidth;
}
inline bool operator==(const UbseFeInfo &a, const UbseFeInfo &b)
{
    return a.name == b.name && a.slotId == b.slotId && a.ubpuId == b.ubpuId &&
            a.iouId == b.iouId && a.entityId == b.entityId && a.fetype == b.fetype;
}
inline bool operator==(const EidGroup &a, const EidGroup &b)
{
    if (a.primaryEid != b.primaryEid || a.portEids != b.portEids)
        return false;
    if (a.feInfo == nullptr && b.feInfo == nullptr)
        return true;
    if (a.feInfo != nullptr && b.feInfo != nullptr)
        return *a.feInfo == *b.feInfo;
    return false;
}
inline bool operator==(const UbseUrmaInfo &a, const UbseUrmaInfo &b)
{
    return a.subPath == b.subPath && a.urmaDevEid == b.urmaDevEid &&
            a.eidGroups == b.eidGroups && a.urmaQosProfile == b.urmaQosProfile &&
            a.urmaDevType == b.urmaDevType && a.state == b.state && a.hwResId == b.hwResId;
}
inline bool operator==(const UbseUrmaUvsFe &a, const UbseUrmaUvsFe &b)
{
    return a.ubpuId == b.ubpuId && a.entityId == b.entityId && a.primaryEid == b.primaryEid;
}
inline bool operator==(const UbseUrmaUvsAggrDev &a, const UbseUrmaUvsAggrDev &b)
{
    return a.urmaDevEid == b.urmaDevEid && a.feList == b.feList;
}
inline bool operator==(const UbseUrmaUvsNodeInfo &a, const UbseUrmaUvsNodeInfo &b)
{
    return a.nodeId == b.nodeId && a.devList == b.devList;
}
inline bool operator==(const UbseUrmaNodeInfo &a, const UbseUrmaNodeInfo &b)
{
    return a.nodeId == b.nodeId && a.urmaList == b.urmaList && a.updateTimeStamp == b.updateTimeStamp;
}
}

#endif // TEST_UBSE_URMA_CONTROLLER_DEF_H
