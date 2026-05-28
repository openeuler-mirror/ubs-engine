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

#include "ubse_mem_buffer_convert.h"

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_constants.h"
#include "ubse_mem_controller_query_api.h"
#include "ubse_mem_util.h"
#include "ubse_node_api_convert.h"
#include "ubse_node_controller_def.h"
#include "ubse_pack_util.h"
#include "ubse_pointer_process.h"
#include "ubse_str_util.h"
#include "securec.h"

constexpr uint32_t MAX_MEM_RESOURCE_NAME_LENGTH = 48;
constexpr uint32_t MAX_LENDER_CNT = TOPOLOGY_MAX_NUMA_PER_SOCKET;
constexpr uint32_t SDK_DEFAULT_VALUE = 0xFFFFFFFF;
constexpr uint32_t MAX_SLOT_ID_COUNT = 16;
namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::utils;
using namespace ubse::nodeController::def;
using namespace ubse::node::api;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::log;

// 定义位掩码和位移常量
constexpr uint16_t ONE_PTH_MASK = 0x1;
constexpr uint16_t WR_DELAY_COMP_MASK = 0x1;
constexpr uint16_t REDUCE_DELAY_COMP_MASK = 0x1;
constexpr uint16_t CMO_DELAY_COMP_MASK = 0x1;
constexpr uint16_t SO_MASK = 0x1;
constexpr uint16_t AD_TR_OCHIP_MASK = 0x1;
constexpr uint16_t CACHEABLE_FLAG_MASK = 0x1;
constexpr uint16_t MAR_ID_MASK = 0x7;
constexpr uint16_t RSV0_MASK = 0x3F;

// 定义字段的位移量
constexpr uint16_t UBSE_MEM_PRIV_DATA_ONE_PTH_SHIFT = 15;
constexpr uint16_t UBSE_MEM_PRIV_DATA_WR_DELAY_COMP_SHIFT = 14;
constexpr uint16_t UBSE_MEM_PRIV_DATA_REDUCE_DELAY_COMP_SHIFT = 13;
constexpr uint16_t UBSE_MEM_PRIV_DATA_CMO_DELAY_COMP_SHIFT = 12;
constexpr uint16_t UBSE_MEM_PRIV_DATA_SO_SHIFT = 11;
constexpr uint16_t UBSE_MEM_PRIV_DATA_AD_TR_OCHIP_SHIFT = 10;
constexpr uint16_t UBSE_MEM_PRIV_DATA_CACHEABLE_FLAG_SHIFT = 9;
constexpr uint16_t UBSE_MEM_PRIV_DATA_MAR_ID_SHIFT = 6;
constexpr uint16_t UBSE_MEM_PRIV_DATA_RSV0_SHIFT = 0;
constexpr uint32_t HIGH_WATER_MARK = 100;
bool UbseOwnerUnpack(UbseUnpackUtil& unpackUtil, FdOwner& owner)
{
    if (!unpackUtil.UnpackUint32(owner.uid)) {
        UBSE_LOG_ERROR << "unpack owner uid failed.";
        return false;
    }
    if (!unpackUtil.UnpackUint32(owner.gid)) {
        UBSE_LOG_ERROR << "unpack owner gid failed.";
        return false;
    }
    if (!unpackUtil.UnpackUint32(
            reinterpret_cast<uint32_t&>(owner.pid))) { // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        UBSE_LOG_ERROR << "unpack owner pid failed.";
        return false;
    }
    if (!unpackUtil.UnpackUint32(owner.mode)) {
        UBSE_LOG_ERROR << "unpack owner mode failed.";
        return false;
    }
    return true;
}

