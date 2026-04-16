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

 #include <cstdint>
#include "securec.h"
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_module.h"     // for UbseModule
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_module.h" // for UbseModule
#include "ubse_node_controller.h"
#include "ubse_smbios.h"
#include "ubse_str_util.h"
#include "ubse_urma_uvs_module.h"
#include "lock/ubse_lock.h"
#include "ubse_urma_uvs.h"

namespace ubse::urma {
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::log;
using namespace ubse::nodeController;
using namespace ubse::utils;
using namespace ubse::adapter_plugins::smbios;

UBSE_DEFINE_THIS_MODULE("ubse");

utils::ReadWriteLock g_invokeUrmaMutex;

UbseResult FillNodeComInfo(const std::vector<PhysicalLink> &allLinkInfo,
                           const std::vector<UbseUrmaUvsNodeInfo> &bondingInfo, std::vector<UbcoreTopoNode> &nodes);
UbseResult ConvertEidStrToHexCharList(const std::string &input, char outBytes[IPV6_BYTE_COUNT]);

UbseResult UbsePushTopoAndBondingToUvs(std::string &current_slot_id, const std::vector<PhysicalLink> &allLinkInfo,
                                       const std::vector<UbseUrmaUvsNodeInfo> &bondingInfo)
{
    UBSE_LOG_DEBUG << "Set Uvs Info";
    std::vector<UbcoreTopoNode> nodes;
    auto ret = FillNodeComInfo(allLinkInfo, bondingInfo, nodes);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "FillNodeComInfo failed";
        return ret;
    }
    for (auto& node : nodes) {
        if (std::to_string(node.id) == current_slot_id) {
            node.is_current = 1;
        }
    }
    auto module = UbseContext::GetInstance().GetModule<UbseUrmaUvsModule>();
    if (!module) {
        UBSE_LOG_ERROR << "Get UbseUrmaUvsModule failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    if (module->uvsSetTopoInfo == nullptr) {
        UBSE_LOG_ERROR << "Failed to find symbol 'uvs_set_topo_info'";
        return UBSE_ERROR_NULLPTR;
    }
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&g_invokeUrmaMutex);
    ret = module->uvsSetTopoInfo(nodes.data(), sizeof(UbcoreTopoNode), static_cast<uint32_t>(nodes.size()));
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Uvs failed to set topology information, ErrorCode=" << ret;
        return ret;
    }
    UBSE_LOG_INFO << "Set uvs Info success. node_size=" << nodes.size();
    return UBSE_OK;
}

UbseResult UbseGetUrmaSubpathByEid(const std::string& urmaEid, std::string& urmaSubpath)
{
    UBSE_LOG_DEBUG << "Get Name By UrmaEid, Eid =" << urmaEid;
    char bondingEid[IPV6_BYTE_COUNT];
    auto ret = ConvertEidStrToHexCharList(urmaEid, bondingEid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to parse bondingEid=" << urmaEid;
        return ret;
    }
    auto module = UbseContext::GetInstance().GetModule<UbseUrmaUvsModule>();
    if (!module) {
        UBSE_LOG_ERROR << "Get UbseUrmaUvsModule failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    char name[DEV_NAME_LEN];
    if (module->uvsGetDeviceNameByUrmaEid == nullptr) {
        UBSE_LOG_ERROR << "Failed to find symbol 'uvs_get_device_name_by_eid'";
        return UBSE_ERROR_NULLPTR;
    }
    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&g_invokeUrmaMutex);
    ret = module->uvsGetDeviceNameByUrmaEid(bondingEid, name, DEV_NAME_LEN);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Uvs failed to get device name";
        return ret;
    }
    urmaSubpath = name;
    return UBSE_OK;
}

