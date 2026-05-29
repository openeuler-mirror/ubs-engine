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

#include "ubse_node_api_convert.h"

#include <netinet/in.h>

#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubs_engine_mem.h"

namespace ubse::node::api {
using namespace ubse::utils;
const uint32_t BITS_PER_HALF_UINT64 = 32;
// 自定义64位网络字节序转换
uint64_t HtonllCustom(uint64_t host_value)
{
    uint32_t low = htonl(static_cast<uint32_t>(host_value));
    uint32_t high = htonl(static_cast<uint32_t>(host_value >> BITS_PER_HALF_UINT64));
    return (static_cast<uint64_t>(low) << BITS_PER_HALF_UINT64) | high;
}

uint64_t NtohllCustom(uint64_t net_value)
{
    uint32_t low = ntohl(static_cast<uint32_t>(net_value));
    uint32_t high = ntohl(static_cast<uint32_t>(net_value >> BITS_PER_HALF_UINT64));
    return ((static_cast<uint64_t>(low) << BITS_PER_HALF_UINT64) | high);
}

size_t UbseStringCalcSize(const std::string& str, size_t maxLen)
{
    size_t len = 0;
    len += sizeof(uint32_t);
    len += std::min(str.size(), maxLen);
    return len;
}

size_t UbseNodeCalcSize(const UbseNode& node)
{
    size_t len = 0;
    len += sizeof(uint32_t);
    len += sizeof(uint32_t) * UBS_TOPO_SOCKET_NUM; // 2个socketId
    len += sizeof(uint32_t) * UBS_TOPO_SOCKET_NUM * UBS_TOPO_NUMA_NUM;
    len += sizeof(ubs_topo_ip_address_t) * UBS_TOPO_IPADDR_NUM;
    len += UbseStringCalcSize(node.hostName, HOST_NAME_MAX);
    return len;
}

size_t UbseNodeListCalcSize(const std::vector<UbseNode>& nodeList)
{
    size_t len = 0;
    len += sizeof(uint32_t);
    for (const auto& node : nodeList) {
        len += UbseNodeCalcSize(node);
    }
    return len;
}

size_t UbseCpuLinkListCalcSize(const std::vector<UbseCpuLink>& cpuLinkList)
{
    size_t cpuLinkSize = sizeof(uint32_t) * 6;
    size_t len = 0;
    len += sizeof(uint32_t);
    len += cpuLinkSize * cpuLinkList.size();
    return len;
}

uint32_t UbseStringUnpack(const ipc::UbseIpcMessage& buffer, std::string& str, uint32_t maxLen)
{
    UbseUnpackUtil unpackUtil{buffer.buffer, buffer.length};
    if (!unpackUtil.UnpackString(str, maxLen)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return UBSE_OK;
}

uint32_t UbseSlotIdUnpack(const ipc::UbseIpcMessage& buffer, uint32_t& slotId)
{
    UbseUnpackUtil unpackUtil{buffer.buffer, buffer.length};
    if (!unpackUtil.UnpackUint32(slotId)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return UBSE_OK;
}

uint32_t UbseBaseNodePackInner(const UbseNode& node, UbsePackUtil& packUtil)
{
    // 打包slotId
    packUtil.UbsePackUint32(node.slotId);
    // 打包socketId
    for (uint32_t socketId : node.socketId) {
        packUtil.UbsePackUint32(socketId);
    }
    // 打包numaIds
    for (auto& socket : node.numaIds) {
        for (auto& numaId : socket) {
            packUtil.UbsePackUint32(numaId);
        }
    }
    // 打包hostname
    if (!packUtil.UbsePackString(node.hostName, HOST_NAME_MAX)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}

uint32_t UbseNodePackInner(const UbseNode& node, UbsePackUtil& packUtil)
{
    auto ret = UbseBaseNodePackInner(node, packUtil);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 打包ips
    for (const ubs_topo_ip_address_t& ipAddr : node.ips) {
        packUtil.UbsePackInt32(ipAddr.af);
        struct in_addr ipv4 = ipAddr.ipv4;
        packUtil.UbsePackUint32(
            *reinterpret_cast<uint32_t*>(&ipv4)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        struct in6_addr ipv6 = ipAddr.ipv6;
        packUtil.UbsePackUint64(
            *(reinterpret_cast<uint64_t*>(&ipv6))); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        packUtil.UbsePackUint64(
            *(reinterpret_cast<uint64_t*>(&ipv6) + 1)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
    return UBSE_OK;
}

uint32_t UbseNodePack(const UbseNode& node, ipc::UbseIpcMessage& buffer)
{
    // 申请内存
    auto size = UbseNodeCalcSize(node);
    buffer.buffer = new (std::nothrow) uint8_t[size];
    if (buffer.buffer == nullptr) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    buffer.length = size;
    // 打包
    UbsePackUtil packUtil(buffer.buffer, size);
    auto ret = UbseNodePackInner(node, packUtil);
    if (ret != UBSE_OK) {
        delete[] buffer.buffer;
        buffer.buffer = nullptr;
        buffer.length = 0;
    }
    return ret;
}

static uint32_t UbseCpuLinkPackInner(const UbseCpuLink& ubseCpuLink, UbsePackUtil& packUtil)
{
    auto ret = true;
    // 打包slotId
    ret &= packUtil.UbsePackUint32(ubseCpuLink.slotId);
    // 打包socketId
    ret &= packUtil.UbsePackUint32(ubseCpuLink.socketId);
    // 打包portId
    ret &= packUtil.UbsePackUint32(ubseCpuLink.portId);
    // 打包peerSlotId
    ret &= packUtil.UbsePackUint32(ubseCpuLink.peerSlotId);
    // 打包peerSocketId
    ret &= packUtil.UbsePackUint32(ubseCpuLink.peerSocketId);
    // 打包peerPortId
    ret &= packUtil.UbsePackUint32(ubseCpuLink.peerPortId);
    return ret ? UBSE_OK : UBSE_ERROR_SERIALIZE_FAILED;
}

uint32_t UbseNodeListPack(const std::vector<UbseNode>& nodeList, ipc::UbseIpcMessage& buffer)
{
    // 申请内存
    auto size = UbseNodeListCalcSize(nodeList);
    buffer.buffer = new (std::nothrow) uint8_t[size];
    if (buffer.buffer == nullptr) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    UbsePackUtil packUtil(buffer.buffer, size);
    // 打包count
    packUtil.UbsePackUint32(nodeList.size());
    // 打包nodeList
    for (const auto& node : nodeList) {
        auto ret = UbseNodePackInner(node, packUtil);
        if (ret != UBSE_OK) {
            delete[] buffer.buffer;
            buffer.buffer = nullptr;
            return ret;
        }
    }
    buffer.length = size;
    return UBSE_OK;
}

uint32_t UbseCpuLinkListPack(const std::vector<UbseCpuLink>& cpuLinkList, ipc::UbseIpcMessage& buffer)
{
    size_t totalSize = UbseCpuLinkListCalcSize(cpuLinkList);
    buffer.buffer = new (std::nothrow) uint8_t[totalSize];
    if (buffer.buffer == nullptr) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    UbsePackUtil packUtil(buffer.buffer, totalSize);

    // 打包count
    packUtil.UbsePackUint32(cpuLinkList.size());
    // 打包cpuLinkList
    for (const auto& cpuLink : cpuLinkList) {
        auto ret = UbseCpuLinkPackInner(cpuLink, packUtil);
        if (ret != UBSE_OK) {
            delete[] buffer.buffer;
            buffer.buffer = nullptr;
            buffer.length = 0;
            return ret;
        }
    }
    buffer.length = totalSize;
    return UBSE_OK;
}

static uint32_t UbseNumaInfoPack(const UbseNumaNodeInfo& numaInfo, UbsePackUtil& packUtil)
{
    try {
        uint32_t slotId = std::stoul(numaInfo.nodeId);
        packUtil.UbsePackUint32(slotId);
    } catch (const std::exception& e) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    packUtil.UbsePackUint32(numaInfo.socketId);
    packUtil.UbsePackUint32(numaInfo.numaId);
    packUtil.UbsePackUint32(static_cast<uint32_t>(NUMA_LOCAL));
    packUtil.UbsePackUint32(static_cast<uint32_t>(numaInfo.mReservedMemRatio));
    packUtil.UbsePackUint64(numaInfo.mMemTotal);
    packUtil.UbsePackUint64(numaInfo.mMemFree);
    packUtil.UbsePackUint32(numaInfo.nrHugepages);
    packUtil.UbsePackUint32(numaInfo.freeHugepages);
    packUtil.UbsePackUint32(0);
    packUtil.UbsePackUint32(0);
    packUtil.UbsePackUint64(numaInfo.mMemBorrowed);
    packUtil.UbsePackUint64(numaInfo.mMemLent);
    return UBSE_OK;
}

uint32_t UbseNumaInfoListPack(const std::vector<UbseNumaNodeInfo>& numaInfoList, ipc::UbseIpcMessage& buffer)
{
    size_t numaInfoSize = sizeof(uint32_t) * 9 + sizeof(uint64_t) * 4;
    size_t totalSize = sizeof(uint32_t) + numaInfoSize * numaInfoList.size();
    buffer.buffer = new (std::nothrow) uint8_t[totalSize];
    if (buffer.buffer == nullptr) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    UbsePackUtil packUtil(buffer.buffer, totalSize);

    // 打包count
    packUtil.UbsePackUint32(numaInfoList.size());
    // 打包cpuLinkList
    for (const auto& numaInfo : numaInfoList) {
        auto ret = UbseNumaInfoPack(numaInfo, packUtil);
        if (ret != UBSE_OK) {
            delete[] buffer.buffer;
            buffer.buffer = nullptr;
            buffer.length = 0;
            return ret;
        }
    }
    buffer.length = totalSize;
    return UBSE_OK;
}
} // namespace ubse::node::api