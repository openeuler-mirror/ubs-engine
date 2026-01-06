/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "ubse_node_controller_msg.h"
#include "ubse_error.h"
#include "ubse_event.h"
#include "ubse_node_controller.h"

namespace ubse::nodeController {
using namespace ubse::event;

void RegNodeControllerHandler()
{
    const ubse::com::UbseComEndpoint collectEndpoint = {static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER),
                                                        static_cast<uint32_t>(UbseOpCode::NODE_CONTROLLER_COLLECT)};
    const ubse::com::UbseComEndpoint allNodeEndpoint = {static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER),
                                                        static_cast<uint32_t>(UbseOpCode::NODE_CONTROLLER_ALL_NODE)};
    const ubse::com::UbseComEndpoint reportTopologyEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER),
        static_cast<uint32_t>(UbseOpCode::NODE_CONTROLLER_LCNE_CHANGE_REPORT_TOPOLOGY)};
    const ubse::com::UbseComEndpoint getDevConnect = {
        static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER),
        static_cast<uint32_t>(UbseOpCode::NODE_CONTROLLER_GET_DEV_CONNECT)};
    UbseRegRpcService(collectEndpoint, CollectNodeInfoHandler);
    UbseRegRpcService(allNodeEndpoint, GetAllNodeInfoFromRemoteHandler);
    UbseRegRpcService(reportTopologyEndpoint, ReportTopologyHandler);
    UbseRegRpcService(getDevConnect, UbseGetDirectConnectInfoFromRemoteHandler);
}

UbseResult ReportTopologyHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UbseNodeInfo info{};
    size_t size = 0;
    auto ret = DeSerializeUbseNode(info, req.data, req.len);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "deSerialize ubse node failed, " << FormatRetCode(ret);
        resp = {nullptr, 0, [size](uint8_t *p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return UBSE_ERROR_NULLPTR;
    }
    ret = UbseNodeController::GetInstance().UpdateNodeInfo(info.nodeId, info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "update ubse node failed, " << FormatRetCode(ret);
        resp = {nullptr, 0, [size](uint8_t *p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return UBSE_ERROR_NULLPTR;
    }
    UbseNodeController::GetInstance().UpdateDevDirConnectInfo(); // 更新链接
    resp = {nullptr, 0, [size](uint8_t *p) noexcept {
                SafeDeleteArray(p, size);
            }};
    return UBSE_OK;
}

UbseResult CollectNodeInfoHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UbseNodeInfo info = UbseNodeController::GetInstance().GetCurNode();
    uint8_t *buffer;
    size_t size = 0;
    auto ret = SerializeUbseNode(info, buffer, size);
    if (ret != UBSE_OK) {
        resp = {nullptr, 0, [size](uint8_t *p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return UBSE_ERROR_NULLPTR;
    }
    resp = {buffer, size, [size](uint8_t *p) noexcept {
                SafeDeleteArray(p, size);
            }};
    return ret;
}

UbseResult GetAllNodeInfoFromRemoteHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    auto nodeInfos = UbseNodeController::GetInstance().GetAllNodes();
    std::vector<UbseNodeInfo> infos{};
    for (auto iter : nodeInfos) {
        infos.push_back(iter.second);
    }
    uint8_t *buffer;
    size_t size = 0;
    auto ret = SerializeUbseNodeList(infos, buffer, size);
    if (ret != UBSE_OK) {
        resp = {nullptr, 0, [size](uint8_t *p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return UBSE_ERROR_NULLPTR;
    }
    resp = {buffer, size, [size](uint8_t *p) noexcept {
                SafeDeleteArray(p, size);
            }};
    return ret;
}

UbseResult UbseGetDirectConnectInfoFromRemoteHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOG_INFO << "UbseGetDirectConnectInfoFromRemoteHandler init";
    auto devDirConnectInfoRemote = UbseNodeController::GetInstance().UbseGetDirectConnectInfo();
    uint8_t *buffer;
    size_t size = 0;
    auto ret = SerializeDevDirConnectInfo(devDirConnectInfoRemote, buffer, size);
    if (ret != UBSE_OK) {
        resp = {nullptr, 0, [size](uint8_t *p) noexcept {
                    SafeDeleteArray(p, size);
            }};
        return UBSE_ERROR_NULLPTR;
    }
    resp = {buffer, size, [size](uint8_t *p) noexcept {
                SafeDeleteArray(p, size);
            }};
    return ret;
}

UbseResult ReportUbseNodeInfo(const std::string &nodeId, UbseNodeInfo info)
{
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(UbseOpCode::NODE_CONTROLLER_LCNE_CHANGE_REPORT_TOPOLOGY),
        .address = nodeId,
    };
    UbseResult reportRet = UBSE_OK;
    uint8_t *buffer;
    size_t size;
    auto ret = SerializeUbseNode(info, buffer, size);
    if (ret != UBSE_OK) {
        return ret;
    }
    UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t *p) noexcept {
                                SafeDeleteArray(p, size);
                            }};
    ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
                      [&reportRet, nodeId](void *ctx, const UbseByteBuffer &respData, uint32_t resCode) -> void {
                          if (resCode != UBSE_OK) {
                              UBSE_LOG_ERROR << "report node to nodeId=" << nodeId << " failed, "
                                             << FormatRetCode(resCode);
                            reportRet = resCode;
                            return;
                        }
                    });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send report nodeId=" << nodeId << " msg failed, " << FormatRetCode(ret);
        return ret;
    }
    return reportRet;
}

