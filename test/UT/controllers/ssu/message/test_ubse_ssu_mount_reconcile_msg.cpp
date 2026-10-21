/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

// 运营对账消息serde UT（reliable_design.md §13.1/§13.2）：
// 摘要/平账请求响应往返、完整挂载记录（NEED_FULL_INFO补齐重发载荷）往返、非法枚举拒收

#include "test_ubse_ssu_mount_reconcile_msg.h"
#include "ubse_error.h"

namespace ubse::ssu::message::ut {

using namespace ubse::plugin::service::ssu;
using namespace ubse::adapter_plugins::ssu::def;

void TestUbseSsuMountReconcileMsg::SetUp()
{
    testing::Test::SetUp();
}

void TestUbseSsuMountReconcileMsg::TearDown()
{
    testing::Test::TearDown();
}

namespace {

// 完整挂载记录（NEED_FULL_INFO补齐重发载荷）：含NS明细+verify列表+聚合块设备选项
UbseSsuAttachRecord MakeFullRecord()
{
    UbseSsuAttachRecord record{};
    record.name = "sp_reconcile_serde";
    record.strategy = UbseSsuAllocStrategy::STRIPED;
    record.recordState = UbseSsuAttachRecordState::SUSPECT;
    record.devName = "agg_dev_serde";
    record.nqn = "nqn.2026-07.com.example:serde";
    record.identity.uid = 100;
    record.identity.userName = "test_user";
    record.blockDeviceOptions = UbseCreateBlockDeviceOptions{};
    record.blockDeviceOptions->addressingType = UbseSsuAddressingType::STRIPED;
    record.blockDeviceOptions->raidLevel = UbseSsuRaidLevel::RAID0;
    record.blockDeviceOptions->chunkSize = 512;

    UbseSsuNameSpaceInfo ns{};
    ns.tgtEid = "eid_serde_000001";
    ns.tgtNqn = "nqn.tgt.serde";
    ns.nsUuid = "uuid-serde";
    ns.namespaceId = 3;
    ns.nsDevPath = "/dev/nvme0n3";
    ns.nsSize = 4096;
    ns.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;
    record.nsList.push_back(ns);

    UbseSsuNsVerifyInfo verify{};
    verify.defaultNqn = "nqn.serde.default";
    verify.jettyId = 7;
    verify.guid = "guid-serde";
    record.nsVerifyList.push_back(verify);
    record.attachedAt = 1700000000;
    return record;
}

} // namespace

/*
 * 用例描述：UbseSsuMountSummaryReqMsg 空请求体序列化/反序列化往返
 * 预期结果：均返回 UBSE_OK
 */
TEST_F(TestUbseSsuMountReconcileMsg, SummaryReqEmptyBodyRoundTrip)
{
    UbseSsuMountSummaryReqMsg original;
    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));
    ASSERT_NE(nullptr, buffer);
    ASSERT_GT(bufferSize, 0);

    UbseSsuMountSummaryReqMsg deserialized;
    EXPECT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));
}

/*
 * 用例描述：UbseSsuMountSummaryRespMsg 摘要响应往返（各recordState枚举+devName）
 * 预期结果：errorCode与逐项name/recordState/devName一致
 */
TEST_F(TestUbseSsuMountReconcileMsg, SummaryRespRoundTrip)
{
    UbseSsuMountSummaryResp resp;
    resp.errorCode = UBSE_OK;
    UbseSsuMountSummaryItem item1;
    item1.name = "sp_attached";
    item1.recordState = UbseSsuAttachRecordState::ATTACHED;
    item1.devName = "agg_dev_1";
    UbseSsuMountSummaryItem item2;
    item2.name = "sp_pending";
    item2.recordState = UbseSsuAttachRecordState::PENDING;
    UbseSsuMountSummaryItem item3;
    item3.name = "sp_suspect";
    item3.recordState = UbseSsuAttachRecordState::SUSPECT;
    resp.items = {item1, item2, item3};

    UbseSsuMountSummaryRespMsg original(resp);
    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuMountSummaryRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &out = deserialized.GetMountSummaryResp();
    EXPECT_EQ(resp.errorCode, out.errorCode);
    ASSERT_EQ(3U, out.items.size());
    EXPECT_EQ("sp_attached", out.items[0].name);
    EXPECT_EQ(UbseSsuAttachRecordState::ATTACHED, out.items[0].recordState);
    EXPECT_EQ("agg_dev_1", out.items[0].devName);
    EXPECT_EQ(UbseSsuAttachRecordState::PENDING, out.items[1].recordState);
    EXPECT_EQ(UbseSsuAttachRecordState::SUSPECT, out.items[2].recordState);
}

