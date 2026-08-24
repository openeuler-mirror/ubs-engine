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

#include "test_ubse_ssu_query_verify_msg.h"
#include "ubse_error.h"

namespace ubse::ssu::message::ut {

using namespace ubse::plugin::service::ssu;

void TestUbseSsuQueryVerifyMsg::SetUp()
{
    testing::Test::SetUp();
}

void TestUbseSsuQueryVerifyMsg::TearDown()
{
    testing::Test::TearDown();
}

// ====== GetNsStats ======

/*
 * 用例描述：测试 UbseSsuGetNsStatsReqMsg 往返
 * 预期结果：各字段值一致
 */
TEST_F(TestUbseSsuQueryVerifyMsg, GetNsStatsReqRoundTrip)
{
    UbseSsuAllocIdentityInfo identity{"stats_user", 400};
    UbseSsuGetNsStatsReqMsg original("stats-req", "node-4", "test_stats", identity);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));
    ASSERT_NE(nullptr, buffer);
    ASSERT_GT(bufferSize, 0);

    UbseSsuGetNsStatsReqMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &req = deserialized.GetGetNsStatsReq();
    EXPECT_EQ("stats-req", req.requestId);
    EXPECT_EQ("node-4", req.requestNodeId);
    EXPECT_EQ("test_stats", req.name);
    EXPECT_EQ("stats_user", req.identityInfo.userName);
    EXPECT_EQ(static_cast<uid_t>(400), req.identityInfo.uid);
}

/*
 * 用例描述：测试 UbseSsuGetNsStatsReqMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuQueryVerifyMsg, GetNsStatsReqDeserializeNull)
{
    UbseSsuGetNsStatsReqMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}

/*
 * 用例描述：测试 UbseSsuGetNsStatsRespMsg 往返（空列表）
 * 预期结果：Serialize/Deserialize 返回 UBSE_OK，列表为空
 */
TEST_F(TestUbseSsuQueryVerifyMsg, GetNsStatsRespEmpty)
{
    UbseSsuGetNsStatsResp resp{"stats-resp", 0, {}};
    UbseSsuGetNsStatsRespMsg original(resp);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuGetNsStatsRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &got = deserialized.GetGetNsStatsResp();
    EXPECT_EQ("stats-resp", got.requestId);
    EXPECT_EQ(0, got.errorCode);
    EXPECT_TRUE(got.statsList.empty());
}

/*
 * 用例描述：测试 UbseSsuGetNsStatsRespMsg 往返（含统计数据）
 * 预期结果：各字段值一致
 */
TEST_F(TestUbseSsuQueryVerifyMsg, GetNsStatsRespWithStats)
{
    UbseSsuNsStats s1{"uuid-ns-1", 1, 1000, 500};
    UbseSsuNsStats s2{"uuid-ns-2", 2, 2000, 1200};
    UbseSsuGetNsStatsResp resp{"stats-resp-2", 0, {s1, s2}};
    UbseSsuGetNsStatsRespMsg original(resp);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuGetNsStatsRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &got = deserialized.GetGetNsStatsResp();
    ASSERT_EQ(2, got.statsList.size());
    EXPECT_EQ("uuid-ns-1", got.statsList[0].nsUuid);
    EXPECT_EQ(1, got.statsList[0].nsId);
    EXPECT_EQ(1000, got.statsList[0].totalSize);
    EXPECT_EQ(500, got.statsList[0].usedSize);
    EXPECT_EQ("uuid-ns-2", got.statsList[1].nsUuid);
    EXPECT_EQ(2, got.statsList[1].nsId);
    EXPECT_EQ(2000, got.statsList[1].totalSize);
    EXPECT_EQ(1200, got.statsList[1].usedSize);
}

/*
 * 用例描述：测试 UbseSsuGetNsStatsRespMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuQueryVerifyMsg, GetNsStatsRespDeserializeNull)
{
    UbseSsuGetNsStatsRespMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}

// ====== ListAllocInfo ======

/*
 * 用例描述：测试 UbseSsuListAllocInfoReqMsg 往返
 * 预期结果：各字段值一致
 */
TEST_F(TestUbseSsuQueryVerifyMsg, ListAllocInfoReqRoundTrip)
{
    UbseSsuAllocIdentityInfo identity{"list_user", 500};
    UbseSsuListAllocInfoReqMsg original("list-req", "node-5", identity);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuListAllocInfoReqMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &req = deserialized.GetListAllocInfoReq();
    EXPECT_EQ("list-req", req.requestId);
    EXPECT_EQ("node-5", req.requestNodeId);
    EXPECT_EQ("list_user", req.identityInfo.userName);
    EXPECT_EQ(static_cast<uid_t>(500), req.identityInfo.uid);
}