void CollectRemoteNodeInfoRespHandler(const std::string &nodeId, const UbseByteBuffer &respData, uint32_t resCode,
                                      UbseNodeInfo &info, UbseResult &collectRet)
{
    if (resCode != UBSE_OK) {
        UBSE_LOG_ERROR << "collect nodeId=" << nodeId << " node info failed, " << FormatRetCode(resCode);
        collectRet = resCode;
        return;
    }
    if (respData.data == nullptr || respData.len == 0) {
        UBSE_LOG_ERROR << "collect nodeId=" << nodeId << " resp null";
        collectRet = UBSE_ERROR_NULLPTR;
        return;
    }
    collectRet = DeSerializeUbseNode(info, respData.data, respData.len);
    if (collectRet != UBSE_OK) {
        UBSE_LOG_ERROR << "collect nodeId=" << nodeId << " deserialize failed, " << FormatRetCode(collectRet);
    }
}

UbseResult CollectRemoteNodeInfo(const std::string &nodeId, UbseNodeInfo &info)
{
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(UbseOpCode::NODE_CONTROLLER_COLLECT),
        .address = nodeId,
    };
    UbseResult collectRet = UBSE_OK;

    uint8_t *buffer;
    size_t size;
    auto ret = SerializeUbseNode(UbseNodeInfo{}, buffer, size);
    if (ret != UBSE_OK) {
        return ret;
    }
    UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t *p) noexcept {
                                SafeDeleteArray(p, size);
                            }};
    ret =
        UbseRpcSend(endpoint, reqBuffer, nullptr,
                    [&info, &collectRet, nodeId](void *ctx, const UbseByteBuffer &respData, uint32_t resCode) -> void {
                        CollectRemoteNodeInfoRespHandler(nodeId, respData, resCode, info, collectRet);
                    });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send collect nodeId=" << nodeId << " msg failed, " << FormatRetCode(ret);
        return ret;
    }
    return collectRet;
}

void GetAllNodeInfoFromRemoteRespHandler(const std::string &nodeId, const UbseByteBuffer &respData, uint32_t resCode,
                                         std::vector<UbseNodeInfo> &infos, UbseResult &getRet)
{
    if (resCode != UBSE_OK) {
        UBSE_LOG_ERROR << "get all node info failed, " << FormatRetCode(resCode);
        getRet = resCode;
        return;
    }
    if (respData.data == nullptr || respData.len == 0) {
        UBSE_LOG_ERROR << "get all node resp null";
        getRet = UBSE_ERROR_NULLPTR;
        return;
    }
    getRet = DeSerializeUbseNodeList(infos, respData.data, respData.len);
    if (getRet != UBSE_OK) {
        UBSE_LOG_ERROR << "get all node deserialize failed, " << FormatRetCode(getRet);
    }
}

UbseResult GetAllNodeInfoFromRemote(const std::string &nodeId, std::vector<UbseNodeInfo> &infos)
{
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(UbseOpCode::NODE_CONTROLLER_ALL_NODE),
        .address = nodeId,
    };
    UbseResult getRet = UBSE_OK;

    uint8_t *buffer;
    size_t size;
    auto ret = SerializeUbseNodeList(std::vector<UbseNodeInfo>{}, buffer, size);
    if (ret != UBSE_OK) {
        return ret;
    }
    UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t *p) noexcept {
                                SafeDeleteArray(p, size);
                            }};
    ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
                      [&infos, &getRet, nodeId](void *ctx, const UbseByteBuffer &respData, uint32_t resCode) -> void {
                          GetAllNodeInfoFromRemoteRespHandler(nodeId, respData, resCode, infos, getRet);
                      });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send get all node msg failed, " << FormatRetCode(ret);
        return ret;
    }
    return getRet;
}

UbseResult UbseGetDirectConnectInfoFromRemote(const std::string &nodeId,
    std::unordered_map<std::string, PhysicalLink> &devDirConnectInfoRemote)
{
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(UbseOpCode::NODE_CONTROLLER_GET_DEV_CONNECT),
        .address = nodeId,
    };
    UbseResult getRet = UBSE_OK;

    uint8_t *buffer = new uint8_t[1]; // com不允许空请求
    size_t size = 1;
    UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t *p) noexcept { SafeDeleteArray(p, size); } };
    auto ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
        [&devDirConnectInfoRemote, &getRet, nodeId](void *ctx, const UbseByteBuffer &respData,
        uint32_t resCode) -> void {
            if (resCode != UBSE_OK) {
                UBSE_LOG_ERROR << "get all node info failed, " << FormatRetCode(resCode);
                getRet = resCode;
                return;
            }
            if (respData.data == nullptr || respData.len == 0) {
                UBSE_LOG_ERROR << "get all node resp null";
                getRet = UBSE_ERROR_NULLPTR;
                return;
            }
            getRet = DeSerializeDevDirConnectInfo(devDirConnectInfoRemote, respData.data, respData.len);
            if (getRet != UBSE_OK) {
                UBSE_LOG_ERROR << "get devDirConnectInfo deserialize failed, " << FormatRetCode(getRet);
            }
        });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send get all node msg failed, " << FormatRetCode(ret);
        return ret;
    }
    return getRet;
}
} // namespace ubse::nodeController