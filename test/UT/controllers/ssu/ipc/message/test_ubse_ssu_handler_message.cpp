/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_handler_message.h"

#include <cstring>
#include <string>
#include <vector>

#include "ubse_error.h"
#include "ubse_pack_util.h"
#include "message/ubse_ssu_obj_message.h"
#include "message/ubse_ssu_handler_message.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::ssu::ipc::message;
using namespace ubse::plugin::service::ssu;
using namespace ubse::utils;
using common::def::UbseResult;
using api::server::UbseIpcMessage;

// ===== 测试数据构造器 =====

UbseSsuVfe TestUbseSsuHandlerMessage::MakeVfe()
{
    UbseSsuVfe vfe{};
    vfe.slotId = 1;
    vfe.chipId = 2;
    vfe.dieId = 3;
    vfe.pfeId = 4;
    vfe.vfeId = 5;
    vfe.vfeGuid = std::string(UBS_SSU_GUID_LENGTH, 'a');                // 定长 32 字节
    vfe.bindBusInstanceGuid = std::string(UBS_SSU_GUID_LENGTH, 'b');    // 定长 32 字节
    return vfe;
}

UbseSsuFe TestUbseSsuHandlerMessage::MakeFe()
{
    UbseSsuFe fe{};
    fe.slotId = 1;
    fe.chipId = 2;
    fe.dieId = 3;
    fe.pfeId = 4;
    fe.pfeGuid = std::string(UBS_SSU_GUID_LENGTH, 'g'); // 定长 32 字节
    fe.vfeList = {MakeVfe(), MakeVfe()};
    return fe;
}

UbseSsuNameSpaceInfo TestUbseSsuHandlerMessage::MakeNsInfo()
{
    UbseSsuNameSpaceInfo info{};
    info.tgtEid = "eid_tgt_001";
    info.tgtNqn = "nqn.target";
    info.nsUuid = "uuid-ns-001";
    info.namespaceId = 100;
    info.nsDevPath = "/dev/nvme0n1";
    info.nsSize = 4096;
    info.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_4K;
    info.allowHostNqnList = {"nqn.host1", "nqn.host2"};
    return info;
}

UbseSsuAllocResult TestUbseSsuHandlerMessage::MakeAllocResult()
{
    UbseSsuAllocResult result{};
    result.name = "alloc_handler_test";
    result.strategy = UbseSsuAllocStrategy::LINEAR;
    result.nameSpaceList = {MakeNsInfo(), MakeNsInfo()};
    return result;
}

UbseSsuNsStats TestUbseSsuHandlerMessage::MakeNsStats()
{
    UbseSsuNsStats stats{};
    stats.nsUuid = "uuid-stats-001";
    stats.nsId = 300;
    stats.totalSize = 1024 * 1024;
    stats.usedSize = 512 * 1024;
    return stats;
}

UbseSsuConnectInfo TestUbseSsuHandlerMessage::MakeConnectInfo()
{
    UbseSsuConnectInfo info{};
    info.srcEid = "eid_src_001";
    info.tgtEid = "eid_tgt_001";
    info.tgtNqn = "nqn.target";
    info.hostNqn = "nqn.host";
    info.nsUuid = "uuid-conn-001";
    info.nsId = 200;
    return info;
}

// ===== SsuAllocResultListPack =====

TEST_F(TestUbseSsuHandlerMessage, SsuAllocResultListPack_EmptyList_Success)
{
    IpcMessageGuard guard;
    std::vector<UbseSsuAllocResult> list;
    EXPECT_EQ(SsuAllocResultListPack(list, guard.msg), UBSE_OK);
    ASSERT_NE(guard.msg.buffer, nullptr);
    EXPECT_EQ(guard.msg.length, sizeof(uint32_t));
    uint32_t listSize = 0;
    std::memcpy(&listSize, guard.msg.buffer, sizeof(uint32_t));
    EXPECT_EQ(listSize, 0u);
}

TEST_F(TestUbseSsuHandlerMessage, SsuAllocResultListPack_TwoItems_Success)
{
    IpcMessageGuard guard;
    std::vector<UbseSsuAllocResult> list = {MakeAllocResult(), MakeAllocResult()};
    EXPECT_EQ(SsuAllocResultListPack(list, guard.msg), UBSE_OK);
    ASSERT_NE(guard.msg.buffer, nullptr);
    EXPECT_GT(guard.msg.length, sizeof(uint32_t));

    uint32_t listSize = 0;
    std::memcpy(&listSize, guard.msg.buffer, sizeof(uint32_t));
    EXPECT_EQ(listSize, 2u);
}