/*
 * 用例描述：测试 UbseSsuListAllocInfoReqMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuQueryVerifyMsg, ListAllocInfoReqDeserializeNull)
{
    UbseSsuListAllocInfoReqMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}

/*
 * 用例描述：测试 UbseSsuListAllocInfoRespMsg 往返（空结果列表）
 * 预期结果：Serialize/Deserialize 返回 UBSE_OK，列表为空
 */
TEST_F(TestUbseSsuQueryVerifyMsg, ListAllocInfoRespEmpty)
{
    UbseSsuListAllocInfoResp resp{"list-resp", 0, {}};
    UbseSsuListAllocInfoRespMsg original(resp);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuListAllocInfoRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &got = deserialized.GetListAllocInfoResp();
    EXPECT_EQ("list-resp", got.requestId);
    EXPECT_EQ(0, got.errorCode);
    EXPECT_TRUE(got.results.empty());
}

/*
 * 用例描述：测试 UbseSsuListAllocInfoRespMsg 往返（含结果）
 * 预期结果：各字段值一致
 */
TEST_F(TestUbseSsuQueryVerifyMsg, ListAllocInfoRespWithResults)
{
    UbseSsuNameSpaceInfo ns1;
    ns1.tgtEid = "eid-x";
    ns1.tgtNqn = "nqn-x";
    ns1.namespaceId = 10;

    UbseSsuAllocResult r1;
    r1.name = "result-1";
    r1.strategy = UbseSsuAllocStrategy::LINEAR;
    r1.nameSpaceList = {ns1};

    UbseSsuListAllocInfoResp resp{"list-resp-2", 0, {r1}};
    UbseSsuListAllocInfoRespMsg original(resp);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuListAllocInfoRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &got = deserialized.GetListAllocInfoResp();
    ASSERT_EQ(1, got.results.size());
    EXPECT_EQ("result-1", got.results[0].name);
    EXPECT_EQ(UbseSsuAllocStrategy::LINEAR, got.results[0].strategy);
    ASSERT_EQ(1, got.results[0].nameSpaceList.size());
    EXPECT_EQ("eid-x", got.results[0].nameSpaceList[0].tgtEid);
    EXPECT_EQ("nqn-x", got.results[0].nameSpaceList[0].tgtNqn);
    EXPECT_EQ(10, got.results[0].nameSpaceList[0].namespaceId);
}

/*
 * 用例描述：测试 UbseSsuListAllocInfoRespMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuQueryVerifyMsg, ListAllocInfoRespDeserializeNull)
{
    UbseSsuListAllocInfoRespMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}

// ====== GetAllocInfoByName ======

/*
 * 用例描述：测试 UbseSsuGetAllocInfoReqMsg 往返
 * 预期结果：各字段值一致
 */
TEST_F(TestUbseSsuQueryVerifyMsg, GetAllocInfoReqRoundTrip)
{
    UbseSsuAllocIdentityInfo identity{"getinfo_user", 600};
    UbseSsuGetAllocInfoReqMsg original("getinfo-req", "node-6", "test_getinfo", identity);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuGetAllocInfoReqMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &req = deserialized.GetGetAllocInfoReq();
    EXPECT_EQ("getinfo-req", req.requestId);
    EXPECT_EQ("node-6", req.requestNodeId);
    EXPECT_EQ("test_getinfo", req.name);
    EXPECT_EQ("getinfo_user", req.identityInfo.userName);
    EXPECT_EQ(static_cast<uid_t>(600), req.identityInfo.uid);
}

/*
 * 用例描述：测试 UbseSsuGetAllocInfoReqMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuQueryVerifyMsg, GetAllocInfoReqDeserializeNull)
{
    UbseSsuGetAllocInfoReqMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}

/*
 * 用例描述：测试 UbseSsuGetAllocInfoRespMsg 往返
 * 预期结果：各字段值一致
 */
TEST_F(TestUbseSsuQueryVerifyMsg, GetAllocInfoRespRoundTrip)
{
    UbseSsuNameSpaceInfo ns;
    ns.tgtEid = "eid-y";
    ns.tgtNqn = "nqn-y";
    ns.namespaceId = 7;
    ns.nsSize = 16384;

    UbseSsuAllocResult result;
    result.name = "getinfo_result";
    result.strategy = UbseSsuAllocStrategy::STRIPED;
    result.nameSpaceList = {ns};

    UbseSsuGetAllocInfoResp resp{"getinfo-resp", 0, result};
    UbseSsuGetAllocInfoRespMsg original(resp);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuGetAllocInfoRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &got = deserialized.GetGetAllocInfoResp();
    EXPECT_EQ("getinfo-resp", got.requestId);
    EXPECT_EQ("getinfo_result", got.result.name);
    ASSERT_EQ(1, got.result.nameSpaceList.size());
    EXPECT_EQ("eid-y", got.result.nameSpaceList[0].tgtEid);
}

