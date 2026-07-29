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

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include <memory>
#include <string>

#include "message/ubse_mem_share_delete_import_ledger_req_simpo.h"
#include "ubse_error.h"

namespace ubse::mem::controller::message::ut {

class TestDeleteImportLedgerReqSimpo : public testing::Test {
public:
    void SetUp() override
    {
        Test::SetUp();
    }

    void TearDown() override
    {
        Test::TearDown();
        GlobalMockObject::verify();
    }
};

// SI-01: Serialize -> create new from raw data -> Deserialize -> roundtrip hit
TEST_F(TestDeleteImportLedgerReqSimpo, SerializeDeserialize_Roundtrip)
{
    UbseMemShareDeleteImportLedgerReqSimpo simpo1;
    simpo1.SetNodeId("node-5");
    EXPECT_EQ(simpo1.Serialize(), UBSE_OK);

    auto sharedData = simpo1.GetSharedOutputData();
    auto size = simpo1.SerializedDataSize();

    UbseMemShareDeleteImportLedgerReqSimpo simpo2;
    EXPECT_EQ(simpo2.SetInputRawDataFromShared(std::move(sharedData), size), UBSE_OK);
    EXPECT_EQ(simpo2.Deserialize(), UBSE_OK);
    EXPECT_EQ(simpo2.GetNodeId(), "node-5");
}

// SI-02: Serialize produces non-empty buffer
TEST_F(TestDeleteImportLedgerReqSimpo, Serialize_ProducesNonEmptyBuffer)
{
    UbseMemShareDeleteImportLedgerReqSimpo simpo;
    simpo.SetNodeId("node-1");
    EXPECT_EQ(simpo.Serialize(), UBSE_OK);
    EXPECT_GT(simpo.SerializedDataSize(), 0u);
    EXPECT_NE(simpo.SerializedData(), nullptr);
}

// SI-03: RawData constructor then Deserialize preserves nodeId
TEST_F(TestDeleteImportLedgerReqSimpo, RawDataConstructor_Deserializes)
{
    UbseMemShareDeleteImportLedgerReqSimpo simpo1;
    simpo1.SetNodeId("test-node");
    EXPECT_EQ(simpo1.Serialize(), UBSE_OK);

    auto sharedData = simpo1.GetSharedOutputData();
    auto size = simpo1.SerializedDataSize();

    UbseMemShareDeleteImportLedgerReqSimpo simpo2(sharedData.get(), size);
    EXPECT_EQ(simpo2.Deserialize(), UBSE_OK);
    EXPECT_EQ(simpo2.GetNodeId(), "test-node");
}

// SI-04: Empty nodeId roundtrip preserves empty string
TEST_F(TestDeleteImportLedgerReqSimpo, EmptyNodeId_Roundtrip)
{
    UbseMemShareDeleteImportLedgerReqSimpo simpo1;
    simpo1.SetNodeId("");
    EXPECT_EQ(simpo1.Serialize(), UBSE_OK);

    auto sharedData = simpo1.GetSharedOutputData();
    auto size = simpo1.SerializedDataSize();

    UbseMemShareDeleteImportLedgerReqSimpo simpo2;
    EXPECT_EQ(simpo2.SetInputRawDataFromShared(std::move(sharedData), size), UBSE_OK);
    EXPECT_EQ(simpo2.Deserialize(), UBSE_OK);
    EXPECT_EQ(simpo2.GetNodeId(), "");
}

// SI-05: Long nodeId (255 chars) roundtrip preserved
TEST_F(TestDeleteImportLedgerReqSimpo, LongNodeId_Roundtrip)
{
    std::string longId(255, 'x');

    UbseMemShareDeleteImportLedgerReqSimpo simpo1;
    simpo1.SetNodeId(longId);
    EXPECT_EQ(simpo1.Serialize(), UBSE_OK);

    auto sharedData = simpo1.GetSharedOutputData();
    auto size = simpo1.SerializedDataSize();

    UbseMemShareDeleteImportLedgerReqSimpo simpo2;
    EXPECT_EQ(simpo2.SetInputRawDataFromShared(std::move(sharedData), size), UBSE_OK);
    EXPECT_EQ(simpo2.Deserialize(), UBSE_OK);
    EXPECT_EQ(simpo2.GetNodeId(), longId);
}

} // namespace ubse::mem::controller::message::ut
