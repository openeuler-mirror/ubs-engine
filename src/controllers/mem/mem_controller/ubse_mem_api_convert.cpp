/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "ubse_mem_api_convert.h"

#include <netinet/in.h>

#include "securec.h"
#include "src/include/ubse_mem_obj.h"
#include "src/sdk/include/ubs_engine.h"
#include "ubse_ipc_common.h"
#include "ubse_mgr_configuration.h"
#include "ubse_str_util.h"

// 定义最小和最大值
const uint64_t MIN_LENDERSIZE = 128 * 1024 * 1024;           // 128MB
const uint64_t MAX_LENDERSIZE = 256ULL * 1024 * 1024 * 1024; // 256GB
namespace api::server {
using namespace ubse::ipc;
using namespace ubse::mem::obj;
uint32_t UbseMemCreateReqUnpack(const UbseIpcMessage &buffer, UbseMemFdBorrowReq &memFdBorrowReq)
{
    // 计算预期长度
    const size_t expectedLen = UBS_MEM_MAX_NAME_LENGTH + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uid_t) +
                               sizeof(gid_t) + sizeof(pid_t) + sizeof(mode_t);

    if (buffer.length != expectedLen) {
        return IPC_ERROR_SERIALIZATION_FAILED; // 缓冲区长度不匹配
    }

    const uint8_t *ptr = buffer.buffer;

    // 解包 name (固定长度字符串)
    const char *nameStart = reinterpret_cast<const char *>(ptr);
    size_t nameLen = strnlen(nameStart, UBS_MEM_MAX_NAME_LENGTH - 1);
    memFdBorrowReq.name.assign(nameStart, nameLen);
    ptr += UBS_MEM_MAX_NAME_LENGTH;
    // 解包size (64位网络字节序转换)
    memFdBorrowReq.size = NtohllCustom(*(const uint64_t *)ptr);
    ptr += sizeof(uint64_t);
    // 解包 uds_info
    memFdBorrowReq.owner.uid = ntohl(*(const uint32_t*)ptr);
    ptr += sizeof(uint32_t);
    memFdBorrowReq.owner.gid = ntohl(*(const uint32_t*)ptr);
    ptr += sizeof(uint32_t);
    memFdBorrowReq.owner.pid = ntohl(*(const uint32_t*)ptr);
    ptr += sizeof(uint32_t);
    // 解包mode
    memFdBorrowReq.owner.mode = ntohl(*reinterpret_cast<const uint32_t*>(ptr));
    ptr += sizeof(uint32_t);
    // 解包distance (32位网络字节序转换)
    memFdBorrowReq.distance = static_cast<UbseMemDistance>(ntohl(*(const uint32_t *)ptr));
    return IPC_SUCCESS;
}

static void LenderInfoUnpack(const uint8_t *&data, ubse::mem::obj::UbseNumaLocation &loc, uint64_t &size)
{
    // 解包节点ID
    auto slotId = ntohl(*reinterpret_cast<const uint32_t *>(data));
    loc.nodeId = std::to_string(slotId);
    data += sizeof(uint32_t);

    // 解包socketid
    auto socketId = ntohl(*reinterpret_cast<const uint32_t *>(data));
    data += sizeof(uint32_t);

    // 解包numaId
    loc.numaId = static_cast<uint32_t>(NtohllCustom(*(const uint64_t *)data));
    data += sizeof(uint64_t);

    // 解包借出大小
    size = NtohllCustom(*reinterpret_cast<const uint64_t *>(data));
    data += sizeof(size);
}