UbseResult UbseGetBondingActiveStateByEid(const std::string& urmaEid, bool& isActive)
{
    UBSE_LOG_DEBUG << "Get State By UrmaEid, Eid =" << urmaEid;
    char bondingEid[IPV6_BYTE_COUNT];
    auto ret = ConvertEidStrToHexCharList(urmaEid, bondingEid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to parse bondingEid=" << urmaEid;
        return ret;
    }
    auto module = UbseContext::GetInstance().GetModule<UbseUrmaUvsModule>();
    if (!module) {
        UBSE_LOG_ERROR << "Get UbseUrmaUvsModule failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    char name[DEV_NAME_LEN];
    if (module->uvsGetDeviceNameByUrmaEid == nullptr) {
        UBSE_LOG_ERROR << "Failed to find symbol 'uvs_get_device_name_by_eid'";
        return UBSE_ERROR_NULLPTR;
    }
    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&g_invokeUrmaMutex);
    ret = module->uvsGetDeviceNameByUrmaEid(bondingEid, name, DEV_NAME_LEN);
    if (UBSE_RESULT_FAIL(ret)) {
        isActive = false;
    } else {
        isActive = true;
    }
    return UBSE_OK;
}

UbseResult UbseActiveBonding(const std::string& urmaEid, const std::string& aggrDevName)
{
    UBSE_LOG_DEBUG << "Activate Bonding Device, Eid =" << urmaEid << ", aggrDevName=" << aggrDevName;
    if (aggrDevName.empty() || aggrDevName.size() >= AGGR_DEV_NAME_LEN) {
        UBSE_LOG_ERROR << "aggrDevName is empty or too long";
        return UBSE_ERROR_INVAL;
    }
    bool isActivate = false;
    if (UbseGetBondingActiveStateByEid(urmaEid, isActivate) == UBSE_OK && isActivate) {
        UBSE_LOG_WARN << "UrmaEid=" << urmaEid << " is already active, skipping.";
        return UBSE_OK;
    }
    char bondingEid[IPV6_BYTE_COUNT];
    auto ret = ConvertEidStrToHexCharList(urmaEid, bondingEid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to parse bondingEid=" << urmaEid;
        return ret;
    }
    auto module = UbseContext::GetInstance().GetModule<UbseUrmaUvsModule>();
    if (!module) {
        UBSE_LOG_ERROR << "Get UbseUrmaUvsModule failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    if (module->uvsCreateAggrDev == nullptr) {
        UBSE_LOG_ERROR << "Failed to find symbol 'uvs_create_agg_dev'";
        return UBSE_ERROR_NULLPTR;
    }
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&g_invokeUrmaMutex);
    ret = module->uvsCreateAggrDev(bondingEid, aggrDevName.c_str());
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Uvs failed to activate bonding device, ErrorCode=" << ret;
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseDeactiveBonding(const std::string& urmaEid)
{
    UBSE_LOG_DEBUG << "Deactivate Bonding Device, Eid =" << urmaEid;
    char bondingEid[IPV6_BYTE_COUNT];
    auto ret = ConvertEidStrToHexCharList(urmaEid, bondingEid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to parse bondingEid=" << urmaEid;
        return ret;
    }
    auto module = UbseContext::GetInstance().GetModule<UbseUrmaUvsModule>();
    if (!module) {
        UBSE_LOG_ERROR << "Get UbseUrmaUvsModule failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    if (module->uvsDeleteAggrDev == nullptr) {
        UBSE_LOG_ERROR << "Failed to find symbol 'uvs_delete_agg_dev'";
        return UBSE_ERROR_NULLPTR;
    }
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&g_invokeUrmaMutex);
    ret = module->uvsDeleteAggrDev(bondingEid);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Uvs failed to deactivate bonding device, ErrorCode=" << ret;
        return ret;
    }
    return UBSE_OK;
}

UbseResult GetSlotIds(const std::vector<UbseUrmaUvsNodeInfo>& bondingInfo, std::set<std::string>& slotIds)
{
    if (bondingInfo.empty()) {
        return UBSE_ERROR;
    }
    for (auto& info : bondingInfo) {
        slotIds.insert(info.nodeId);
    }
    std::ostringstream oss;
    oss << "Found " << slotIds.size() << " slots, includes: ";
    for (auto& id : slotIds) {
        oss << id << " ";
    }
    UBSE_LOG_DEBUG << oss.str();
    return UBSE_OK;
}

