/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "ubse_node_api_convert.h"

#include <netinet/in.h>

#include "securec.h"
#include "ubse_ipc_common.h"

namespace ubse::node::api {
// 自定义64位网络字节序转换
uint64_t HtonllCustom(uint64_t host_value)
{
    uint32_t low = htonl(static_cast<uint32_t>(host_value));
    uint32_t high = htonl(static_cast<uint32_t>(host_value >> 32));
    return (static_cast<uint64_t>(low) << 32) | high;
}

uint64_t NtohllCustom(uint64_t net_value)
{
    uint32_t low = ntohl(static_cast<uint32_t>(net_value));
    uint32_t high = ntohl(static_cast<uint32_t>(net_value >> 32));
    return ((static_cast<uint64_t>(low) << 32) | high);
}

uint32_t UbseNodePack(const UbseNode &ubseNode, uint8_t *buffer)
{
    uint8_t *ptr = buffer;

    // 打包slotId
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(ubseNode.slotId);
    ptr += sizeof(uint32_t);
    // 打包socketId
    for (uint32_t socket : ubseNode.socketId) {
        *(reinterpret_cast<uint32_t *>(ptr)) = htonl(socket);
        ptr += sizeof(uint32_t);
    }
    // 打包hostname
    if (ubseNode.hostName.size() > HOST_NAME_MAX - 1) {
        return IPC_ERROR_SERIALIZATION_FAILED; // 超长直接返回错误
    }
    auto ret = memcpy_s(ptr, HOST_NAME_MAX, ubseNode.hostName.c_str(), ubseNode.hostName.size() + 1);
    if (ret != EOK) {
        return IPC_ERROR_SERIALIZATION_FAILED;
    }
    ptr[ubseNode.hostName.size()] = '\0';
    ptr += HOST_NAME_MAX;
    return IPC_SUCCESS;
}

static uint32_t UbseCpuLinkPack(const UbseCpuLink &ubseCpuLink, uint8_t *&buffer)
{
    uint8_t *ptr = buffer;

    // 打包slotId
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(ubseCpuLink.slotId);
    ptr += sizeof(uint32_t);
    // 打包socketId
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(ubseCpuLink.socketId);
    ptr += sizeof(uint32_t);

    // 打包peerSlotId
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(ubseCpuLink.peerSlotId);
    ptr += sizeof(uint32_t);
    // 打包peerSocketId
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(ubseCpuLink.peerSocketId);
    return IPC_SUCCESS;
}

uint32_t UbseNodeListPack(const std::vector<UbseNode> &ubseNodeList, uint8_t *buffer)
{
    uint8_t *ptr = buffer;

    // 打包count
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(static_cast<uint32_t>(ubseNodeList.size()));
    ptr += sizeof(uint32_t);
    // 打包fdDescList
    for (const auto &ubseNode : ubseNodeList) {
        auto ret = UbseNodePack(ubseNode, ptr);
        if (ret != IPC_SUCCESS) {
            return ret;
        }
        ptr += UBSE_NODE_SIZE;
    }
    return IPC_SUCCESS;
}

uint32_t UbseCpuLinkListPack(const std::vector<UbseCpuLink> &ubseCpuLinkList, uint8_t *buffer)
{
    uint8_t *ptr = buffer;

    // 打包count
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(static_cast<uint32_t>(ubseCpuLinkList.size()));
    ptr += sizeof(uint32_t);
    // 打包fdDescList
    for (const auto &ubseCpuLink : ubseCpuLinkList) {
        auto ret = UbseCpuLinkPack(ubseCpuLink, ptr);
        if (ret != IPC_SUCCESS) {
            return ret;
        }
        ptr += UBSE_CPU_LINK_SIZE;
    }
    return IPC_SUCCESS;
}
} // namespace ubse::node::api