static uint32_t UbseMemFdDescPackInner(const ubse::mem::def::UbseMemFdDesc& fdDesc, UbsePackUtil& packUtil)
{
    // 参数校验
    if (fdDesc.memIds.size() > UBS_MEM_MAX_MEMID_NUM) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    // 打包name
    if (!packUtil.UbsePackString(fdDesc.name, MAX_MEM_RESOURCE_NAME_LENGTH - 1)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    // 打包memid_num
    packUtil.UbsePackUint32(fdDesc.memIds.size());
    // 打包memids数组 (仅打包有效部分)
    for (auto memId : fdDesc.memIds) {
        packUtil.UbsePackUint64(memId);
    }

    // 打包mem_size (64位)
    packUtil.UbsePackUint64(fdDesc.totalMemSize);

    // 打包unit_size
    packUtil.UbsePackUint64(fdDesc.unitSize);

    // 打包exportNode
    auto ret = UbseBaseNodePackInner(fdDesc.exportNode, packUtil);
    if (ret != UBSE_OK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    ret = UbseBaseNodePackInner(fdDesc.importNode, packUtil);
    if (ret != UBSE_OK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    // 打包state
    packUtil.UbsePackUint32(static_cast<uint32_t>(fdDesc.state));
    return UBSE_OK;
}
static uint32_t UbseMemShmImportDescPackInner(const def::UbseMemShmImportDesc& importDesc, UbsePackUtil& packUtil)
{
    // 打包memid_num
    packUtil.UbsePackUint32(importDesc.memIds.size());
    // 打包memids数组 (仅打包有效部分)
    for (const auto memId : importDesc.memIds) {
        packUtil.UbsePackUint64(memId);
    }
    // 打包importNode
    auto ret = UbseBaseNodePackInner(importDesc.importNode, packUtil);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "importNode pack failed";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    // 打包state
    packUtil.UbsePackUint32(static_cast<uint32_t>(importDesc.state));
    return UBSE_OK;
}
static uint32_t UbseMemShmDescPackInner(const def::UbseMemShmDesc& shmDesc, UbsePackUtil& packUtil)
{
    // 打包importDesc size
    packUtil.UbsePackUint32(shmDesc.importDesc.size());

    // 打包name
    if (!packUtil.UbsePackString(shmDesc.name, MAX_MEM_RESOURCE_NAME_LENGTH - 1)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    // 打包mem_size (64位)
    packUtil.UbsePackUint64(shmDesc.totalMemSize);

    // 打包unit_size
    packUtil.UbsePackUint64(shmDesc.unitSize);
    // 打包exportNode
    auto ret = UbseBaseNodePackInner(shmDesc.exportNode, packUtil);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "exportNode pack failed.";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    // 打包usrInfo
    for (int i = 0; i < UBSE_MAX_USR_INFO_LEN; i++) {
        if (!packUtil.UbsePackUint8(shmDesc.userInfo[i])) {
            UBSE_LOG_ERROR << "userInfo pack failed.";
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
    }
    // 打包state
    packUtil.UbsePackUint32(static_cast<uint32_t>(shmDesc.state));
    // 打包importDesc;
    int count = 0;
    for (auto importDesc : shmDesc.importDesc) {
        ret = UbseMemShmImportDescPackInner(importDesc, packUtil);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "importDesc pack failed: ret:" << ret << ", index: " << count;
            return ret;
        }
        count++;
    }
    return UBSE_OK;
}

static uint32_t UnpackNodeList(def::UbseMemShmRegion& region, UbseUnpackUtil& unPackUtil)
{
    if (!unPackUtil.UnpackUint32(region.nodeCnt)) {
        UBSE_LOG_WARN << "unpack region nodeCnt failed.";
        region.nodeCnt = 0;
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (region.nodeCnt > UBS_MEM_MAX_SLOT_NUM) {
        UBSE_LOG_ERROR << "nodeCnt(" << region.nodeCnt << ") is invalid.";
        return UBSE_ERROR_INVAL;
    }
    for (int i = 0; i < region.nodeCnt; i++) {
        if (!unPackUtil.UnpackUint32(region.slotIds[i])) {
            UBSE_LOG_WARN << "unpack region slotIds[" << i << "]failed.";
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
    }
    return UBSE_OK;
}

static uint32_t UnpackshmRegionList(UbseShmRegionDesc& region, UbseUnpackUtil& unPackUtil)
{
    region.nodelist.clear();
    if (!unPackUtil.UnpackUint32(region.nodeNum)) {
        UBSE_LOG_WARN << "unpack region nodeCnt failed.";
        region.nodeNum = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (region.nodeNum > UBS_MEM_MAX_SLOT_NUM) {
        UBSE_LOG_ERROR << "nodeCnt(" << region.nodeNum << ") is invalid.";
        return UBSE_ERROR_INVAL;
    }
    for (int i = 0; i < region.nodeNum; i++) {
        uint32_t slotId;
        if (!unPackUtil.UnpackUint32(slotId)) {
            UBSE_LOG_WARN << "unpack region slotIds[" << i << "]failed.";
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
        ubse::adapter_plugins::mmi::UbseNodeInfo ubseNodeInfo;
        ubseNodeInfo.index = slotId;
        const std::string shmNodeId = std::to_string(slotId);
        ubseNodeInfo.nodeId = shmNodeId;
        region.nodelist.push_back(ubseNodeInfo);
    }
    return UBSE_OK;
}

static uint32_t LenderInfoUnpack(UbseUnpackUtil& unpackUtil, UbseNumaLocation& loc, uint64_t& lenderSize,
                                 uint32_t& socketId, uint32_t& portId)
{
    if (!unpackUtil.UnpackUint64(lenderSize)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    uint32_t slotId{};
    if (!unpackUtil.UnpackUint32(slotId)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    loc.nodeId = std::to_string(slotId);
    if (!unpackUtil.UnpackUint32(socketId)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    uint32_t numaId{};
    if (!unpackUtil.UnpackUint32(numaId)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    loc.numaId = numaId;
    if (!unpackUtil.UnpackUint32(portId)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}

uint32_t UnpackMemName(std::string& name, UbseUnpackUtil& unpackUtil)
{
    // 解包 name
    if (!unpackUtil.UnpackString(name, MAX_MEM_RESOURCE_NAME_LENGTH - 1)) {
        UBSE_LOG_ERROR << "unpack name failed.";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (!util::CheckName(name)) {
        UBSE_LOG_ERROR << "Invalid name";
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

uint32_t UbseMemShmCreateReqUnpack(const UbseIpcMessage& buffer, def::UbseMemShmDispatcher& memShmDispatcher)
{
    UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    // 解包 name
    if (auto ret = UnpackMemName(memShmDispatcher.name, unpackUtil); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack name failed.";
        return ret;
    }
    // 解包size
    if (!unpackUtil.UnpackUint64(memShmDispatcher.size)) {
        UBSE_LOG_ERROR << "unpack size failed.";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    // 解包usr_info
    for (int i = 0; i < UBSE_MAX_USR_INFO_LEN; i++) {
        if (!unpackUtil.UnpackUint8(memShmDispatcher.usrInfo[i])) {
            UBSE_LOG_ERROR << "unpack usrInfo failed.";
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
    }
    // 解包flag
    if (!unpackUtil.UnpackUint64(memShmDispatcher.flag)) {
        UBSE_LOG_ERROR << "unpack flag failed.";
    }
    // 解包region
    auto ret = UnpackNodeList(memShmDispatcher.shmRegion, unpackUtil);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack failed.";
        return ret;
    }
    // 解包provier
    ret = UnpackNodeList(memShmDispatcher.provider, unpackUtil);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "provider unpack unsuccessful.";
        if (ret == UBSE_ERROR_INVAL) {
            UBSE_LOG_ERROR << "provider node cnt is invalid";
            return ret;
        }
    }
    return UBSE_OK;
}

uint32_t UbseMemShmCreateWithAffinityReqUnpack(const UbseIpcMessage& buffer,
                                               def::UbseMemShmDispatcher& memShmDispatcher)
{
    UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    // 解包 name
    if (auto ret = UnpackMemName(memShmDispatcher.name, unpackUtil); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack name failed.";
        return ret;
    }
    // 解包size
    if (!unpackUtil.UnpackUint64(memShmDispatcher.size)) {
        UBSE_LOG_ERROR << "unpack size failed.";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    // 解包usr_info
    for (int i = 0; i < UBSE_MAX_USR_INFO_LEN; i++) {
        if (!unpackUtil.UnpackUint8(memShmDispatcher.usrInfo[i])) {
            UBSE_LOG_ERROR << "unpack usrInfo failed.";
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
    }
    // 解包flag
    if (!unpackUtil.UnpackUint64(memShmDispatcher.flag)) {
        UBSE_LOG_ERROR << "unpack flag failed.";
    }

    // 解包affinitySocketId
    if (!unpackUtil.UnpackUint32(memShmDispatcher.affinitySocketId)) {
        UBSE_LOG_ERROR << "affinitySocketId is invalid";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    // 解包region
    auto ret = UnpackNodeList(memShmDispatcher.shmRegion, unpackUtil);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack failed.";
        return ret;
    }
    // 解包provier
    ret = UnpackNodeList(memShmDispatcher.provider, unpackUtil);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "provider unpack unsuccessful.";
        if (ret == UBSE_ERROR_INVAL) {
            UBSE_LOG_ERROR << "provider node cnt is invalid";
            return ret;
        }
    }
    return UBSE_OK;
}

uint32_t UbseMemShmCreateWithLenderReqUnpack(const UbseIpcMessage& buffer, UbseMemShareBorrowReq& memShmBorrowReq,
                                             uint64_t& flag)
{
    UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    if (auto ret = UnpackMemName(memShmBorrowReq.name, unpackUtil); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack name failed.";
        return ret;
    }

    for (int i = 0; i < UBSE_MAX_USR_INFO_LEN; i++) {
        if (!unpackUtil.UnpackUint8(memShmBorrowReq.usrInfo[i])) {
            UBSE_LOG_ERROR << "unpack usrInfo failed.";
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
    }
    if (!unpackUtil.UnpackUint64(flag)) {
        UBSE_LOG_ERROR << "unpack flag failed.";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    auto ret = UnpackshmRegionList(memShmBorrowReq.shmRegion, unpackUtil);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack failed.";
        return ret;
    }

    UbseNumaLocation numaLocation{};
    uint64_t lenderSize{};
    uint32_t socketId{};
    uint32_t portId{};
    if (LenderInfoUnpack(unpackUtil, numaLocation, lenderSize, socketId, portId) != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack failed.";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    memShmBorrowReq.size = lenderSize;
    memShmBorrowReq.lenderInfo.lender_size = lenderSize;
    memShmBorrowReq.lenderInfo.nodeId = numaLocation.nodeId;
    memShmBorrowReq.lenderInfo.socketId = socketId;
    memShmBorrowReq.lenderInfo.numaId = numaLocation.numaId;
    memShmBorrowReq.lenderInfo.portId = portId;
    return UBSE_OK;
}

uint32_t UbseMemShmAttachReqUnpack(const UbseIpcMessage& buffer, UbseMemShareAttachReq& memShareAttachReq)
{
    UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    // 解包name
    if (auto ret = UnpackMemName(memShareAttachReq.name, unpackUtil); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack name failed.";
        return ret;
    }

    // 解包 owner
    if (!UbseOwnerUnpack(unpackUtil, memShareAttachReq.owner)) {
        UBSE_LOG_ERROR << "unpack owner failed.";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}
uint32_t UbseMemNameUnpack(const UbseIpcMessage& buffer, std::string& name)
{
    UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    // 解包 name
    if (auto ret = UnpackMemName(name, unpackUtil); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack name failed.";
        return ret;
    }
    return UBSE_OK;
}
uint32_t UbseMemShmGetReqUnpack(const UbseIpcMessage& buffer, std::string& name)
{
    return UbseMemNameUnpack(buffer, name);
}
uint32_t UbseMemShmDetachReqUnpack(const UbseIpcMessage& buffer, UbseMemShareDetachReq& memShareDetachReq)
{
    return UbseMemNameUnpack(buffer, memShareDetachReq.name);
}
uint32_t UbseMemShmDeleteReqUnpack(const UbseIpcMessage& buffer, UbseMemReturnReq& memReturnReq)
{
    return UbseMemNameUnpack(buffer, memReturnReq.name);
}
uint32_t UbseMemShmAttachResponsePack(def::UbseMemShmDesc& shmDesc, UbseIpcMessage& buffer)
{
    return UbseMemShmGetResponsePack(shmDesc, buffer);
}

size_t UbseMemNodeCalcSize(const def::UbseNode& node)
{
    size_t len = 0;
    len += sizeof(uint32_t);
    len += sizeof(uint32_t) * UBS_TOPO_SOCKET_NUM; // 2个socketId
    len += sizeof(uint32_t) * UBS_TOPO_SOCKET_NUM * UBS_TOPO_NUMA_NUM;
    len += UbseStringCalcSize(node.hostName, HOST_NAME_MAX);
    return len;
}

size_t UbseMemShmImportDescCalSize(const def::UbseMemShmImportDesc& importDesc)
{
    size_t len = sizeof(uint32_t);                      // memIds size = memid_cnt
    len += sizeof(uint64_t) * importDesc.memIds.size(); // memIds
    len += UbseMemNodeCalcSize(importDesc.importNode);  // importNode
    len += sizeof(uint32_t);                            // state
    return len;
}
size_t UbseMemShmImportDescListCalSize(std::vector<def::UbseMemShmImportDesc> importDescs)
{
    if (importDescs.empty()) {
        return 0;
    }
    size_t len = 0;
    for (const auto& importDesc : importDescs) {
        len += UbseMemShmImportDescCalSize(importDesc);
    }
    return len;
}
size_t UbseMemShmDescCalcSize(const def::UbseMemShmDesc& shmDesc)
{
    // import_node_cnt放首位，方便解包时，取出import_node_cnt,然后分配全量内存
    return sizeof(uint32_t) +                                                   // import_node_cnt
           UbseStringCalcSize(shmDesc.name, MAX_MEM_RESOURCE_NAME_LENGTH - 1) + // name
           sizeof(uint64_t) +                                                   // mem_size
           sizeof(size_t) +                                                     // unit_size
           UbseMemNodeCalcSize(shmDesc.exportNode) +                            // exportNode
           UBSE_MAX_USR_INFO_LEN +                                              // usr_info
           sizeof(uint32_t) +                                                   // state
           UbseMemShmImportDescListCalSize(shmDesc.importDesc);                 // importDesc
}
size_t UbseMemShmDescListCalcSize(const std::vector<def::UbseMemShmDesc>& shmDescs)
{
    uint32_t len = 0;
    // desc数量
    len += sizeof(uint32_t); // shm_desc_cnt;
    // desc中每个import_desc_cnt数量；
    len += shmDescs.size() * sizeof(uint32_t); // shm_desc_cnt * import_desc_cnt;
    // 每个ShmDesc中每个元素的长度
    for (auto shmDesc : shmDescs) {
        len += UbseMemShmDescCalcSize(shmDesc);
    }
    return len;
}
uint32_t UbseMemShmGetResponsePack(def::UbseMemShmDesc& shmDesc, UbseIpcMessage& buffer)
{
    // 计算总需求长度
    const size_t requiredLength = UbseMemShmDescCalcSize(shmDesc);
    buffer.length = requiredLength;
    buffer.buffer = new (std::nothrow) uint8_t[requiredLength];
    if (buffer.buffer == nullptr) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbsePackUtil packUtil(buffer.buffer, requiredLength);
    if (const auto ret = UbseMemShmDescPackInner(shmDesc, packUtil); ret != UBSE_OK) {
        delete[] buffer.buffer;
        buffer.buffer = nullptr;
        buffer.length = 0;
        return ret;
    }
    return UBSE_OK;
}
size_t UbseMemShmMemStatusDescCalcSize(const def::UbseMemShmMemStatusDesc& shmDesc)
{
    uint32_t len = 0;
    len += sizeof(uint32_t);
    size_t memIdCnt = shmDesc.memIds.size();
    for (int i = 0; i < memIdCnt; i++) {
        len += sizeof(uint64_t); // for memid
        len += sizeof(uint32_t); // for state
    }
    return len;
}
static uint32_t UbseMemShmMemFaultGetResponsePackInner(const def::UbseMemShmMemStatusDesc& statusDesc,
                                                       UbsePackUtil& packUtil)
{
    if (statusDesc.memIds.size() > UBS_MEM_MAX_MEMID_NUM) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    packUtil.UbsePackUint32(statusDesc.memIds.size());
    for (int i = 0; i < statusDesc.memIds.size(); i++) {
        packUtil.UbsePackUint64(statusDesc.memIds[i]);
        packUtil.UbsePackUint32(static_cast<uint32_t>(statusDesc.faultTypes[i]));
    }
    return UBSE_OK;
}
uint32_t UbseMemShmMemFaultGetResponsePack(def::UbseMemShmMemStatusDesc& statusDesc, UbseIpcMessage& buffer)
{
    const size_t requiredLength = UbseMemShmMemStatusDescCalcSize(statusDesc);

    buffer.length = requiredLength;
    buffer.buffer = new (std::nothrow) uint8_t[requiredLength];
    if (buffer.buffer == nullptr) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbsePackUtil packUtil(buffer.buffer, requiredLength);
    if (const auto ret = UbseMemShmMemFaultGetResponsePackInner(statusDesc, packUtil); ret != UBSE_OK) {
        delete[] buffer.buffer;
        buffer.buffer = nullptr;
        buffer.length = 0;
        return ret;
    }
    return UBSE_OK;
}

uint32_t UbseMemShmtatusGetReqUnPack(const UbseIpcMessage& buffer, std::string& name)
{
    UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    // 解包 name
    if (auto ret = UnpackMemName(name, unpackUtil); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack name failed.";
        return ret;
    }
    return UBSE_OK;
}
uint32_t UbseMemShmListResponsePack(const std::vector<def::UbseMemShmDesc>& shmDescs, UbseIpcMessage& buffer)
{
    const uint32_t requiredLength = UbseMemShmDescListCalcSize(shmDescs);
    buffer.length = requiredLength;
    buffer.buffer = new (std::nothrow) uint8_t[requiredLength];
    if (buffer.buffer == nullptr) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbsePackUtil packUtil(buffer.buffer, requiredLength);
    // 打包 desc 数量
    packUtil.UbsePackUint32(shmDescs.size());
    // 打包每个desc中的import_desc数量
    for (const auto& shmDesc : shmDescs) {
        packUtil.UbsePackUint32(shmDesc.importDesc.size());
    }
    // 循环打包每个 desc
    for (const auto& shmDesc : shmDescs) {
        if (const auto ret = UbseMemShmDescPackInner(shmDesc, packUtil); ret != UBSE_OK) {
            delete[] buffer.buffer;
            buffer.buffer = nullptr;
            buffer.length = 0;
            return ret;
        }
    }
    return UBSE_OK;
}

uint32_t UbseMemCreateReqUnpack(const UbseIpcMessage& buffer, UbseMemFdBorrowReq& memFdBorrowReq)
{
    UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    // 解包 name
    if (auto ret = UnpackMemName(memFdBorrowReq.name, unpackUtil); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack name failed.";
        return ret;
    }
    // 解包size
    if (!unpackUtil.UnpackUint64(memFdBorrowReq.size)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    // 解包 owner
    if (!UbseOwnerUnpack(unpackUtil, memFdBorrowReq.owner)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    // 解包distance
    if (!unpackUtil.UnpackEnum(memFdBorrowReq.distance)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}

static bool CandidateInfoUnpack(UbseUnpackUtil& unpackUtil, std::vector<std::string>& candidateNodeList)
{
    // 解包slotId
    uint32_t slotIdCount;
    if (!unpackUtil.UnpackUint32(slotIdCount)) {
        return false;
    }
    if (slotIdCount > MAX_SLOT_ID_COUNT) {
        UBSE_LOG_ERROR << "slotIdCount is too large, slotIdCount:" << slotIdCount;
        return false;
    }
    // 解包slotIds
    uint32_t slotId = 0;
    for (int i = 0; i < slotIdCount; i++) {
        if (!unpackUtil.UnpackUint32(slotId)) {
            return false;
        }
        candidateNodeList.push_back(std::to_string(slotId));
    }
    return true;
}

uint32_t UbseMemCreateWithLenderReqUnpack(const UbseIpcMessage& buffer, UbseMemFdBorrowReq& memFdBorrowReq)
{
    UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    // 解包 name
    if (auto ret = UnpackMemName(memFdBorrowReq.name, unpackUtil); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack name failed.";
        return ret;
    }
    // 解包 owner
    if (!UbseOwnerUnpack(unpackUtil, memFdBorrowReq.owner)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    // 解包lenderCnt
    uint32_t lenderCnt{};
    if (!unpackUtil.UnpackUint32(lenderCnt)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (lenderCnt > MAX_LENDER_CNT || lenderCnt == 0) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    memFdBorrowReq.size = 0;
    for (int i = 0; i < lenderCnt; ++i) {
        // 解包lender
        UbseNumaLocation numaLocation{};
        uint64_t lenderSize{};
        uint32_t socketId{};
        uint32_t portId{};
        if (LenderInfoUnpack(unpackUtil, numaLocation, lenderSize, socketId, portId) != UBSE_OK) {
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
        memFdBorrowReq.lenderLocs.push_back(std::move(numaLocation));
        memFdBorrowReq.lenderSizes.push_back(lenderSize);
        memFdBorrowReq.size += lenderSize;
    }

    return UBSE_OK;
}

uint32_t UbseMemCreateWithCandidateReqUnpack(const UbseIpcMessage& buffer, UbseMemFdBorrowReq& memFdBorrowReq)
{
    UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    // 解包 name
    if (auto ret = UnpackMemName(memFdBorrowReq.name, unpackUtil); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack name failed.";
        return ret;
    }
    // 解包size
    if (!unpackUtil.UnpackUint64(memFdBorrowReq.size)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    // 解包 uds_info
    if (!UbseOwnerUnpack(unpackUtil, memFdBorrowReq.owner)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    // 解包candidateInfo
    if (!CandidateInfoUnpack(unpackUtil, memFdBorrowReq.candidateNodeList)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}

uint32_t UbseMemFdPermissionReqUnpack(const UbseIpcMessage& buffer, UbseMemFdPermissionReq& memFdPermissionReq)
{
    UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    // 解包 name
    if (auto ret = UnpackMemName(memFdPermissionReq.name, unpackUtil); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack name failed.";
        return ret;
    }
    // 解包 owner
    if (!UbseOwnerUnpack(unpackUtil, memFdPermissionReq.fdOwner)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}

size_t UbseStringCalcSize(const std::string& str, size_t maxLen)
{
    size_t len = 0;
    len += sizeof(uint32_t);
    len += std::min(str.size(), maxLen);
    return len;
}

size_t UbseMemFdDescCalcSize(const UbseMemOperationResp& memOperationResp, const UbseNode& exportNode,
                             const UbseNode& importNode)
{
    return UbseStringCalcSize(memOperationResp.name, MAX_MEM_RESOURCE_NAME_LENGTH - 1) + // name
           sizeof(uint32_t) +                                                            // memid_cnt
           sizeof(uint64_t) * memOperationResp.memIdList.size() +                        // memids有效部分
           sizeof(uint64_t) +                                                            // mem_size
           sizeof(size_t) +                                                              // unit_size
           UbseMemNodeCalcSize(exportNode) +                                             // exportNode
           UbseMemNodeCalcSize(importNode);                                              // importNode
}

size_t UbseMemFdDescCalcSize(const ubse::mem::def::UbseMemFdDesc& fdDesc)
{
    return UbseStringCalcSize(fdDesc.name, MAX_MEM_RESOURCE_NAME_LENGTH - 1) + // name
           sizeof(uint32_t) +                                                  // memid_cnt
           sizeof(uint64_t) * fdDesc.memIds.size() +                           // memids有效部分
           sizeof(uint64_t) +                                                  // mem_size
           sizeof(size_t) +                                                    // unit_size
           UbseMemNodeCalcSize(fdDesc.exportNode) +                            // exportNode
           UbseMemNodeCalcSize(fdDesc.importNode) +                            // importNode
           sizeof(uint32_t);                                                   // state
}

size_t UbseMemFdDescListCalcSize(const std::vector<ubse::mem::def::UbseMemFdDesc>& fdDescList)
{
    uint32_t len = 0;
    len += sizeof(uint32_t);
    for (const auto& fdDesc : fdDescList) {
        len += UbseMemFdDescCalcSize(fdDesc);
    }
    return len;
}

size_t UbseMemNumaDescCalcSize(const ubse::mem::def::UbseMemNumaDesc& numaDesc)
{
    return UbseStringCalcSize(numaDesc.name, MAX_MEM_RESOURCE_NAME_LENGTH - 1) + // name
           sizeof(int64_t) +                                                     // numaId
           UbseMemNodeCalcSize(numaDesc.exportNode) +                            // exportNode
           UbseMemNodeCalcSize(numaDesc.importNode) +                            // importNode
           sizeof(int64_t) +                                                     // size
           sizeof(uint32_t) +                                                    // state
           UBSE_MAX_USR_INFO_LENGTH;                                             // usrInfo
}

size_t UbseMemNumaDescListCalcSize(const std::vector<ubse::mem::def::UbseMemNumaDesc>& numaDescList)
{
    uint32_t len = 0;
    len += sizeof(uint32_t);
    for (const auto& numaDesc : numaDescList) {
        len += UbseMemNumaDescCalcSize(numaDesc);
    }
    return len;
}

uint32_t UbseMemFdDeleteReqUnpack(const UbseIpcMessage& buffer, UbseMemReturnReq& req)
{
    UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    // 解包 name
    if (auto ret = UnpackMemName(req.name, unpackUtil); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack name failed.";
        return ret;
    }
    return UBSE_OK;
}

uint32_t UbseMemNumaCreateReqUnpack(const UbseIpcMessage& buffer, UbseMemNumaBorrowReq& memNumaBorrowReq)
{
    UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    // 解包 name
    if (auto ret = UnpackMemName(memNumaBorrowReq.name, unpackUtil); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack name failed.";
        return ret;
    }
    // 解包size
    if (!unpackUtil.UnpackUint64(memNumaBorrowReq.size)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    // 解包distance
    if (!unpackUtil.UnpackEnum(memNumaBorrowReq.distance)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}

uint32_t UbseMemNumaCreateLenderReqUnpack(const UbseIpcMessage& buffer, UbseMemNumaBorrowReq& memNumaBorrowReq)
{
    UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    // 解包 name
    if (auto ret = UnpackMemName(memNumaBorrowReq.name, unpackUtil); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack name failed.";
        return ret;
    }
    // 解包lenderCnt
    uint32_t lenderCnt{};
    if (!unpackUtil.UnpackUint32(lenderCnt)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (lenderCnt > MAX_LENDER_CNT || lenderCnt == 0) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    // 解包lender
    std::vector<UbseNumaLocation> lenderLocs{};
    std::vector<uint64_t> lenderSizes{};
    UbseMemLenderLinkInfo linkInfo{};
    for (int i = 0; i < lenderCnt; ++i) {
        UbseNumaLocation numaLocation{};
        uint64_t lenderSize{};
        uint32_t portId{};
        uint32_t socketId{};
        if (LenderInfoUnpack(unpackUtil, numaLocation, lenderSize, socketId, portId) != UBSE_OK) {
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
        if (portId != SDK_DEFAULT_VALUE) {
            linkInfo.lenderNode = numaLocation.nodeId;
            linkInfo.lenderPort = portId;
            linkInfo.lenderSocketId = static_cast<int>(socketId);
            memNumaBorrowReq.lowWatermark = 0;
            memNumaBorrowReq.highWatermark = HIGH_WATER_MARK;
        }
        memNumaBorrowReq.size += lenderSize;
        if (numaLocation.numaId == SDK_DEFAULT_VALUE) {
            continue;
        }
        lenderLocs.push_back(numaLocation);
        lenderSizes.push_back(lenderSize);
    }
    memNumaBorrowReq.linkInfo = linkInfo;
    memNumaBorrowReq.lenderLocs = std::move(lenderLocs);
    memNumaBorrowReq.lenderSizes = std::move(lenderSizes);
    return UBSE_OK;
}

uint32_t UbseMemNumaCreateWithCandidateReqUnpack(const UbseIpcMessage& buffer, UbseMemNumaBorrowReq& memNumaBorrowReq)
{
    UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    // 解包 name
    if (auto ret = UnpackMemName(memNumaBorrowReq.name, unpackUtil); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack name failed.";
        return ret;
    }
    // 解包size
    if (!unpackUtil.UnpackUint64(memNumaBorrowReq.size)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    // 解包candidate
    if (!CandidateInfoUnpack(unpackUtil, memNumaBorrowReq.candidateNodeList)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return UBSE_OK;
}

uint32_t UbseMemNumaDeleteUnpack(const UbseIpcMessage& buffer, UbseMemReturnReq& req)
{
    UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    // 解包 name
    if (auto ret = UnpackMemName(req.name, unpackUtil); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack name failed.";
        return ret;
    }
    return UBSE_OK;
}

uint32_t UbseMemFdDescPack(const ubse::mem::def::UbseMemFdDesc& fdDesc, UbseIpcMessage& buffer)
{
    // 参数校验
    if (fdDesc.memIds.size() > UBS_MEM_MAX_MEMID_NUM) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    // 计算总需求长度
    const size_t requiredLength = UbseMemFdDescCalcSize(fdDesc);

    buffer.buffer = new (std::nothrow) uint8_t[requiredLength];
    if (buffer.buffer == nullptr) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbsePackUtil packUtil(buffer.buffer, requiredLength);
    auto ret = UbseMemFdDescPackInner(fdDesc, packUtil);
    if (ret != UBSE_OK) {
        delete[] buffer.buffer;
        buffer.buffer = nullptr;
        buffer.length = 0;
        return ret;
    }
    buffer.length = requiredLength;
    return UBSE_OK;
}

uint32_t UbseMemFdDescListPack(const std::vector<ubse::mem::def::UbseMemFdDesc>& fdDescList, UbseIpcMessage& buffer)
{
    // 计算总需求长度
    const size_t requiredLength = UbseMemFdDescListCalcSize(fdDescList);

    buffer.buffer = new (std::nothrow) uint8_t[requiredLength];
    if (buffer.buffer == nullptr) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbsePackUtil packUtil(buffer.buffer, requiredLength);
    // 打包count
    packUtil.UbsePackUint32(fdDescList.size());
    // 打包fdDescList
    for (const auto& fdDesc : fdDescList) {
        auto ret = UbseMemFdDescPackInner(fdDesc, packUtil);
        if (ret != UBSE_OK) {
            delete[] buffer.buffer;
            buffer.buffer = nullptr;
            buffer.length = 0;
            return ret;
        }
    }
    buffer.length = requiredLength;
    return UBSE_OK;
}

static uint32_t UbseMemNumaDescPackInner(const ubse::mem::def::UbseMemNumaDesc& numaDesc, UbsePackUtil& packUtil)
{
    // 打包name
    if (!packUtil.UbsePackString(numaDesc.name, MAX_MEM_RESOURCE_NAME_LENGTH - 1)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    // 打包numaId
    packUtil.UbsePackInt64(numaDesc.numaId);

    // 打包exportNode
    auto ret = UbseBaseNodePackInner(numaDesc.exportNode, packUtil);
    if (ret != UBSE_OK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    ret = UbseBaseNodePackInner(numaDesc.importNode, packUtil);
    if (ret != UBSE_OK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    // 打包size
    packUtil.UbsePackUint64(numaDesc.size);

    // 打包state
    packUtil.UbsePackUint32(static_cast<uint32_t>(numaDesc.state));

    // 打包usrInfo
    for (const uint8_t& i : numaDesc.usrInfo) {
        packUtil.UbsePackUint8(i);
    }
    return UBSE_OK;
}

uint32_t UbseMemNumaDescPack(const ubse::mem::def::UbseMemNumaDesc& numaDesc, UbseIpcMessage& buffer)
{
    // 计算总需求长度
    const size_t requiredLength = UbseMemNumaDescCalcSize(numaDesc);

    buffer.buffer = new (std::nothrow) uint8_t[requiredLength];
    if (buffer.buffer == nullptr) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbsePackUtil packUtil(buffer.buffer, requiredLength);
    auto ret = UbseMemNumaDescPackInner(numaDesc, packUtil);
    if (ret != UBSE_OK) {
        delete[] buffer.buffer;
        buffer.buffer = nullptr;
        buffer.length = 0;
        return ret;
    }
    buffer.length = requiredLength;
    return UBSE_OK;
}

uint32_t UbseMemNumaDescListPack(const std::vector<ubse::mem::def::UbseMemNumaDesc>& numaDescList,
                                 UbseIpcMessage& buffer)
{
    // 计算总需求长度
    const size_t requiredLength = UbseMemNumaDescListCalcSize(numaDescList);

    buffer.buffer = new (std::nothrow) uint8_t[requiredLength];
    if (buffer.buffer == nullptr) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbsePackUtil packUtil(buffer.buffer, requiredLength);
    // 打包count
    packUtil.UbsePackUint32(numaDescList.size());
    // 打包fdDescList
    for (const auto& numaDesc : numaDescList) {
        auto ret = UbseMemNumaDescPackInner(numaDesc, packUtil);
        if (ret != UBSE_OK) {
            delete[] buffer.buffer;
            buffer.buffer = nullptr;
            buffer.length = 0;
            return ret;
        }
    }
    buffer.length = requiredLength;
    return UBSE_OK;
}

uint32_t UbseMemGetMemIdByImportReqUnpack(const UbseIpcMessage& buffer, def::UbseMemIdQueryRequest& req)
{
    if (!buffer.buffer) {
        UBSE_LOG_ERROR << "buffer.buffer is null";
        return UBSE_ERROR_NULLPTR;
    }
    UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    // 解包 name
    if (auto ret = UnpackMemName(req.name, unpackUtil); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack name failed.";
        return ret;
    }
    // 解包import_memid
    if (!unpackUtil.UnpackUint64(req.importMemId)) {
        UBSE_LOG_ERROR << "unpack importMemId failed.";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return UBSE_OK;
}

uint32_t UbseMemGetMemIdByImportResponsePack(const def::UbseExportMemDesc& memDesc, UbseIpcMessage& buffer)
{
    uint32_t len = 0;
    len += sizeof(uint32_t); // for exportSlotId
    len += sizeof(uint64_t); // for exportMemId

    buffer.length = len;
    buffer.buffer = new (std::nothrow) uint8_t[len];

    if (buffer.buffer == nullptr) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbsePackUtil packUtil(buffer.buffer, len);
    packUtil.UbsePackUint32(memDesc.exportSlotId);
    packUtil.UbsePackUint64(memDesc.exportMemId);
    return UBSE_OK;
}
} // namespace ubse::mem::controller
