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

#include "ubse_urma_uvs_module.h"

#include <dlfcn.h>
#include "securec.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_logger_inner.h"
#include "ubse_str_util.h"

namespace ubse::urma {
using namespace ubse::log;
using namespace ubse::common::def;
using namespace ubse::utils;

DYNAMIC_CREATE(UbseUrmaUvsModule);

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_URMA_UVS_MID)

bool g_isMocked = true;

UbseResult UbseUrmaUvsModule::Initialize()
{
    Cleanup();
    handle = dlopen("/usr/lib64/libtpsa.so", RTLD_LAZY);
    if (handle == nullptr) {
        UBSE_LOG_ERROR << "dlopen libtpsa.so failed";
        return UBSE_ERROR_NOENT;
    }

    uvsSetTopoInfo = (UvsSetTopoInfo)dlsym(handle, "uvs_set_topo_info");
    if (uvsSetTopoInfo == nullptr) {
        UBSE_LOG_WARN << "Failed to find symbol 'uvs_set_topo_info'";
    }

    uvsGetDeviceNameByUrmaEid = (UvsGetDeviceNameByUrmaEid)dlsym(handle, "uvs_get_device_name_by_eid");
    if (uvsGetDeviceNameByUrmaEid == nullptr) {
        UBSE_LOG_WARN << "Failed to find symbol 'uvs_get_device_name_by_eid'";
    }

    uvsCreateAggrDev = (UvsCreateAggrDev)dlsym(handle, "uvs_create_agg_dev");
    if (uvsCreateAggrDev == nullptr) {
        UBSE_LOG_WARN << "Failed to find symbol 'uvs_create_agg_dev'";
    }

    uvsDeleteAggrDev = (UvsDeleteAggrDev)dlsym(handle, "uvs_delete_agg_dev");
    if (uvsDeleteAggrDev == nullptr) {
        UBSE_LOG_WARN << "Failed to find symbol 'uvs_delete_agg_dev'";
    }

    if (uvsSetTopoInfo == nullptr || uvsGetDeviceNameByUrmaEid == nullptr || uvsCreateAggrDev == nullptr ||
        uvsDeleteAggrDev == nullptr) {
        UBSE_LOG_WARN << "Failed to find symbol in libtpsa.so";
    }
    return UBSE_OK;
}

void UbseUrmaUvsModule::UnInitialize()
{
    Cleanup();
}

UbseResult UbseUrmaUvsModule::Start()
{
    return UBSE_OK;
}

void UbseUrmaUvsModule::Stop() {}

void UbseUrmaUvsModule::Cleanup()
{
    if (handle != nullptr) {
        dlclose(handle);
        handle = nullptr;
        uvsSetTopoInfo = nullptr;
        uvsGetDeviceNameByUrmaEid = nullptr;
        uvsCreateAggrDev = nullptr;
        uvsDeleteAggrDev = nullptr;
    }
}

UbseResult UbseUrmaUvsModule::SetUvsInfo(std::string &current_slot_id, const std::vector<PhysicalLink> &allLinkInfo,
                                         const std::vector<UbseUrmaUvsNodeInfo> &bondingInfo)
{
    UBSE_LOG_DEBUG << "Set Uvs Info";
    std::vector<UbcoreTopoNode> nodes;
    auto ret = FillNodeComInfo(allLinkInfo, bondingInfo, nodes);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "FillNodeComInfo failed";
        return ret;
    }
    for (auto &node : nodes) {
        if (std::to_string(node.id) == current_slot_id) {
            node.is_current = true;
        }
    }
    if (g_isMocked) {
        return UBSE_OK;
    }
    if (uvsSetTopoInfo == nullptr) {
        UBSE_LOG_ERROR << "Failed to find symbol 'uvs_set_topo_info'";
        return UBSE_ERROR_NOENT;
    }
    ret = uvsSetTopoInfo(nodes.data(), nodes.size());
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Uvs failed to set topology information, ErrorCode=" << ret;
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaUvsModule::GetNameByUrmaEid(const std::string &urmaEid, std::string &urmaEidName)
{
    UBSE_LOG_DEBUG << "Get Name By UrmaEid";
    char bondingEid[IPV6_BYTE_COUNT];
    auto ret = ParseColonHexString(urmaEid, bondingEid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to parse bondingEid=" << urmaEid;
        return ret;
    }
    char name[DEV_NAME_LEN];
    if (g_isMocked) {
        char lastChar = urmaEid.back();
        urmaEidName = "mockname" + std::string(1, lastChar);
        return UBSE_OK;
    }
    if (uvsGetDeviceNameByUrmaEid == nullptr) {
        UBSE_LOG_ERROR << "Failed to find symbol 'uvs_get_device_name_by_eid'";
        return UBSE_ERROR_NOENT;
    }
    ret = uvsGetDeviceNameByUrmaEid(bondingEid, name, DEV_NAME_LEN);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Uvs failed to get device name";
        return ret;
    }
    urmaEidName = name;
    return UBSE_OK;
}

