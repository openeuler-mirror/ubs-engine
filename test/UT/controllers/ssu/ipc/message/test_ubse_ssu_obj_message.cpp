/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_obj_message.h"

#include <cstring>
#include <string>
#include <vector>

#include "ubse_error.h"
#include "ubse_pack_util.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::ssu::ipc::message;
using namespace ubse::plugin::service::ssu;
using namespace ubse::utils;
using common::def::UbseResult;

// ===== 测试数据构造器 =====

UbseSsuVfe TestUbseSsuObjMessage::MakeVfe()
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

UbseSsuNameSpaceInfo TestUbseSsuObjMessage::MakeNameSpaceInfo()
{
    UbseSsuNameSpaceInfo info{};
    info.tgtEid = std::string(MAX_EID_LEN, 'e');
    info.tgtNqn = std::string(MAX_NQN_LEN, 'n');
    info.nsUuid = std::string(MAX_UUID_LEN, 'u');
    info.namespaceId = 100;
    info.nsDevPath = "/dev/nvme0n1";
    info.nsSize = 4096;
    info.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_4K;
    info.allowHostNqnList = {std::string(MAX_NQN_LEN, 'h'), std::string(10, 'x')};
    return info;
}

UbseSsuAllocResult TestUbseSsuObjMessage::MakeAllocResult()
{
    UbseSsuAllocResult result{};
    result.name = "alloc_test";
    result.strategy = UbseSsuAllocStrategy::STRIPED;
    result.nameSpaceList = {MakeNameSpaceInfo(), MakeNameSpaceInfo()};
    return result;
}

UbseSsuConnectInfo TestUbseSsuObjMessage::MakeConnectInfo()
{
    UbseSsuConnectInfo info{};
    info.srcEid = std::string(MAX_EID_LEN, 's');
    info.tgtEid = std::string(MAX_EID_LEN, 't');
    info.tgtNqn = std::string(MAX_NQN_LEN, 'n');
    info.hostNqn = std::string(MAX_NQN_LEN, 'h');
    info.nsUuid = std::string(MAX_UUID_LEN, 'u');
    info.nsId = 200;
    return info;
}

UbseSsuNsStats TestUbseSsuObjMessage::MakeNsStats()
{
    UbseSsuNsStats stats{};
    stats.nsUuid = std::string(MAX_UUID_LEN, 'u');
    stats.nsId = 300;
    stats.totalSize = 1024 * 1024;
    stats.usedSize = 512 * 1024;
    return stats;
}

UbseSsuFe TestUbseSsuObjMessage::MakeFe()
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

// ===== String pack/unpack =====

TEST_F(TestUbseSsuObjMessage, String_RoundTrip_Normal)
{
    const uint32_t maxLen = 48;
    const std::string src = "hello ssu";
    uint8_t buf[128] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_EQ(StringPack(packer, src, maxLen), UBSE_OK);

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    std::string dst;
    ASSERT_EQ(StringUnpack(unpacker, dst, maxLen), UBSE_OK);
    EXPECT_EQ(dst, src);
}

TEST_F(TestUbseSsuObjMessage, String_RoundTrip_Empty)
{
    const uint32_t maxLen = 48;
    const std::string src;
    uint8_t buf[16] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_EQ(StringPack(packer, src, maxLen), UBSE_OK);

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    std::string dst = "should_be_cleared";
    ASSERT_EQ(StringUnpack(unpacker, dst, maxLen), UBSE_OK);
    EXPECT_TRUE(dst.empty());
}

TEST_F(TestUbseSsuObjMessage, String_RoundTrip_MaxLength)
{
    const uint32_t maxLen = MAX_NAME_LEN;
    const std::string src(maxLen, 'x');
    uint8_t buf[64] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_EQ(StringPack(packer, src, maxLen), UBSE_OK);

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    std::string dst;
    ASSERT_EQ(StringUnpack(unpacker, dst, maxLen), UBSE_OK);
    EXPECT_EQ(dst, src);
}