UbseResult FillTopo(const std::vector<PhysicalLink>& allLinkInfo,
                    std::unordered_map<std::string, UbcoreTopoNode>& nodeMap)
{
    if (allLinkInfo.empty()) {
        UBSE_LOG_INFO << "No link info found";
        return UBSE_OK;
    }
    for (const auto& topo : allLinkInfo) {
        std::string curSlotId = std::to_string(topo.slotId);
        std::string peerSlotId = std::to_string(topo.peerSlotId);

        uint32_t iodie_idx = topo.chipId - 1;
        uint32_t peer_iodie_idx = topo.peerChipId - 1;
        if (iodie_idx >= IODIE_NUM || topo.portId >= PORT_NUM || peer_iodie_idx >= IODIE_NUM ||
            topo.peerPortId >= PORT_NUM) {
            UBSE_LOG_ERROR << std::to_string(topo.portId) << " or " << std::to_string(topo.peerPortId);
            UBSE_LOG_ERROR << std::to_string(iodie_idx) << " or " << std::to_string(peer_iodie_idx);
            return UBSE_ERROR;
        }

        if (nodeMap.find(curSlotId) != nodeMap.end()) {
            nodeMap[curSlotId].link[iodie_idx][topo.portId].peer_node = topo.peerSlotId;
            nodeMap[curSlotId].link[iodie_idx][topo.portId].peer_iodie = peer_iodie_idx;
            nodeMap[curSlotId].link[iodie_idx][topo.portId].peer_port = topo.peerPortId;
        } else {
            UBSE_LOG_WARN << "Failed to find slotId " << curSlotId << " in nodes, skip fill this node";
        }

        if (nodeMap.find(peerSlotId) != nodeMap.end()) {
            nodeMap[peerSlotId].link[peer_iodie_idx][topo.peerPortId].peer_node = topo.slotId;
            nodeMap[peerSlotId].link[peer_iodie_idx][topo.peerPortId].peer_iodie = iodie_idx;
            nodeMap[peerSlotId].link[peer_iodie_idx][topo.peerPortId].peer_port = topo.portId;
        } else {
            UBSE_LOG_WARN << "Failed to find peerSlotId " << peerSlotId << " in nodes, skip fill this node";
        }
    }
    return UBSE_OK;
}

UbseResult FillFeInfo(const std::vector<UbseUrmaUvsFe>& fes, UbcoreTopoAggrDev& aggr_dev)
{
    auto fe_num = fes.size();
    if (fe_num == 0) {
        UBSE_LOG_ERROR << "No fe info found";
        return UBSE_ERROR;
    }
    if (fe_num > IODIE_NUM) {
        UBSE_LOG_ERROR << "Too many fe in one aggr_device";
        return UBSE_ERROR;
    }
    for (size_t i = 0; i < fe_num; i++) {
        auto ret = ConvertStrToUint32(fes[i].ubpuId, aggr_dev.fe[i].chip_id);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Convert ubpuId failed, " << FormatRetCode(ret);
            return ret;
        }
        aggr_dev.fe[i].die_id = 1;
        ret = ConvertStrToUint32(fes[i].entityId, aggr_dev.fe[i].entity_id);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Convert entityId failed, " << FormatRetCode(ret);
            return ret;
        }
        ret = ConvertEidStrToHexCharList(fes[i].primaryEid, aggr_dev.fe[i].primary_eid);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to parse primaryEid=" << fes[i].primaryEid;
            return ret;
        }
        for (auto& port : fes[i].portEid) {
            uint32_t portId;
            ret = ConvertStrToUint32(port.first, portId);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Convert portId failed, " << FormatRetCode(ret);
                return ret;
            }
            if (portId >= PORT_NUM) {
                UBSE_LOG_ERROR << "Port id exceeded";
                return UBSE_ERROR;
            }
            ret = ConvertEidStrToHexCharList(port.second, aggr_dev.fe[i].port_eid[portId]);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to parse portEid=" << port.second;
                return ret;
            }
        }
    }
    return UBSE_OK;
}

