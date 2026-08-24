/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_ssu_alloc_msg.h"
#include "ubse_error.h"

namespace ubse::ssu::message::ut {

using namespace ubse::plugin::service::ssu;

void TestUbseSsuAllocMsg::SetUp()
{
    testing::Test::SetUp();
}

void TestUbseSsuAllocMsg::TearDown()
{
    testing::Test::TearDown();
}

/*
 * 用例描述：测试 UbseSsuAllocReqMsg 序列化与反序列化往返
 * 预期结果：各字段值一致
 */
TEST_F(TestUbseSsuAllocMsg, ReqRoundTrip)
{
    UbseSsuAllocSpaceReq allocReq;
    allocReq.name = "test_alloc";
    allocReq.nsSize = 4096;
    allocReq.nsNum = 2;
    allocReq.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_4K;
    allocReq.strategy = UbseSsuAllocStrategy::LINEAR;
    allocReq.tenant = "tenant-1";

    UbseSsuAllocIdentityInfo identity{"alloc_user", 300};
    UbseSsuAllocReqMsg original("alloc-req", "node-3", identity, allocReq);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));
    ASSERT_NE(nullptr, buffer);
    ASSERT_GT(bufferSize, 0);

    UbseSsuAllocReqMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &got = deserialized.GetAllocRequest();
    EXPECT_EQ("alloc-req", got.requestId);
    EXPECT_EQ("node-3", got.requestNodeId);
    EXPECT_EQ("alloc_user", got.identityInfo.userName);
    EXPECT_EQ(static_cast<uid_t>(300), got.identityInfo.uid);
    EXPECT_EQ("test_alloc", got.allocReq.name);
    EXPECT_EQ(4096, got.allocReq.nsSize);
    EXPECT_EQ(2, got.allocReq.nsNum);
    EXPECT_EQ(UbseSsuLBAFormat::LBA_FORMAT_4K, got.allocReq.lbaFormat);
    EXPECT_EQ(UbseSsuAllocStrategy::LINEAR, got.allocReq.strategy);
    EXPECT_EQ("tenant-1", got.allocReq.tenant);
}

/*
 * 用例描述：测试 UbseSsuAllocReqMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuAllocMsg, ReqDeserializeNull)
{
    UbseSsuAllocReqMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}

/*
 * 用例描述：测试 UbseSsuAllocRespMsg 往返（空 NameSpaceList）
 * 预期结果：Serialize/Deserialize 返回 UBSE_OK，列表为空
 */
TEST_F(TestUbseSsuAllocMsg, RespEmptyNsList)
{
    UbseSsuAllocResult result;
    result.name = "test_result";
    result.strategy = UbseSsuAllocStrategy::LINEAR;

    UbseSsuAllocResp resp{"alloc-resp", 0, UbseSsuNsState::ATTACHED, result};
    UbseSsuAllocRespMsg original(resp);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuAllocRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &got = deserialized.GetSsuAllocResp();
    EXPECT_EQ("alloc-resp", got.requestId);
    EXPECT_EQ(0, got.errorCode);
    EXPECT_EQ(UbseSsuNsState::ATTACHED, got.state);
    EXPECT_EQ("test_result", got.allocResult.name);
    EXPECT_TRUE(got.allocResult.nameSpaceList.empty());
}

/*
 * 用例描述：测试 UbseSsuAllocRespMsg 带 NameSpaceList 的往返
 * 预期结果：各字段值一致
 */
