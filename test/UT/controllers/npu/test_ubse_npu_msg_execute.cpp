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
#include <array>
#include <vector>
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
// HEAD_SIZE defined in ubse_npu_msg_execute.cpp: 6 * sizeof(uint8_t)
constexpr size_t HEAD_SIZE = 6 * sizeof(uint8_t);
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

TEST_F(TestUbseNpuMsgExecute, QueryDeviceExecute_RespPackFailed)
{
    TransReqMsg req{nullptr, 0};
    TransRespMsg resp{nullptr, 0};

    // Mock QueryAllDevicesImpl to succeed
    std::vector<std::shared_ptr<IResource>> devList;
    MOCKER(QueryAllDevicesImpl).stubs().with(outBound(devList)).will(returnValue(UBSE_OK));
    // Mock QueryDeviceRespPack to fail
    MOCKER(QueryDeviceRespPack).stubs().will(returnValue(UBSE_ERROR_SERIALIZE_FAILED));

    EXPECT_EQ(QueryDeviceExecute(req, resp), UBSE_ERROR_SERIALIZE_FAILED);
}

TEST_F(TestUbseNpuMsgExecute, AllocDeviceExecute_UnpackFailed)
{
    TransReqMsg req{nullptr, 0};
    TransRespMsg resp{nullptr, 0};

    // Mock UbseAllocRequestUnpack to fail
    MOCKER(UbseAllocRequestUnpack).stubs().will(returnValue(UBSE_ERROR_DESERIALIZE_FAILED));

    EXPECT_EQ(AllocDeviceExecute(req, resp), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseNpuMsgExecute, AllocDeviceExecute_RespPackFailed)
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
    // Mock AllocDevResponsePack to fail
    MOCKER(AllocDevResponsePack).stubs().will(returnValue(UBSE_ERROR_SERIALIZE_FAILED));

    EXPECT_EQ(AllocDeviceExecute(req, resp), UBSE_ERROR_SERIALIZE_FAILED);
}

