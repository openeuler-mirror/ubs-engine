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

#ifndef UBSE_MEM_AUTO_SERIAL_H
#define UBSE_MEM_AUTO_SERIAL_H

#include <type_traits>

#include "ubse_mem_controller.h"
#include "ubse_node_controller_def.h"
#include "adapter_plugins/mmi/ubse_mmi_interface.h"
#include "adapter_plugins/mti/ubse_mti_mami_def.h"
#include "src/controllers/mem/mem_controller/ubse_mem_controller_def.h"
#include "src/framework/misc/ubse_auto_serial_util.hpp"
#include "ubs_engine_topo.h"

// Trait specializations for raw_memcopy / bitfield types.
namespace ubse::serial::util {

using namespace ubse::adapter_plugins::mmi;
using namespace ubse::mem::def;
using namespace ubse::mem::controller;
using namespace ubse::nodeController::def;
using namespace ubse::adapter_plugins::mti::mami;

// --- Raw memory copy (opt-in via is_raw_memcopy trait, defined in ubse_auto_serial_util.hpp) ---
template <>
struct is_raw_memcopy<ubse_mem_obmm_mem_desc> : std::true_type {
};
template <>
struct is_raw_memcopy<ubs_topo_ip_address_t> : std::true_type {
};
template <>
struct is_raw_memcopy<UbseTopoIpAddress> : std::true_type {
};
template <>
struct is_raw_memcopy<UbseMemAddrInfo> : std::true_type {
};

// --- UbseMemPrivData (bitfield packed as uint16_t) ---
template <>
struct is_bitfield<UbseMemPrivData> : std::true_type {
};

template <>
struct bitfield_traits<UbseMemPrivData> {
    static uint16_t pack(const UbseMemPrivData& data)
    {
        uint16_t val = 0;
        val |= static_cast<uint16_t>(data.onePth) << 0;
        val |= static_cast<uint16_t>(data.wrDelayComp) << 1;
        val |= static_cast<uint16_t>(data.reduceDelayComp) << 2;
        val |= static_cast<uint16_t>(data.cmoDelayComp) << 3;
        val |= static_cast<uint16_t>(data.so) << 4;
        val |= static_cast<uint16_t>(data.adTrOchip) << 5;
        val |= static_cast<uint16_t>(data.cacheableFlag) << 6;
        val |= static_cast<uint16_t>(data.marId) << 7;
        val |= static_cast<uint16_t>(data.rsv0) << 10;
        return val;
    }

    static void unpack(uint16_t val, UbseMemPrivData& data)
    {
        data.onePth = (val >> 0) & 0x1;
        data.wrDelayComp = (val >> 1) & 0x1;
        data.reduceDelayComp = (val >> 2) & 0x1;
        data.cmoDelayComp = (val >> 3) & 0x1;
        data.so = (val >> 4) & 0x1;
        data.adTrOchip = (val >> 5) & 0x1;
        data.cacheableFlag = (val >> 6) & 0x1;
        data.marId = (val >> 7) & 0x7;
        data.rsv0 = (val >> 10) & 0x3F;
    }
};

} // namespace ubse::serial::util

