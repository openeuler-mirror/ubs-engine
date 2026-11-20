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

#include "test_ubse_mem_fd_borrow_req_simpo.h"

#include "ubse_error.h"
#include "message/ubse_mem_borrow_simpo.h"

namespace ubse::mem::controller::message::ut {
namespace {
template <class Request, class Owner, class Item>
void CheckVectorDecodeLimit(std::vector<Item> Owner::*member, size_t limit)
{
    for (size_t size : {limit, limit + 1}) {
        Request request{};
        (request.*member).resize(size);
        ubse::simpo::util::UbseAutoSimpo<Request> encoded;
        encoded.SetUbseMesgInfo(request);
        ASSERT_EQ(UBSE_OK, encoded.Serialize());

        ubse::simpo::util::UbseAutoSimpo<Request> decoded;
        ASSERT_EQ(UBSE_OK, decoded.SetInputRawData(encoded.SerializedData(), encoded.SerializedDataSize()));
        ASSERT_EQ(size == limit ? UBSE_OK : UBSE_ERROR, decoded.Deserialize()) << "size=" << size;
        EXPECT_EQ(size == limit ? limit : 0, (decoded.GetUbseMesgInfo().*member).size());
    }
}
} // namespace

void TestUbseMemFdBorrowReqSimpo::SetUp()
{
    Test::SetUp();
    obj = new ubse::simpo::util::UbseAutoSimpo<UbseMemFdBorrowReq>();
}

void TestUbseMemFdBorrowReqSimpo::TearDown()
{
    Test::TearDown();
}

TEST_F(TestUbseMemFdBorrowReqSimpo, SerializeDeserialize)
{
    UbseMemFdBorrowReq request{};
    request.name = "fd-request";
    request.requestId = 42;
    request.importNodeId = "node-b";
    request.lenderSizes = {1024};
    request.candidateNodeList = {"node-a"};
    obj->SetUbseMesgInfo(request);
    obj->SetErrCode(17);

    ASSERT_EQ(UBSE_OK, obj->Serialize());
    ubse::simpo::util::UbseAutoSimpo<UbseMemFdBorrowReq> decoded;
    ASSERT_EQ(UBSE_OK, decoded.SetInputRawData(obj->SerializedData(), obj->SerializedDataSize()));
    ASSERT_EQ(UBSE_OK, decoded.Deserialize());
    auto result = decoded.GetUbseMesgInfo();
    EXPECT_EQ(request.name, result.name);
    EXPECT_EQ(request.requestId, result.requestId);
    EXPECT_EQ(request.importNodeId, result.importNodeId);
    EXPECT_EQ(request.lenderSizes, result.lenderSizes);
    EXPECT_EQ(request.candidateNodeList, result.candidateNodeList);
    EXPECT_EQ(17U, decoded.GetErrCode());
}

TEST_F(TestUbseMemFdBorrowReqSimpo, LenderLocsDecodeLimit)
{
    CheckVectorDecodeLimit<UbseMemFdBorrowReq>(&UbseMemFdBorrowReq::lenderLocs, 1024);
}

TEST_F(TestUbseMemFdBorrowReqSimpo, LenderSizesDecodeLimit)
{
    CheckVectorDecodeLimit<UbseMemFdBorrowReq>(&UbseMemFdBorrowReq::lenderSizes, 1024);
}

TEST_F(TestUbseMemFdBorrowReqSimpo, CandidateNodeListDecodeLimit)
{
    CheckVectorDecodeLimit<UbseMemFdBorrowReq>(&UbseMemFdBorrowReq::candidateNodeList, 128);
}

TEST_F(TestUbseMemFdBorrowReqSimpo, TrustRingSignedDatasDecodeLimit)
{
    using ubse::adapter_plugins::mmi::UbseTrustRingData;
    CheckVectorDecodeLimit<UbseTrustRingData>(&UbseTrustRingData::lendSignedDatas, 1024);
}
} // namespace ubse::mem::controller::message::ut