TEST_F(TestUbseNpuMsgExecute, FreeDeviceExecute_UnpackFailed)
{
    TransReqMsg req{nullptr, 0};
    TransRespMsg resp{nullptr, 0};

    // Mock UbseAllocRequestUnpack to fail
    MOCKER(UbseAllocRequestUnpack).stubs().will(returnValue(UBSE_ERROR_DESERIALIZE_FAILED));

    EXPECT_EQ(FreeDeviceExecute(req, resp), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseNpuMsgExecute, QueryTidUbaSizeExecute_UnpackFailed)
{
    TransReqMsg req{nullptr, 0};
    TransRespMsg resp{nullptr, 0};

    // Mock UbseQueryTidUbaRequestUnpack to fail
    MOCKER(UbseQueryTidUbaRequestUnpack).stubs().will(returnValue(UBSE_ERROR_DESERIALIZE_FAILED));

    EXPECT_EQ(QueryTidUbaSizeExecute(req, resp), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseNpuMsgExecute, QueryTidUbaSizeExecute_RespPackFailed)
{
    TransReqMsg req{nullptr, 0};
    TransRespMsg resp{nullptr, 0};

    // Mock UbseQueryTidUbaRequestUnpack to succeed
    MOCKER(UbseQueryTidUbaRequestUnpack).stubs().will(returnValue(UBSE_OK));
    // Mock QueryUbaTidSizeImpl to succeed
    UbaTidSize info;
    MOCKER(QueryUbaTidSizeImpl).stubs().with(any(), outBound(info)).will(returnValue(UBSE_OK));
    // Mock QueryTidUbaResponsePack to fail
    MOCKER(QueryTidUbaResponsePack).stubs().will(returnValue(UBSE_ERROR_SERIALIZE_FAILED));

    EXPECT_EQ(QueryTidUbaSizeExecute(req, resp), UBSE_ERROR_SERIALIZE_FAILED);
}

// ===== Tests for internal functions (real implementations, no mocking) =====

TEST_F(TestUbseNpuMsgExecute, QueryDeviceRespPack_EmptyDevList_Success)
{
    std::vector<std::shared_ptr<IResource>> devList;
    TransRespMsg resp{nullptr, 0};

    EXPECT_EQ(QueryDeviceRespPack(devList, resp), UBSE_OK);
    EXPECT_NE(resp.buffer, nullptr);
    EXPECT_EQ(resp.length, HEAD_SIZE);
    delete[] resp.buffer;
}

TEST_F(TestUbseNpuMsgExecute, UbseAllocRequestUnpack_NullBuffer_Failed)
{
    TransReqMsg req{nullptr, 0};
    UbseAllocRequest requestInfo;

    EXPECT_EQ(UbseAllocRequestUnpack(req, requestInfo, true), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseNpuMsgExecute, UbseQueryTidUbaRequestUnpack_NullBuffer_Failed)
{
    TransReqMsg req{nullptr, 0};
    std::string requestInfo;

    EXPECT_EQ(UbseQueryTidUbaRequestUnpack(req, requestInfo), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseNpuMsgExecute, UbseQueryTidUbaRequestUnpack_ValidBuffer_Success)
{
    // Build a valid buffer with UBSE_UB_DEVICE_GUID_SIZE bytes
    std::vector<uint8_t> buf(UBSE_UB_DEVICE_GUID_SIZE, 0x41); // 'A'
    TransReqMsg req{buf.data(), static_cast<uint32_t>(buf.size())};
    std::string requestInfo;

    EXPECT_EQ(UbseQueryTidUbaRequestUnpack(req, requestInfo), UBSE_OK);
    EXPECT_EQ(requestInfo.size(), UBSE_UB_DEVICE_GUID_SIZE);
}

TEST_F(TestUbseNpuMsgExecute, QueryTidUbaResponsePack_Success)
{
    uint32_t tid = 100;
    uint64_t uba = 0x1000;
    uint64_t size = 0x2000;
    TransRespMsg resp{nullptr, 0};

    EXPECT_EQ(QueryTidUbaResponsePack(tid, uba, size, resp), UBSE_OK);
    EXPECT_NE(resp.buffer, nullptr);
    EXPECT_EQ(resp.length, sizeof(tid) + sizeof(uba) + sizeof(size));
    delete[] resp.buffer;
}

TEST_F(TestUbseNpuMsgExecute, AllocDevResponsePack_EmptyDevList_Success)
{
    std::array<uint8_t, UBSE_UB_DEVICE_GUID_SIZE> newBusInstanceGuid{};
    std::vector<std::shared_ptr<IResource>> devList;
    TransRespMsg resp{nullptr, 0};

    EXPECT_EQ(AllocDevResponsePack(newBusInstanceGuid, devList, resp), UBSE_OK);
    EXPECT_NE(resp.buffer, nullptr);
    delete[] resp.buffer;
}

// ===== Tests with real resource objects to cover pack paths =====

TEST_F(TestUbseNpuMsgExecute, QueryDeviceRespPack_WithNpuDevice_Success)
{
    // Create a real NpuResource to exercise CountDevicesByType and PackDevList
    auto npu = std::make_shared<NpuResource>();
    npu->SetLoc(1, 2);
    npu->SetGuid(std::string(UBSE_UB_DEVICE_GUID_SIZE, 'A'));
    npu->SetBusInstanceGuid(std::string(UBSE_UB_DEVICE_GUID_SIZE, 'B'));
    std::vector<std::shared_ptr<IResource>> devList{npu};
    TransRespMsg resp{nullptr, 0};

    EXPECT_EQ(QueryDeviceRespPack(devList, resp), UBSE_OK);
    EXPECT_NE(resp.buffer, nullptr);
    EXPECT_GT(resp.length, HEAD_SIZE);
    delete[] resp.buffer;
}

TEST_F(TestUbseNpuMsgExecute, QueryDeviceRespPack_WithBusiDevice_Success)
{
    auto busi = std::make_shared<BusiResource>();
    busi->SetGuid(std::string(UBSE_UB_DEVICE_GUID_SIZE, 'C'));
    std::vector<std::shared_ptr<IResource>> devList{busi};
    TransRespMsg resp{nullptr, 0};

    EXPECT_EQ(QueryDeviceRespPack(devList, resp), UBSE_OK);
    EXPECT_NE(resp.buffer, nullptr);
    delete[] resp.buffer;
}

TEST_F(TestUbseNpuMsgExecute, QueryDeviceRespPack_WithNicPfeDevice_Success)
{
    auto pfe = std::make_shared<NicPfeResource>();
    pfe->SetLoc(1, 2, 3);
    pfe->SetGuid(std::string(UBSE_UB_DEVICE_GUID_SIZE, 'D'));
    pfe->SetBusInstanceGuid(std::string(UBSE_UB_DEVICE_GUID_SIZE, 'E'));
    std::vector<std::shared_ptr<IResource>> devList{pfe};
    TransRespMsg resp{nullptr, 0};

    EXPECT_EQ(QueryDeviceRespPack(devList, resp), UBSE_OK);
    EXPECT_NE(resp.buffer, nullptr);
    delete[] resp.buffer;
}

TEST_F(TestUbseNpuMsgExecute, QueryDeviceRespPack_WithNicVfeDevice_Success)
{
    auto vfe = std::make_shared<NicVfeResource>();
    vfe->SetLoc(1, 2, 3, 4);
    vfe->SetGuid(std::string(UBSE_UB_DEVICE_GUID_SIZE, 'F'));
    vfe->SetBusInstanceGuid(std::string(UBSE_UB_DEVICE_GUID_SIZE, 'G'));
    std::vector<std::shared_ptr<IResource>> devList{vfe};
    TransRespMsg resp{nullptr, 0};

    EXPECT_EQ(QueryDeviceRespPack(devList, resp), UBSE_OK);
    EXPECT_NE(resp.buffer, nullptr);
    delete[] resp.buffer;
}

TEST_F(TestUbseNpuMsgExecute, AllocDevResponsePack_WithNpuDevice_Success)
{
    auto npu = std::make_shared<NpuResource>();
    npu->SetLoc(1, 2);
    npu->SetGuid(std::string(UBSE_UB_DEVICE_GUID_SIZE, 'A'));
    npu->SetBusInstanceGuid(std::string(UBSE_UB_DEVICE_GUID_SIZE, 'B'));
    std::array<uint8_t, UBSE_UB_DEVICE_GUID_SIZE> newBusInstanceGuid{};
    std::vector<std::shared_ptr<IResource>> devList{npu};
    TransRespMsg resp{nullptr, 0};

    EXPECT_EQ(AllocDevResponsePack(newBusInstanceGuid, devList, resp), UBSE_OK);
    EXPECT_NE(resp.buffer, nullptr);
    delete[] resp.buffer;
}

TEST_F(TestUbseNpuMsgExecute, UbseAllocRequestUnpack_ValidBuffer_Success)
{
    // Build a valid buffer: UPI(4 bytes) + GUID(32 bytes) + devListSize(1 byte, value 0)
    std::vector<uint8_t> buf;
    // UPI str: 4 bytes of '0' to make ConvertStrToUint16 succeed (isAlloc=false skips conversion)
    for (size_t i = 0; i < UBSE_UB_UPI_STR_SIZE; i++) {
        buf.push_back(0x30); // '0'
    }
    // GUID: 32 bytes
    for (size_t i = 0; i < UBSE_UB_DEVICE_GUID_SIZE; i++) {
        buf.push_back(0x41); // 'A'
    }
    // devListSize = 0
    buf.push_back(0);

    TransReqMsg req{buf.data(), static_cast<uint32_t>(buf.size())};
    UbseAllocRequest requestInfo{};

    EXPECT_EQ(UbseAllocRequestUnpack(req, requestInfo, false), UBSE_OK);
    EXPECT_EQ(requestInfo.ubDevList.size(), 0);
}

TEST_F(TestUbseNpuMsgExecute, UbseAllocRequestUnpack_WithDevices_Success)
{
    // Build a valid buffer with one device in the list
    UbsePackUtil packUtil(nullptr, 0);
    // First compute required size: UPI(4) + GUID(32) + devListSize(1) + 1 device(1+1+1+1+2+2=8)
    std::vector<uint8_t> buf(UBSE_UB_UPI_STR_SIZE + UBSE_UB_DEVICE_GUID_SIZE + 1 + 8, 0);
    UbsePackUtil writer(buf.data(), buf.size());
    // UPI str
    for (size_t i = 0; i < UBSE_UB_UPI_STR_SIZE; i++) {
        writer.UbsePackUint8(0x30);
    }
    // GUID
    for (size_t i = 0; i < UBSE_UB_DEVICE_GUID_SIZE; i++) {
        writer.UbsePackUint8(0x41);
    }
    // devListSize = 1
    writer.UbsePackUint8(1);
    // device: type(uchar) + slotId + chipId + dieId + pfId(uint16) + vfId(uint16)
    writer.UbsePackUChar(static_cast<unsigned char>(ResourceType::NPU));
    writer.UbsePackUint8(1);
    writer.UbsePackUint8(2);
    writer.UbsePackUint8(3);
    writer.UbsePackUint16(4);
    writer.UbsePackUint16(5);

    TransReqMsg req{buf.data(), static_cast<uint32_t>(buf.size())};
    UbseAllocRequest requestInfo{};

    EXPECT_EQ(UbseAllocRequestUnpack(req, requestInfo, false), UBSE_OK);
    EXPECT_EQ(requestInfo.ubDevList.size(), 1);
    EXPECT_EQ(requestInfo.ubDevList[0].slotId, 1);
}

TEST_F(TestUbseNpuMsgExecute, UbseAllocRequestUnpack_InsufficientBuffer_Failed)
{
    // Buffer too small to hold UPI str
    std::vector<uint8_t> buf(2, 0x30);
    TransReqMsg req{buf.data(), static_cast<uint32_t>(buf.size())};
    UbseAllocRequest requestInfo{};

    EXPECT_EQ(UbseAllocRequestUnpack(req, requestInfo, false), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseNpuMsgExecute, UbseAllocRequestUnpack_IsAllocInvalidUpi_Failed)
{
    // Build buffer with invalid UPI (non-hex chars) for isAlloc=true path
    std::vector<uint8_t> buf;
    for (size_t i = 0; i < UBSE_UB_UPI_STR_SIZE; i++) {
        buf.push_back(0x7A); // 'z' - invalid hex
    }
    for (size_t i = 0; i < UBSE_UB_DEVICE_GUID_SIZE; i++) {
        buf.push_back(0x41);
    }
    buf.push_back(0);

    TransReqMsg req{buf.data(), static_cast<uint32_t>(buf.size())};
    UbseAllocRequest requestInfo{};

    EXPECT_EQ(UbseAllocRequestUnpack(req, requestInfo, true), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseNpuMsgExecute, UbseAllocRequestUnpack_IsAllocValidUpi_Success)
{
    // Build buffer with valid hex UPI "0010" for isAlloc=true path
    std::vector<uint8_t> buf;
    buf.push_back('0');
    buf.push_back('0');
    buf.push_back('1');
    buf.push_back('0');
    for (size_t i = 0; i < UBSE_UB_DEVICE_GUID_SIZE; i++) {
        buf.push_back(0x41);
    }
    buf.push_back(0);

    TransReqMsg req{buf.data(), static_cast<uint32_t>(buf.size())};
    UbseAllocRequest requestInfo{};

    EXPECT_EQ(UbseAllocRequestUnpack(req, requestInfo, true), UBSE_OK);
    EXPECT_EQ(requestInfo.upiStr, 0x0010);
}

} // namespace ubse::npu::controller::ut