UbseResult FillBondingInfo(const std::vector<UbseUrmaUvsNodeInfo>& bondingInfo,
                           std::unordered_map<std::string, UbcoreTopoNode>& nodeMap)
{
    if (bondingInfo.empty()) {
        UBSE_LOG_ERROR << "No bonding info found";
        return UBSE_ERROR;
    }

    for (auto& info : bondingInfo) {
        uint32_t bondingDevSize = static_cast<uint32_t>(info.devList.size());
        if (bondingDevSize > DEV_NUM) {
            UBSE_LOG_ERROR << "aggr_device num exceeded";
            return UBSE_ERROR;
        }

        for (size_t i = 0; i < bondingDevSize; i++) {
            auto ret = ConvertEidStrToHexCharList(info.devList[i].urmaDevEid,
                                                  nodeMap[info.nodeId].aggr_dev[i].aggr_eid);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to parse bondingEid=" << info.devList[i].urmaDevEid;
                return ret;
            }
            ret = FillFeInfo(info.devList[i].feList, nodeMap[info.nodeId].aggr_dev[i]);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to fill fe info for aggr_device.";
                return ret;
            }
        }
    }
    return UBSE_OK;
}

void InitialNodes(const std::set<std::string>& slotIds, std::unordered_map<std::string, UbcoreTopoNode>& nodeMap)
{
    nodeMap.clear();
    for (auto& id : slotIds) {
        UbcoreTopoNode node{};
        auto ret = ConvertStrToUint32(id, node.id);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to convert " << id << " to uint32";
        }
        node.is_current = 0; // default not current node
        node.type = 0;       // default full mesh type

        for (uint32_t i_iodie = 0; i_iodie < IODIE_NUM; i_iodie++) {
            for (uint32_t j_port = 0; j_port < PORT_NUM; j_port++) {
                node.link[i_iodie][j_port].peer_node = UINT32_MAX;
                node.link[i_iodie][j_port].peer_iodie = UINT32_MAX;
                node.link[i_iodie][j_port].peer_port = UINT32_MAX;
            }
        }
        nodeMap[id] = std::move(node);
    }
}

UbseResult FillClusterInfo(std::unordered_map<std::string, UbcoreTopoNode>& nodeMap)
{
    uint16_t superNodeId = 0;
    if (auto ret = UbseSmbios::GetInstance().GetSuperPodId(superNodeId); ret != UBSE_OK) {
        UBSE_LOG_WARN << "get bios data mesh_type failed, ret: " << FormatRetCode(ret);
    }

    for (auto& pair : nodeMap) {
        nodeMap[pair.first].super_node_id = superNodeId;
        nodeMap[pair.first].type = UbseSmbios::GetInstance().IsClosType() ?  1 : 0;
    }
    return UBSE_OK;
}

UbseResult FillNodeComInfo(const std::vector<PhysicalLink>& allLinkInfo,
                           const std::vector<UbseUrmaUvsNodeInfo>& bondingInfo, std::vector<UbcoreTopoNode>& nodes)
{
    nodes.clear();
    std::set<std::string> slotIds;
    auto ret = GetSlotIds(bondingInfo, slotIds);
    if (ret != UBSE_OK || slotIds.empty()) {
        UBSE_LOG_ERROR << "Failed to get slotIds";
        return UBSE_ERROR;
    }

    std::unordered_map<std::string, UbcoreTopoNode> nodeMap;
    InitialNodes(slotIds, nodeMap);
    ret = FillClusterInfo(nodeMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to fill cluster info";
        return ret;
    }
    ret = FillTopo(allLinkInfo, nodeMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to fill topo";
        return ret;
    }
    ret = FillBondingInfo(bondingInfo, nodeMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to fill bondingInfo";
        return ret;
    }
    UBSE_LOG_INFO << "Found " << slotIds.size() << " nodes, and successfully filled uvs data";

    nodes.reserve(nodeMap.size());
    for (auto& pair : nodeMap) {
        nodes.push_back(pair.second);
    }
    return UBSE_OK;
}

UbseResult ConvertEidStrToHexCharList(const std::string& input, char outBytes[IPV6_BYTE_COUNT])
{
    // input 表示Eid，是长度为40的字符串.其格式为 4245:4944:0000:0000:0000:0000:0100:0000
    if (input.size() != IPV6_FULL_FORMAT_LENGTH) {
        return UBSE_ERROR;
    }

    // 将 char* 转换为 unsigned char* 以匹配 sscanf 的格式要求
    auto* uOut = reinterpret_cast<unsigned char*>(outBytes); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

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
    // outBytes经过转换后，是长度为16的char数组.
    // 其格式为[0x42, 0x45, 0x49, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00]
    return scanned == IPV6_BYTE_COUNT ? UBSE_OK : UBSE_ERROR;
}
} // namespace ubse::urma