/*
 * 用例描述：UbseSsuReconcileReqMsg 平账请求往返——REMOUNT首发（仅name/action）、
 * REMOUNT补齐重发（hasRecord=true携带完整挂载记录）、CLEANUP/REPORT轻量首发
 * 预期结果：动作/hasRecord/记录全字段/逐项result一致
 */
TEST_F(TestUbseSsuMountReconcileMsg, ReconcileReqRoundTrip)
{
    const auto fullRecord = MakeFullRecord();

    UbseSsuReconcileReq req;
    UbseSsuReconcileItem light;
    light.name = "sp_light";
    light.action = UbseSsuReconcileAction::REMOUNT; // 轻量首发
    UbseSsuReconcileItem refill;
    refill.name = "sp_refill";
    refill.action = UbseSsuReconcileAction::REMOUNT;
    refill.hasRecord = true; // NEED_FULL_INFO补齐重发
    refill.record = fullRecord;
    UbseSsuReconcileItem cleanup;
    cleanup.name = "sp_cleanup";
    cleanup.action = UbseSsuReconcileAction::CLEANUP;
    UbseSsuReconcileItem report;
    report.name = "sp_report";
    report.action = UbseSsuReconcileAction::REPORT;
    req.items = {light, refill, cleanup, report};

    UbseSsuReconcileReqMsg original(req);
    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuReconcileReqMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &out = deserialized.GetReconcileReq();
    ASSERT_EQ(4U, out.items.size());
    EXPECT_EQ("sp_light", out.items[0].name);
    EXPECT_EQ(UbseSsuReconcileAction::REMOUNT, out.items[0].action);
    EXPECT_FALSE(out.items[0].hasRecord);

    const auto &refillOut = out.items[1];
    EXPECT_EQ("sp_refill", refillOut.name);
    ASSERT_TRUE(refillOut.hasRecord);
    const auto &record = refillOut.record;
    EXPECT_EQ(fullRecord.name, record.name);
    EXPECT_EQ(fullRecord.strategy, record.strategy);
    EXPECT_EQ(fullRecord.recordState, record.recordState);
    EXPECT_EQ(fullRecord.devName, record.devName);
    EXPECT_EQ(fullRecord.nqn, record.nqn);
    EXPECT_EQ(fullRecord.identity.uid, record.identity.uid);
    EXPECT_EQ(fullRecord.identity.userName, record.identity.userName);
    ASSERT_TRUE(record.blockDeviceOptions.has_value());
    EXPECT_EQ(UbseSsuAddressingType::STRIPED, record.blockDeviceOptions->addressingType);
    EXPECT_EQ(UbseSsuRaidLevel::RAID0, record.blockDeviceOptions->raidLevel);
    EXPECT_EQ(512U, record.blockDeviceOptions->chunkSize);
    ASSERT_EQ(1U, record.nsList.size());
    EXPECT_EQ(fullRecord.nsList[0].tgtEid, record.nsList[0].tgtEid);
    EXPECT_EQ(fullRecord.nsList[0].nsUuid, record.nsList[0].nsUuid);
    EXPECT_EQ(fullRecord.nsList[0].nsDevPath, record.nsList[0].nsDevPath);
    ASSERT_EQ(1U, record.nsVerifyList.size());
    EXPECT_EQ(fullRecord.nsVerifyList[0].defaultNqn, record.nsVerifyList[0].defaultNqn);
    EXPECT_EQ(fullRecord.nsVerifyList[0].jettyId, record.nsVerifyList[0].jettyId);
    EXPECT_EQ(fullRecord.nsVerifyList[0].guid, record.nsVerifyList[0].guid);
    EXPECT_EQ(fullRecord.attachedAt, record.attachedAt);

    EXPECT_EQ(UbseSsuReconcileAction::CLEANUP, out.items[2].action);
    EXPECT_EQ(UbseSsuReconcileAction::REPORT, out.items[3].action);
}

