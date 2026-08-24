/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_ipc_helper.h"

#include <cstring>
#include <string>

#include "ipc/message/ubse_ssu_obj_message.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::plugin::service::ssu;
using namespace ubse::ssu::ipc::message;
using namespace ubse::utils;
using api::server::UbseIpcMessage;

// 通用辅助：用 UbsePackUtil 构造一个精确大小的请求 buffer
UbseIpcMessage BuildReqFromBuffer(const uint8_t *src, uint32_t len)
{
    auto *buf = new uint8_t[len];
    std::memcpy(buf, src, len);
    return {buf, len};
}

// ===== 请求 buffer 构造器 =====

UbseIpcMessage MakeAllocSpaceReq(const std::string &name)
{
    uint8_t tmp[256] = {0};
    UbsePackUtil packer(tmp, sizeof(tmp));
    packer.UbsePackString(name, MAX_NAME_LEN);
    packer.UbsePackUint64(4096 * 10);
    packer.UbsePackUint32(2);
    packer.UbsePackUint32(static_cast<uint32_t>(UbseSsuLBAFormat::LBA_FORMAT_4K));
    packer.UbsePackUint8(static_cast<uint8_t>(UbseSsuAllocStrategy::STRIPED));
    packer.UbsePackString("tenant_01", MAX_TENANT_LEN);
    const std::string tenant = "tenant_01";
    uint32_t actual = sizeof(uint32_t) + name.length() + sizeof(uint64_t) + sizeof(uint32_t) +
                      sizeof(uint32_t) + sizeof(uint8_t) + (sizeof(uint32_t) + tenant.length());
    return BuildReqFromBuffer(tmp, actual);
}

UbseIpcMessage MakeFreeSpaceReq(const std::string &name)
{
    uint8_t tmp[128] = {0};
    UbsePackUtil packer(tmp, sizeof(tmp));
    packer.UbsePackString(name, MAX_NAME_LEN);
    uint32_t actual = sizeof(uint32_t) + name.length();
    return BuildReqFromBuffer(tmp, actual);
}

UbseIpcMessage MakeGetAllocInfoByNameReq(const std::string &name)
{
    uint8_t tmp[128] = {0};
    UbsePackUtil packer(tmp, sizeof(tmp));
    packer.UbsePackString(name, MAX_NAME_LEN);
    uint32_t actual = sizeof(uint32_t) + name.length();
    return BuildReqFromBuffer(tmp, actual);
}

UbseIpcMessage MakeGetNsStatsReq(const std::string &name)
{
    uint8_t tmp[128] = {0};
    UbsePackUtil packer(tmp, sizeof(tmp));
    packer.UbsePackString(name, MAX_NAME_LEN);
    uint32_t actual = sizeof(uint32_t) + name.length();
    return BuildReqFromBuffer(tmp, actual);
}

UbseIpcMessage MakeGetConnectInfoReq(bool withVfe, const std::string &name)
{
    uint8_t tmp[256] = {0};
    UbsePackUtil packer(tmp, sizeof(tmp));
    packer.UbsePackString(name, MAX_NAME_LEN);
    packer.UbsePackUint8(withVfe ? 1 : 0);
    uint32_t actual = sizeof(uint32_t) + name.length() + sizeof(uint8_t);
    if (withVfe) {
        UbseSsuVfe vfe{};
        vfe.slotId = 1; vfe.chipId = 2; vfe.dieId = 3; vfe.pfeId = 4; vfe.vfeId = 5;
        vfe.vfeGuid = std::string(UBS_SSU_GUID_LENGTH, 'a');
        vfe.bindBusInstanceGuid = std::string(UBS_SSU_GUID_LENGTH, 'b');
        VfePack(packer, vfe);
        actual += VfeCalcSize(vfe);
    }
    return BuildReqFromBuffer(tmp, actual);
}

UbseIpcMessage MakeAccessPermissionReq(const std::string &name, const std::string &nqn)
{
    uint8_t tmp[256] = {0};
    UbsePackUtil packer(tmp, sizeof(tmp));
    packer.UbsePackString(name, MAX_NAME_LEN);
    packer.UbsePackString(nqn, MAX_NQN_LEN);
    uint32_t actual = sizeof(uint32_t) + name.length() + sizeof(uint32_t) + nqn.length();
    return BuildReqFromBuffer(tmp, actual);
}

UbseIpcMessage MakeSpaceReq(const std::string &name, const std::string &nqn, const std::string &srcEid)
{
    uint8_t tmp[256] = {0};
    UbsePackUtil packer(tmp, sizeof(tmp));
    packer.UbsePackString(name, MAX_NAME_LEN);
    packer.UbsePackString(nqn, MAX_NQN_LEN);
    packer.UbsePackString(srcEid, MAX_EID_LEN);
    uint32_t actual = sizeof(uint32_t) + name.length() + sizeof(uint32_t) + nqn.length() +
                      sizeof(uint32_t) + srcEid.length();
    return BuildReqFromBuffer(tmp, actual);
}

UbseIpcMessage MakeLinearSpaceReq(const std::string &name, const std::string &devName)
{
    const std::string nqn = "nqn.linear";
    const std::string srcEid = "eid_linear";
    uint8_t tmp[256] = {0};
    UbsePackUtil packer(tmp, sizeof(tmp));
    packer.UbsePackString(name, MAX_NAME_LEN);
    packer.UbsePackString(nqn, MAX_NQN_LEN);
    packer.UbsePackString(srcEid, MAX_EID_LEN);
    packer.UbsePackString(devName, MAX_DEV_NAME_LEN);
    uint32_t actual = sizeof(uint32_t) + name.length() + sizeof(uint32_t) + nqn.length() +
                      sizeof(uint32_t) + srcEid.length() + sizeof(uint32_t) + devName.length();
    return BuildReqFromBuffer(tmp, actual);
}