TEST_F(TestUbseSsuObjMessage, StringPack_TooLong_ReturnsError)
{
    const uint32_t maxLen = 10;
    const std::string src(maxLen + 1, 'a');
    uint8_t buf[64] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    EXPECT_EQ(StringPack(packer, src, maxLen), UBSE_ERROR_SERIALIZE_FAILED);
}

TEST_F(TestUbseSsuObjMessage, StringUnpack_DeclaredLenExceedsMax_ReturnsError)
{
    const uint32_t maxLen = 5;
    uint8_t buf[16] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    // 写入 len=10，超过 maxLen=5（UbsePackString 会截断为 maxLen 参数值，这里用 maxLen=10 写入完整）
    ASSERT_TRUE(packer.UbsePackString(std::string(10, 'x'), 10));

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    std::string dst;
    EXPECT_EQ(StringUnpack(unpacker, dst, maxLen), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseSsuObjMessage, StringUnpack_DeclaredLenExceedsRemaining_ReturnsError)
{
    const uint32_t maxLen = 100;
    uint8_t buf[8] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    // 声明 len=200，但缓冲区剩余不足以容纳 200 字节
    ASSERT_TRUE(packer.UbsePackUint32(200));

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    std::string dst;
    EXPECT_EQ(StringUnpack(unpacker, dst, maxLen), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseSsuObjMessage, StringCalcSize_MatchesActual)
{
    const uint32_t maxLen = 48;
    const std::string src = "calc_size_test";
    auto expected = StringCalcSize(src, maxLen);
    EXPECT_EQ(expected, sizeof(uint32_t) + src.length());
}

// ===== Guid pack/unpack（定长 32 字节二进制） =====

TEST_F(TestUbseSsuObjMessage, Guid_RoundTrip_Normal)
{
    const std::string src(UBS_SSU_GUID_LENGTH, 'g');
    uint8_t buf[UBS_SSU_GUID_LENGTH] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_EQ(GuidPack(packer, src), UBSE_OK);

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    std::string dst;
    ASSERT_EQ(GuidUnpack(unpacker, dst), UBSE_OK);
    EXPECT_EQ(dst, src);
}

TEST_F(TestUbseSsuObjMessage, Guid_RoundTrip_EmptyString)
{
    const std::string src; // 空 GUID → 32 字节全 0
    uint8_t buf[UBS_SSU_GUID_LENGTH] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_EQ(GuidPack(packer, src), UBSE_OK);

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    std::string dst;
    ASSERT_EQ(GuidUnpack(unpacker, dst), UBSE_OK);
    // 全 0 解包后为空字符串
    EXPECT_TRUE(dst.empty());
}

TEST_F(TestUbseSsuObjMessage, GuidPack_TruncatesLongString)
{
    // GUID 超过 32 字节时，GuidPack 截断为 32 字节（不报错）
    const std::string src(UBS_SSU_GUID_LENGTH + 10, 'z');
    uint8_t buf[UBS_SSU_GUID_LENGTH] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    EXPECT_EQ(GuidPack(packer, src), UBSE_OK);

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    std::string dst;
    ASSERT_EQ(GuidUnpack(unpacker, dst), UBSE_OK);
    EXPECT_EQ(dst, std::string(UBS_SSU_GUID_LENGTH, 'z'));
}

TEST_F(TestUbseSsuObjMessage, GuidPack_BufferTooSmall_ReturnsError)
{
    const std::string src(UBS_SSU_GUID_LENGTH, 'g');
    uint8_t buf[4] = {0}; // 远不足 32 字节
    UbsePackUtil packer(buf, sizeof(buf));
    EXPECT_EQ(GuidPack(packer, src), UBSE_ERROR_SERIALIZE_FAILED);
}

TEST_F(TestUbseSsuObjMessage, GuidUnpack_BufferTooSmall_ReturnsError)
{
    uint8_t buf[4] = {0}; // 不足 32 字节
    UbseUnpackUtil unpacker(buf, sizeof(buf));
    std::string dst;
    EXPECT_EQ(GuidUnpack(unpacker, dst), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseSsuObjMessage, StringToArrayForGuid_PadsShortString)
{
    const std::string src = "abc"; // 3 字节
    auto arr = StringToArrayForGuid(src);
    EXPECT_EQ(arr[0], 'a');
    EXPECT_EQ(arr[1], 'b');
    EXPECT_EQ(arr[2], 'c');
    EXPECT_EQ(arr[3], 0); // 剩余补 0
    EXPECT_EQ(arr[UBS_SSU_GUID_LENGTH - 1], 0);
}

TEST_F(TestUbseSsuObjMessage, StringToArrayForGuid_TruncatesLongString)
{
    const std::string src(UBS_SSU_GUID_LENGTH + 5, 'x');
    auto arr = StringToArrayForGuid(src);
    for (size_t i = 0; i < UBS_SSU_GUID_LENGTH; ++i) {
        EXPECT_EQ(arr[i], 'x');
    }
}

// ===== Vfe pack/unpack =====

TEST_F(TestUbseSsuObjMessage, Vfe_RoundTrip)
{
    const auto src = MakeVfe();
    uint8_t buf[256] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_EQ(VfePack(packer, src), UBSE_OK);

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    UbseSsuVfe dst{};
    ASSERT_EQ(VfeUnpack(unpacker, dst), UBSE_OK);
    EXPECT_EQ(dst.slotId, src.slotId);
    EXPECT_EQ(dst.chipId, src.chipId);
    EXPECT_EQ(dst.dieId, src.dieId);
    EXPECT_EQ(dst.pfeId, src.pfeId);
    EXPECT_EQ(dst.vfeId, src.vfeId);
    EXPECT_EQ(dst.vfeGuid, src.vfeGuid);
    EXPECT_EQ(dst.bindBusInstanceGuid, src.bindBusInstanceGuid);
}

TEST_F(TestUbseSsuObjMessage, Vfe_RoundTrip_EmptyGuids)
{
    auto src = MakeVfe();
    src.vfeGuid.clear();
    src.bindBusInstanceGuid.clear();
    uint8_t buf[256] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_EQ(VfePack(packer, src), UBSE_OK);

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    UbseSsuVfe dst{};
    ASSERT_EQ(VfeUnpack(unpacker, dst), UBSE_OK);
    // 空 GUID 解包后为空字符串（全 0 转 empty）
    EXPECT_TRUE(dst.vfeGuid.empty());
    EXPECT_TRUE(dst.bindBusInstanceGuid.empty());
}

TEST_F(TestUbseSsuObjMessage, VfePack_BufferTooSmall_ReturnsError)
{
    const auto src = MakeVfe();
    uint8_t buf[4] = {0}; // 远不足
    UbsePackUtil packer(buf, sizeof(buf));
    EXPECT_EQ(VfePack(packer, src), UBSE_ERROR_SERIALIZE_FAILED);
}

TEST_F(TestUbseSsuObjMessage, VfeUnpack_BufferTooSmall_ReturnsError)
{
    uint8_t buf[4] = {0}; // 不足 Vfe 定长部分
    UbseUnpackUtil unpacker(buf, sizeof(buf));
    UbseSsuVfe dst{};
    EXPECT_EQ(VfeUnpack(unpacker, dst), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseSsuObjMessage, VfeCalcSize_MatchesFields)
{
    const auto src = MakeVfe();
    auto expected = VfeCalcSize(src);
    // 3*uint8 + 2*uint16 + 2*GUID(32)
    auto manual = sizeof(uint8_t) * 3 + sizeof(uint16_t) * 2 + UBS_SSU_GUID_LENGTH * 2;
    EXPECT_EQ(expected, manual);
}

// ===== NameSpaceInfo pack/unpack =====

TEST_F(TestUbseSsuObjMessage, NameSpaceInfo_RoundTrip)
{
    const auto src = MakeNameSpaceInfo();
    uint8_t buf[512] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_EQ(NameSpaceInfoPack(packer, src), UBSE_OK);

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    UbseSsuNameSpaceInfo dst{};
    ASSERT_EQ(NameSpaceInfoUnpack(unpacker, dst), UBSE_OK);
    EXPECT_EQ(dst.tgtEid, src.tgtEid);
    EXPECT_EQ(dst.tgtNqn, src.tgtNqn);
    EXPECT_EQ(dst.nsUuid, src.nsUuid);
    EXPECT_EQ(dst.namespaceId, src.namespaceId);
    EXPECT_EQ(dst.nsDevPath, src.nsDevPath);
    EXPECT_EQ(dst.nsSize, src.nsSize);
    EXPECT_EQ(static_cast<uint32_t>(dst.lbaFormat), static_cast<uint32_t>(src.lbaFormat));
    EXPECT_EQ(dst.allowHostNqnList, src.allowHostNqnList);
}

TEST_F(TestUbseSsuObjMessage, NameSpaceInfoPack_TgtNqnTooLong_ReturnsError)
{
    auto src = MakeNameSpaceInfo();
    src.tgtNqn = std::string(MAX_NQN_LEN + 1, 'n');
    uint8_t buf[512] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    EXPECT_EQ(NameSpaceInfoPack(packer, src), UBSE_ERROR_SERIALIZE_FAILED);
}

TEST_F(TestUbseSsuObjMessage, NameSpaceInfoPack_BufferTooSmall_ReturnsError)
{
    const auto src = MakeNameSpaceInfo();
    uint8_t buf[8] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    EXPECT_EQ(NameSpaceInfoPack(packer, src), UBSE_ERROR_SERIALIZE_FAILED);
}

TEST_F(TestUbseSsuObjMessage, NameSpaceInfo_RoundTrip_EmptyAllowHostNqnList)
{
    auto src = MakeNameSpaceInfo();
    src.allowHostNqnList.clear();
    uint8_t buf[512] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_EQ(NameSpaceInfoPack(packer, src), UBSE_OK);

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    UbseSsuNameSpaceInfo dst{};
    ASSERT_EQ(NameSpaceInfoUnpack(unpacker, dst), UBSE_OK);
    EXPECT_TRUE(dst.allowHostNqnList.empty());
}

TEST_F(TestUbseSsuObjMessage, NameSpaceInfoUnpack_InvalidLBAFormat_ReturnsError)
{
    auto src = MakeNameSpaceInfo();
    // 手动构造非法 lbaFormat 的 buffer
    uint8_t buf[512] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_TRUE(packer.UbsePackString(src.tgtEid, MAX_EID_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.tgtNqn, MAX_NQN_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.nsUuid, MAX_UUID_LEN));
    ASSERT_TRUE(packer.UbsePackUint32(src.namespaceId));
    ASSERT_TRUE(packer.UbsePackString(src.nsDevPath, MAX_DEV_PATH_LEN));
    ASSERT_TRUE(packer.UbsePackUint64(src.nsSize));
    ASSERT_TRUE(packer.UbsePackUint32(1024)); // 非法 lbaFormat
    ASSERT_TRUE(packer.UbsePackUint32(0));    // nqnCount

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    UbseSsuNameSpaceInfo dst{};
    EXPECT_EQ(NameSpaceInfoUnpack(unpacker, dst), UBSE_ERROR_DESERIALIZE_FAILED);
}

// ===== AllocResult pack =====

TEST_F(TestUbseSsuObjMessage, AllocResultPack_WithCalcSizeBuffer_Success)
{
    const auto src = MakeAllocResult();
    auto expectedSize = AllocResultCalcSize(src);
    std::vector<uint8_t> buf(expectedSize, 0);
    UbsePackUtil packer(buf.data(), buf.size());
    // CalcSize 精确预分配 buffer, Pack 成功即验证大小匹配
    ASSERT_EQ(AllocResultPack(packer, src), UBSE_OK);
}

TEST_F(TestUbseSsuObjMessage, AllocResultPack_NameTooLong_ReturnsError)
{
    auto src = MakeAllocResult();
    src.name = std::string(MAX_NAME_LEN + 1, 'n');
    uint8_t buf[1024] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    EXPECT_EQ(AllocResultPack(packer, src), UBSE_ERROR_SERIALIZE_FAILED);
}

TEST_F(TestUbseSsuObjMessage, AllocResultPack_BufferTooSmall_ReturnsError)
{
    const auto src = MakeAllocResult();
    uint8_t buf[4] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    EXPECT_EQ(AllocResultPack(packer, src), UBSE_ERROR_SERIALIZE_FAILED);
}

// ===== NsDevPaths pack =====

TEST_F(TestUbseSsuObjMessage, NsDevPathsPack_WithCalcSizeBuffer_Success)
{
    const std::vector<std::string> src = {"/dev/nvme0n1", "/dev/nvme1n2", "/dev/nvme2n3"};
    auto expectedSize = NsDevPathsCalcSize(src);
    std::vector<uint8_t> buf(expectedSize, 0);
    UbsePackUtil packer(buf.data(), buf.size());
    ASSERT_EQ(NsDevPathsPack(packer, src), UBSE_OK);
}

TEST_F(TestUbseSsuObjMessage, NsDevPathsPack_EmptyList_Success)
{
    const std::vector<std::string> src;
    uint8_t buf[16] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_EQ(NsDevPathsPack(packer, src), UBSE_OK);
}

TEST_F(TestUbseSsuObjMessage, NsDevPathsPack_PathTooLong_ReturnsError)
{
    const std::vector<std::string> src = {std::string(MAX_DEV_PATH_LEN + 1, 'p')};
    uint8_t buf[256] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    EXPECT_EQ(NsDevPathsPack(packer, src), UBSE_ERROR_SERIALIZE_FAILED);
}

// ===== ConnectInfo pack =====

TEST_F(TestUbseSsuObjMessage, ConnectInfoPack_WithCalcSizeBuffer_Success)
{
    const auto src = MakeConnectInfo();
    auto expectedSize = ConnectInfoCalcSize(src);
    std::vector<uint8_t> buf(expectedSize, 0);
    UbsePackUtil packer(buf.data(), buf.size());
    ASSERT_EQ(ConnectInfoPack(packer, src), UBSE_OK);
}

TEST_F(TestUbseSsuObjMessage, ConnectInfoPack_HostNqnTooLong_ReturnsError)
{
    auto src = MakeConnectInfo();
    src.hostNqn = std::string(MAX_NQN_LEN + 1, 'h');
    uint8_t buf[512] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    EXPECT_EQ(ConnectInfoPack(packer, src), UBSE_ERROR_SERIALIZE_FAILED);
}

// ===== NsStats pack =====

TEST_F(TestUbseSsuObjMessage, NsStatsPack_WithCalcSizeBuffer_Success)
{
    const auto src = MakeNsStats();
    auto expectedSize = NsStatsCalcSize(src);
    std::vector<uint8_t> buf(expectedSize, 0);
    UbsePackUtil packer(buf.data(), buf.size());
    ASSERT_EQ(NsStatsPack(packer, src), UBSE_OK);
}

TEST_F(TestUbseSsuObjMessage, NsStatsPack_NsUuidTooLong_ReturnsError)
{
    auto src = MakeNsStats();
    src.nsUuid = std::string(MAX_UUID_LEN + 1, 'u');
    uint8_t buf[128] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    EXPECT_EQ(NsStatsPack(packer, src), UBSE_ERROR_SERIALIZE_FAILED);
}

// ===== Fe pack =====

TEST_F(TestUbseSsuObjMessage, FePack_WithCalcSizeBuffer_Success)
{
    const auto src = MakeFe();
    auto expectedSize = FeCalcSize(src);
    std::vector<uint8_t> buf(expectedSize, 0);
    UbsePackUtil packer(buf.data(), buf.size());
    ASSERT_EQ(FePack(packer, src), UBSE_OK);
}

TEST_F(TestUbseSsuObjMessage, FePack_BufferTooSmall_ReturnsError)
{
    const auto src = MakeFe();
    uint8_t buf[4] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    EXPECT_EQ(FePack(packer, src), UBSE_ERROR_SERIALIZE_FAILED);
}

TEST_F(TestUbseSsuObjMessage, FePack_EmptyVfeList_Success)
{
    auto src = MakeFe();
    src.vfeList.clear();
    uint8_t buf[128] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_EQ(FePack(packer, src), UBSE_OK);
}

// ===== SpaceReq / LinearSpaceReq / StripedSpaceReq Unpack =====
// 注意：SpaceReqUnpack 只解 name/nqn/srcEid，不含 identity（identity 由 handler Handle 时赋值）

TEST_F(TestUbseSsuObjMessage, SpaceReqUnpack_RoundTrip)
{
    UbseSsuSpaceReq src{};
    src.name = "space_test";
    src.nqn = "nqn.2024-01.com.huawei:uuid:1234";
    src.srcEid = "eid_src_001";

    uint8_t buf[256] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_TRUE(packer.UbsePackString(src.name, MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.nqn, MAX_NQN_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.srcEid, MAX_EID_LEN));

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    UbseSsuSpaceReq dst{};
    ASSERT_EQ(SpaceReqUnpack(unpacker, dst), UBSE_OK);
    EXPECT_EQ(dst.name, src.name);
    EXPECT_EQ(dst.nqn, src.nqn);
    EXPECT_EQ(dst.srcEid, src.srcEid);
}

TEST_F(TestUbseSsuObjMessage, SpaceReqUnpack_NameTooLong_ReturnsError)
{
    uint8_t buf[128] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_TRUE(packer.UbsePackString(std::string(MAX_NAME_LEN + 1, 'n'), MAX_NAME_LEN + 1));

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    UbseSsuSpaceReq dst{};
    EXPECT_EQ(SpaceReqUnpack(unpacker, dst), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseSsuObjMessage, LinearSpaceReqUnpack_RoundTrip)
{
    UbseSsuLinearSpaceReq src{};
    src.name = "linear_test";
    src.nqn = "nqn.linear";
    src.srcEid = "eid_l_01";
    src.devName = "linear_dev0";

    uint8_t buf[256] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_TRUE(packer.UbsePackString(src.name, MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.nqn, MAX_NQN_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.srcEid, MAX_EID_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.devName, MAX_DEV_NAME_LEN));

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    UbseSsuLinearSpaceReq dst{};
    ASSERT_EQ(LinearSpaceReqUnpack(unpacker, dst), UBSE_OK);
    EXPECT_EQ(dst.name, src.name);
    EXPECT_EQ(dst.nqn, src.nqn);
    EXPECT_EQ(dst.srcEid, src.srcEid);
    EXPECT_EQ(dst.devName, src.devName);
}

TEST_F(TestUbseSsuObjMessage, StripedSpaceReqUnpack_RoundTrip_raid0_4k)
{
    UbseSsuStripedSpaceReq src{};
    src.name = "striped_test";
    src.nqn = "nqn.striped";
    src.srcEid = "eid_s_01";
    src.devName = "striped_dev0";
    src.level = UbseSsuAggregationRaidLevel::RAID0;
    src.chunkSize = UbseSsuChunkSize::CHUNK_SIZE_4K;

    uint8_t buf[256] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_TRUE(packer.UbsePackString(src.name, MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.nqn, MAX_NQN_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.srcEid, MAX_EID_LEN));
    ASSERT_TRUE(packer.UbsePackString(src.devName, MAX_DEV_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackUint8(static_cast<uint8_t>(src.level)));
    ASSERT_TRUE(packer.UbsePackUint32(static_cast<uint32_t>(src.chunkSize)));

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    UbseSsuStripedSpaceReq dst{};
    ASSERT_EQ(StripedSpaceReqUnpack(unpacker, dst), UBSE_OK);
    EXPECT_EQ(dst.name, src.name);
    EXPECT_EQ(dst.devName, src.devName);
    EXPECT_EQ(static_cast<uint8_t>(dst.level), static_cast<uint8_t>(src.level));
    EXPECT_EQ(static_cast<uint32_t>(dst.chunkSize), static_cast<uint32_t>(src.chunkSize));
}

TEST_F(TestUbseSsuObjMessage, StripedSpaceReqUnpack_InvalidRaidLevel_ReturnsError)
{
    uint8_t buf[256] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_TRUE(packer.UbsePackString("name", MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackString("nqn", MAX_NQN_LEN));
    ASSERT_TRUE(packer.UbsePackString("eid", MAX_EID_LEN));
    ASSERT_TRUE(packer.UbsePackString("dev", MAX_DEV_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackUint8(2)); // 非法 raidLevel（合法值 0/5）
    ASSERT_TRUE(packer.UbsePackUint32(static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_4K)));

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    UbseSsuStripedSpaceReq dst{};
    EXPECT_EQ(StripedSpaceReqUnpack(unpacker, dst), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseSsuObjMessage, StripedSpaceReqUnpack_InvalidChunkSize_ReturnsError)
{
    uint8_t buf[256] = {0};
    UbsePackUtil packer(buf, sizeof(buf));
    ASSERT_TRUE(packer.UbsePackString("name", MAX_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackString("nqn", MAX_NQN_LEN));
    ASSERT_TRUE(packer.UbsePackString("eid", MAX_EID_LEN));
    ASSERT_TRUE(packer.UbsePackString("dev", MAX_DEV_NAME_LEN));
    ASSERT_TRUE(packer.UbsePackUint8(static_cast<uint8_t>(UbseSsuAggregationRaidLevel::RAID0)));
    ASSERT_TRUE(packer.UbsePackUint32(8)); // 非法 chunkSize

    UbseUnpackUtil unpacker(buf, sizeof(buf));
    UbseSsuStripedSpaceReq dst{};
    EXPECT_EQ(StripedSpaceReqUnpack(unpacker, dst), UBSE_ERROR_DESERIALIZE_FAILED);
}

// ===== IsValid 辅助函数 =====

TEST_F(TestUbseSsuObjMessage, IsValidLBAFormat_AllBranches)
{
    EXPECT_TRUE(IsValidLBAFormat(static_cast<uint32_t>(UbseSsuLBAFormat::LBA_FORMAT_512)));
    EXPECT_TRUE(IsValidLBAFormat(static_cast<uint32_t>(UbseSsuLBAFormat::LBA_FORMAT_4K)));
    EXPECT_FALSE(IsValidLBAFormat(0));
    EXPECT_FALSE(IsValidLBAFormat(1024));
    EXPECT_FALSE(IsValidLBAFormat(4097));
}

TEST_F(TestUbseSsuObjMessage, IsValidAllocStrategy_AllBranches)
{
    EXPECT_TRUE(IsValidAllocStrategy(static_cast<uint8_t>(UbseSsuAllocStrategy::STRIPED)));
    EXPECT_TRUE(IsValidAllocStrategy(static_cast<uint8_t>(UbseSsuAllocStrategy::LINEAR)));
    EXPECT_FALSE(IsValidAllocStrategy(3));
    EXPECT_FALSE(IsValidAllocStrategy(255));
}

TEST_F(TestUbseSsuObjMessage, IsValidAggregationRaidLevel_AllBranches)
{
    EXPECT_TRUE(IsValidAggregationRaidLevel(static_cast<uint8_t>(UbseSsuAggregationRaidLevel::RAID0)));
    EXPECT_TRUE(IsValidAggregationRaidLevel(static_cast<uint8_t>(UbseSsuAggregationRaidLevel::RAID5)));
    EXPECT_FALSE(IsValidAggregationRaidLevel(1));
    EXPECT_FALSE(IsValidAggregationRaidLevel(6));
}

TEST_F(TestUbseSsuObjMessage, IsValidChunkSize_AllBranches)
{
    EXPECT_TRUE(IsValidChunkSize(static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_4K)));
    EXPECT_TRUE(IsValidChunkSize(static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_16K)));
    EXPECT_TRUE(IsValidChunkSize(static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_32K)));
    EXPECT_TRUE(IsValidChunkSize(static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_64K)));
    EXPECT_TRUE(IsValidChunkSize(static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_128K)));
    EXPECT_TRUE(IsValidChunkSize(static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_256K)));
    EXPECT_TRUE(IsValidChunkSize(static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_512K)));
    EXPECT_FALSE(IsValidChunkSize(0));
    EXPECT_FALSE(IsValidChunkSize(8));
    EXPECT_FALSE(IsValidChunkSize(1024));
}

} // namespace ubse::ssu::ipc::ut
