/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_mem_api_convert.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_mem_api_convert.h"

namespace ubse::mem_controller::ut {
using namespace api::server;

void TestUbseMemApiConvert::SetUp()
{
    Test::SetUp();
}
void TestUbseMemApiConvert::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemApiConvert, UbseMemCreateReqUnpack)
{
    UbseIpcMessage buffer{};
    UbseMemFdBorrowReq memFdBorrowReq{};
    EXPECT_EQ(UbseMemCreateReqUnpack(buffer, memFdBorrowReq), IPC_ERROR_SERIALIZATION_FAILED);

    buffer.length = UBS_MEM_MAX_NAME_LENGTH + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uid_t) + sizeof(gid_t) +
        sizeof(pid_t) + sizeof(mode_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    EXPECT_EQ(UbseMemCreateReqUnpack(buffer, memFdBorrowReq), IPC_SUCCESS);
}

TEST_F(TestUbseMemApiConvert, UbseMemCreateWithLenderReqUnpack)
{
    UbseIpcMessage buffer{};
    UbseMemFdBorrowReq memFdBorrowReq{};
    EXPECT_EQ(UbseMemCreateWithLenderReqUnpack(buffer, memFdBorrowReq), IPC_ERROR_DESERIALIZATION_FAILED);
}

TEST_F(TestUbseMemApiConvert, UbseMemFdCreateResponsePack)
{
    UbseMemOperationResp memOperationResp{};
    UbseIpcMessage buffer{};

    memOperationResp.name = std::string(UBS_MEM_MAX_NAME_LENGTH, 'a');
    EXPECT_EQ(UbseMemFdCreateResponsePack(memOperationResp, buffer), IPC_ERROR_SERIALIZATION_FAILED);

    memOperationResp.name = std::string(UBS_MEM_MAX_NAME_LENGTH - 1, 'a');
    for (int i = 0; i < UBS_MEM_MAX_MEMID_NUM + 1; i++) {
        memOperationResp.memIdList.emplace_back(1);
    }
    EXPECT_EQ(UbseMemFdCreateResponsePack(memOperationResp, buffer), IPC_ERROR_SERIALIZATION_FAILED);

    memOperationResp.memIdList.erase(memOperationResp.memIdList.begin() + 1);
    EXPECT_EQ(UbseMemFdCreateResponsePack(memOperationResp, buffer), IPC_SUCCESS);
}

TEST_F(TestUbseMemApiConvert, UbseMemFdDeleteReqUnpack)
{
    UbseIpcMessage buffer{};
    UbseMemReturnReq req{};
    EXPECT_EQ(UbseMemFdDeleteReqUnpack(buffer, req), IPC_ERROR_DESERIALIZATION_FAILED);

    buffer.length = UBS_MEM_MAX_NAME_LENGTH;
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    EXPECT_EQ(UbseMemFdDeleteReqUnpack(buffer, req), IPC_SUCCESS);
}

