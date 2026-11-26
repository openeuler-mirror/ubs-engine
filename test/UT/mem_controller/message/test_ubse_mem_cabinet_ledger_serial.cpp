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

#include "test_ubse_mem_cabinet_ledger_serial.h"

#include "ubse_error.h"
#include "message/ubse_mem_cabinet_ledger_serial.h"

namespace ubse::mem::serial::ut {
using namespace ubse::serial;
using namespace ubse::mem::controller;
using ubse::adapter_plugins::mmi::NodeMemDebtInfo;
using ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo;
using ubse::adapter_plugins::mmi::UbseMemFdBorrowExportObj;
using ubse::common::def::UbseResult;

namespace {
// 带 fd 导出条目的账本，用于覆盖 NodeMemDebtInfo 嵌套序列化
NodeMemDebtInfo MakeDebtInfo(const std::string& peer, uint64_t size)
{
    NodeMemDebtInfo info{};
    UbseMemFdBorrowExportObj obj{};
    UbseMemDebtNumaInfo numa{};
    numa.nodeId = peer;
    numa.size = size;
    obj.algoResult.exportNumaInfos.push_back(numa);
    info.fdExportObjMap.emplace("res1", obj);
    return info;
}
} // namespace

void TestUbseMemCabinetLedgerSerial::SetUp()
{
    Test::SetUp();
}
void TestUbseMemCabinetLedgerSerial::TearDown()
{
    Test::TearDown();
}

TEST_F(TestUbseMemCabinetLedgerSerial, QueryReqRoundTrip)
{
    UbseCabinetLedgerQueryReq data{};
    data.requesterMasterNodeId = "node-11";
    data.requesterGroupId = 7;
    UbseSerialization out;
    UbseCabinetLedgerQueryReqSerialization(out, data);
    UbseDeSerialization in(out.GetBuffer(false), out.GetLength());
    UbseCabinetLedgerQueryReq decoded{};
    EXPECT_TRUE(UbseCabinetLedgerQueryReqDeserialization(in, decoded));
    EXPECT_EQ(data.requesterMasterNodeId, decoded.requesterMasterNodeId);
    EXPECT_EQ(data.requesterGroupId, decoded.requesterGroupId);
}

TEST_F(TestUbseMemCabinetLedgerSerial, QueryRespRoundTrip)
{
    UbseCabinetLedgerQueryResp data{};
    data.ret = UBSE_OK;
    data.cabinetMasterNodeId = "node-5";
    UbseCabinetLedgerItem okItem{};
    okItem.nodeId = "node-6";
    okItem.status = UbseCollectStatus::SUCCESS;
    okItem.debtInfo = MakeDebtInfo("node-6", 4096);
    UbseCabinetLedgerItem skipItem{};
    skipItem.nodeId = "node-7";
    skipItem.status = UbseCollectStatus::SKIPPED;
    data.items = {okItem, skipItem};
    UbseSerialization out;
    UbseCabinetLedgerQueryRespSerialization(out, data);
    UbseDeSerialization in(out.GetBuffer(false), out.GetLength());
    UbseCabinetLedgerQueryResp decoded{};
    EXPECT_TRUE(UbseCabinetLedgerQueryRespDeserialization(in, decoded));
    EXPECT_EQ(UBSE_OK, decoded.ret);
    EXPECT_EQ(data.cabinetMasterNodeId, decoded.cabinetMasterNodeId);
    ASSERT_EQ(2U, decoded.items.size());
    EXPECT_EQ("node-6", decoded.items[0].nodeId);
    EXPECT_EQ(UbseCollectStatus::SUCCESS, decoded.items[0].status);
    EXPECT_EQ(1U, decoded.items[0].debtInfo.fdExportObjMap.size());
    EXPECT_EQ(4096U, decoded.items[0].debtInfo.fdExportObjMap["res1"].algoResult.exportNumaInfos[0].size);
    EXPECT_EQ(UbseCollectStatus::SKIPPED, decoded.items[1].status);
}

TEST_F(TestUbseMemCabinetLedgerSerial, QueryRespRejectOversizeItems)
{
    // 报文伪造 items 长度越上限：反序列化必须失败，禁止无界分配
    UbseSerialization out;
    UbseResult ret{0};
    std::string master = "node-5";
    out << ret << master << array_len_insert(static_cast<uint64_t>(4096 + 1));
    UbseDeSerialization in(out.GetBuffer(false), out.GetLength());
    UbseCabinetLedgerQueryResp decoded{};
    EXPECT_FALSE(UbseCabinetLedgerQueryRespDeserialization(in, decoded));
}

TEST_F(TestUbseMemCabinetLedgerSerial, InvalidateForwardReqRoundTrip)
{
    UbseCabinetInvalidateForwardReq data{};
    data.requesterMasterNodeId = "node-11";
    data.items = {
        {"node-3", UbseMemBorrowType::FD_BORROW, "fd-res-1"},
        {"node-4", UbseMemBorrowType::NUMA_BORROW, "numa-res-1"},
        {"node-5", UbseMemBorrowType::ADDR_BORROW, "addr-res-1"},
    };
    UbseSerialization out;
    UbseCabinetInvalidateForwardReqSerialization(out, data);
    UbseDeSerialization in(out.GetBuffer(false), out.GetLength());
    UbseCabinetInvalidateForwardReq decoded{};
    EXPECT_TRUE(UbseCabinetInvalidateForwardReqDeserialization(in, decoded));
    ASSERT_EQ(3U, decoded.items.size());
    EXPECT_EQ(data.items[1].nodeId, decoded.items[1].nodeId);
    EXPECT_EQ(UbseMemBorrowType::NUMA_BORROW, decoded.items[1].borrowType);
    EXPECT_EQ(data.items[2].debtName, decoded.items[2].debtName);
}

TEST_F(TestUbseMemCabinetLedgerSerial, InvalidateForwardReqRejectOversizeItems)
{
    UbseSerialization out;
    std::string master = "node-11";
    out << master << array_len_insert(static_cast<uint64_t>(4096 + 1));
    UbseDeSerialization in(out.GetBuffer(false), out.GetLength());
    UbseCabinetInvalidateForwardReq decoded{};
    EXPECT_FALSE(UbseCabinetInvalidateForwardReqDeserialization(in, decoded));
}

TEST_F(TestUbseMemCabinetLedgerSerial, InvalidateForwardRespRoundTrip)
{
    UbseCabinetInvalidateForwardResp data{};
    data.ret = UBSE_ERROR;
    data.failCount = 2;
    UbseSerialization out;
    UbseCabinetInvalidateForwardRespSerialization(out, data);
    UbseDeSerialization in(out.GetBuffer(false), out.GetLength());
    UbseCabinetInvalidateForwardResp decoded{};
    EXPECT_TRUE(UbseCabinetInvalidateForwardRespDeserialization(in, decoded));
    EXPECT_EQ(UBSE_ERROR, decoded.ret);
    EXPECT_EQ(2U, decoded.failCount);
}

TEST_F(TestUbseMemCabinetLedgerSerial, RespBufferOk)
{
    UbseSerialization out;
    UbseCabinetInvalidateForwardResp data{};
    data.ret = UBSE_OK;
    data.failCount = 3;
    UbseCabinetInvalidateForwardRespSerialization(out, data);
    UbseByteBuffer resp{};
    ASSERT_EQ(UBSE_OK, UbseCabinetRespBuffer(out, resp));
    ASSERT_NE(nullptr, resp.data);
    EXPECT_GT(resp.len, 0U);
    UbseDeSerialization in(resp.data, resp.len);
    UbseCabinetInvalidateForwardResp decoded{};
    EXPECT_TRUE(UbseCabinetInvalidateForwardRespDeserialization(in, decoded));
    EXPECT_EQ(3U, decoded.failCount);
}

} // namespace ubse::mem::serial::ut