uint32_t UbseMemCreateWithLenderReqUnpack(const UbseIpcMessage &buffer, UbseMemFdBorrowReq &memFdBorrowReq)
{
    // 计算总长度
    const size_t totalSize = UBS_MEM_MAX_NAME_LENGTH +                       // name
                             sizeof(uid_t) + sizeof(gid_t) + sizeof(pid_t) + // uds_info
                             +sizeof(uint32_t) +                             // mode
                             UBSE_MEM_LENDER_SIZE;                           // lender

    if (buffer.length < totalSize) {
        return IPC_ERROR_DESERIALIZATION_FAILED;
    }
    const uint8_t *ptr = buffer.buffer;
    // 解包 name (固定长度字符串)
    const char *nameStart = reinterpret_cast<const char *>(ptr);
    size_t nameLen = strnlen(nameStart, UBS_MEM_MAX_NAME_LENGTH - 1);
    memFdBorrowReq.name.assign(nameStart, nameLen);
    ptr += UBS_MEM_MAX_NAME_LENGTH;

    // 解包 uds_info
    memFdBorrowReq.owner.uid = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);
    memFdBorrowReq.owner.gid = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);
    memFdBorrowReq.owner.pid = static_cast<pid_t>(ntohl(*(const uint32_t *)ptr));
    ptr += sizeof(uint32_t);
    // 解包mode
    memFdBorrowReq.owner.mode = ntohl(*reinterpret_cast<const uint32_t *>(ptr));
    // 解包lender
    ubse::mem::obj::UbseNumaLocation numaLocation{};
    uint64_t lenderSize{};
    LenderInfoUnpack(ptr, numaLocation, lenderSize);
    // 检查lenderSize是否在有效范围内
    if (lenderSize < MIN_LENDERSIZE || lenderSize > MAX_LENDERSIZE) {
        return IPC_ERROR_INVALID_ARGUMENT;
    }
    memFdBorrowReq.lenderLocs.push_back(std::move(numaLocation));
    memFdBorrowReq.lenderSizes.push_back(lenderSize);
    memFdBorrowReq.size = lenderSize;
    return IPC_SUCCESS;
}

uint32_t UbseMemFdCreateResponsePack(const UbseMemOperationResp &memOperationResp, UbseIpcMessage &buffer)
{
    // 计算总需求长度
    const size_t requiredLength = UBS_MEM_MAX_NAME_LENGTH +                              // name
                                  sizeof(uint32_t) +                                     // memid_cnt
                                  sizeof(uint64_t) * memOperationResp.memIdList.size() + // memids有效部分
                                  sizeof(uint64_t) +                                     // mem_size
                                  sizeof(size_t);                                        // unit_size

    buffer.buffer = new uint8_t[requiredLength];
    if (buffer.buffer == nullptr) {
        return IPC_ERROR_SERIALIZATION_FAILED;
    }
    uint8_t *ptr = buffer.buffer;

    // 打包name (固定48字节)
    if (memOperationResp.name.size() > UBS_MEM_MAX_NAME_LENGTH - 1) {
        return IPC_ERROR_SERIALIZATION_FAILED; // 超长直接返回错误
    }
    auto ret = memcpy_s(ptr, UBS_MEM_MAX_NAME_LENGTH, memOperationResp.name.c_str(), memOperationResp.name.size());
    if (ret != EOK) {
        return IPC_ERROR_SERIALIZATION_FAILED;
    }
    ptr[memOperationResp.name.size()] = '\0';
    ptr += UBS_MEM_MAX_NAME_LENGTH;

    // 校验memid_num合法性
    if (memOperationResp.memIdList.size() > UBS_MEM_MAX_MEMID_NUM) {
        return IPC_ERROR_SERIALIZATION_FAILED;
    }

    // 打包memid_num (32位)
    *(uint32_t *)ptr = htonl(memOperationResp.memIdList.size());
    ptr += sizeof(uint32_t);

    // 打包memids数组 (仅打包有效部分)
    for (auto memId : memOperationResp.memIdList) {
        *(reinterpret_cast<uint64_t *>(ptr)) = HtonllCustom(memId);
        ptr += sizeof(uint64_t);
    }

    // 打包mem_size (64位)
    size_t idx = 0;
    uint64_t memSize{};
    ubse::utils::ConvertStrToUint64(memOperationResp.realSize, memSize); // 10进制
    // 确保整个字符串都被解析
    if (idx != memOperationResp.realSize.size()) {
        return IPC_ERROR_SERIALIZATION_FAILED;
    }

    *(reinterpret_cast<uint64_t *>(ptr)) = HtonllCustom(memSize);
    ptr += sizeof(uint64_t);

    // 打包unit_size
    *(reinterpret_cast<uint64_t *>(ptr)) = HtonllCustom(static_cast<uint64_t>(1));
    ptr += sizeof(uint64_t);
    buffer.length = requiredLength;
    return IPC_SUCCESS;
}