TEST_F(TestUbseSsuHandlerMessage, SsuAllocResultListPack_NameTooLong_ReturnsError)
{
    IpcMessageGuard guard;
    auto bad = MakeAllocResult();
    bad.name = std::string(MAX_NAME_LEN + 1, 'n');
    std::vector<UbseSsuAllocResult> list = {bad};
    EXPECT_EQ(SsuAllocResultListPack(list, guard.msg), UBSE_ERROR_SERIALIZE_FAILED);
    EXPECT_EQ(guard.msg.buffer, nullptr);
    EXPECT_EQ(guard.msg.length, 0u);
}

// ===== SsuGetAllocInfoByNameUnpack / Pack =====

TEST_F(TestUbseSsuHandlerMessage, SsuGetAllocInfoByName_RoundTrip)
{
    const std::string srcName = "alloc_by_name";
    uint8_t reqBuf[64] = {0};
    UbsePackUtil packer(reqBuf, sizeof(reqBuf));
    ASSERT_TRUE(packer.UbsePackString(srcName, MAX_NAME_LEN));
    UbseIpcMessage req{reqBuf, sizeof(reqBuf)};

    std::string name;
    EXPECT_EQ(SsuGetAllocInfoByNameUnpack(req, name), UBSE_OK);
    EXPECT_EQ(name, srcName);

    IpcMessageGuard guard;
    const auto result = MakeAllocResult();
    EXPECT_EQ(SsuGetAllocInfoByNamePack(result, guard.msg), UBSE_OK);
    ASSERT_NE(guard.msg.buffer, nullptr);
    EXPECT_GT(guard.msg.length, 0u);
}