// UBSE_SERIALIZE type registrations.
namespace ubse::serial::util {

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseUdsInfo, void, &::ubse::adapter_plugins::mmi::UbseUdsInfo::uid,
               &::ubse::adapter_plugins::mmi::UbseUdsInfo::gid, &::ubse::adapter_plugins::mmi::UbseUdsInfo::pid,
               &::ubse::adapter_plugins::mmi::UbseUdsInfo::username)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::FdOwner, void, &::ubse::adapter_plugins::mmi::FdOwner::uid,
               &::ubse::adapter_plugins::mmi::FdOwner::gid, &::ubse::adapter_plugins::mmi::FdOwner::pid,
               &::ubse::adapter_plugins::mmi::FdOwner::mode)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseNumaLocation, void,
               &::ubse::adapter_plugins::mmi::UbseNumaLocation::nodeId,
               &::ubse::adapter_plugins::mmi::UbseNumaLocation::numaId)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseTrustRingData, void,
               &::ubse::adapter_plugins::mmi::UbseTrustRingData::trustRingId,
               &::ubse::adapter_plugins::mmi::UbseTrustRingData::reqSignedData,
               &::ubse::adapter_plugins::mmi::UbseTrustRingData::lendSignedDatas)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo, void,
               &::ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo::nodeId,
               &::ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo::socketId,
               &::ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo::numaId,
               &::ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo::size,
               &::ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo::chipId,
               &::ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo::portId)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemAlgoResult, void,
               &::ubse::adapter_plugins::mmi::UbseMemAlgoResult::exportNumaInfos,
               &::ubse::adapter_plugins::mmi::UbseMemAlgoResult::importNumaInfos,
               &::ubse::adapter_plugins::mmi::UbseMemAlgoResult::blockSize,
               &::ubse::adapter_plugins::mmi::UbseMemAlgoResult::attachSocketId)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemObmmInfo, void,
               &::ubse::adapter_plugins::mmi::UbseMemObmmInfo::memId,
               &::ubse::adapter_plugins::mmi::UbseMemObmmInfo::desc,
               &::ubse::adapter_plugins::mmi::UbseMemObmmInfo::memIdStatus)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemImportResult, void,
               &::ubse::adapter_plugins::mmi::UbseMemImportResult::memId,
               &::ubse::adapter_plugins::mmi::UbseMemImportResult::numaId)

UBSE_SERIALIZE(::ubse::adapter_plugins::mti::mami::UbseMamiMemImportResult, void,
               &::ubse::adapter_plugins::mti::mami::UbseMamiMemImportResult::marId,
               &::ubse::adapter_plugins::mti::mami::UbseMamiMemImportResult::hpa,
               &::ubse::adapter_plugins::mti::mami::UbseMamiMemImportResult::handle,
               &::ubse::adapter_plugins::mti::mami::UbseMamiMemImportResult::staticHandle,
               &::ubse::adapter_plugins::mti::mami::UbseMamiMemImportResult::valid)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemLenderLinkInfo, void,
               &::ubse::adapter_plugins::mmi::UbseMemLenderLinkInfo::lenderNode,
               &::ubse::adapter_plugins::mmi::UbseMemLenderLinkInfo::lenderSocketId,
               &::ubse::adapter_plugins::mmi::UbseMemLenderLinkInfo::lenderPort)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseNodeInfo, void, &::ubse::adapter_plugins::mmi::UbseNodeInfo::index,
               &::ubse::adapter_plugins::mmi::UbseNodeInfo::nodeId,
               &::ubse::adapter_plugins::mmi::UbseNodeInfo::hostName,
               &::ubse::adapter_plugins::mmi::UbseNodeInfo::status)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemLenderInfo, void,
               &::ubse::adapter_plugins::mmi::UbseMemLenderInfo::lender_size,
               &::ubse::adapter_plugins::mmi::UbseMemLenderInfo::nodeId,
               &::ubse::adapter_plugins::mmi::UbseMemLenderInfo::socketId,
               &::ubse::adapter_plugins::mmi::UbseMemLenderInfo::numaId,
               &::ubse::adapter_plugins::mmi::UbseMemLenderInfo::portId)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemShmAffinitySocketInfo, void,
               &::ubse::adapter_plugins::mmi::UbseMemShmAffinitySocketInfo::affinitySocketId,
               &::ubse::adapter_plugins::mmi::UbseMemShmAffinitySocketInfo::createReqNodeId,
               &::ubse::adapter_plugins::mmi::UbseMemShmAffinitySocketInfo::enableCreateWithAffinity)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemAttachResourceShareAttr, void,
               &::ubse::adapter_plugins::mmi::UbseMemAttachResourceShareAttr::uid,
               &::ubse::adapter_plugins::mmi::UbseMemAttachResourceShareAttr::gid,
               &::ubse::adapter_plugins::mmi::UbseMemAttachResourceShareAttr::size,
               &::ubse::adapter_plugins::mmi::UbseMemAttachResourceShareAttr::owner)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseShmRegionDesc, void,
               &::ubse::adapter_plugins::mmi::UbseShmRegionDesc::nodeNum,
               &::ubse::adapter_plugins::mmi::UbseShmRegionDesc::nodelist)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemImportStatus, void,
               &::ubse::adapter_plugins::mmi::UbseMemImportStatus::errCode,
               &::ubse::adapter_plugins::mmi::UbseMemImportStatus::scna,
               &::ubse::adapter_plugins::mmi::UbseMemImportStatus::importResults,
               &::ubse::adapter_plugins::mmi::UbseMemImportStatus::decoderResult,
               &::ubse::adapter_plugins::mmi::UbseMemImportStatus::expectState,
               &::ubse::adapter_plugins::mmi::UbseMemImportStatus::state)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemExportStatus, void,
               &::ubse::adapter_plugins::mmi::UbseMemExportStatus::errCode,
               &::ubse::adapter_plugins::mmi::UbseMemExportStatus::exportObmmInfo,
               &::ubse::adapter_plugins::mmi::UbseMemExportStatus::expectState,
               &::ubse::adapter_plugins::mmi::UbseMemExportStatus::state)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::SocketCnaInfo, void,
               &::ubse::adapter_plugins::mmi::SocketCnaInfo::importNodeId,
               &::ubse::adapter_plugins::mmi::SocketCnaInfo::importSocketId,
               &::ubse::adapter_plugins::mmi::SocketCnaInfo::seid,
               &::ubse::adapter_plugins::mmi::SocketCnaInfo::exportNodeId,
               &::ubse::adapter_plugins::mmi::SocketCnaInfo::exportSocketId,
               &::ubse::adapter_plugins::mmi::SocketCnaInfo::deid, &::ubse::adapter_plugins::mmi::SocketCnaInfo::scna,
               &::ubse::adapter_plugins::mmi::SocketCnaInfo::dcna, &::ubse::adapter_plugins::mmi::SocketCnaInfo::marId)