UbseResult UbseUrmaUvsModule::GetStateByUrmaEid(const std::string &urmaEid, bool isactivate)
{
    UBSE_LOG_DEBUG << "Get State By UrmaEid";
    char bondingEid[IPV6_BYTE_COUNT];
    auto ret = ParseColonHexString(urmaEid, bondingEid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to parse bondingEid=" << urmaEid;
        return ret;
    }
    char name[DEV_NAME_LEN];
    if (g_isMocked) {
        isactivate = true;
        return UBSE_OK;
    }
    if (uvsGetDeviceNameByUrmaEid == nullptr) {
        UBSE_LOG_ERROR << "Failed to find symbol 'uvs_get_device_name_by_eid'";
        return UBSE_ERROR_NOENT;
    }
    ret = uvsGetDeviceNameByUrmaEid(bondingEid, name, DEV_NAME_LEN);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Uvs failed to get state";
        isactivate = false;
        return ret;
    }
    isactivate = true;
    return UBSE_OK;
}

UbseResult UbseUrmaUvsModule::ActivateBondingDevice(const std::string &urmaEid)
{
    UBSE_LOG_DEBUG << "Activate Bonding Device";
    char bondingEid[IPV6_BYTE_COUNT];
    auto ret = ParseColonHexString(urmaEid, bondingEid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to parse bondingEid=" << urmaEid;
        return ret;
    }
    if (g_isMocked) {
        return UBSE_OK;
    }
    if (uvsCreateAggrDev == nullptr) {
        UBSE_LOG_ERROR << "Failed to find symbol 'uvs_create_agg_dev'";
        return UBSE_ERROR_NOENT;
    }
    ret = uvsCreateAggrDev(bondingEid);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Uvs failed to activate bonding device, ErrorCode=" << ret;
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaUvsModule::DeactivateBondingDevice(const std::string &urmaEid)
{
    UBSE_LOG_DEBUG << "Deactivate Bonding Device";
    char bondingEid[IPV6_BYTE_COUNT];
    auto ret = ParseColonHexString(urmaEid, bondingEid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to parse bondingEid=" << urmaEid;
        return ret;
    }
    if (g_isMocked) {
        return UBSE_OK;
    }
    if (uvsDeleteAggrDev == nullptr) {
        UBSE_LOG_ERROR << "Failed to find symbol 'uvs_delete_agg_dev'";
        return UBSE_ERROR_NOENT;
    }
    ret = uvsDeleteAggrDev(bondingEid);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Uvs failed to deactivate bonding device, ErrorCode=" << ret;
        return ret;
    }
    return UBSE_OK;
}

UbseResult GetSlotIds(const std::vector<UbseUrmaUvsNodeInfo> &bondingInfo, std::set<std::string> &slotIds)
{
    if (bondingInfo.empty()) {
        return UBSE_ERROR;
    }
    for (auto &info : bondingInfo) {
        slotIds.insert(info.nodeId);
    }
    std::ostringstream oss;
    oss << "Found " << slotIds.size() << " slots, includes: ";
    for (auto &id : slotIds) {
        oss << id << " ";
    }
    UBSE_LOG_DEBUG << oss.str();
    return UBSE_OK;
}

UbseResult UbseUrmaUvsModule::FillNodeComInfo(const std::vector<PhysicalLink> &allLinkInfo,
                                              const std::vector<UbseUrmaUvsNodeInfo> &bondingInfo,
                                              std::vector<UbcoreTopoNode> &nodes)
{
    nodes.clear();
    std::set<std::string> slotIds;
    auto ret = GetSlotIds(bondingInfo, slotIds);
    if (ret != UBSE_OK || slotIds.empty()) {
        UBSE_LOG_ERROR << "Failed to get slotIds";
        return UBSE_ERROR;
    }

    std::unordered_map<std::string, UbcoreTopoNode> nodeMap;
    // 每个 slot 构建一个 UbcoreTopoNode
    InitialNodes(slotIds, nodeMap);
    // 填充每个 node 的链路信息（根据 PhysicalLink）
    ret = FillTopo(allLinkInfo, nodeMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to fill topo";
        return ret;
    }
    // 处理 bonding 信息
    ret = FillBondingInfo(bondingInfo, nodeMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to fill bondingInfo";
        return ret;
    }
    UBSE_LOG_INFO << "Found " << slotIds.size() << " nodes, and successfully filled uvs data";

    nodes.reserve(nodeMap.size());
    for (auto &pair : nodeMap) {
        nodes.push_back(std::move(pair.second));
    }
    return UBSE_OK;
}