TEST_F(TestUbseSsuHandlerMessage, SsuGetAllocInfoByNameUnpack_NullBuffer_ReturnsError)
{
    UbseIpcMessage req{nullptr, 0};
    std::string name;
    EXPECT_EQ(SsuGetAllocInfoByNameUnpack(req, name), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseSsuHandlerMessage, SsuGetAllocInfoByNameUnpack_ZeroLength_ReturnsError)
{
    uint8_t buf[8] = {0};
    UbseIpcMessage req{buf, 0};
    std::string name;
    EXPECT_EQ(SsuGetAllocInfoByNameUnpack(req, name), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseSsuHandlerMessage, SsuGetAllocInfoByNameUnpack_NameTooLong_ReturnsError)
{
    uint8_t buf[128] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_TRUE(packer.UbsePackString(std::string(MAX_NAME_LEN + 1, 'n'), MAX_NAME_LEN + 1));
    UbseIpcMessage req{buf, sizeof(buf)};
    std::string name;
    EXPECT_EQ(SsuGetAllocInfoByNameUnpack(req, name), UBSE_ERROR_DESERIALIZE_FAILED);
}

// ===== SsuGetNsStatsUnpack / Pack =====

TEST_F(TestUbseSsuHandlerMessage, SsuGetNsStats_RoundTrip)
{
    const std::string srcName = "ns_stats_test";
    uint8_t reqBuf[64] = {0};
    UbsePackUtil packer(reqBuf, sizeof(reqBuf));
    ASSERT_TRUE(packer.UbsePackString(srcName, MAX_NAME_LEN));
    UbseIpcMessage req{reqBuf, sizeof(reqBuf)};

    std::string name;
    EXPECT_EQ(SsuGetNsStatsUnpack(req, name), UBSE_OK);
    EXPECT_EQ(name, srcName);

    IpcMessageGuard guard;
    std::vector<UbseSsuNsStats> statsList = {MakeNsStats(), MakeNsStats()};
    EXPECT_EQ(SsuGetNsStatsPack(statsList, guard.msg), UBSE_OK);
    ASSERT_NE(guard.msg.buffer, nullptr);
    EXPECT_GT(guard.msg.length, sizeof(uint32_t));
}

TEST_F(TestUbseSsuHandlerMessage, SsuGetNsStatsUnpack_NullBuffer_ReturnsError)
{
    UbseIpcMessage req{nullptr, 0};
    std::string name;
    EXPECT_EQ(SsuGetNsStatsUnpack(req, name), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseSsuHandlerMessage, SsuGetNsStatsPack_EmptyList_Success)
{
    IpcMessageGuard guard;
    std::vector<UbseSsuNsStats> empty;
    EXPECT_EQ(SsuGetNsStatsPack(empty, guard.msg), UBSE_OK);
    EXPECT_EQ(guard.msg.length, sizeof(uint32_t));
}

// ===== SsuGetConnectInfoUnpack / Pack（含 hasVfe 分支） =====

TEST_F(TestUbseSsuHandlerMessage, SsuGetConnectInfo_RoundTrip_WithVfe)
{
    const std::string srcName = "conn_info_with_vfe";
    const auto srcVfe = MakeVfe();

    uint8_t reqBuf[256] = {0};
    UbsePackUtil packer(reqBuf, sizeof(reqBuf));
    ASSERT_TRUE(packer.UbsePackString(srcName, MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackUint8(1)); // hasVfe=1
    ASSERT_EQ(VfePack(packer, srcVfe), UBSE_OK);
    UbseIpcMessage req{reqBuf, sizeof(reqBuf)};

    std::string name;
    std::optional<UbseSsuVfe> vfe;
    EXPECT_EQ(SsuGetConnectInfoUnpack(req, name, vfe), UBSE_OK);
    EXPECT_EQ(name, srcName);
    ASSERT_TRUE(vfe.has_value());
    EXPECT_EQ(vfe->slotId, srcVfe.slotId);
    EXPECT_EQ(vfe->vfeId, srcVfe.vfeId);
    EXPECT_EQ(vfe->vfeGuid, srcVfe.vfeGuid);

    IpcMessageGuard guard;
    std::vector<UbseSsuConnectInfo> list = {MakeConnectInfo()};
    EXPECT_EQ(SsuGetConnectInfoPack(list, guard.msg), UBSE_OK);
    ASSERT_NE(guard.msg.buffer, nullptr);
}

TEST_F(TestUbseSsuHandlerMessage, SsuGetConnectInfo_RoundTrip_WithoutVfe)
{
    const std::string srcName = "conn_info_no_vfe";

    uint8_t reqBuf[64] = {0};
    UbsePackUtil packer(reqBuf, sizeof(reqBuf));
    ASSERT_TRUE(packer.UbsePackString(srcName, MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackUint8(0)); // hasVfe=0
    UbseIpcMessage req{reqBuf, sizeof(reqBuf)};

    std::string name;
    std::optional<UbseSsuVfe> vfe;
    EXPECT_EQ(SsuGetConnectInfoUnpack(req, name, vfe), UBSE_OK);
    EXPECT_EQ(name, srcName);
    EXPECT_FALSE(vfe.has_value());
}

TEST_F(TestUbseSsuHandlerMessage, SsuGetConnectInfoUnpack_NullBuffer_ReturnsError)
{
    UbseIpcMessage req{nullptr, 0};
    std::string name;
    std::optional<UbseSsuVfe> vfe;
    EXPECT_EQ(SsuGetConnectInfoUnpack(req, name, vfe), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseSsuHandlerMessage, SsuGetConnectInfoUnpack_BufferTruncated_FailsVfeUnpack)
{
    // hasVfe=1 但 buffer 不足以读完 Vfe 的定长部分
    uint8_t buf[16] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_TRUE(packer.UbsePackString("name", MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackUint8(1)); // hasVfe=1
    // 不写 Vfe 内容，buffer 不足
    UbseIpcMessage req{buf, sizeof(buf)};

    std::string name;
    std::optional<UbseSsuVfe> vfe;
    EXPECT_EQ(SsuGetConnectInfoUnpack(req, name, vfe), UBSE_ERROR_DESERIALIZE_FAILED);
}

// ===== SsuAllocSpaceUnpack / Pack =====

TEST_F(TestUbseSsuHandlerMessage, SsuAllocSpace_RoundTrip)
{
    UbseSsuAllocSpaceReq src{};
    src.name = "alloc_req_test";
    src.nsSize = 4096 * 10;
    src.nsNum = 2;
    src.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_4K;
    src.strategy = UbseSsuAllocStrategy::STRIPED;
    src.tenant = "tenant_01";

    uint8_t reqBuf[128] = {0};
    UbsePackUtil packer(reqBuf, sizeof(reqBuf));
    ASSERT_TRUE(packer.UbsePackString(src.name, MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackUint64(src.nsSize));
    ASSERT_TRUE(packer.UbsePackUint32(src.nsNum));
    ASSERT_TRUE(packer.UbsePackUint32(static_cast<uint32_t>(src.lbaFormat)));
    ASSERT_TRUE(packer.UbsePackUint8(static_cast<uint8_t>(src.strategy)));
    ASSERT_TRUE(packer.UbsePackString(src.tenant, MAX_TENANT_LEN));
    UbseIpcMessage req{reqBuf, sizeof(reqBuf)};

    UbseSsuAllocSpaceReq dst{};
    EXPECT_EQ(SsuAllocSpaceUnpack(req, dst), UBSE_OK);
    EXPECT_EQ(dst.name, src.name);
    EXPECT_EQ(dst.nsSize, src.nsSize);
    EXPECT_EQ(dst.nsNum, src.nsNum);
    EXPECT_EQ(static_cast<uint32_t>(dst.lbaFormat), static_cast<uint32_t>(src.lbaFormat));
    EXPECT_EQ(static_cast<uint8_t>(dst.strategy), static_cast<uint8_t>(src.strategy));
    EXPECT_EQ(dst.tenant, src.tenant);

    IpcMessageGuard guard;
    const auto result = MakeAllocResult();
    EXPECT_EQ(SsuAllocSpacePack(result, guard.msg), UBSE_OK);
    ASSERT_NE(guard.msg.buffer, nullptr);
}

TEST_F(TestUbseSsuHandlerMessage, SsuAllocSpaceUnpack_NullBuffer_ReturnsError)
{
    UbseIpcMessage req{nullptr, 0};
    UbseSsuAllocSpaceReq dst{};
    EXPECT_EQ(SsuAllocSpaceUnpack(req, dst), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseSsuHandlerMessage, SsuAllocSpaceUnpack_InvalidLBAFormat_ReturnsError)
{
    uint8_t buf[128] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_TRUE(packer.UbsePackString("name", MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackUint64(4096));
    ASSERT_TRUE(packer.UbsePackUint32(1));
    ASSERT_TRUE(packer.UbsePackUint32(1024)); // 非法 lbaFormat
    ASSERT_TRUE(packer.UbsePackUint8(0));
    ASSERT_TRUE(packer.UbsePackString("t", MAX_TENANT_LEN));
    UbseIpcMessage req{buf, sizeof(buf)};

    UbseSsuAllocSpaceReq dst{};
    EXPECT_EQ(SsuAllocSpaceUnpack(req, dst), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseSsuHandlerMessage, SsuAllocSpaceUnpack_InvalidStrategy_ReturnsError)
{
    uint8_t buf[128] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_TRUE(packer.UbsePackString("name", MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackUint64(4096));
    ASSERT_TRUE(packer.UbsePackUint32(1));
    ASSERT_TRUE(packer.UbsePackUint32(static_cast<uint32_t>(UbseSsuLBAFormat::LBA_FORMAT_4K)));
    ASSERT_TRUE(packer.UbsePackUint8(2)); // 非法 strategy
    ASSERT_TRUE(packer.UbsePackString("t", MAX_TENANT_LEN));
    UbseIpcMessage req{buf, sizeof(buf)};

    UbseSsuAllocSpaceReq dst{};
    EXPECT_EQ(SsuAllocSpaceUnpack(req, dst), UBSE_ERROR_DESERIALIZE_FAILED);
}

// ===== SsuFreeSpaceUnpack =====

TEST_F(TestUbseSsuHandlerMessage, SsuFreeSpaceUnpack_RoundTrip)
{
    const std::string srcName = "free_space_test";
    uint8_t buf[64] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_TRUE(packer.UbsePackString(srcName, MAX_NAME_LEN));
    UbseIpcMessage req{buf, sizeof(buf)};

    std::string name;
    EXPECT_EQ(SsuFreeSpaceUnpack(req, name), UBSE_OK);
    EXPECT_EQ(name, srcName);
}

TEST_F(TestUbseSsuHandlerMessage, SsuFreeSpaceUnpack_NullBuffer_ReturnsError)
{
    UbseIpcMessage req{nullptr, 0};
    std::string name;
    EXPECT_EQ(SsuFreeSpaceUnpack(req, name), UBSE_ERROR_DESERIALIZE_FAILED);
}

// ===== SsuAddAccessPermissionUnpack / SsuRemoveAccessPermissionUnpack =====

TEST_F(TestUbseSsuHandlerMessage, SsuAddAccessPermissionUnpack_RoundTrip)
{
    const std::string srcName = "add_perm_test";
    const std::string srcNqn = "nqn.add.perm";
    uint8_t buf[128] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_TRUE(packer.UbsePackString(srcName, MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackString(srcNqn, MAX_NQN_LEN));
    UbseIpcMessage req{buf, sizeof(buf)};

    std::string name;
    std::string nqn;
    EXPECT_EQ(SsuAddAccessPermissionUnpack(req, name, nqn), UBSE_OK);
    EXPECT_EQ(name, srcName);
    EXPECT_EQ(nqn, srcNqn);
}

TEST_F(TestUbseSsuHandlerMessage, SsuRemoveAccessPermissionUnpack_RoundTrip)
{
    const std::string srcName = "rm_perm_test";
    const std::string srcNqn = "nqn.rm.perm";
    uint8_t buf[128] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_TRUE(packer.UbsePackString(srcName, MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackString(srcNqn, MAX_NQN_LEN));
    UbseIpcMessage req{buf, sizeof(buf)};

    std::string name;
    std::string nqn;
    EXPECT_EQ(SsuRemoveAccessPermissionUnpack(req, name, nqn), UBSE_OK);
    EXPECT_EQ(name, srcName);
    EXPECT_EQ(nqn, srcNqn);
}

TEST_F(TestUbseSsuHandlerMessage, SsuAddAccessPermissionUnpack_NullBuffer_ReturnsError)
{
    UbseIpcMessage req{nullptr, 0};
    std::string name;
    std::string nqn;
    EXPECT_EQ(SsuAddAccessPermissionUnpack(req, name, nqn), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseSsuHandlerMessage, SsuRemoveAccessPermissionUnpack_NullBuffer_ReturnsError)
{
    UbseIpcMessage req{nullptr, 0};
    std::string name;
    std::string nqn;
    EXPECT_EQ(SsuRemoveAccessPermissionUnpack(req, name, nqn), UBSE_ERROR_DESERIALIZE_FAILED);
}

// ===== SsuAttachSpaceUnpack / Pack =====
// 注意：SpaceReqUnpack 只解 name/nqn/srcEid，不含 identity

TEST_F(TestUbseSsuHandlerMessage, SsuAttachSpace_RoundTrip)
{
    UbseSsuSpaceReq src{};
    src.name = "attach_test";
    src.nqn = "nqn.attach";
    src.srcEid = "eid_attach";

    uint8_t reqBuf[256] = {0};
    UbsePackUtil packer(reqBuf, sizeof(reqBuf));
    ASSERT_TRUE(packer.UbsePackString(src.name, MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.nqn, MAX_NQN_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.srcEid, MAX_EID_LEN));
    UbseIpcMessage req{reqBuf, sizeof(reqBuf)};

    UbseSsuSpaceReq dst{};
    EXPECT_EQ(SsuAttachSpaceUnpack(req, dst), UBSE_OK);
    EXPECT_EQ(dst.name, src.name);
    EXPECT_EQ(dst.nqn, src.nqn);
    EXPECT_EQ(dst.srcEid, src.srcEid);

    IpcMessageGuard guard;
    std::vector<std::string> paths = {"/dev/nvme0n1", "/dev/nvme1n2"};
    EXPECT_EQ(SsuAttachSpacePack(paths, guard.msg), UBSE_OK);
    ASSERT_NE(guard.msg.buffer, nullptr);
    EXPECT_GT(guard.msg.length, sizeof(uint32_t));
}

TEST_F(TestUbseSsuHandlerMessage, SsuAttachSpaceUnpack_NullBuffer_ReturnsError)
{
    UbseIpcMessage req{nullptr, 0};
    UbseSsuSpaceReq dst{};
    EXPECT_EQ(SsuAttachSpaceUnpack(req, dst), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseSsuHandlerMessage, SsuAttachSpacePack_EmptyList_Success)
{
    IpcMessageGuard guard;
    std::vector<std::string> empty;
    EXPECT_EQ(SsuAttachSpacePack(empty, guard.msg), UBSE_OK);
    EXPECT_EQ(guard.msg.length, sizeof(uint32_t));
}

// ===== SsuDetachSpaceUnpack =====

TEST_F(TestUbseSsuHandlerMessage, SsuDetachSpaceUnpack_RoundTrip)
{
    UbseSsuSpaceReq src{};
    src.name = "detach_test";
    src.nqn = "nqn.detach";
    src.srcEid = "eid_detach";

    uint8_t reqBuf[256] = {0};
    UbsePackUtil packer(reqBuf, sizeof(reqBuf));
    ASSERT_TRUE(packer.UbsePackString(src.name, MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.nqn, MAX_NQN_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.srcEid, MAX_EID_LEN));
    UbseIpcMessage req{reqBuf, sizeof(reqBuf)};

    UbseSsuSpaceReq dst{};
    EXPECT_EQ(SsuDetachSpaceUnpack(req, dst), UBSE_OK);
    EXPECT_EQ(dst.name, src.name);
    EXPECT_EQ(dst.nqn, src.nqn);
}

TEST_F(TestUbseSsuHandlerMessage, SsuDetachSpaceUnpack_NullBuffer_ReturnsError)
{
    UbseIpcMessage req{nullptr, 0};
    UbseSsuSpaceReq dst{};
    EXPECT_EQ(SsuDetachSpaceUnpack(req, dst), UBSE_ERROR_DESERIALIZE_FAILED);
}

// ===== SsuAttachLinearSpaceUnpack / Pack =====

TEST_F(TestUbseSsuHandlerMessage, SsuAttachLinearSpace_RoundTrip)
{
    UbseSsuLinearSpaceReq src{};
    src.name = "linear_attach";
    src.nqn = "nqn.linear";
    src.srcEid = "eid_linear";
    src.devName = "linear_dev";

    uint8_t reqBuf[256] = {0};
    UbsePackUtil packer(reqBuf, sizeof(reqBuf));
    ASSERT_TRUE(packer.UbsePackString(src.name, MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.nqn, MAX_NQN_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.srcEid, MAX_EID_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.devName, MAX_DEV_NAME_LEN));
    UbseIpcMessage req{reqBuf, sizeof(reqBuf)};

    UbseSsuLinearSpaceReq dst{};
    EXPECT_EQ(SsuAttachLinearSpaceUnpack(req, dst), UBSE_OK);
    EXPECT_EQ(dst.name, src.name);
    EXPECT_EQ(dst.devName, src.devName);

    IpcMessageGuard guard;
    std::vector<std::string> paths = {"/dev/ns0", "/dev/ns1"};
    std::string devPath = "/dev/mapper/linear0";
    EXPECT_EQ(SsuAttachLinearSpacePack(paths, devPath, guard.msg), UBSE_OK);
    ASSERT_NE(guard.msg.buffer, nullptr);
}

TEST_F(TestUbseSsuHandlerMessage, SsuAttachLinearSpaceUnpack_NullBuffer_ReturnsError)
{
    UbseIpcMessage req{nullptr, 0};
    UbseSsuLinearSpaceReq dst{};
    EXPECT_EQ(SsuAttachLinearSpaceUnpack(req, dst), UBSE_ERROR_DESERIALIZE_FAILED);
}

// ===== SsuDetachLinearSpaceUnpack =====

TEST_F(TestUbseSsuHandlerMessage, SsuDetachLinearSpaceUnpack_RoundTrip)
{
    UbseSsuLinearSpaceReq src{};
    src.name = "linear_detach";
    src.nqn = "nqn.lin.det";
    src.srcEid = "eid_ld";
    src.devName = "linear_dev";

    uint8_t reqBuf[256] = {0};
    UbsePackUtil packer(reqBuf, sizeof(reqBuf));
    ASSERT_TRUE(packer.UbsePackString(src.name, MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.nqn, MAX_NQN_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.srcEid, MAX_EID_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.devName, MAX_DEV_NAME_LEN));
    UbseIpcMessage req{reqBuf, sizeof(reqBuf)};

    UbseSsuLinearSpaceReq dst{};
    EXPECT_EQ(SsuDetachLinearSpaceUnpack(req, dst), UBSE_OK);
    EXPECT_EQ(dst.name, src.name);
    EXPECT_EQ(dst.devName, src.devName);
}

// ===== SsuAttachStripedSpaceUnpack / Pack =====

TEST_F(TestUbseSsuHandlerMessage, SsuAttachStripedSpace_RoundTrip)
{
    UbseSsuStripedSpaceReq src{};
    src.name = "striped_attach";
    src.nqn = "nqn.striped";
    src.srcEid = "eid_s";
    src.devName = "striped_dev";
    src.level = UbseSsuAggregationRaidLevel::RAID5;
    src.chunkSize = UbseSsuChunkSize::CHUNK_SIZE_64K;

    uint8_t reqBuf[256] = {0};
    UbsePackUtil packer(reqBuf, sizeof(reqBuf));
    ASSERT_TRUE(packer.UbsePackString(src.name, MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.nqn, MAX_NQN_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.srcEid, MAX_EID_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.devName, MAX_DEV_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackUint8(static_cast<uint8_t>(src.level)));
    ASSERT_TRUE(packer.UbsePackUint32(static_cast<uint32_t>(src.chunkSize)));
    UbseIpcMessage req{reqBuf, sizeof(reqBuf)};

    UbseSsuStripedSpaceReq dst{};
    EXPECT_EQ(SsuAttachStripedSpaceUnpack(req, dst), UBSE_OK);
    EXPECT_EQ(dst.name, src.name);
    EXPECT_EQ(static_cast<uint8_t>(dst.level), static_cast<uint8_t>(src.level));
    EXPECT_EQ(static_cast<uint32_t>(dst.chunkSize), static_cast<uint32_t>(src.chunkSize));

    IpcMessageGuard guard;
    std::vector<std::string> paths = {"/dev/ns0", "/dev/ns1", "/dev/ns2"};
    std::string devPath = "/dev/mapper/striped0";
    EXPECT_EQ(SsuAttachStripedSpacePack(paths, devPath, guard.msg), UBSE_OK);
    ASSERT_NE(guard.msg.buffer, nullptr);
}

TEST_F(TestUbseSsuHandlerMessage, SsuAttachStripedSpaceUnpack_InvalidRaidLevel_ReturnsError)
{
    uint8_t buf[256] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_TRUE(packer.UbsePackString("n", MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackString("n", MAX_NQN_LEN));
    ASSERT_TRUE(packer.UbsePackString("e", MAX_EID_LEN));
    ASSERT_TRUE(packer.UbsePackString("d", MAX_DEV_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackUint8(3)); // 非法 raidLevel
    ASSERT_TRUE(packer.UbsePackUint32(static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_4K)));
    UbseIpcMessage req{buf, sizeof(buf)};

    UbseSsuStripedSpaceReq dst{};
    EXPECT_EQ(SsuAttachStripedSpaceUnpack(req, dst), UBSE_ERROR_DESERIALIZE_FAILED);
}

// ===== SsuDetachStripedSpaceUnpack =====

TEST_F(TestUbseSsuHandlerMessage, SsuDetachStripedSpaceUnpack_RoundTrip)
{
    UbseSsuStripedSpaceReq src{};
    src.name = "striped_detach";
    src.nqn = "nqn.sd";
    src.srcEid = "eid_sd";
    src.devName = "striped_dev";
    src.level = UbseSsuAggregationRaidLevel::RAID0;
    src.chunkSize = UbseSsuChunkSize::CHUNK_SIZE_32K;

    uint8_t reqBuf[256] = {0};
    UbsePackUtil packer(reqBuf, sizeof(reqBuf));
    ASSERT_TRUE(packer.UbsePackString(src.name, MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.nqn, MAX_NQN_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.srcEid, MAX_EID_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.devName, MAX_DEV_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackUint8(static_cast<uint8_t>(src.level)));
    ASSERT_TRUE(packer.UbsePackUint32(static_cast<uint32_t>(src.chunkSize)));
    UbseIpcMessage req{reqBuf, sizeof(reqBuf)};

    UbseSsuStripedSpaceReq dst{};
    EXPECT_EQ(SsuDetachStripedSpaceUnpack(req, dst), UBSE_OK);
    EXPECT_EQ(dst.name, src.name);
    EXPECT_EQ(dst.devName, src.devName);
}

// ===== SsuGetFeDeviceListPack =====

TEST_F(TestUbseSsuHandlerMessage, SsuGetFeDeviceListPack_EmptyList_Success)
{
    IpcMessageGuard guard;
    std::vector<UbseSsuFe> empty;
    EXPECT_EQ(SsuGetFeDeviceListPack(empty, guard.msg), UBSE_OK);
    EXPECT_EQ(guard.msg.length, sizeof(uint32_t));
}

TEST_F(TestUbseSsuHandlerMessage, SsuGetFeDeviceListPack_TwoItems_Success)
{
    IpcMessageGuard guard;
    std::vector<UbseSsuFe> list = {MakeFe(), MakeFe()};
    EXPECT_EQ(SsuGetFeDeviceListPack(list, guard.msg), UBSE_OK);
    ASSERT_NE(guard.msg.buffer, nullptr);
    EXPECT_GT(guard.msg.length, sizeof(uint32_t));

    uint32_t listSize = 0;
    std::memcpy(&listSize, guard.msg.buffer, sizeof(uint32_t));
    EXPECT_EQ(listSize, 2u);
}

TEST_F(TestUbseSsuHandlerMessage, SsuGetFeDeviceListPack_SingleItem_Success)
{
    IpcMessageGuard guard;
    std::vector<UbseSsuFe> list = {MakeFe()};
    EXPECT_EQ(SsuGetFeDeviceListPack(list, guard.msg), UBSE_OK);
    ASSERT_NE(guard.msg.buffer, nullptr);
    EXPECT_GT(guard.msg.length, sizeof(uint32_t));

    uint32_t listSize = 0;
    std::memcpy(&listSize, guard.msg.buffer, sizeof(uint32_t));
    EXPECT_EQ(listSize, 1u);
}

// ===== SsuFeDeviceAllocUnpack / Pack 往返 =====
// 请求 buffer：upi(uint32) + VfePack + busInstanceGuid(GuidPack 32字节定长)
// 响应 buffer：busInstanceGuid(GuidPack 32字节定长)

TEST_F(TestUbseSsuHandlerMessage, SsuFeDeviceAlloc_RoundTrip)
{
    const uint32_t srcUpi = 42;
    const auto srcVfe = MakeVfe();
    const std::string srcBusGuid(UBS_SSU_GUID_LENGTH, 'c'); // 定长 32 字节 GUID

    uint8_t reqBuf[256] = {0};
    UbsePackUtil packer(reqBuf, sizeof(reqBuf));
    ASSERT_TRUE(packer.UbsePackUint32(srcUpi));
    ASSERT_EQ(VfePack(packer, srcVfe), UBSE_OK);
    ASSERT_EQ(GuidPack(packer, srcBusGuid), UBSE_OK); // busInstanceGuid 用 GuidPack
    UbseIpcMessage req{reqBuf, sizeof(reqBuf)};

    uint32_t upi = 0;
    UbseSsuVfe vfe{};
    std::string busGuid;
    EXPECT_EQ(SsuFeDeviceAllocUnpack(req, upi, vfe, busGuid), UBSE_OK);
    EXPECT_EQ(upi, srcUpi);
    EXPECT_EQ(vfe.vfeId, srcVfe.vfeId);
    EXPECT_EQ(vfe.vfeGuid, srcVfe.vfeGuid);
    EXPECT_EQ(busGuid, srcBusGuid);

    // Pack 响应：只含 32 字节 GUID
    IpcMessageGuard guard;
    std::string respBusGuid(UBS_SSU_GUID_LENGTH, 'd');
    EXPECT_EQ(SsuFeDeviceAllocPack(respBusGuid, guard.msg), UBSE_OK);
    ASSERT_NE(guard.msg.buffer, nullptr);
    EXPECT_EQ(guard.msg.length, UBS_SSU_GUID_LENGTH); // 响应固定 32 字节
}

TEST_F(TestUbseSsuHandlerMessage, SsuFeDeviceAllocUnpack_NullBuffer_ReturnsError)
{
    UbseIpcMessage req{nullptr, 0};
    uint32_t upi = 0;
    UbseSsuVfe vfe{};
    std::string busGuid;
    EXPECT_EQ(SsuFeDeviceAllocUnpack(req, upi, vfe, busGuid), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseSsuHandlerMessage, SsuFeDeviceAllocUnpack_BufferTruncated_FailsVfeUnpack)
{
    // 只含 upi，不含完整 Vfe
    uint8_t buf[4] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_TRUE(packer.UbsePackUint32(1));
    UbseIpcMessage req{buf, sizeof(buf)};

    uint32_t upi = 0;
    UbseSsuVfe vfe{};
    std::string busGuid;
    EXPECT_EQ(SsuFeDeviceAllocUnpack(req, upi, vfe, busGuid), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseSsuHandlerMessage, SsuFeDeviceAllocPack_EmptyGuid_Success)
{
    IpcMessageGuard guard;
    std::string emptyGuid; // 空 GUID → 32 字节全 0
    EXPECT_EQ(SsuFeDeviceAllocPack(emptyGuid, guard.msg), UBSE_OK);
    ASSERT_NE(guard.msg.buffer, nullptr);
    EXPECT_EQ(guard.msg.length, UBS_SSU_GUID_LENGTH);
}

// ===== SsuFeDeviceFreeUnpack =====
// 请求 buffer：upi(uint32) + VfePack（不含 busInstanceGuid）

TEST_F(TestUbseSsuHandlerMessage, SsuFeDeviceFreeUnpack_RoundTrip)
{
    const uint32_t srcUpi = 99;
    const auto srcVfe = MakeVfe();

    uint8_t reqBuf[256] = {0};
    UbsePackUtil packer(reqBuf, sizeof(reqBuf));
    ASSERT_TRUE(packer.UbsePackUint32(srcUpi));
    ASSERT_EQ(VfePack(packer, srcVfe), UBSE_OK);
    UbseIpcMessage req{reqBuf, sizeof(reqBuf)};

    uint32_t upi = 0;
    UbseSsuVfe vfe{};
    EXPECT_EQ(SsuFeDeviceFreeUnpack(req, upi, vfe), UBSE_OK);
    EXPECT_EQ(upi, srcUpi);
    EXPECT_EQ(vfe.vfeId, srcVfe.vfeId);
    EXPECT_EQ(vfe.vfeGuid, srcVfe.vfeGuid);
}

TEST_F(TestUbseSsuHandlerMessage, SsuFeDeviceFreeUnpack_NullBuffer_ReturnsError)
{
    UbseIpcMessage req{nullptr, 0};
    uint32_t upi = 0;
    UbseSsuVfe vfe{};
    EXPECT_EQ(SsuFeDeviceFreeUnpack(req, upi, vfe), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseSsuHandlerMessage, SsuFeDeviceFreeUnpack_BufferTruncated_FailsVfeUnpack)
{
    // 只含 upi，不含完整 Vfe
    uint8_t buf[4] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_TRUE(packer.UbsePackUint32(1));
    UbseIpcMessage req{buf, sizeof(buf)};

    uint32_t upi = 0;
    UbseSsuVfe vfe{};
    EXPECT_EQ(SsuFeDeviceFreeUnpack(req, upi, vfe), UBSE_ERROR_DESERIALIZE_FAILED);
}

} // namespace ubse::ssu::ipc::ut
