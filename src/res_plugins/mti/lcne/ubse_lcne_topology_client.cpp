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

#include "ubse_lcne_topology_client.h"
#include <httplib.h>
#include "ubse_error.h"
#include "ubse_logger_inner.h"
#include "ubse_pointer_process.h"
#include "ubse_xml.h"

namespace ubse::lcne {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_LCNE_MID);
using namespace ubse::log;
using namespace ubse::utils;

uint32_t UbseLcneTopologyClient::GetTopology(std::vector<LcneNodeInfo> &lcneNodes)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;

    req.method = "GET";
    req.path = "/restconf/data/huawei-lingqu-topology:lingqu-topology/nodes";
    req.headers.emplace("Accept", "application/yang-data+xml");
    req.headers.emplace("Content-Type", "application/yang-data+xml");

    auto res = UbseHttpModule::HttpSend(host, port, req, rsp);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Access the LCNE topology information interface via HTTP is failed. "
                     << FormatRetCode(res);
        return res;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK)) {
        UBSE_LOG_ERROR << "[MTI] Access the LCNE topology information interface is failed.The HTTP status code is "
                     << rsp.status;
        return UBSE_ERROR;
    }
    if (rsp.body.empty()) {
        UBSE_LOG_ERROR << "[MTI] LCNE Topology response is empty.";
        return UBSE_ERROR;
    }

    // 解析xml响应
    res = ParseData(rsp.body, lcneNodes);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Topology Info Parse XML data is failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseLcneTopologyClient::ParseData(std::string &resBody, std::vector<LcneNodeInfo> &lcneNodes)
{
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(resBody);
    if (ubseXml == nullptr) {
        return UBSE_ERROR_NOMEM;
    }
    const auto ret = ubseXml->Parse();
    if (ret != UbseXmlError::OK || ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Topology resBody parse failed.";
        return UBSE_ERROR;
    }
    ubseXml = ubseXml->Next("nodes");
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Topology xml parse nodes failed.";
        return UBSE_ERROR;
    }
    size_t index = 0;
    while (ubseXml->Next("node", index) != nullptr) {
        LcneNodeInfo lcneNodeInfo{};
        lcneNodeInfo.slotId = ubseXml->Child("slot")->Text();
        lcneNodeInfo.chipId = ubseXml->Child("ubpu")->Text();
        lcneNodeInfo.cardId = ubseXml->Child("iou")->Text();
        lcneNodeInfo.type = ubseXml->Child("ubpu-type")->Text();
        ubseXml = ubseXml->Next("physical-ports");
        if (ubseXml == nullptr) {
            UBSE_LOG_ERROR << "[MTI] topology xml parse ports failed.";
            return UBSE_ERROR;
        }

        size_t innerIndex = 0;
        while (ubseXml->Next("physical-port", innerIndex) != nullptr) {
            LcnePortInfo lcnePortInfo{};
            lcnePortInfo.portId = ubseXml->Child("physical-port-id")->Text();
            lcnePortInfo.ifName = ubseXml->Child("interface-name")->Text();
            lcnePortInfo.portRole = ubseXml->Child("physical-port-role")->Text();
            lcnePortInfo.portStatus = ubseXml->Child("physical-port-status")->Text();
            lcnePortInfo.remoteSlotId = ubseXml->Child("remote-slot")->Text();
            lcnePortInfo.remoteChipId = ubseXml->Child("remote-ubpu")->Text();
            lcnePortInfo.remoteCardId = ubseXml->Child("remote-iou")->Text();
            lcnePortInfo.remotePortId = ubseXml->Child("remote-physical-port-id")->Text();
            lcneNodeInfo.ports.emplace_back(lcnePortInfo);
            ubseXml->Previous();
            innerIndex++;
        }
        index++;
        lcneNodes.emplace_back(lcneNodeInfo);
        ubseXml->Previous();
        ubseXml->Previous();
    }
    UBSE_LOG_INFO << GetLcneNodesString(lcneNodes);
    return UBSE_OK;
}

std::string UbseLcneTopologyClient::GetLcneNodeInfoString(const LcneNodeInfo &node)
{
    std::ostringstream oss;

    oss << "Slot ID=" << node.slotId << ", ";
    oss << "Chip ID=" << node.chipId << ", ";
    oss << "Card ID=" << node.cardId << ", ";
    oss << "Type=" << node.type;
    oss << "Ports Info=:" << "\n";
    for (const auto &port : node.ports) {
        oss << "  Port ID=" << port.portId << ", ";
        oss << "  Interface Name=" << port.ifName << ", ";
        oss << "  Port Role=" << port.portRole << ", ";
        oss << "  Port Status=" << port.portStatus << ", ";
        oss << "  Remote Slot ID=" << port.remoteSlotId << ", ";
        oss << "  Remote Chip ID=" << port.remoteChipId << ", ";
        oss << "  Remote Card ID=" << port.remoteCardId << ", ";
        oss << "  Remote Interface Name=" << port.remoteIfName;
        oss << "\n";
    }

    return oss.str();
}

std::string UbseLcneTopologyClient::GetLcneNodesString(const std::vector<LcneNodeInfo> &lcneNodes)
{
    std::ostringstream oss;
    oss << "[MTI] Topology information printing:" << "\n";
    for (size_t i = 0; i < lcneNodes.size(); ++i) {
        oss << (i + 1) << ": ";
        oss << GetLcneNodeInfoString(lcneNodes[i]);
        oss << "\n";
    }
    return oss.str();
}
} // namespace ubse::lcne