uint32_t UbseMemFdDeleteReqUnpack(const UbseIpcMessage &buffer, UbseMemReturnReq &req)
{
    // 计算总长度
    const size_t totalSize = UBS_MEM_MAX_NAME_LENGTH; // name
    if (buffer.length < totalSize) {
        return IPC_ERROR_DESERIALIZATION_FAILED;
    }
    const uint8_t *ptr = buffer.buffer;
    // 解包 name (固定长度字符串)
    const char *nameStart = reinterpret_cast<const char *>(ptr);
    size_t nameLen = strnlen(nameStart, UBS_MEM_MAX_NAME_LENGTH - 1);
    req.name.assign(nameStart, nameLen);
    return IPC_SUCCESS;
}
uint32_t UbseMemNumaCreateReqUnpack(const UbseIpcMessage &buffer, UbseMemNumaBorrowReq &memNumaBorrowReq)
{
    // 计算预期长度
    const size_t expectedLen = UBS_MEM_MAX_NAME_LENGTH + // name
                               sizeof(uint64_t) +        // size
                               sizeof(uint32_t);         // distance

    if (buffer.length != expectedLen) {
        return IPC_ERROR_SERIALIZATION_FAILED; // 缓冲区长度不匹配
    }

    const uint8_t *ptr = buffer.buffer;
    // 解包 name (固定长度字符串)
    const char *nameStart = reinterpret_cast<const char *>(ptr);
    size_t nameLen = strnlen(nameStart, UBS_MEM_MAX_NAME_LENGTH - 1);
    memNumaBorrowReq.name.assign(nameStart, nameLen);
    ptr += UBS_MEM_MAX_NAME_LENGTH;

    // 解包size (64位网络字节序转换)
    memNumaBorrowReq.size = NtohllCustom(*reinterpret_cast<const uint64_t *>(ptr));
    ptr += sizeof(uint64_t);
    // 解包distance (32位网络字节序转换)
    memNumaBorrowReq.distance = static_cast<UbseMemDistance>(ntohl(*reinterpret_cast<const uint32_t *>(ptr)));
    return IPC_SUCCESS;
}

uint32_t UbseMemNumaCreateLenderReqUnpack(const UbseIpcMessage &buffer, UbseMemNumaBorrowReq &memNumaBorrowReq)
{
    const size_t expectedLen = UBS_MEM_MAX_NAME_LENGTH + // name
                               UBSE_MEM_LENDER_SIZE;     // lender
    if (buffer.length < expectedLen) {
        return IPC_ERROR_SERIALIZATION_FAILED; // 缓冲区长度不足
    }
    const uint8_t *ptr = buffer.buffer;
    // 解包 name (固定长度字符串)
    const char *nameStart = reinterpret_cast<const char *>(ptr);
    size_t nameLen = strnlen(nameStart, UBS_MEM_MAX_NAME_LENGTH - 1);
    memNumaBorrowReq.name.assign(nameStart, nameLen);
    ptr += UBS_MEM_MAX_NAME_LENGTH;

    // 解包lender
    ubse::mem::obj::UbseNumaLocation numaLocation{};
    uint64_t lenderSize{};
    LenderInfoUnpack(ptr, numaLocation, lenderSize);
    memNumaBorrowReq.size = lenderSize;
    return IPC_SUCCESS;
}

uint32_t UbseMemNumaDeleteUnPack(const UbseIpcMessage &buffer, UbseMemReturnReq &req)
{
    const size_t expectedLen = UBS_MEM_MAX_NAME_LENGTH; // name
    if (buffer.length < expectedLen) {
        return IPC_ERROR_SERIALIZATION_FAILED; // 缓冲区长度不足
    }
    const uint8_t *ptr = buffer.buffer;
    // 解包 name (固定长度字符串)
    const char *nameStart = reinterpret_cast<const char *>(ptr);
    size_t nameLen = strnlen(nameStart, UBS_MEM_MAX_NAME_LENGTH - 1);
    req.name.assign(nameStart, nameLen);
    return IPC_SUCCESS;
}

uint32_t UbseMemNumaCreateResponsePack(const UbseMemOperationResp &memOperationResp, UbseIpcMessage &buffer)
{
    // 计算总长度
    const size_t requiredLength = UBS_MEM_MAX_NAME_LENGTH + // name
                                  sizeof(int64_t) +         // numaid
                                  sizeof(uint32_t);         // state
    buffer.buffer = new uint8_t[requiredLength];
    if (buffer.buffer == nullptr) {
        return IPC_ERROR_SERIALIZATION_FAILED;
    }
    uint8_t *ptr = buffer.buffer;

    // 打包name
    if (memOperationResp.name.size() > UBS_MEM_MAX_NAME_LENGTH - 1) {
        return IPC_ERROR_SERIALIZATION_FAILED; // 超长直接返回错误
    }
    auto ret = memcpy_s(ptr, UBS_MEM_MAX_NAME_LENGTH, memOperationResp.name.c_str(), memOperationResp.name.size());
    if (ret != EOK) {
        return IPC_ERROR_SERIALIZATION_FAILED;
    }
    ptr[memOperationResp.name.size()] = '\0';
    ptr += UBS_MEM_MAX_NAME_LENGTH;

    // 打包memid
    *(reinterpret_cast<uint64_t *>(ptr)) = NtohllCustom(memOperationResp.remoteNumaId);
    ptr += sizeof(uint64_t);
    buffer.length = requiredLength;
    return IPC_SUCCESS;
}