// ============================================================
// Borrow request types
// ============================================================

// UbseMemBaseBorrowReq: name, requestNodeId, requestId, udsInfo, trustRingData
UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemBaseBorrowReq, void,
               &::ubse::adapter_plugins::mmi::UbseMemBaseBorrowReq::name,
               &::ubse::adapter_plugins::mmi::UbseMemBaseBorrowReq::requestId,
               &::ubse::adapter_plugins::mmi::UbseMemBaseBorrowReq::requestNodeId,
               &::ubse::adapter_plugins::mmi::UbseMemBaseBorrowReq::udsInfo,
               &::ubse::adapter_plugins::mmi::UbseMemBaseBorrowReq::trustRingData,
               &::ubse::adapter_plugins::mmi::UbseMemBaseBorrowReq::ubseMemPrivData)

// FdPermission types (used in rpc_processor, not simple borrow types)
UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemFdPermissionReq, ::ubse::adapter_plugins::mmi::UbseMemBaseBorrowReq,
               &::ubse::adapter_plugins::mmi::UbseMemFdPermissionReq::fdOwner)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemFdPermissionResp, void,
               &::ubse::adapter_plugins::mmi::UbseMemFdPermissionResp::result,
               &::ubse::adapter_plugins::mmi::UbseMemFdPermissionResp::requestId)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemFdBorrowReq, ::ubse::adapter_plugins::mmi::UbseMemBaseBorrowReq,
               &::ubse::adapter_plugins::mmi::UbseMemFdBorrowReq::importNodeId,
               &::ubse::adapter_plugins::mmi::UbseMemFdBorrowReq::size,
               &::ubse::adapter_plugins::mmi::UbseMemFdBorrowReq::distance,
               &::ubse::adapter_plugins::mmi::UbseMemFdBorrowReq::lenderLocs,
               &::ubse::adapter_plugins::mmi::UbseMemFdBorrowReq::lenderSizes,
               &::ubse::adapter_plugins::mmi::UbseMemFdBorrowReq::candidateNodeList,
               &::ubse::adapter_plugins::mmi::UbseMemFdBorrowReq::owner)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq, ::ubse::adapter_plugins::mmi::UbseMemFdBorrowReq,
               &::ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq::srcSocket,
               &::ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq::srcNuma,
               &::ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq::highWatermark,
               &::ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq::lowWatermark,
               &::ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq::linkInfo,
               &::ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq::usrInfo)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemShareBorrowReq, ::ubse::adapter_plugins::mmi::UbseMemBaseBorrowReq,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowReq::baseNodeId,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowReq::size,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowReq::distance,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowReq::shmRegion,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowReq::providerList,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowReq::usrInfo,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowReq::shmAnonymous,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowReq::withAffinity,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowReq::lenderInfo)