UbseIpcMessage MakeStripedSpaceReq(const std::string &name, const std::string &devName,
                                   uint8_t level, uint32_t chunkSize)
{
    const std::string nqn = "nqn.striped";
    const std::string srcEid = "eid_striped";
    uint8_t tmp[256] = {0};
    UbsePackUtil packer(tmp, sizeof(tmp));
    packer.UbsePackString(name, MAX_NAME_LEN);
    packer.UbsePackString(nqn, MAX_NQN_LEN);
    packer.UbsePackString(srcEid, MAX_EID_LEN);
    packer.UbsePackString(devName, MAX_DEV_NAME_LEN);
    packer.UbsePackUint8(level);
    packer.UbsePackUint32(chunkSize);
    uint32_t actual = sizeof(uint32_t) + name.length() + sizeof(uint32_t) + nqn.length() +
                      sizeof(uint32_t) + srcEid.length() + sizeof(uint32_t) + devName.length() +
                      sizeof(uint8_t) + sizeof(uint32_t);
    return BuildReqFromBuffer(tmp, actual);
}

UbseIpcMessage MakeFeDeviceAllocReq(uint32_t upi, const std::string &busGuid)
{
    uint8_t tmp[256] = {0};
    UbsePackUtil packer(tmp, sizeof(tmp));
    packer.UbsePackUint32(upi);
    UbseSsuVfe vfe{};
    vfe.slotId = 1; vfe.chipId = 2; vfe.dieId = 3; vfe.pfeId = 4; vfe.vfeId = 5;
    vfe.vfeGuid = std::string(UBS_SSU_GUID_LENGTH, 'a');
    vfe.bindBusInstanceGuid = std::string(UBS_SSU_GUID_LENGTH, 'b');
    VfePack(packer, vfe);
    GuidPack(packer, busGuid);
    uint32_t actual = sizeof(uint32_t) + VfeCalcSize(vfe) + UBS_SSU_GUID_LENGTH;
    return BuildReqFromBuffer(tmp, actual);
}

UbseIpcMessage MakeFeDeviceFreeReq(uint32_t upi)
{
    uint8_t tmp[256] = {0};
    UbsePackUtil packer(tmp, sizeof(tmp));
    packer.UbsePackUint32(upi);
    UbseSsuVfe vfe{};
    vfe.slotId = 1; vfe.chipId = 2; vfe.dieId = 3; vfe.pfeId = 4; vfe.vfeId = 5;
    vfe.vfeGuid = std::string(UBS_SSU_GUID_LENGTH, 'a');
    vfe.bindBusInstanceGuid = std::string(UBS_SSU_GUID_LENGTH, 'b');
    VfePack(packer, vfe);
    uint32_t actual = sizeof(uint32_t) + VfeCalcSize(vfe);
    return BuildReqFromBuffer(tmp, actual);
}

UbseIpcMessage MakeEmptyReq()
{
    auto *buf = new uint8_t[4];
    uint32_t zero = 0;
    std::memcpy(buf, &zero, sizeof(uint32_t));
    return {buf, sizeof(uint32_t)};
}

UbseIpcMessage MakeInvalidReq_NameTooLong()
{
    uint8_t tmp[256] = {0};
    UbsePackUtil packer(tmp, sizeof(tmp));
    packer.UbsePackString(std::string(MAX_NAME_LEN + 10, 'x'), MAX_NAME_LEN + 10);
    uint32_t actual = sizeof(uint32_t) + MAX_NAME_LEN + 10;
    return BuildReqFromBuffer(tmp, actual);
}

UbseIpcMessage MakeFeDeviceReq_TruncatedAfterUpi(uint32_t upi)
{
    uint8_t tmp[4] = {0};
    UbsePackUtil packer(tmp, sizeof(tmp));
    packer.UbsePackUint32(upi);
    return BuildReqFromBuffer(tmp, sizeof(uint32_t));
}

UbseIpcMessage MakeFeDeviceReq_TruncatedInVfeGuid(uint32_t upi)
{
    uint8_t tmp[16] = {0};
    UbsePackUtil packer(tmp, sizeof(tmp));
    packer.UbsePackUint32(upi);
    packer.UbsePackUint8(1);  // slotId
    packer.UbsePackUint8(2);  // chipId
    packer.UbsePackUint8(3);  // dieId
    packer.UbsePackUint16(4); // pfeId
    packer.UbsePackUint16(5); // vfeId
    const uint32_t actual = sizeof(uint32_t) + 3 * sizeof(uint8_t) + 2 * sizeof(uint16_t);
    return BuildReqFromBuffer(tmp, actual);
}

UbseIpcMessage MakeFeDeviceReq_TruncatedAfterVfe(uint32_t upi)
{
    uint8_t tmp[256] = {0};
    UbsePackUtil packer(tmp, sizeof(tmp));
    packer.UbsePackUint32(upi);
    UbseSsuVfe vfe{};
    vfe.slotId = 1; vfe.chipId = 2; vfe.dieId = 3; vfe.pfeId = 4; vfe.vfeId = 5;
    vfe.vfeGuid = std::string(UBS_SSU_GUID_LENGTH, 'a');
    vfe.bindBusInstanceGuid = std::string(UBS_SSU_GUID_LENGTH, 'b');
    VfePack(packer, vfe);
    const uint32_t actual = sizeof(uint32_t) + VfeCalcSize(vfe);
    return BuildReqFromBuffer(tmp, actual);
}

} // namespace ubse::ssu::ipc::ut