static uint32_t UbseMemFdDescPackInner(const ubse::mem::def::UbseMemFdDesc &fdDesc, uint8_t *&buffer)
{
    uint8_t *ptr = buffer;
    // 打包name (固定49字节)
    if (fdDesc.name.size() > UBS_MEM_MAX_NAME_LENGTH - 1) {
        return IPC_ERROR_SERIALIZATION_FAILED; // 超长直接返回错误
    }
    auto ret = memcpy_s(ptr, UBS_MEM_MAX_NAME_LENGTH, fdDesc.name.c_str(), fdDesc.name.size());
    if (ret != EOK) {
        return IPC_ERROR_SERIALIZATION_FAILED;
    }
    ptr[fdDesc.name.size()] = '\0';
    ptr += UBS_MEM_MAX_NAME_LENGTH;

    // 校验memid_num合法性
    if (fdDesc.memIds.size() > UBS_MEM_MAX_MEMID_NUM) {
        return IPC_ERROR_SERIALIZATION_FAILED;
    }

    // 打包memid_num (32位)
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(static_cast<uint32_t>(fdDesc.memIds.size()));
    ptr += sizeof(uint32_t);

    // 打包memids数组 (仅打包有效部分)
    for (auto memId : fdDesc.memIds) {
        *(reinterpret_cast<uint64_t *>(ptr)) = HtonllCustom(memId);
        ptr += sizeof(uint64_t);
    }

    // 打包mem_size (64位)
    *(reinterpret_cast<uint64_t *>(ptr)) = HtonllCustom(fdDesc.totalMemSize);
    ptr += sizeof(uint64_t);

    // 打包unit_size
    *(reinterpret_cast<uint64_t *>(ptr)) = HtonllCustom(static_cast<uint64_t>(fdDesc.unitSize));
    ptr += sizeof(uint64_t);
    buffer = ptr;
    return IPC_SUCCESS;
}
uint32_t UbseMemFdDescPack(const ubse::mem::def::UbseMemFdDesc &fdDesc, uint8_t *buffer)
{
    uint8_t *ptr = buffer;
    return UbseMemFdDescPackInner(fdDesc, ptr);
}

uint32_t UbseMemFdDescListPackedSize(const std::vector<ubse::mem::def::UbseMemFdDesc> &fdDescList, size_t &requestSize)
{
    // 基础空间：描述符数量字段
    size_t totalSize = sizeof(uint32_t);

    // 每个描述符所需空间
    for (const auto &fdDesc : fdDescList) {
        // 验证参数
        if (fdDesc.memIds.size() > UBS_MEM_MAX_MEMID_NUM) {
            return IPC_ERROR_SERIALIZATION_FAILED;
        }

        // 各字段空间计算
        totalSize += (UBS_MEM_MAX_NAME_LENGTH);               // name + terminator
        totalSize += sizeof(uint32_t);                        // memid_cnt
        totalSize += fdDesc.memIds.size() * sizeof(uint64_t); // memids
        totalSize += sizeof(uint64_t);                        // mem_size
        totalSize += sizeof(uint64_t);                        // unit_size
    }

    requestSize = totalSize;
    return IPC_SUCCESS;
}

uint32_t UbseMemFdDescListPack(const std::vector<ubse::mem::def::UbseMemFdDesc> &fdDescList, uint8_t *buffer)
{
    uint8_t *ptr = buffer;
    // 打包count
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(static_cast<uint32_t>(fdDescList.size()));
    ptr += sizeof(uint32_t);
    // 打包fdDescList
    for (const auto &fdDesc : fdDescList) {
        auto ret = UbseMemFdDescPackInner(fdDesc, ptr);
        if (ret != IPC_SUCCESS) {
            return ret;
        }
    }
    return IPC_SUCCESS;
}