// AddrBorrowReq: base fields are serialized LAST in the legacy format, so list all fields explicitly
UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq, void,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq::importNodeId,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq::importPid,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq::exportNodeId,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq::exportPid,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq::srcSocket,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq::srcNuma,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq::dstSocket,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq::dstNuma,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq::exportAddrList,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq::name,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq::requestId,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq::requestNodeId,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq::udsInfo,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq::trustRingData,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq::ubseMemPrivData)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemReturnReq, ::ubse::adapter_plugins::mmi::UbseMemBaseBorrowReq,
               &::ubse::adapter_plugins::mmi::UbseMemReturnReq::uid,
               &::ubse::adapter_plugins::mmi::UbseMemReturnReq::gid,
               &::ubse::adapter_plugins::mmi::UbseMemReturnReq::importNodeId)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemShareAttachReq, ::ubse::adapter_plugins::mmi::UbseMemBaseBorrowReq,
               &::ubse::adapter_plugins::mmi::UbseMemShareAttachReq::importNodeId,
               &::ubse::adapter_plugins::mmi::UbseMemShareAttachReq::size,
               &::ubse::adapter_plugins::mmi::UbseMemShareAttachReq::owner)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemShareDetachReq, ::ubse::adapter_plugins::mmi::UbseMemBaseBorrowReq,
               &::ubse::adapter_plugins::mmi::UbseMemShareDetachReq::unImportNodeId)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemOperationResp, void,
               &::ubse::adapter_plugins::mmi::UbseMemOperationResp::name,
               &::ubse::adapter_plugins::mmi::UbseMemOperationResp::requestNodeId,
               &::ubse::adapter_plugins::mmi::UbseMemOperationResp::errorCode,
               &::ubse::adapter_plugins::mmi::UbseMemOperationResp::errMsg,
               &::ubse::adapter_plugins::mmi::UbseMemOperationResp::realSize,
               &::ubse::adapter_plugins::mmi::UbseMemOperationResp::memIdList,
               &::ubse::adapter_plugins::mmi::UbseMemOperationResp::remoteNumaId,
               &::ubse::adapter_plugins::mmi::UbseMemOperationResp::requestId)

// ============================================================
// Borrow Export/Import base & derived objects
// ============================================================

// Export base: only persistent fields (exclude isCreateReportReceived, isDestroyedReportReceived)
// Wire order: errorCode → algoResult → status
UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemBorrowExportBaseObj, void,
               &::ubse::adapter_plugins::mmi::UbseMemBorrowExportBaseObj::errorCode,
               &::ubse::adapter_plugins::mmi::UbseMemBorrowExportBaseObj::algoResult,
               &::ubse::adapter_plugins::mmi::UbseMemBorrowExportBaseObj::status)

// Import base: only persistent fields (exclude 3 non-persistent booleans)
// Wire order: errorCode → algoResult → exportObmmInfo → status
UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemBorrowImportBaseObj, void,
               &::ubse::adapter_plugins::mmi::UbseMemBorrowImportBaseObj::errorCode,
               &::ubse::adapter_plugins::mmi::UbseMemBorrowImportBaseObj::algoResult,
               &::ubse::adapter_plugins::mmi::UbseMemBorrowImportBaseObj::exportObmmInfo,
               &::ubse::adapter_plugins::mmi::UbseMemBorrowImportBaseObj::status)

// FD
UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemFdBorrowExportObj,
               ::ubse::adapter_plugins::mmi::UbseMemBorrowExportBaseObj,
               &::ubse::adapter_plugins::mmi::UbseMemFdBorrowExportObj::req,
               &::ubse::adapter_plugins::mmi::UbseMemFdBorrowExportObj::returnReq)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemFdBorrowImportObj,
               ::ubse::adapter_plugins::mmi::UbseMemBorrowImportBaseObj,
               &::ubse::adapter_plugins::mmi::UbseMemFdBorrowImportObj::req,
               &::ubse::adapter_plugins::mmi::UbseMemFdBorrowImportObj::returnReq)

