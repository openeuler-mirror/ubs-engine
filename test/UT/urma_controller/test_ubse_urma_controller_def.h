#ifndef TEST_UBSE_URMA_CONTROLLER_DEF_H
#define TEST_UBSE_URMA_CONTROLLER_DEF_H

#include "ubse_node_controller.h"
#include "ubse_thread_pool_module.h"
#include "ubse_urma_uvs_module.h"
#include "ubse_node_com_urma_collector.h"
#include "ubse_urma_controller_rpc.h"
#include "ubse_urma_controller_manager.h"

namespace ubse::urmaController {
extern UbseResult QueryAllPortsDown(bool &isAllPortDown);
extern bool IsUrmaDevActivated(const std::string &urmaName);
extern std::shared_ptr<UbseFeInfo> GetUrmaVfeFromEidGroup(EidGroup &eidGroup);
extern void SetUrmaInfoState(const std::string &urmaDevEid, bool isActive, const std::string &nodeId);
extern std::string GetUrmaDevEidByUrmaName(const std::string &urmaName);
extern bool IsUrmaBondingActivated(const std::string &urmaName);
extern UbseResult RecoverOneUrmaDeviceForOneNode(const std::string &nodeId, UbseUrmaUvsAggrDev &dev);
extern UbseResult FillUrmaDevByUvsInfo(const std::string &nodeId, UbseUrmaUvsAggrDev &dev);
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

class UbseUrmaActivateUrmaInfoReqSimpo : public ubse::message::UbseBaseMessage {
public:
    UbseUrmaActivateUrmaInfoReqSimpo() = default;
    explicit UbseUrmaActivateUrmaInfoReqSimpo(uint8_t *data, uint32_t size) { SetInputRawData(data, size); }
    void SetNodeId(const std::string &nodeId) { nodeId_ = nodeId; }
    void SetUrmaName(const std::string &urmaName) { urmaName_ = urmaName; }
    std::string GetNodeId() const { return nodeId_; }
    std::string GetUrmaName() const { return urmaName_; }
    UbseResult Serialize() override
    {
        ubse::serial::UbseSerialization out;
        out << nodeId_ << urmaName_;
        if (!out.Check()) {
            return UBSE_ERROR;
        }
        mOutputRawDataSize = out.GetLength();
        mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
        return UBSE_OK;
    }
    UbseResult Deserialize() override
    {
        if (mInputRawData == nullptr || mInputRawDataSize == 0) {
            return UBSE_ERROR;
        }
        ubse::serial::UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
        in >> nodeId_ >> urmaName_;
        if (!in.Check()) {
            return UBSE_ERROR;
        }
        return UBSE_OK;
    }
private:
    std::string nodeId_;
    std::string urmaName_;
};

class UbseUrmaActivateUrmaInfoRspSimpo : public ubse::message::UbseBaseMessage {
public:
    UbseUrmaActivateUrmaInfoRspSimpo() = default;
    explicit UbseUrmaActivateUrmaInfoRspSimpo(uint8_t *data, uint32_t size) { SetInputRawData(data, size); }
    UbseResult Serialize() override
    {
        ubse::serial::UbseSerialization out;
        out << mErrCode;
        if (!out.Check()) {
            return UBSE_ERROR;
        }
        mOutputRawDataSize = out.GetLength();
        mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
        return UBSE_OK;
    }
    UbseResult Deserialize() override
    {
        if (mInputRawData == nullptr || mInputRawDataSize == 0) {
            return UBSE_ERROR;
        }
        ubse::serial::UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
        in >> mErrCode;
        if (!in.Check()) {
            return UBSE_ERROR;
        }
        return UBSE_OK;
    }
};

class UbseUrmaActivateUrmaInfoMessageHandler : public ubse::com::UbseComBaseMessageHandler {
public:
    UbseResult Handle(const ubse::message::UbseBaseMessagePtr &req, const ubse::message::UbseBaseMessagePtr &rsp,
                      ubse::com::UbseComBaseMessageHandlerCtxPtr ctx) override
    {
        (void)ctx;
        if (ubse::context::g_globalStop) {
            return UBSE_OK;
        }
        if (req == nullptr || rsp == nullptr || req.Get() == nullptr || rsp.Get() == nullptr) {
            return UBSE_ERROR;
        }
        ubse::election::UbseRoleInfo curNodeInfo;
        if (auto ret = ubse::election::UbseGetCurrentNodeInfo(curNodeInfo); ret != UBSE_OK) {
            return UBSE_ERROR_AGAIN;
        }
        ubse::election::UbseRoleInfo masterInfo;
        if (auto ret = ubse::election::UbseGetMasterInfo(masterInfo); ret != UBSE_OK) {
            return UBSE_ERROR_AGAIN;
        }
        // Deserialize request
        auto reqSimpo = ubse::message::UbseBaseMessage::DeConvert<UbseUrmaActivateUrmaInfoReqSimpo>(req);
        if (reqSimpo == nullptr) {
            rsp->SetErrCode(static_cast<uint32_t>(UBSE_ERROR_NULLPTR));
            return UBSE_ERROR_NULLPTR;
        }
        // Local activation: target matches current node
        if (reqSimpo->GetNodeId() == curNodeInfo.nodeId) {
            auto ret = UrmaController::GetInstance().ActivateSpecifyUrmaDev(reqSimpo->GetUrmaName());
            rsp->SetErrCode(static_cast<uint32_t>(ret));
            return UBSE_OK;
        }
        // Only master forwards to other nodes
        if (curNodeInfo.nodeId != masterInfo.nodeId) {
            rsp->SetErrCode(static_cast<uint32_t>(UBSE_ERROR_INVAL));
            return UBSE_ERROR_INVAL;
        }
        auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
        if (comModule == nullptr) {
            rsp->SetErrCode(static_cast<uint32_t>(UBSE_ERROR_NULLPTR));
            return UBSE_ERROR_NULLPTR;
        }
        return ForwardActiveReqToSpecifyNode(reqSimpo->GetNodeId(), req, rsp);
    }
    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(ubse::com::UbseUrmaRpcOpCode::URMA_RPC_DEV_QUERY);
    }
    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(ubse::com::UbseModuleCode::UBSE_URMA);
    }
};
}

namespace ubse::nodeController {
inline bool operator==(const PhysicalLink &a, const PhysicalLink &b)
{
    return a.slotId == b.slotId && a.chipId == b.chipId && a.portId == b.portId &&
           a.interfaceName == b.interfaceName && a.peerSlotId == b.peerSlotId &&
           a.peerChipId == b.peerChipId && a.peerPortId == b.peerPortId &&
           a.peerInterfaceName == b.peerInterfaceName && a.linkStatus == b.linkStatus;
}
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
