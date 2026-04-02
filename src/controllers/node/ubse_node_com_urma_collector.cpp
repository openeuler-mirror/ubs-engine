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

#include "ubse_node_com_urma_collector.h"
#include <bitset>
#include <vector>
#include "securec.h"
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_lcne_module.h"
#include "ubse_logger_module.h"
#include "ubse_node_controller.h"
#include "ubse_smbios.h"
#include "ubse_str_util.h"
#include "ubse_urma_uvs_module.h"

namespace ubse::nodeController {
using namespace ubse::utils;
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::adapter_plugins::mti;
using namespace ubse::adapter_plugins::smbios;
using namespace ubse::urma;
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseNodeComUrmaCollector::FillComUrmaInfo()
{
    comUrmaInfos.clear();
    auto lcneModule = UbseContext::GetInstance().GetModule<mti::UbseLcneModule>();
    if (lcneModule == nullptr) {
        UBSE_LOG_ERROR << "Get lcne module failed. ";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    std::vector<mti::MtiNodeInfo> ubseNodeInfos{};
    auto ret = lcneModule->UbseGetAllNodeInfos(ubseNodeInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseGetAllNodeInfos failed, ret=" << FormatRetCode(ret);
        return ret;
    }
    for (const auto &ubseNodeInfo : ubseNodeInfos) {
        comUrmaInfos[ubseNodeInfo.nodeId].urmaDevEid = ubseNodeInfo.eid;
    }

    auto allSocketComEid = lcneModule->GetAllSocketComEid();
    for (const auto &socketComEid : allSocketComEid) {
        std::string nodeId;
        std::string ubpuId;
        socketComEid.first.SplitDevName(nodeId, ubpuId);

        UbseUrmaUvsFe fe{};
        fe.ubpuId = ubpuId;
        fe.primaryEid = socketComEid.second.primaryEid;
        fe.entityId = socketComEid.second.entityId;
        for (auto &port : socketComEid.second.portEidList) {
            fe.portEid[port.first] = port.second;
        }
        comUrmaInfos[nodeId].feList.push_back(fe);
    }
    return UBSE_OK;
}

UbseResult UbseNodeComUrmaCollector::FillComUrmaInfoClos()
{
    auto ret = FillComUrmaInfo();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "FillComUrmaInfo failed, ret=" << FormatRetCode(ret);
        return ret;
    }
    std::string curNodeId = UbseNodeController::GetInstance().GetCurrentNodeId();
    uint32_t podsSize = NO_8;
    uint32_t clusterNodeSize = NO_8;
    for (uint32_t podId = 0; podId < podsSize; podId++) {
        for (uint32_t slotId = 0; slotId < clusterNodeSize; slotId++) {
            ret = ProcessClusterNode(curNodeId, podId, slotId);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Process ClusterNode failed, podId=" << podId << ", slotId="
                               << slotId << ", ret=" << ret;
                return ret;
            }
        }
    }
    return UBSE_OK;
}

UbseResult UbseNodeComUrmaCollector::ProcessClusterNode(const std::string& curNodeId, uint32_t podId, uint32_t slotId)
{
    uint32_t clusterNodeSize = NO_8;
    uint32_t nodeId = podId * clusterNodeSize + slotId + 1;
    std::string targetNodeId = std::to_string(nodeId);
    if (targetNodeId == curNodeId) {
        return UBSE_OK; // 当前节点跳过处理
    }

    UbseUrmaUvsAggrDev aggr_dev{};
    auto ret = GenerateBondingEid(0x44494542, 0, 0, nodeId, aggr_dev.urmaDevEid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Generate bondingEid failed, nodeId=" << nodeId << ", ret=" << ret;
        return ret;
    }

    // 处理当前节点的FE列表，生成聚合设备的FE信息
    for (auto& fe : comUrmaInfos[curNodeId].feList) {
        UbseUrmaUvsFe fe_aggr_dev{};
        ret = ProcessFeDevice(podId, slotId, fe, fe_aggr_dev);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "ProcessFeDevice failed, podId=" << podId << ", slotId=" << slotId << ", ret=" << ret;
            return ret;
        }
        aggr_dev.feList.push_back(fe_aggr_dev);
    }

    comUrmaInfos[targetNodeId] = aggr_dev;
    return UBSE_OK;
}