// NUMA
UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemNumaBorrowExportObj,
               ::ubse::adapter_plugins::mmi::UbseMemBorrowExportBaseObj,
               &::ubse::adapter_plugins::mmi::UbseMemNumaBorrowExportObj::req,
               &::ubse::adapter_plugins::mmi::UbseMemNumaBorrowExportObj::returnReq)

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemNumaBorrowImportObj,
               ::ubse::adapter_plugins::mmi::UbseMemBorrowImportBaseObj,
               &::ubse::adapter_plugins::mmi::UbseMemNumaBorrowImportObj::req,
               &::ubse::adapter_plugins::mmi::UbseMemNumaBorrowImportObj::returnReq)

// ADDR export: wire order is errorCode → req → returnReq → algoResult → status
UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemAddrBorrowExportObj, void,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowExportObj::errorCode,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowExportObj::req,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowExportObj::returnReq,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowExportObj::algoResult,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowExportObj::status)

// ADDR import: wire order is errorCode → req → returnReq → algoResult → exportObmmInfo → status
UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemAddrBorrowImportObj, void,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowImportObj::errorCode,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowImportObj::req,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowImportObj::returnReq,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowImportObj::algoResult,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowImportObj::exportObmmInfo,
               &::ubse::adapter_plugins::mmi::UbseMemAddrBorrowImportObj::status)

// SHARE export: wire order is errorCode → req → returnReq → algoResult → status
UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj, void,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj::errorCode,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj::req,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj::returnReq,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj::algoResult,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj::status)

// SHARE import: wire order is realExe → importNodeId → shareAttr → req → returnReq → base fields
UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj, void,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj::realExe,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj::importNodeId,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj::shareAttr,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj::req,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj::returnReq,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj::errorCode,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj::algoResult,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj::exportObmmInfo,
               &::ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj::status)

// ============================================================
// UbseNode (from node controller def) — has C arrays
// ============================================================

UBSE_SERIALIZE(::ubse::nodeController::def::UbseNode, void, &::ubse::nodeController::def::UbseNode::slotId,
               &::ubse::nodeController::def::UbseNode::socketId, &::ubse::nodeController::def::UbseNode::numaIds,
               &::ubse::nodeController::def::UbseNode::ips, &::ubse::nodeController::def::UbseNode::hostName)

// ============================================================
// Types from ubse_mem_controller.h (namespace ubse::mem::controller)
// ============================================================

UBSE_SERIALIZE(::ubse::mem::controller::UbseMemResult, void, &::ubse::mem::controller::UbseMemResult::name,
               &::ubse::mem::controller::UbseMemResult::importNodeId, &::ubse::mem::controller::UbseMemResult::realSize,
               &::ubse::mem::controller::UbseMemResult::stage)

UBSE_SERIALIZE(::ubse::mem::controller::UbseTopoNode, void, &::ubse::mem::controller::UbseTopoNode::slotId,
               &::ubse::mem::controller::UbseTopoNode::socketIdList, &::ubse::mem::controller::UbseTopoNode::numaIdList,
               &::ubse::mem::controller::UbseTopoNode::ips, &::ubse::mem::controller::UbseTopoNode::hostName)

UBSE_SERIALIZE(::ubse::mem::controller::UbseMemProcessLender, void,
               &::ubse::mem::controller::UbseMemProcessLender::slotId,
               &::ubse::mem::controller::UbseMemProcessLender::socketId,
               &::ubse::mem::controller::UbseMemProcessLender::pid,
               &::ubse::mem::controller::UbseMemProcessLender::vaLists)

UBSE_SERIALIZE(::ubse::mem::controller::UbseMemNumaDesc, void, &::ubse::mem::controller::UbseMemNumaDesc::name,
               &::ubse::mem::controller::UbseMemNumaDesc::numaId, &::ubse::mem::controller::UbseMemNumaDesc::exportNode,
               &::ubse::mem::controller::UbseMemNumaDesc::importNode, &::ubse::mem::controller::UbseMemNumaDesc::size,
               &::ubse::mem::controller::UbseMemNumaDesc::usrInfo)

