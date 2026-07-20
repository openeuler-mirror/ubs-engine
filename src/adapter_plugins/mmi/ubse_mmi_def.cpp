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

#include "ubse_mmi_def.h"
namespace ubse::adapter_plugins::mmi {
bool UbseUdsInfo::CheckPermission(const UbseUdsInfo &other) const
{
    if (other.username == "ubse" || other.username == "root" || other.uid == 0) {
        return true;
    }
    // 优先检查username：当前对象的username存在时，使用username比较
    if (!username.empty()) {
        return username == other.username;
    }
    // 否则使用uid比较
    return uid == other.uid;
}

std::ostream &operator<<(std::ostream &os, const UbseUdsInfo &obj)
{
    return os << "(uid: " << obj.uid << " gid: " << obj.gid << " pid: " << obj.pid << " username: " << obj.username
              << ")";
}

std::ostream &operator<<(std::ostream &os, const FdOwner &obj)
{
    return os << "(uid: " << obj.uid << " gid: " << obj.gid << " pid: " << obj.pid << " mode: " << obj.mode << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemAddrInfo &obj)
{
    return os << "(addr: " << obj.addr << " size: " << obj.size << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseNumaLocation &obj)
{
    return os << "(nodeId: " << obj.nodeId << " numaId: " << obj.numaId << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemBaseBorrowReq &obj)
{
    return os << "(name: " << obj.name << " requestNodeId: " << obj.requestNodeId << " requestId: " << obj.requestId
              << " username: " << obj.udsInfo.username << " uid: " << obj.udsInfo.uid << ")";
}

std::ostream &operator << (std::ostream &os, const UbseTrustRingData &obj)
{
    os << "(trustRingId: " << obj.trustRingId;
    return os << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemFdBorrowReq &obj)
{
    os << "(" << static_cast<const UbseMemBaseBorrowReq &>(obj) << " requestNodeId: " << obj.requestNodeId
       << " importNodeId: " << obj.importNodeId << " size: " << obj.size << " distance: " << obj.distance
       << " lenderLocs: ";
    for (const auto &loc : obj.lenderLocs) {
        os << loc;
    }
    os << " owner: " << obj.owner;
    return os << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemFdPermissionReq &req)
{
    return os << "(: " << static_cast<const UbseMemBaseBorrowReq &>(req) << " fdOwner: " << req.fdOwner << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemFdPermissionResp &resp)
{
    return os << "(result=" << resp.result << " request_id=" << resp.requestId << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemNumaBorrowReq &obj)
{
    return os << "(" << static_cast<const UbseMemFdBorrowReq &>(obj) << " srcSocket: " << obj.srcSocket
              << " srcNuma: " << obj.srcNuma << " highWatermark: " << obj.highWatermark
              << " lowWatermark: " << obj.lowWatermark << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemShareBorrowReq &obj)
{
    return os << "(name: " << obj.name << " requestNodeId: " << obj.requestNodeId << " baseNodeId: " << obj.baseNodeId
              << " size: " << obj.size << " distance: " << obj.distance << " udsInfo: " << obj.udsInfo << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemShareAttachReq &obj)
{
    return os << "(name: " << obj.name << " requestNodeId: " << obj.requestNodeId
              << " importNodeId: " << obj.importNodeId << " size: " << obj.size << " udsInfo: " << obj.udsInfo << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemShareDetachReq &obj)
{
    return os << "(name: " << obj.name << " requestNodeId: " << obj.requestNodeId
              << " unImportNodeId: " << obj.unImportNodeId << " udsInfo: " << obj.udsInfo << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemAddrBorrowReq &obj)
{
    os << "(" << static_cast<const UbseMemBaseBorrowReq &>(obj) << " requestNodeId: " << obj.requestNodeId
       << " importNodeId: " << obj.importNodeId << " srcSocket: " << obj.srcSocket << " srcNuma: " << obj.srcNuma
       << " dstSocket: " << obj.dstSocket << " dstNuma: " << obj.dstNuma << " importPid: " << obj.importPid
       << " exportNodeId: " << obj.exportNodeId << " exportPid: " << obj.exportPid << " wrDelayComp: "
       << obj.wrDelayComp << " exportAccessMode: " << static_cast<int>(obj.exportAccessMode) << " exportAddrList: ";
    for (const auto &addrList : obj.exportAddrList) {
        os << addrList << ",";
    }
    return os << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemImportResult &obj)
{
    return os << "(memId: " << obj.memId << " numaId: " << obj.numaId << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemImportStatus &obj)
{
    os << "(errCode: " << obj.errCode << " scna: " << obj.scna << " importResults: ";
    for (const auto &result : obj.importResults) {
        os << result << ",";
    }
    return os << " expectState: " << obj.expectState << " state: " << obj.state << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemObmmInfo &obj)
{
    return os << "(memId: " << obj.memId << " desc: " << obj.desc.addr << 
    " scna: " << obj.desc.scna << " dcna: " << obj.desc.dcna << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemExportStatus &obj)
{
    os << "(errCode: " << obj.errCode << " exportObmmInfo: ";
    for (const auto &obmmInfo : obj.exportObmmInfo) {
        os << obmmInfo << ",";
    }
    return os << " expectState: " << obj.expectState << " state: " << obj.state << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemDebtNumaInfo &obj)
{
    return os << "(nodeId: " << obj.nodeId << " socketId: " << obj.socketId << " numaId: " << obj.numaId
              << " size: " << obj.size << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemAlgoResult &obj)
{
    os << "(exportNumaInfos: ";
    for (const auto &info : obj.exportNumaInfos) {
        os << info << ",";
    }
    os << " importNumaInfos: ";
    for (const auto &info : obj.importNumaInfos) {
        os << info << ",";
    }
    return os << " blockSize: " << obj.blockSize << " attachSocketId: " << obj.attachSocketId << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemBorrowExportBaseObj &obj)
{
    return os << "(algoResult: " << obj.algoResult << " status: " << obj.status << " errorCode: " << obj.errorCode
              << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemBorrowImportBaseObj &obj)
{
    os << "(algoResult: " << obj.algoResult << " exportObmmInfo: ";
    for (const auto &obmmInfo : obj.exportObmmInfo) {
        os << obmmInfo << ",";
    }
    return os << " status: " << obj.status << " errorCode: " << obj.errorCode << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemFdBorrowExportObj &obj)
{
    return os << "(" << static_cast<const UbseMemBorrowExportBaseObj &>(obj) << " req: " << obj.req << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemFdBorrowImportObj &obj)
{
    return os << "(" << static_cast<const UbseMemBorrowImportBaseObj &>(obj) << " req: " << obj.req << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemNumaBorrowExportObj &obj)
{
    return os << "(" << static_cast<const UbseMemBorrowExportBaseObj &>(obj) << " req: " << obj.req << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemNumaBorrowImportObj &obj)
{
    return os << "(" << static_cast<const UbseMemBorrowImportBaseObj &>(obj) << " req: " << obj.req << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemAddrBorrowExportObj &obj)
{
    return os << "(" << static_cast<const UbseMemBorrowExportBaseObj &>(obj) << " req: " << obj.req << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemAddrBorrowImportObj &obj)
{
    return os << "(" << static_cast<const UbseMemBorrowImportBaseObj &>(obj) << " req: " << obj.req << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemShareBorrowExportObj &obj)
{
    return os << "(" << static_cast<const UbseMemBorrowExportBaseObj &>(obj) << " req: " << obj.req << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemAttachResourceShareAttr &obj)
{
    return os << "("
              << "uid: " << obj.uid << " gid: " << obj.gid << " size: " << obj.size << " owner: " << obj.owner
              << ")";
}

std::ostream &operator<<(std::ostream &os, const UbseMemShareBorrowImportObj &obj)
{
    return os << "(" << static_cast<const UbseMemBorrowImportBaseObj &>(obj) << " realExe: " << obj.realExe
              << " importNodeId: " << obj.importNodeId << " shareAttr: " << obj.shareAttr << " req: " << obj.req
              << ")";
}

bool UbseMemNumaLoc::operator==(const UbseMemNumaLoc &other) const
{
    return nodeId == other.nodeId && socketId == other.socketId && numaId == other.numaId;
}
// 重载 < 运算符
bool UbseMemNumaLoc::operator<(const UbseMemNumaLoc &other) const
{
    if (nodeId != other.nodeId) {
        return nodeId < other.nodeId;
    }
    if (socketId != other.socketId) {
        return socketId < other.socketId;
    }
    return numaId < other.numaId;
}
std::ostream &operator<<(std::ostream &os, const UbseMemNumaLoc &obj)
{
    return os << "nodeId=" << obj.nodeId << ", socketId=" << obj.socketId << ", numaId=" << obj.numaId;
}

std::ostream &operator<<(std::ostream &os, const SocketCnaInfo &obj)
{
    return os << "importNodeId=" << obj.importNodeId << ", importSocketId=" << obj.importSocketId
              << ", exportNodeId=" << obj.exportNodeId << ", exportSocketId=" << obj.exportSocketId
              << ", scna=" << obj.scna << ", dcna=" << obj.dcna << ", marId=" << obj.marId << ", seid=" << obj.seid
              << ", deid=" << obj.deid;
}
} // namespace ubse::adapter_plugins::mmi