TEST_F(TestUbseMemApiConvert, UbseMemNumaCreateReqUnpack)
{
    UbseIpcMessage buffer{};
    UbseMemNumaBorrowReq req{};
    EXPECT_EQ(UbseMemNumaCreateReqUnpack(buffer, req), IPC_ERROR_SERIALIZATION_FAILED);

    buffer.length = UBS_MEM_MAX_NAME_LENGTH + sizeof(uint64_t) + sizeof(uint32_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    EXPECT_EQ(UbseMemNumaCreateReqUnpack(buffer, req), IPC_SUCCESS);
}

TEST_F(TestUbseMemApiConvert, UbseMemNumaCreateLenderReqUnpack)
{
    UbseIpcMessage buffer{};
    UbseMemNumaBorrowReq req{};
    EXPECT_EQ(UbseMemNumaCreateLenderReqUnpack(buffer, req), IPC_ERROR_SERIALIZATION_FAILED);

    buffer.length = UBS_MEM_MAX_NAME_LENGTH + UBSE_MEM_LENDER_SIZE;
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    EXPECT_EQ(UbseMemNumaCreateLenderReqUnpack(buffer, req), IPC_SUCCESS);
}

TEST_F(TestUbseMemApiConvert, UbseMemNumaDeleteUnPack)
{
    UbseIpcMessage buffer{};
    UbseMemReturnReq req{};
    EXPECT_EQ(UbseMemNumaDeleteUnPack(buffer, req), IPC_ERROR_SERIALIZATION_FAILED);

    buffer.length = UBS_MEM_MAX_NAME_LENGTH;
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    EXPECT_EQ(UbseMemNumaDeleteUnPack(buffer, req), IPC_SUCCESS);
}

TEST_F(TestUbseMemApiConvert, UbseMemNumaCreateResponsePack)
{
    UbseMemOperationResp memOperationResp{};
    UbseIpcMessage buffer{};

    memOperationResp.name = std::string(UBS_MEM_MAX_NAME_LENGTH, 'a');
    EXPECT_EQ(UbseMemNumaCreateResponsePack(memOperationResp, buffer), IPC_ERROR_SERIALIZATION_FAILED);

    memOperationResp.name = std::string(UBS_MEM_MAX_NAME_LENGTH - 1, 'a');
    for (int i = 0; i < UBS_MEM_MAX_MEMID_NUM + 1; i++) {
        memOperationResp.memIdList.emplace_back(1);
    }
    EXPECT_EQ(UbseMemNumaCreateResponsePack(memOperationResp, buffer), IPC_SUCCESS);
}

TEST_F(TestUbseMemApiConvert, UbseMemFdDescPack)
{
    mem::def::UbseMemFdDesc fdDesc{};
    uint8_t buffer1{};
    fdDesc.name = std::string(UBS_MEM_MAX_NAME_LENGTH, 'a');
    EXPECT_EQ(UbseMemFdDescPack(fdDesc, &buffer1), IPC_ERROR_SERIALIZATION_FAILED);

    uint8_t buffer2{};
    fdDesc.name = "name";
    EXPECT_EQ(UbseMemFdDescPack(fdDesc, &buffer2), IPC_SUCCESS);

    uint8_t buffer3{};
    for (int i = 0; i <= UBS_MEM_MAX_MEMID_NUM; i++) {
        fdDesc.memIds.emplace_back(1);
    }
    EXPECT_EQ(UbseMemFdDescPack(fdDesc, &buffer3), IPC_ERROR_SERIALIZATION_FAILED);
}

TEST_F(TestUbseMemApiConvert, UbseMemFdDescListPackedSize)
{
    std::vector<mem::def::UbseMemFdDesc> fdDescList{};
    size_t requestSize{};

    fdDescList.emplace_back(mem::def::UbseMemFdDesc{});
    for (int i = 0; i <= UBS_MEM_MAX_MEMID_NUM; i++) {
        fdDescList[0].memIds.emplace_back(1);
    }
    EXPECT_EQ(UbseMemFdDescListPackedSize(fdDescList, requestSize), IPC_ERROR_SERIALIZATION_FAILED);
    fdDescList[0].memIds.erase(fdDescList[0].memIds.begin() + 1);
    EXPECT_EQ(UbseMemFdDescListPackedSize(fdDescList, requestSize), IPC_SUCCESS);
}

TEST_F(TestUbseMemApiConvert, UbseMemFdDescListPack)
{
    std::vector<mem::def::UbseMemFdDesc> fdDescList{};
    fdDescList.emplace_back(mem::def::UbseMemFdDesc{});
    uint8_t buffer{};
    EXPECT_EQ(UbseMemFdDescListPack(fdDescList, &buffer), IPC_SUCCESS);

    fdDescList[0].name = std::string(UBS_MEM_MAX_NAME_LENGTH, 'a');
    EXPECT_EQ(UbseMemFdDescListPack(fdDescList, &buffer), IPC_ERROR_SERIALIZATION_FAILED);
}

TEST_F(TestUbseMemApiConvert, UbseMemNumaDescPack)
{
    mem::def::UbseMemNumaDesc numaDesc{};
    uint8_t buffer{};

    numaDesc.name = std::string(UBS_MEM_MAX_NAME_LENGTH, 'a');
    EXPECT_EQ(UbseMemNumaDescPack(numaDesc, &buffer), IPC_ERROR_SERIALIZATION_FAILED);

    numaDesc.name = "name";
    EXPECT_EQ(UbseMemNumaDescPack(numaDesc, &buffer), IPC_SUCCESS);
}

TEST_F(TestUbseMemApiConvert, UbseMemNumaDescListPack)
{
    std::vector<mem::def::UbseMemNumaDesc> numaDescList{};
    numaDescList.emplace_back(mem::def::UbseMemNumaDesc{});
    uint8_t buffer{};
    EXPECT_EQ(UbseMemNumaDescListPack(numaDescList, &buffer), IPC_SUCCESS);

    numaDescList[0].name = std::string(UBS_MEM_MAX_NAME_LENGTH, 'a');
    EXPECT_EQ(UbseMemNumaDescListPack(numaDescList, &buffer), IPC_ERROR_SERIALIZATION_FAILED);
}

TEST_F(TestUbseMemApiConvert, UbseNumaInfoListPack)
{
    std::vector<nodeController::UbseNumaInfo> numaInfoList{};
    uint8_t buffer{};
    nodeController::UbseNumaInfo numaInfo{};
    numaInfo.location.nodeId = "1";
    numaInfo.socketId = 1;
    numaInfo.location.numaId = 1;
    numaInfoList.emplace_back(numaInfo);

    EXPECT_EQ(UbseNumaInfoListPack(numaInfoList, &buffer), IPC_SUCCESS);
}
}