UBSE_SERIALIZE(::ubse::mem::controller::UbseMemAddrDesc, void, &::ubse::mem::controller::UbseMemAddrDesc::name,
               &::ubse::mem::controller::UbseMemAddrDesc::numaId, &::ubse::mem::controller::UbseMemAddrDesc::lender,
               &::ubse::mem::controller::UbseMemAddrDesc::importNode, &::ubse::mem::controller::UbseMemAddrDesc::size)

UBSE_SERIALIZE(::ubse::mem::controller::UbseMemAddrBorrowLocAndSizeByPid, void,
               &::ubse::mem::controller::UbseMemAddrBorrowLocAndSizeByPid::addr,
               &::ubse::mem::controller::UbseMemAddrBorrowLocAndSizeByPid::size)

// ============================================================
// Types from ubse_mem_controller_def.h (namespace ubse::mem::def)
// ============================================================

UBSE_SERIALIZE(::ubse::mem::def::UbseMemFdDesc, void, &::ubse::mem::def::UbseMemFdDesc::name,
               &::ubse::mem::def::UbseMemFdDesc::memIds, &::ubse::mem::def::UbseMemFdDesc::totalMemSize,
               &::ubse::mem::def::UbseMemFdDesc::unitSize, &::ubse::mem::def::UbseMemFdDesc::exportNode,
               &::ubse::mem::def::UbseMemFdDesc::importNode, &::ubse::mem::def::UbseMemFdDesc::state)

UBSE_SERIALIZE(::ubse::mem::def::UbseMemShmMemStatusDesc, void, &::ubse::mem::def::UbseMemShmMemStatusDesc::memIds,
               &::ubse::mem::def::UbseMemShmMemStatusDesc::faultTypes)

UBSE_SERIALIZE(::ubse::mem::def::UbseMemNumaDesc, void, &::ubse::mem::def::UbseMemNumaDesc::name,
               &::ubse::mem::def::UbseMemNumaDesc::numaId, &::ubse::mem::def::UbseMemNumaDesc::importNode,
               &::ubse::mem::def::UbseMemNumaDesc::exportNode, &::ubse::mem::def::UbseMemNumaDesc::size,
               &::ubse::mem::def::UbseMemNumaDesc::state, &::ubse::mem::def::UbseMemNumaDesc::usrInfo)

UBSE_SERIALIZE(::ubse::mem::def::UbseMemShmImportDesc, void, &::ubse::mem::def::UbseMemShmImportDesc::memIds,
               &::ubse::mem::def::UbseMemShmImportDesc::importNode, &::ubse::mem::def::UbseMemShmImportDesc::state)

UBSE_SERIALIZE(::ubse::mem::def::UbseMemShmDesc, void, &::ubse::mem::def::UbseMemShmDesc::name,
               &::ubse::mem::def::UbseMemShmDesc::totalMemSize, &::ubse::mem::def::UbseMemShmDesc::unitSize,
               &::ubse::mem::def::UbseMemShmDesc::exportNode, &::ubse::mem::def::UbseMemShmDesc::userInfo,
               &::ubse::mem::def::UbseMemShmDesc::importDesc, &::ubse::mem::def::UbseMemShmDesc::state,
               &::ubse::mem::def::UbseMemShmDesc::region)

UBSE_SERIALIZE(::ubse::mem::def::UbseMemShmRegion, void, &::ubse::mem::def::UbseMemShmRegion::nodeCnt,
               &::ubse::mem::def::UbseMemShmRegion::slotIds)

UBSE_SERIALIZE(::ubse::mem::def::UbseMemShmDispatcher, void, &::ubse::mem::def::UbseMemShmDispatcher::name,
               &::ubse::mem::def::UbseMemShmDispatcher::size, &::ubse::mem::def::UbseMemShmDispatcher::usrInfo,
               &::ubse::mem::def::UbseMemShmDispatcher::flag, &::ubse::mem::def::UbseMemShmDispatcher::udsInfo,
               &::ubse::mem::def::UbseMemShmDispatcher::shmRegion, &::ubse::mem::def::UbseMemShmDispatcher::provider,
               &::ubse::mem::def::UbseMemShmDispatcher::affinitySocketId)