/*
 * 用例描述：测试 UbseSsuGetAllocInfoRespMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuQueryVerifyMsg, GetAllocInfoRespDeserializeNull)
{
    UbseSsuGetAllocInfoRespMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}

// ====== GetConnectInfo ======

/*
 * 用例描述：测试 UbseSsuGetConnectInfoReqMsg 往返（不含 VFE）
 * 预期结果：hasVfe 为 false
 */
TEST_F(TestUbseSsuQueryVerifyMsg, GetConnectInfoReqNoVfe)
{
    UbseSsuAllocIdentityInfo identity{"conn_user", 700};
    UbseSsuGetConnectInfoReqMsg original("conn-req", "node-7", "test_conn", identity, nullptr);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuGetConnectInfoReqMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &req = deserialized.GetGetConnectInfoReq();
    EXPECT_EQ("conn-req", req.requestId);
    EXPECT_FALSE(req.hasVfe);
}

/*
 * 用例描述：测试 UbseSsuGetConnectInfoReqMsg 往返（含 VFE）
 * 预期结果：各 VFE 字段值一致
 */
TEST_F(TestUbseSsuQueryVerifyMsg, GetConnectInfoReqWithVfe)
{
    UbseSsuVfe vfe;
    vfe.slotId = 1;
    vfe.chipId = 2;
    vfe.dieId = 3;
    vfe.pfeId = 4;
    vfe.vfeId = 5;
    vfe.vfeGuid = "vfe-guid-123";
    vfe.bindBusInstanceGuid = "bus-guid-456";

    UbseSsuAllocIdentityInfo identity{"conn_user_vfe", 701};
    UbseSsuGetConnectInfoReqMsg original("conn-req-vfe", "node-8", "test_conn_vfe", identity, &vfe);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuGetConnectInfoReqMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &req = deserialized.GetGetConnectInfoReq();
    EXPECT_TRUE(req.hasVfe);
    EXPECT_EQ(1, req.vfe.slotId);
    EXPECT_EQ(2, req.vfe.chipId);
    EXPECT_EQ(3, req.vfe.dieId);
    EXPECT_EQ(4, req.vfe.pfeId);
    EXPECT_EQ(5, req.vfe.vfeId);
    EXPECT_EQ("vfe-guid-123", req.vfe.vfeGuid);
    EXPECT_EQ("bus-guid-456", req.vfe.bindBusInstanceGuid);
}

/*
 * 用例描述：测试 UbseSsuGetConnectInfoReqMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuQueryVerifyMsg, GetConnectInfoReqDeserializeNull)
{
    UbseSsuGetConnectInfoReqMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}

/*
 * 用例描述：测试 UbseSsuGetConnectInfoRespMsg 往返（空列表）
 * 预期结果：Serialize/Deserialize 返回 UBSE_OK，列表为空
 */
TEST_F(TestUbseSsuQueryVerifyMsg, GetConnectInfoRespEmpty)
{
    UbseSsuGetConnectInfoResp resp{"conn-resp", 0, {}};
    UbseSsuGetConnectInfoRespMsg original(resp);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuGetConnectInfoRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &got = deserialized.GetGetConnectInfoResp();
    EXPECT_EQ("conn-resp", got.requestId);
    EXPECT_TRUE(got.connectInfoList.empty());
}

/*
 * 用例描述：测试 UbseSsuGetConnectInfoRespMsg 往返（含连接信息列表）
 * 预期结果：各字段值一致
 */
TEST_F(TestUbseSsuQueryVerifyMsg, GetConnectInfoRespWithList)
{
    UbseSsuConnectInfo ci1{"src-eid-1", "tgt-eid-1", "tgt-nqn-1", "host-nqn-1", "uuid-conn-1", 1};
    UbseSsuConnectInfo ci2{"src-eid-2", "tgt-eid-2", "tgt-nqn-2", "host-nqn-2", "uuid-conn-2", 2};
    UbseSsuGetConnectInfoResp resp{"conn-resp-2", 0, {ci1, ci2}};
    UbseSsuGetConnectInfoRespMsg original(resp);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuGetConnectInfoRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &got = deserialized.GetGetConnectInfoResp();
    ASSERT_EQ(2, got.connectInfoList.size());
    EXPECT_EQ("src-eid-1", got.connectInfoList[0].srcEid);
    EXPECT_EQ("tgt-eid-1", got.connectInfoList[0].tgtEid);
    EXPECT_EQ(1, got.connectInfoList[0].nsId);
    EXPECT_EQ("src-eid-2", got.connectInfoList[1].srcEid);
    EXPECT_EQ(2, got.connectInfoList[1].nsId);
}

/*
 * 用例描述：测试 UbseSsuGetConnectInfoRespMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuQueryVerifyMsg, GetConnectInfoRespDeserializeNull)
{
    UbseSsuGetConnectInfoRespMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}
} // namespace ubse::ssu::message::ut