TEST_F(TestUbseSsuAllocMsg, RespWithNsList)
{
    UbseSsuNameSpaceInfo ns1;
    ns1.tgtEid = "eid-1";
    ns1.tgtNqn = "nqn-1";
    ns1.nsUuid = "uuid-1";
    ns1.namespaceId = 1;
    ns1.nsDevPath = "/dev/nvme0n1";
    ns1.nsSize = 4096;
    ns1.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;

    UbseSsuNameSpaceInfo ns2;
    ns2.tgtEid = "eid-2";
    ns2.tgtNqn = "nqn-2";
    ns2.nsUuid = "uuid-2";
    ns2.namespaceId = 2;
    ns2.nsDevPath = "/dev/nvme0n2";
    ns2.nsSize = 8192;
    ns2.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_4K;

    UbseSsuAllocResult result;
    result.name = "test_multi_ns";
    result.strategy = UbseSsuAllocStrategy::STRIPED;
    result.nameSpaceList = {ns1, ns2};

    UbseSsuAllocResp resp{"alloc-resp-2", 0, UbseSsuNsState::CREATED, result};
    UbseSsuAllocRespMsg original(resp);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuAllocRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &got = deserialized.GetSsuAllocResp();
    EXPECT_EQ("alloc-resp-2", got.requestId);
    EXPECT_EQ(0, got.errorCode);
    EXPECT_EQ(UbseSsuNsState::CREATED, got.state);
    EXPECT_EQ("test_multi_ns", got.allocResult.name);
    EXPECT_EQ(UbseSsuAllocStrategy::STRIPED, got.allocResult.strategy);
    ASSERT_EQ(2, got.allocResult.nameSpaceList.size());
    EXPECT_EQ("eid-1", got.allocResult.nameSpaceList[0].tgtEid);
    EXPECT_EQ("nqn-1", got.allocResult.nameSpaceList[0].tgtNqn);
    EXPECT_EQ(1, got.allocResult.nameSpaceList[0].namespaceId);
    EXPECT_EQ(4096, got.allocResult.nameSpaceList[0].nsSize);
    EXPECT_EQ(UbseSsuLBAFormat::LBA_FORMAT_512, got.allocResult.nameSpaceList[0].lbaFormat);
    EXPECT_EQ("eid-2", got.allocResult.nameSpaceList[1].tgtEid);
    EXPECT_EQ(2, got.allocResult.nameSpaceList[1].namespaceId);
    EXPECT_EQ(8192, got.allocResult.nameSpaceList[1].nsSize);
    EXPECT_EQ(UbseSsuLBAFormat::LBA_FORMAT_4K, got.allocResult.nameSpaceList[1].lbaFormat);
}

/*
 * 用例描述：测试 UbseSsuAllocRespMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuAllocMsg, RespDeserializeNull)
{
    UbseSsuAllocRespMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}

/*
 * 用例描述：测试 UbseSsuAllocRespMsg 反序列化时 nameSpaceList 计数超过上限(1024)
 * 预期结果：返回 UBSE_ERROR，不会预分配超大 vector
 */
TEST_F(TestUbseSsuAllocMsg, RespNsListExceedsLimit)
{
    // 手动构造一个合法的 buffer，但将 nameSpaceList 计数字段设为 MAX_ALLOC_RESULT_NS_NUM + 1
    using ubse::serial::enum_v;
    ubse::serial::UbseSerialization out;
    std::string overflowResp("overflow-resp");
    out << overflowResp;
    uint32_t errCode = 0;
    out << errCode;
    out << enum_v(UbseSsuNsState::CREATED);
    std::string name("overflow");
    out << name;
    uint32_t overflowCnt = 1025; // 超过 MAX_ALLOC_RESULT_NS_NUM = 1024
    out << overflowCnt;
    out << enum_v(UbseSsuAllocStrategy::LINEAR);
    // 不再写入任何 NameSpaceInfo，让反序列化在 cnt 校验阶段失败

    std::unique_ptr<uint8_t[]> badBuffer;
    uint32_t badSize = out.GetLength();
    badBuffer.reset(out.GetBuffer(true));

    UbseSsuAllocRespMsg deserialized;
    EXPECT_NE(UBSE_OK, deserialized.Deserialize(badBuffer.get(), badSize));
}
} // namespace ubse::ssu::message::ut