UBSE_SERIALIZE(::ubse::mem::def::UserInfo, void, &::ubse::mem::def::UserInfo::udsInfo,
               &::ubse::mem::def::UserInfo::flag, &::ubse::mem::def::UserInfo::mode)

UBSE_SERIALIZE(::ubse::mem::def::UbseMemDebtQueryRequest, void, &::ubse::mem::def::UbseMemDebtQueryRequest::name,
               &::ubse::mem::def::UbseMemDebtQueryRequest::importNodeId,
               &::ubse::mem::def::UbseMemDebtQueryRequest::exportNodeId,
               &::ubse::mem::def::UbseMemDebtQueryRequest::udsInfo)

UBSE_SERIALIZE(::ubse::mem::def::UbseMemIdQueryRequest, void, &::ubse::mem::def::UbseMemIdQueryRequest::name,
               &::ubse::mem::def::UbseMemIdQueryRequest::importNodeId,
               &::ubse::mem::def::UbseMemIdQueryRequest::importMemId,
               &::ubse::mem::def::UbseMemIdQueryRequest::borrowType, &::ubse::mem::def::UbseMemIdQueryRequest::udsInfo)

UBSE_SERIALIZE(::ubse::mem::def::UbseNodeBorrowInfo, void, &::ubse::mem::def::UbseNodeBorrowInfo::borrowSlotId,
               &::ubse::mem::def::UbseNodeBorrowInfo::borrowHostname, &::ubse::mem::def::UbseNodeBorrowInfo::lendSlotId,
               &::ubse::mem::def::UbseNodeBorrowInfo::lendHostname, &::ubse::mem::def::UbseNodeBorrowInfo::size)

UBSE_SERIALIZE(::ubse::mem::def::UbseExportMemDesc, void, &::ubse::mem::def::UbseExportMemDesc::exportSlotId,
               &::ubse::mem::def::UbseExportMemDesc::exportMemId)

UBSE_SERIALIZE(::ubse::mem::def::ShareHandleInfo, void, &::ubse::mem::def::ShareHandleInfo::name,
               &::ubse::mem::def::ShareHandleInfo::memIds, &::ubse::mem::def::ShareHandleInfo::udsInfo)

UBSE_SERIALIZE(::ubse::mem::def::NumaHandleInfo, void, &::ubse::mem::def::NumaHandleInfo::name,
               &::ubse::mem::def::NumaHandleInfo::numaIds, &::ubse::mem::def::NumaHandleInfo::udsInfo)

// ============================================================
// NodeMemDebtInfo — aggregate of all 8 debt maps
// ============================================================

UBSE_SERIALIZE(::ubse::adapter_plugins::mmi::NodeMemDebtInfo, void,
               &::ubse::adapter_plugins::mmi::NodeMemDebtInfo::fdImportObjMap,
               &::ubse::adapter_plugins::mmi::NodeMemDebtInfo::fdExportObjMap,
               &::ubse::adapter_plugins::mmi::NodeMemDebtInfo::numaImportObjMap,
               &::ubse::adapter_plugins::mmi::NodeMemDebtInfo::numaExportObjMap,
               &::ubse::adapter_plugins::mmi::NodeMemDebtInfo::shareImportObjMap,
               &::ubse::adapter_plugins::mmi::NodeMemDebtInfo::shareExportObjMap,
               &::ubse::adapter_plugins::mmi::NodeMemDebtInfo::addrImportObjMap,
               &::ubse::adapter_plugins::mmi::NodeMemDebtInfo::addrExportObjMap)

// ============================================================
// LedgerResp — for ledger response serialization
// ============================================================

UBSE_SERIALIZE(::ubse::mem::def::LedgerResp, void, &::ubse::mem::def::LedgerResp::ret,
               &::ubse::mem::def::LedgerResp::debtInfoMap)

} // namespace ubse::serial::util

#endif // UBSE_MEM_AUTO_SERIAL_H
