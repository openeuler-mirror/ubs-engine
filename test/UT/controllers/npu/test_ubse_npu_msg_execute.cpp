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

#include "test_ubse_npu_msg_execute.h"
#include "ubse_error.h"
#include "ubse_npu_resource_collection.h"

// Forward declarations for internal functions defined in ubse_npu_msg_execute.cpp
// (not exposed in public headers, but needed for mocking)
namespace ubse::npu::controller {
uint32_t QueryDeviceRespPack(const std::vector<std::shared_ptr<IResource>>& devList, TransRespMsg& buffer);
uint32_t UbseAllocRequestUnpack(const TransReqMsg& buffer, UbseAllocRequest& requestInfo, bool isAlloc);
uint32_t AllocDevResponsePack(const std::array<uint8_t, UBSE_UB_DEVICE_GUID_SIZE>& newBusInstanceGuid,
                              const std::vector<std::shared_ptr<IResource>>& devList, TransRespMsg& buffer);
uint32_t UbseQueryTidUbaRequestUnpack(const TransReqMsg& buffer, std::string& requestInfo);
uint32_t QueryTidUbaResponsePack(uint32_t& tid, uint64_t& uba, uint64_t& size, TransRespMsg& buffer);
} // namespace ubse::npu::controller

using namespace ubse::npu::controller;
using namespace ubse::common::def;

namespace ubse::npu::controller::ut {

void TestUbseNpuMsgExecute::SetUp()
{
    Test::SetUp();
}

void TestUbseNpuMsgExecute::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNpuMsgExecute, QueryDeviceExecute_QueryAllDevicesImplFailed)
{
    TransReqMsg req{nullptr, 0};
    TransRespMsg resp{nullptr, 0};

    // Mock QueryAllDevicesImpl to fail
    std::vector<std::shared_ptr<IResource>> devList;
    MOCKER(QueryAllDevicesImpl).stubs().with(outBound(devList)).will(returnValue(UBSE_ERROR));

    EXPECT_EQ(QueryDeviceExecute(req, resp), UBSE_ERROR);
}

TEST_F(TestUbseNpuMsgExecute, QueryDeviceExecute_Success)
{
    TransReqMsg req{nullptr, 0};
    TransRespMsg resp{nullptr, 0};

    // Mock QueryAllDevicesImpl to succeed
    std::vector<std::shared_ptr<IResource>> devList;
    MOCKER(QueryAllDevicesImpl).stubs().with(outBound(devList)).will(returnValue(UBSE_OK));
    // Mock QueryDeviceRespPack to succeed (pack logic)
    MOCKER(QueryDeviceRespPack).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(QueryDeviceExecute(req, resp), UBSE_OK);
}

TEST_F(TestUbseNpuMsgExecute, AllocDeviceExecute_AllocDevicesImplFailed)
{
    TransReqMsg req{nullptr, 0};
    TransRespMsg resp{nullptr, 0};

    // Mock UbseAllocRequestUnpack to succeed
    MOCKER(UbseAllocRequestUnpack).stubs().will(returnValue(UBSE_OK));
    // Mock AllocDevicesImpl to fail
    std::string newBusInstanceGuid;
    std::vector<std::shared_ptr<IResource>> devList;
    MOCKER(AllocDevicesImpl)
        .stubs()
        .with(any(), outBound(newBusInstanceGuid), outBound(devList))
        .will(returnValue(UBSE_ERROR));

    EXPECT_EQ(AllocDeviceExecute(req, resp), UBSE_ERROR);
}

TEST_F(TestUbseNpuMsgExecute, AllocDeviceExecute_Success)
{
    TransReqMsg req{nullptr, 0};
    TransRespMsg resp{nullptr, 0};

    // Mock UbseAllocRequestUnpack to succeed
    MOCKER(UbseAllocRequestUnpack).stubs().will(returnValue(UBSE_OK));
    // Mock AllocDevicesImpl to succeed
    std::string newBusInstanceGuid;
    std::vector<std::shared_ptr<IResource>> devList;
    MOCKER(AllocDevicesImpl)
        .stubs()
        .with(any(), outBound(newBusInstanceGuid), outBound(devList))
        .will(returnValue(UBSE_OK));
    // Mock AllocDevResponsePack to succeed (pack logic)
    MOCKER(AllocDevResponsePack).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(AllocDeviceExecute(req, resp), UBSE_OK);
}

TEST_F(TestUbseNpuMsgExecute, FreeDeviceExecute_FreeUbDevicesImplFailed)
{
    TransReqMsg req{nullptr, 0};
    TransRespMsg resp{nullptr, 0};

    // Mock UbseAllocRequestUnpack to succeed
    MOCKER(UbseAllocRequestUnpack).stubs().will(returnValue(UBSE_OK));
    // Mock FreeUbDevicesImpl to fail
    MOCKER(FreeUbDevicesImpl).stubs().with(any()).will(returnValue(UBSE_ERROR));

    EXPECT_EQ(FreeDeviceExecute(req, resp), UBSE_ERROR);
}

TEST_F(TestUbseNpuMsgExecute, FreeDeviceExecute_Success)
{
    TransReqMsg req{nullptr, 0};
    TransRespMsg resp{nullptr, 0};

    // Mock UbseAllocRequestUnpack to succeed
    MOCKER(UbseAllocRequestUnpack).stubs().will(returnValue(UBSE_OK));
    // Mock FreeUbDevicesImpl to succeed
    MOCKER(FreeUbDevicesImpl).stubs().with(any()).will(returnValue(UBSE_OK));

    EXPECT_EQ(FreeDeviceExecute(req, resp), UBSE_OK);
}

TEST_F(TestUbseNpuMsgExecute, QueryTidUbaSizeExecute_QueryUbaTidSizeImplFailed)
{
    TransReqMsg req{nullptr, 0};
    TransRespMsg resp{nullptr, 0};

    // Mock UbseQueryTidUbaRequestUnpack to succeed
    MOCKER(UbseQueryTidUbaRequestUnpack).stubs().will(returnValue(UBSE_OK));
    // Mock QueryUbaTidSizeImpl to fail
    UbaTidSize info;
    MOCKER(QueryUbaTidSizeImpl).stubs().with(any(), outBound(info)).will(returnValue(UBSE_ERROR));

    EXPECT_EQ(QueryTidUbaSizeExecute(req, resp), UBSE_ERROR);
}

TEST_F(TestUbseNpuMsgExecute, QueryTidUbaSizeExecute_Success)
{
    TransReqMsg req{nullptr, 0};
    TransRespMsg resp{nullptr, 0};

    // Mock UbseQueryTidUbaRequestUnpack to succeed
    MOCKER(UbseQueryTidUbaRequestUnpack).stubs().will(returnValue(UBSE_OK));
    // Mock QueryUbaTidSizeImpl to succeed
    UbaTidSize info;
    MOCKER(QueryUbaTidSizeImpl).stubs().with(any(), outBound(info)).will(returnValue(UBSE_OK));
    // Mock QueryTidUbaResponsePack to succeed (pack logic)
    MOCKER(QueryTidUbaResponsePack).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(QueryTidUbaSizeExecute(req, resp), UBSE_OK);
}

} // namespace ubse::npu::controller::ut