/*
 * 用例描述：UbseSsuReconcileRespMsg 平账响应往返——逐项result（UBSE_OK/NEED_FULL_INFO）
 * 预期结果：errorCode与逐项result一致（master据此做NEED_FULL_INFO补齐重发裁决）
 */
TEST_F(TestUbseSsuMountReconcileMsg, ReconcileRespRoundTrip)
{
    UbseSsuReconcileResp resp;
    resp.errorCode = UBSE_OK;
    UbseSsuReconcileItem ok;
    ok.name = "sp_ok";
    ok.action = UbseSsuReconcileAction::REMOUNT;
    ok.result = UBSE_OK;
    UbseSsuReconcileItem needFull;
    needFull.name = "sp_need_full";
    needFull.action = UbseSsuReconcileAction::REMOUNT;
    needFull.result = UBSE_SSU_ERROR_NEED_FULL_INFO;
    resp.items = {ok, needFull};

    UbseSsuReconcileRespMsg original(resp);
    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuReconcileRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &out = deserialized.GetReconcileResp();
    EXPECT_EQ(resp.errorCode, out.errorCode);
    ASSERT_EQ(2U, out.items.size());
    EXPECT_EQ(UBSE_OK, out.items[0].result);
    EXPECT_EQ(UBSE_SSU_ERROR_NEED_FULL_INFO, out.items[1].result);
}

/*
 * 用例描述：REMOUNT首发记录identity为空（如账本重建漏补identity）→ 字段校验失败拒收
 * 预期结果：整个对账请求反序列化失败（IsRecordFieldsValid要求userName非空，
 * 上游BuildRecordFromLedger必须从NS customData补齐identity，否则对账无法收敛）
 */
TEST_F(TestUbseSsuMountReconcileMsg, ReconcileReqEmptyIdentityRejected)
{
    auto record = MakeFullRecord();
    record.identity = {}; // 模拟账本重建未补identity

    UbseSsuReconcileReq req;
    UbseSsuReconcileItem item;
    item.name = record.name;
    item.action = UbseSsuReconcileAction::REMOUNT;
    item.hasRecord = true;
    item.record = std::move(record);
    req.items.push_back(std::move(item));

    UbseSsuReconcileReqMsg original(req);
    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuReconcileReqMsg deserialized;
    EXPECT_NE(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));
}

/*
 * 用例描述：摘要/平账响应 Deserialize 传入空指针与非法数据
 * 预期结果：返回非 UBSE_OK（防篡改防御性解析）
 */
TEST_F(TestUbseSsuMountReconcileMsg, DeserializeNullAndInvalid)
{
    UbseSsuMountSummaryRespMsg summaryResp;
    EXPECT_NE(UBSE_OK, summaryResp.Deserialize(nullptr, 0));

    UbseSsuReconcileReqMsg reconcileReq;
    EXPECT_NE(UBSE_OK, reconcileReq.Deserialize(nullptr, 0));

    UbseSsuReconcileRespMsg reconcileResp;
    EXPECT_NE(UBSE_OK, reconcileResp.Deserialize(nullptr, 0));

    // 残缺数据（长度不足）：数量字段读取失败
    const uint8_t truncated[] = {0x01, 0x02, 0x03};
    EXPECT_NE(UBSE_OK, reconcileReq.Deserialize(truncated, sizeof(truncated)));
    EXPECT_NE(UBSE_OK, summaryResp.Deserialize(truncated, sizeof(truncated)));
}
} // namespace ubse::ssu::message::ut