UbseResult UbseNodeComUrmaCollector::ProcessFeDevice(uint32_t podId, uint32_t slotId,
                                                     const UbseUrmaUvsFe& srcFe, UbseUrmaUvsFe& destFe)
{
    destFe.ubpuId = srcFe.ubpuId;
    destFe.entityId = srcFe.entityId;

    // 处理primaryEID
    auto ret = OverwriteEid(podId, slotId, srcFe.primaryEid, destFe.primaryEid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Overwrite primaryEid failed, podId=" << podId << ", slotId=" << slotId << ", ret=" << ret;
        return ret;
    }

    // 处理portEID列表
    for (const auto& port : srcFe.portEid) {
        ret = OverwriteEid(podId, slotId, port.second, destFe.portEid[port.first]);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Overwrite portEid failed, podId=" << podId << ", slotId=" << slotId
                           << ", port=" << port.first << ", ret=" << ret;
            return ret;
        }
    }

    return UBSE_OK;
}

UbseResult GenerateBondingEid(const uint32_t id0, const uint32_t id1, const uint32_t id2,
                              const uint32_t id3, std::string &devEid)
{
    unsigned char bondingEid[IPV6_BYTE_COUNT];
    uint32_t copyCnt = 0;
    // [id0, id1, id2, id3]，每个字段占32位
    auto ret = memcpy_s(bondingEid + IPV6_SEGMENT_LENGTH * copyCnt++, IPV6_SEGMENT_LENGTH, &id0, IPV6_SEGMENT_LENGTH);
    ret |= memcpy_s(bondingEid + IPV6_SEGMENT_LENGTH * copyCnt++, IPV6_SEGMENT_LENGTH, &id1, IPV6_SEGMENT_LENGTH);
    ret |= memcpy_s(bondingEid + IPV6_SEGMENT_LENGTH * copyCnt++, IPV6_SEGMENT_LENGTH, &id2, IPV6_SEGMENT_LENGTH);
    ret |= memcpy_s(bondingEid + IPV6_SEGMENT_LENGTH * copyCnt++, IPV6_SEGMENT_LENGTH, &id3, IPV6_SEGMENT_LENGTH);
    if (ret != EOK) {
        UBSE_LOG_ERROR << "Failed to generate bonding eid, ret=" << ret;
        return UBSE_ERROR;
    }
    const uint32_t fullFormatLength = ubse::urma::IPV6_FULL_FORMAT_LENGTH;
    std::array<char, fullFormatLength + 1> buffer{};
    // 格式化EID地址，每16位(2字节)为一组，共16组，组间用冒号分隔
    int res = snprintf_s(buffer.data(), buffer.size(), fullFormatLength,
                         "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                         bondingEid[NO_0], bondingEid[NO_1], bondingEid[NO_2], bondingEid[NO_3],
                         bondingEid[NO_4], bondingEid[NO_5], bondingEid[NO_6], bondingEid[NO_7],
                         bondingEid[NO_8], bondingEid[NO_9], bondingEid[NO_10], bondingEid[NO_11],
                         bondingEid[NO_12], bondingEid[NO_13], bondingEid[NO_14], bondingEid[NO_15]);
    if (res < 0) {
        UBSE_LOG_ERROR << "Failed to convert bytes to bonding eid string";
        return UBSE_ERROR;
    }
    devEid = buffer.data();
    return UBSE_OK;
}