static uint32_t UbseMemNumaDescPackInner(const ubse::mem::def::UbseMemNumaDesc &numaDesc, uint8_t *&buffer)
{
    uint8_t *ptr = buffer;

    // 打包name
    if (numaDesc.name.size() > UBS_MEM_MAX_NAME_LENGTH - 1) {
        return IPC_ERROR_SERIALIZATION_FAILED; // 超长直接返回错误
    }
    auto ret = memcpy_s(ptr, UBS_MEM_MAX_NAME_LENGTH, numaDesc.name.c_str(), numaDesc.name.size());
    if (ret != EOK) {
        return IPC_ERROR_SERIALIZATION_FAILED;
    }
    ptr[numaDesc.name.size()] = '\0';
    ptr += UBS_MEM_MAX_NAME_LENGTH;

    // 打包memid
    *(reinterpret_cast<uint64_t *>(ptr)) = HtonllCustom(static_cast<uint64_t>(numaDesc.numaId));
    ptr += sizeof(int64_t);

    buffer = ptr;
    return IPC_SUCCESS;
}

uint32_t UbseMemNumaDescPack(const ubse::mem::def::UbseMemNumaDesc &numaDesc, uint8_t *buffer)
{
    uint8_t *ptr = buffer;
    return UbseMemNumaDescPackInner(numaDesc, ptr);
}

uint32_t UbseMemNumaDescListPack(const std::vector<ubse::mem::def::UbseMemNumaDesc> &numaDescList, uint8_t *buffer)
{
    uint8_t *ptr = buffer;

    // 打包count
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(static_cast<uint32_t>(numaDescList.size()));
    ptr += sizeof(uint32_t);
    // 打包fdDescList
    for (const auto &numaDesc : numaDescList) {
        auto ret = UbseMemNumaDescPackInner(numaDesc, ptr);
        if (ret != IPC_SUCCESS) {
            return ret;
        }
    }
    return IPC_SUCCESS;
}

static uint32_t UbseNumaInfoPack(const ubse::nodeController::UbseNumaInfo &numaInfo, uint8_t *buffer)
{
    uint8_t *ptr = buffer;

    // 打包slotId
    uint32_t slotId = std::stoul(numaInfo.location.nodeId);
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(slotId);
    ptr += sizeof(uint32_t);

    // 打包socketId (32位)
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(numaInfo.socketId);
    ptr += sizeof(uint32_t);

    // 打包numaId (32位)
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(numaInfo.location.numaId);
    ptr += sizeof(uint32_t);

    // 打包type
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(static_cast<uint32_t>(NUMA_LOCAL));
    ptr += sizeof(uint32_t);

    // 打包mem_reserved_ratio (32位)
    uint32_t poolMemRatio = 0;
    poolMemRatio = ubse::mem::strategy::MemMgrConfiguration::GetInstance().GetSystemPoolMemRatio();
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(poolMemRatio);
    ptr += sizeof(uint32_t);

    // 打包mem_total (64位)
    *(reinterpret_cast<uint64_t *>(ptr)) = HtonllCustom(numaInfo.size);
    ptr += sizeof(uint64_t);

    // 打包mem_free (64位)
    *(reinterpret_cast<uint64_t *>(ptr)) = HtonllCustom(numaInfo.freeSize);
    ptr += sizeof(uint64_t);

    // 打包huge_pages (32位)
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(numaInfo.nr_hugepages_2M);
    ptr += sizeof(uint32_t);

    // 打包free_huge_pages (32位)
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(numaInfo.free_hugepages_2M);
    ptr += sizeof(uint32_t);
    return IPC_SUCCESS;
}

uint32_t UbseNumaInfoListPack(const std::vector<ubse::nodeController::UbseNumaInfo> &numaInfoList, uint8_t *buffer)
{
    uint8_t *ptr = buffer;

    // 打包count
    *(reinterpret_cast<uint32_t *>(ptr)) = htonl(static_cast<uint32_t>(numaInfoList.size()));
    ptr += sizeof(uint32_t);
    // 打包numaInfoList
    for (const auto &numaInfo : numaInfoList) {
        auto ret = UbseNumaInfoPack(numaInfo, ptr);
        if (ret != IPC_SUCCESS) {
            return ret;
        }
        ptr += UBSE_NODE_NUMA_MEM_SIZE;
    }
    return IPC_SUCCESS;
}
} // namespace api::server