void UbseUrmaUvsModule::InitialNodes(const std::set<std::string> slotIds,
                                     std::unordered_map<std::string, UbcoreTopoNode> &nodeMap)
{
    nodeMap.clear();
    for (auto &id : slotIds) {
        UbcoreTopoNode node{};
        auto ret = ConvertStrToUint32(id, node.id);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to convert " << id << " to uint32";
        }
        node.is_current = false;

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

UbseResult UbseUrmaUvsModule::FillTopo(const std::vector<PhysicalLink> &allLinkInfo,
                                       std::unordered_map<std::string, UbcoreTopoNode> &nodeMap)
{
    if (allLinkInfo.size() == 0) {
        UBSE_LOG_INFO << "No link info found";
        return UBSE_OK;
    }
    for (const auto &topo : allLinkInfo) {
        std::string curSlotId = std::to_string(topo.slotId);
        std::string peerSlotId = std::to_string(topo.peerSlotId);
        if (nodeMap.find(curSlotId) == nodeMap.end() || nodeMap.find(peerSlotId) == nodeMap.end()) {
            UBSE_LOG_ERROR << "Failed to find slotId " << curSlotId << " or peer slotId " << peerSlotId << " in nodes";
            return UBSE_ERROR;
        }

        uint32_t iodie_idx = topo.chipId - 1;
        uint32_t peer_iodie_idx = topo.peerChipId - 1;
        if (iodie_idx >= IODIE_NUM || topo.portId >= PORT_NUM || peer_iodie_idx >= IODIE_NUM ||
            topo.peerPortId >= PORT_NUM) {
            UBSE_LOG_ERROR << std::to_string(topo.portId) << " or " << std::to_string(topo.peerPortId);
            UBSE_LOG_ERROR << std::to_string(iodie_idx) << " or " << std::to_string(peer_iodie_idx) ;
            return UBSE_ERROR;
        }
        nodeMap[curSlotId].link[iodie_idx][topo.portId].peer_node = topo.peerSlotId;
        nodeMap[curSlotId].link[iodie_idx][topo.portId].peer_iodie = peer_iodie_idx;
        nodeMap[curSlotId].link[iodie_idx][topo.portId].peer_port = topo.peerPortId;

        nodeMap[peerSlotId].link[peer_iodie_idx][topo.peerPortId].peer_node = topo.slotId;
        nodeMap[peerSlotId].link[peer_iodie_idx][topo.peerPortId].peer_iodie = iodie_idx;
        nodeMap[peerSlotId].link[peer_iodie_idx][topo.peerPortId].peer_port = topo.portId;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaUvsModule::FillFeInfo(const std::vector<UbseUrmaUvsFe> &fes, UbcoreTopoAggrDev &aggr_dev)
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
        auto ret = ConvertStrToUint32(fes[i].ubpuId, aggr_dev.fe[i].socket_id);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Convert socket_id failed, " << FormatRetCode(ret);
            return ret;
        }
        ret = ParseColonHexString(fes[i].primaryEid, aggr_dev.fe[i].primary_eid);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to parse primaryEid=" << fes[i].primaryEid;
            return ret;
        }
        for (auto &port : fes[i].portEid) {
            uint32_t portId;
            ret = ConvertStrToUint32(port.first, portId);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Convert socket_id failed, " << FormatRetCode(ret);
                return ret;
            }
            ret = ParseColonHexString(port.second, aggr_dev.fe[i].port_eid[portId]);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to parse portEid=" << port.second;
                return ret;
            }
        }
    }
    return UBSE_OK;
}

UbseResult UbseUrmaUvsModule::FillBondingInfo(const std::vector<UbseUrmaUvsNodeInfo> &bondingInfo,
                                              std::unordered_map<std::string, UbcoreTopoNode> &nodeMap)
{
    if (bondingInfo.empty()) {
        UBSE_LOG_ERROR << "No bonding info found";
        return UBSE_ERROR;
    }

    for (auto &info : bondingInfo) {
        uint32_t bondingDevSize = info.devList.size();
        if (bondingDevSize > DEV_NUM) {
            UBSE_LOG_ERROR << "aggr_device num exceeded";
            return UBSE_ERROR;
        }

        for (size_t i = 0; i < bondingDevSize; i++) {
            auto ret = ParseColonHexString(info.devList[i].urmaDevEid, nodeMap[info.nodeId].aggr_dev[i].aggr_eid);
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

UbseResult UbseUrmaUvsModule::ParseColonHexString(const std::string &input, char outBytes[IPV6_BYTE_COUNT])
{
    if (input.size() != IPV6_FULL_FORMAT_LENGTH) {
        return UBSE_ERROR;
    }

    // 将 char* 转换为 unsigned char* 以匹配 sscanf 的格式要求
    auto *uOut = reinterpret_cast<unsigned char *>(outBytes);

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

    return scanned == IPV6_BYTE_COUNT ? UBSE_OK : UBSE_ERROR;
}
} // namespace ubse::urma