UbseResult UbseNodeComUrmaCollector::SetComUrma(std::vector<PhysicalLink> &allLinkInfo, bool isBeforeElection)
{
    UbseNodeInfo ubseNodeInfo = UbseNodeController::GetInstance().GetCurNode();
    if (ubseNodeInfo.nodeId.empty()) {
        UBSE_LOG_ERROR << "Current node id is empty.";
        return UBSE_ERROR;
    }

    std::vector<UbseUrmaUvsNodeInfo> hostUrmaInfos;
    auto ret = GetAllComUrma(hostUrmaInfos);
    if (ret != UBSE_OK || hostUrmaInfos.empty()) {
        UBSE_LOG_ERROR << "Get all com urma info failed.";
        return UBSE_ERROR;
    }

    ret = UbsePushTopoAndBondingToUvs(ubseNodeInfo.nodeId, allLinkInfo, hostUrmaInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Set urma_uvs failed.";
        return ret;
    }

    if (isBeforeElection) {
        ret = UbseActiveBonding(comUrmaInfos[ubseNodeInfo.nodeId].urmaDevEid);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Activate urmaDevEid=" << comUrmaInfos[ubseNodeInfo.nodeId].urmaDevEid << " failed.";
        }
    }
    return ret;
}

UbseResult UbseNodeComUrmaCollector::GetAllComUrma(std::vector<UbseUrmaUvsNodeInfo> &hostUrmaInfos)
{
    hostUrmaInfos.clear();
    hostUrmaInfos.reserve(comUrmaInfos.size());
    for (const auto &kv : comUrmaInfos) {
        std::vector<UbseUrmaUvsAggrDev> aggrs;
        aggrs.push_back(kv.second);
        UbseUrmaUvsNodeInfo info{kv.first, aggrs};
        hostUrmaInfos.push_back(info);
    }
    return UBSE_OK;
}

