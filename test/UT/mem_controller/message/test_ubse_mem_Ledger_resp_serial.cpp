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

#include "test_ubse_mem_Ledger_resp_serial.h"

#include <memory>
#include "ubse_error.h"
#include "ubse_serial_util.h"
#include "message/ubse_mem_controller_serial.h"

namespace ubse::mem::controller::message::ut {
using namespace ubse::mem::serial;
using namespace ubse::adapter_plugins::mmi;

void TestUbseMemLedgerRespSerial::SetUp()
{
    Test::SetUp();
}

void TestUbseMemLedgerRespSerial::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemLedgerRespSerial, Serialize_Success)
{
    LedgerResp resp;
    resp.ret = 42;
    obj.SetLedgerResp(std::move(resp));
    EXPECT_EQ(obj.Serialize(), UBSE_OK);
}

TEST_F(TestUbseMemLedgerRespSerial, Serialize_CheckFail)
{
    LedgerResp resp;
    resp.ret = 0;
    obj.SetLedgerResp(std::move(resp));
    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(false));
    EXPECT_EQ(obj.Serialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemLedgerRespSerial, Deserialize_NullInput)
{
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemLedgerRespSerial, Deserialize_CheckFail)
{
    LedgerResp resp;
    resp.ret = 0;
    obj.SetLedgerResp(std::move(resp));
    EXPECT_EQ(obj.Serialize(), UBSE_OK);

    auto sharedData = obj.GetSharedOutputData();
    auto outputSize = obj.SerializedDataSize();

    UbseMemLedgerRespSerial obj2;
    obj2.SetInputRawDataFromShared(sharedData, outputSize);

    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false));
    EXPECT_EQ(obj2.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemLedgerRespSerial, SerializeDeserialize_RoundTrip)
{
    LedgerResp resp;
    resp.ret = 123;
    obj.SetLedgerResp(std::move(resp));
    EXPECT_EQ(obj.Serialize(), UBSE_OK);

    auto sharedData = obj.GetSharedOutputData();
    auto outputSize = obj.SerializedDataSize();
    ASSERT_GT(outputSize, 0);
    ASSERT_NE(sharedData, nullptr);

    UbseMemLedgerRespSerial obj2;
    obj2.SetInputRawDataFromShared(sharedData, outputSize);

    EXPECT_EQ(obj2.Deserialize(), UBSE_OK);
    auto resp2 = obj2.GetLedgerResp();
    EXPECT_EQ(resp2.ret, 123);
}

TEST_F(TestUbseMemLedgerRespSerial, SerializeDeserialize_WithData)
{
    NodeMemDebtInfoMap debtInfoMap;
    NodeMemDebtInfo nodeInfo;

    UbseMemFdBorrowExportObj fdExportObj;
    fdExportObj.req.name = "test_fd";
    fdExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    nodeInfo.fdExportObjMap["test_fd"] = fdExportObj;

    UbseMemNumaBorrowImportObj numaImportObj;
    numaImportObj.req.name = "test_numa";
    numaImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    nodeInfo.numaImportObjMap["test_numa"] = numaImportObj;

    debtInfoMap["node1"] = nodeInfo;

    LedgerResp resp;
    resp.ret = 99;
    resp.debtInfoMap = std::move(debtInfoMap);
    obj.SetLedgerResp(std::move(resp));
    EXPECT_EQ(obj.Serialize(), UBSE_OK);

    auto sharedData = obj.GetSharedOutputData();
    auto outputSize = obj.SerializedDataSize();
    ASSERT_GT(outputSize, 0);
    ASSERT_NE(sharedData, nullptr);

    UbseMemLedgerRespSerial obj2;
    obj2.SetInputRawDataFromShared(sharedData, outputSize);

    EXPECT_EQ(obj2.Deserialize(), UBSE_OK);
    auto resp2 = obj2.GetLedgerResp();
    EXPECT_EQ(resp2.ret, 99);
    ASSERT_EQ(resp2.debtInfoMap.size(), 1);
    EXPECT_EQ(resp2.debtInfoMap["node1"].fdExportObjMap.size(), 1);
    EXPECT_EQ(resp2.debtInfoMap["node1"].numaImportObjMap.size(), 1);
}
} // namespace ubse::mem::controller::message::ut