UbseResult UbseNodeComUrmaCollector::GetCurNodeTopo(std::vector<PhysicalLink> &allLinkInfo)
{
    auto lcneModule = UbseContext::GetInstance().GetModule<mti::UbseLcneModule>();
    if (lcneModule == nullptr) {
        UBSE_LOG_ERROR << "Get lcne module failed. ";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    UbseDevTopology devTopology{};
    if (auto ret = lcneModule->UbseGetDevTopology(devTopology); ret != UBSE_OK) {
        UBSE_LOG_WARN << "get topology info not successful, ret: " << FormatRetCode(ret);
        return ret;
    }
    for (const auto &kv : devTopology) {
        std::string nodeId;
        std::string ubpuId;
        kv.first.SplitDevName(nodeId, ubpuId);
        for (const auto &portKv : kv.second.second) {
            if (portKv.second.portStatus == UbseMtiCpuTopoPortStatus::DOWN) {
                continue;
            }
            PhysicalLink link{};
            if (ConvertStrToUint32(nodeId, link.slotId) != UBSE_OK || ConvertStrToUint32(ubpuId, link.chipId) != UBSE_OK
                || ConvertStrToUint32(portKv.second.portId, link.portId) != UBSE_OK) {
                UBSE_LOG_WARN << "Failed to convert nodeId=" << nodeId << ", ubpuId=" << ubpuId
                              << ", portId=" << portKv.second.portId << ", skip this link";
                continue;
            }
            if (!UbseSmbios::GetInstance().IsClosType()) {
                if (ConvertStrToUint32(portKv.second.remoteSlotId, link.peerSlotId) != UBSE_OK ||
                    ConvertStrToUint32(portKv.second.remoteChipId, link.peerChipId) != UBSE_OK ||
                    ConvertStrToUint32(portKv.second.remotePortId, link.peerPortId) != UBSE_OK) {
                    UBSE_LOG_WARN << "Failed to convert nodeId=" << portKv.second.remoteSlotId << ", ubpuId="
                        << portKv.second.remoteChipId << ", portId=" << portKv.second.remotePortId
                        << ", skip this link";
                    continue;
                }
            }
            link.linkStatus = LinkStatus::init;
            allLinkInfo.push_back(link);
        }
    }
    return UBSE_OK;
}

UbseResult UbseNodeComUrmaCollector::GetCurNodeIouList(std::vector<UbseMtiIouInfo> &iouList)
{
    auto lcneModule = UbseContext::GetInstance().GetModule<mti::UbseLcneModule>();
    if (lcneModule == nullptr) {
        UBSE_LOG_ERROR << "Get lcne module failed. ";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    UbseDevTopology devTopology{};
    auto ret = lcneModule->UbseGetDevTopology(devTopology);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "get topology info not successful, ret: " << FormatRetCode(ret);
        return ret;
    }
    iouList.clear();
    iouList.reserve(devTopology.size());

    for (const auto &kv : devTopology) {
        const auto& deviceInfo = kv.second.first;
        iouList.push_back(UbseMtiIouInfo{
            .slotId = deviceInfo.slotId,
            .ubpuId = deviceInfo.chipId,
            .iouId  = deviceInfo.cardId
        });
    }
    return UBSE_OK;
}

UbseResult ParseBaseEid(const std::string &baseEid, std::string &bitStr)
{
    bitStr.clear();
    std::stringstream ss(baseEid);
    std::vector<std::string> segments;
    std::string segment;
    while (std::getline(ss, segment, ':')) {
        segments.push_back(segment);
    }

    std::vector<uint16_t> values;
    for (const auto &seg : segments) {
        try {
            values.push_back(static_cast<uint16_t>(std::stoul(seg, nullptr, NO_16)));
        } catch (const std::invalid_argument &e) {
            UBSE_LOG_ERROR << "Failed to convert segment to uint16_t " << seg;
            return UBSE_ERROR;
        }
    }

    for (auto value : values) {
        std::bitset<NO_16> bits(value);
        bitStr += bits.to_string();
    }
    return UBSE_OK;
}

void ConstructEid(const std::string &bitStr, std::string &eid)
{
    // eid 4245:4944:0000:0000:0000:0000:0100:0000 格式，bits字符长128位
    eid.clear();
    for (size_t i = 0; i < bitStr.size(); i += NO_16) {
        if (i > 0) {
            eid += ":";
        }
        std::string bitChunk = bitStr.substr(i, NO_16);
        std::bitset<NO_16> bits(bitChunk);
        uint16_t value = bits.to_ulong();
        char hexStr[10];
        auto res = snprintf_s(hexStr, sizeof(hexStr), sizeof(hexStr) - 1, "%04X", value);
        if (res == -1) {
            return;
        }
        eid += hexStr;
    }
}


UbseResult OverwriteEid(const uint32_t podId, const uint32_t serverId, const std::string &baseEid, std::string &result)
{
    uint32_t n = 104; // 从第105位开始，8位podId，5位serverId
    uint8_t podBitSize = NO_8;
    uint8_t serverBitSize = NO_5;
    
    std::string bitStr;
    auto ret = ParseBaseEid(baseEid, bitStr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "ParseBaseEid failed, baseEid=" << baseEid << ", ret=" << FormatRetCode(ret);
        return ret;
    }

    if (n + podBitSize + serverBitSize > bitStr.size()) {
        UBSE_LOG_ERROR << "OverwriteEid failed, baseEid=" << baseEid << ", podId=" << podId
                       << ", serverId=" << serverId << ", ret=" << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    std::bitset<NO_32> podIdBits(podId);
    std::bitset<NO_32> serverIdBits(serverId);
    
    std::string podIdBitStr = podIdBits.to_string().substr(NO_32 - podBitSize);
    std::string serverIdBitStr = serverIdBits.to_string().substr(NO_32 - serverBitSize);

    std::string eidBitStr = bitStr.substr(0, n) + podIdBitStr + serverIdBitStr +
                            bitStr.substr(n + podBitSize + serverBitSize);
    ConstructEid(eidBitStr, result);
    return UBSE_OK;
}
} // namespace ubse::nodeController