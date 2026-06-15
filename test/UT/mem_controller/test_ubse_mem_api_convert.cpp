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

#include "test_ubse_mem_api_convert.h"
#include <securec.h>
#include <ubse_node_api_convert.h>
#include <ubse_pointer_process.h>

#include <mockcpp/mockcpp.hpp>
#include "ubse_ipc_common.h"
#include "ubse_mem_account.h"
#include "ubse_mem_buffer_convert.h"
#include "ubse_mem_controller_def.h"
#include "ubse_mem_buffer_convert.cpp"

namespace ubse::mem_controller::ut {
using namespace api::server;
using namespace ubse::mem::controller;
using namespace ubse::mem::def;
using namespace ubse::adapter_plugins::mmi;

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
    ubse::adapter_plugins::mmi::UbseMemFdBorrowReq memFdBorrowReq{};
    EXPECT_EQ(UbseMemCreateReqUnpack(buffer, memFdBorrowReq), UBSE_ERROR_DESERIALIZE_FAILED);
    SafeDeleteArray(buffer.buffer);

    buffer.length = UBS_MEM_MAX_NAME_LENGTH + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uid_t) + sizeof(gid_t) +
                    sizeof(pid_t) + sizeof(mode_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    EXPECT_NE(UbseMemCreateReqUnpack(buffer, memFdBorrowReq), UBSE_OK);
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemCreateWithLenderReqUnpack)
{
    UbseIpcMessage buffer{};
    UbseMemFdBorrowReq memFdBorrowReq{};
    EXPECT_NE(UbseMemCreateWithLenderReqUnpack(buffer, memFdBorrowReq), UBSE_OK);
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemFdDeleteReqUnpack)
{
    UbseIpcMessage buffer{};
    UbseMemReturnReq req{};
    EXPECT_EQ(UbseMemFdDeleteReqUnpack(buffer, req), UBSE_ERROR_DESERIALIZE_FAILED);
    SafeDeleteArray(buffer.buffer);

    buffer.length = UBS_MEM_MAX_NAME_LENGTH;
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    EXPECT_NE(UbseMemFdDeleteReqUnpack(buffer, req), UBSE_OK);
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemNumaCreateReqUnpack)
{
    UbseIpcMessage buffer{};
    UbseMemNumaBorrowReq req{};
    EXPECT_EQ(UbseMemNumaCreateReqUnpack(buffer, req), UBSE_ERROR_DESERIALIZE_FAILED);
    SafeDeleteArray(buffer.buffer);

    buffer.length = UBS_MEM_MAX_NAME_LENGTH + sizeof(uint64_t) + sizeof(uint32_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    EXPECT_NE(UbseMemNumaCreateReqUnpack(buffer, req), UBSE_OK);
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemNumaCreateLenderReqUnpack)
{
    UbseIpcMessage buffer{};
    UbseMemNumaBorrowReq req{};
    EXPECT_EQ(UbseMemNumaCreateLenderReqUnpack(buffer, req), UBSE_ERROR_DESERIALIZE_FAILED);
    SafeDeleteArray(buffer.buffer);

    buffer.length = UBS_MEM_MAX_NAME_LENGTH + UBS_MEM_MAX_LENDER_CNT;
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    EXPECT_NE(UbseMemNumaCreateLenderReqUnpack(buffer, req), UBSE_OK);
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemNumaDeleteUnPack)
{
    UbseIpcMessage buffer{};
    UbseMemReturnReq req{};
    EXPECT_NE(UbseMemNumaDeleteUnpack(buffer, req), UBSE_OK);
    SafeDeleteArray(buffer.buffer);

    buffer.length = UBS_MEM_MAX_NAME_LENGTH;
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    EXPECT_NE(UbseMemNumaDeleteUnpack(buffer, req), UBSE_OK);
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemFdDescPack)
{
    mem::def::UbseMemFdDesc fdDesc{};
    UbseIpcMessage buffer1{};
    fdDesc.name = std::string(UBS_MEM_MAX_NAME_LENGTH, 'a');
    EXPECT_EQ(UbseMemFdDescPack(fdDesc, buffer1), UBSE_OK);
    SafeDeleteArray(buffer1.buffer);

    UbseIpcMessage buffer2{};
    fdDesc.name = "name";
    EXPECT_EQ(UbseMemFdDescPack(fdDesc, buffer2), UBSE_OK);
    SafeDeleteArray(buffer2.buffer);

    UbseIpcMessage buffer3{};
    for (int i = 0; i <= UBS_MEM_MAX_MEMID_NUM; i++) {
        fdDesc.memIds.emplace_back(1);
    }
    EXPECT_EQ(UbseMemFdDescPack(fdDesc, buffer3), UBSE_ERROR_SERIALIZE_FAILED);
    SafeDeleteArray(buffer3.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemFdDescListPack)
{
    std::vector<mem::def::UbseMemFdDesc> fdDescList{};
    fdDescList.emplace_back(mem::def::UbseMemFdDesc{});
    UbseIpcMessage buffer{};
    EXPECT_EQ(UbseMemFdDescListPack(fdDescList, buffer), UBSE_OK);
    SafeDeleteArray(buffer.buffer);

    fdDescList[0].name = std::string(UBS_MEM_MAX_NAME_LENGTH, 'a');
    EXPECT_EQ(UbseMemFdDescListPack(fdDescList, buffer), UBSE_OK);
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemNumaDescPack)
{
    mem::def::UbseMemNumaDesc numaDesc{};
    UbseIpcMessage buffer{};

    numaDesc.name = std::string(UBS_MEM_MAX_NAME_LENGTH, 'a');
    EXPECT_EQ(UbseMemNumaDescPack(numaDesc, buffer), UBSE_OK);
    SafeDeleteArray(buffer.buffer);

    numaDesc.name = "name";
    EXPECT_EQ(UbseMemNumaDescPack(numaDesc, buffer), UBSE_OK);
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemNumaDescListPack)
{
    std::vector<mem::def::UbseMemNumaDesc> numaDescList{};
    numaDescList.emplace_back(mem::def::UbseMemNumaDesc{});
    UbseIpcMessage buffer{};
    EXPECT_EQ(UbseMemNumaDescListPack(numaDescList, buffer), UBSE_OK);
    SafeDeleteArray(buffer.buffer);

    numaDescList[0].name = std::string(UBS_MEM_MAX_NAME_LENGTH, 'a');
    EXPECT_EQ(UbseMemNumaDescListPack(numaDescList, buffer), UBSE_OK);
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseNumaInfoListPack)
{
    std::vector<mem::account::UbseNumaNodeInfo> numaInfoList{};
    mem::account::UbseNumaNodeInfo numaInfo{};
    numaInfo.nodeId = "1";
    numaInfo.socketId = 1;
    numaInfo.numaId = 1;
    numaInfoList.emplace_back(numaInfo);
    UbseIpcMessage message{};
    EXPECT_EQ(node::api::UbseNumaInfoListPack(numaInfoList, message), UBSE_OK);
    SafeDeleteArray(message.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemShmMemFaultGetResponsePack)
{
    UbseMemShmMemStatusDesc statusDesc;
    UbseIpcMessage buffer{};
    statusDesc.memIds.emplace_back(0);
    statusDesc.memIds.emplace_back(1);
    statusDesc.faultTypes.emplace_back(UB_MEM_ATOMIC_DATA_ERR);
    statusDesc.faultTypes.emplace_back(UB_MEM_READ_DATA_ERR);
    EXPECT_EQ(UbseMemShmMemFaultGetResponsePack(statusDesc, buffer), UBSE_OK);
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemShmListResponsePack)
{
    std::vector<UbseMemShmDesc> shmDescs;
    UbseMemShmDesc shmDesc;
    shmDesc.name = "test";
    shmDesc.totalMemSize = 1024;
    shmDesc.unitSize = 128;
    UbseNode node;
    node.socketId[0] = 36;
    node.socketId[1] = 216;
    node.slotId = 1;
    node.hostName = "test";
    for (int i = 0; i < UBS_TOPO_NUMA_NUM; ++i) {
        node.numaIds[0][i] = i;
    }
    ubs_topo_ip_address_t ipAddress;
    ipAddress.af = 0;
    ipAddress.ipv4 = {.s_addr = 1};
    ipAddress.ipv6 = {};
    node.ips[0] = ipAddress;
    shmDesc.exportNode = node;
    UbseMemShmImportDesc importDesc;
    importDesc.memIds.push_back(1);
    importDesc.memIds.push_back(2);
    shmDesc.userInfo[0] = 1;
    shmDesc.importDesc.push_back(importDesc);
    shmDescs.emplace_back(shmDesc);
    UbseIpcMessage buffer{};
    EXPECT_EQ(UbseMemShmListResponsePack(shmDescs, buffer), UBSE_OK);
    SafeDeleteArray(buffer.buffer);
}
TEST_F(TestUbseMemApiConvert, UbseMemShmCreateReqUnpackSuccess)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    ubse::mem::def::UbseMemShmDispatcher memShmDispatcher{};

    // 模拟一个有效的 name
    buffer.length = MAX_MEM_RESOURCE_NAME_LENGTH + sizeof(uint64_t) +
                    ubse::mem::controller::UBSE_MAX_USR_INFO_LEN * sizeof(uint8_t) + sizeof(uint64_t) +
                    sizeof(uint32_t) + UBS_MEM_MAX_SLOT_NUM * sizeof(uint32_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];

    // 填充缓冲区数据
    uint8_t* ptr = buffer.buffer;
    const char* name = "test_shm";
    uint32_t nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    uint64_t size = 1024;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &size, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    for (int i = 0; i < ubse::mem::controller::UBSE_MAX_USR_INFO_LEN; i++) {
        uint8_t userInfo = static_cast<uint8_t>(i);
        memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &userInfo, sizeof(uint8_t));
        ptr += sizeof(uint8_t);
    }
    uint64_t flag = 0x1;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &flag, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    uint32_t nodeCnt = 1;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nodeCnt, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    uint32_t slotId = 1;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &slotId, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nodeCnt, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &slotId, sizeof(uint32_t));

    // 调用函数并验证结果
    EXPECT_EQ(UbseMemShmCreateReqUnpack(buffer, memShmDispatcher), UBSE_OK);
    EXPECT_EQ(memShmDispatcher.name, "test_shm");
    EXPECT_EQ(memShmDispatcher.size, 1024);
    EXPECT_EQ(memShmDispatcher.flag, 0x1);
    EXPECT_EQ(memShmDispatcher.shmRegion.nodeCnt, 1);
    EXPECT_EQ(memShmDispatcher.shmRegion.slotIds[0], 1);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemShmCreateReqUnpackFailed)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    ubse::mem::def::UbseMemShmDispatcher memShmDispatcher{};

    // 模拟 name 解包失败
    buffer.length = MAX_MEM_RESOURCE_NAME_LENGTH + sizeof(uint64_t) +
                    ubse::mem::controller::UBSE_MAX_USR_INFO_LEN * sizeof(uint8_t) + sizeof(uint64_t) +
                    sizeof(uint32_t) + UBS_MEM_MAX_SLOT_NUM * sizeof(uint32_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    uint8_t* ptr = buffer.buffer;
    const char* name = "test_shm";
    uint32_t nameLen = MAX_MEM_RESOURCE_NAME_LENGTH + 1; // 超出最大长度
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);

    EXPECT_EQ(UbseMemShmCreateReqUnpack(buffer, memShmDispatcher), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 size 解包失败
    buffer.length = strlen(name) + sizeof(uint32_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;

    EXPECT_EQ(UbseMemShmCreateReqUnpack(buffer, memShmDispatcher), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 usr_info 解包失败
    buffer.length = strlen(name) + sizeof(uint32_t) + sizeof(uint64_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    uint64_t size = 1024;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &size, sizeof(uint64_t));
    ptr += sizeof(uint64_t);

    EXPECT_EQ(UbseMemShmCreateReqUnpack(buffer, memShmDispatcher), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 region 解包失败
    buffer.length = strlen(name) + sizeof(uint32_t) + sizeof(uint64_t) +
                    ubse::mem::controller::UBSE_MAX_USR_INFO_LEN * sizeof(uint8_t) +
                    sizeof(uint64_t); // 缺少 region 数据
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &size, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    for (int i = 0; i < ubse::mem::controller::UBSE_MAX_USR_INFO_LEN; i++) {
        uint8_t userInfo = static_cast<uint8_t>(i);
        memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &userInfo, sizeof(uint8_t));
        ptr += sizeof(uint8_t);
    }
    uint64_t flag = 0x1;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &flag, sizeof(uint64_t));

    EXPECT_EQ(UbseMemShmCreateReqUnpack(buffer, memShmDispatcher), UBSE_ERROR_DESERIALIZE_FAILED);
    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemShmCreateWithAffinityReqUnpackSuccess)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    ubse::mem::def::UbseMemShmDispatcher memShmDispatcher{};

    // 模拟一个有效的 name
    buffer.length = MAX_MEM_RESOURCE_NAME_LENGTH + sizeof(uint64_t) +
                    ubse::mem::controller::UBSE_MAX_USR_INFO_LEN * sizeof(uint8_t) + sizeof(uint64_t) +
                    sizeof(uint32_t) + UBS_MEM_MAX_SLOT_NUM * sizeof(uint32_t) + sizeof(uint32_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];

    // 填充缓冲区数据
    uint8_t* ptr = buffer.buffer;
    const char* name = "test_shm";
    uint32_t nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    uint64_t size = 1024;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &size, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    for (int i = 0; i < ubse::mem::controller::UBSE_MAX_USR_INFO_LEN; i++) {
        uint8_t userInfo = static_cast<uint8_t>(i);
        memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &userInfo, sizeof(uint8_t));
        ptr += sizeof(uint8_t);
    }
    uint64_t flag = 0x1;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &flag, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    uint32_t affinitySocketId = 1;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &affinitySocketId, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    uint32_t nodeCnt = 1;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nodeCnt, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    uint32_t slotId = 1;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &slotId, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nodeCnt, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &slotId, sizeof(uint32_t));

    // 调用函数并验证结果
    EXPECT_EQ(UbseMemShmCreateWithAffinityReqUnpack(buffer, memShmDispatcher), UBSE_OK);
    EXPECT_EQ(memShmDispatcher.name, "test_shm");
    EXPECT_EQ(memShmDispatcher.size, 1024);
    EXPECT_EQ(memShmDispatcher.flag, 0x1);
    EXPECT_EQ(memShmDispatcher.affinitySocketId, 1);
    EXPECT_EQ(memShmDispatcher.shmRegion.nodeCnt, 1);
    EXPECT_EQ(memShmDispatcher.shmRegion.slotIds[0], 1);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemShmCreateWithLenderReqUnpackSuccess)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    // 模拟一个有效的 name
    buffer.length = MAX_MEM_RESOURCE_NAME_LENGTH + ubse::mem::controller::UBSE_MAX_USR_INFO_LEN * sizeof(uint8_t) +
                    sizeof(uint64_t) + sizeof(uint32_t) + UBS_MEM_MAX_SLOT_NUM * sizeof(uint32_t) + sizeof(uint64_t) +
                    sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    // 填充缓冲区数据
    uint8_t* ptr = buffer.buffer;
    const char* name = "test_shm";
    uint32_t nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    for (int i = 0; i < ubse::mem::controller::UBSE_MAX_USR_INFO_LEN; i++) {
        uint8_t userInfo = static_cast<uint8_t>(i);
        memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &userInfo, sizeof(uint8_t));
        ptr += sizeof(uint8_t);
    }
    uint64_t flag = 0x1;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &flag, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    uint32_t nodeCnt = 1;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nodeCnt, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    uint32_t slotId = 1;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &slotId, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    uint64_t size = 1024;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &size, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    uint32_t slot_id = 1;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &slot_id, sizeof(uint64_t));
    ptr += sizeof(uint32_t);
    uint32_t socket_id = 36;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &socket_id, sizeof(uint64_t));
    ptr += sizeof(uint32_t);
    uint32_t numa_id = 1;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &numa_id, sizeof(uint64_t));
    ptr += sizeof(uint32_t);
    uint32_t port_id = 1;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &port_id, sizeof(uint64_t));
    ptr += sizeof(uint32_t);

    // 调用函数并验证结果
    UbseMemShareBorrowReq memShmBorrowReq;
    flag = 0;
    EXPECT_EQ(UbseMemShmCreateWithLenderReqUnpack(buffer, memShmBorrowReq, flag), UBSE_OK);
    EXPECT_EQ(memShmBorrowReq.name, "test_shm");
    EXPECT_EQ(memShmBorrowReq.shmRegion.nodeNum, 1);
    EXPECT_EQ(memShmBorrowReq.shmRegion.nodelist[0].nodeId, "1");
    EXPECT_EQ(memShmBorrowReq.lenderInfo.lender_size, 1024);
    EXPECT_EQ(memShmBorrowReq.lenderInfo.nodeId, "1");
    EXPECT_EQ(memShmBorrowReq.lenderInfo.socketId, 36);
    EXPECT_EQ(memShmBorrowReq.lenderInfo.numaId, 1);
    EXPECT_EQ(memShmBorrowReq.lenderInfo.portId, 1);
    EXPECT_EQ(flag, 1);
}

TEST_F(TestUbseMemApiConvert, UbseMemShmCreateWithAffinityReqUnpackFailed)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    ubse::mem::def::UbseMemShmDispatcher memShmDispatcher{};

    // 模拟 name 解包失败
    buffer.length = MAX_MEM_RESOURCE_NAME_LENGTH + sizeof(uint64_t) +
                    ubse::mem::controller::UBSE_MAX_USR_INFO_LEN * sizeof(uint8_t) + sizeof(uint64_t) +
                    sizeof(uint32_t) + UBS_MEM_MAX_SLOT_NUM * sizeof(uint32_t) + sizeof(uint32_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    uint8_t* ptr = buffer.buffer;
    uint32_t nameLen = MAX_MEM_RESOURCE_NAME_LENGTH + 1; // 超出最大长度
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    const char* name = "test_shm";
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);

    EXPECT_EQ(UbseMemShmCreateWithAffinityReqUnpack(buffer, memShmDispatcher), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 size 解包失败
    buffer.length = strlen(name) + sizeof(uint32_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;

    EXPECT_EQ(UbseMemShmCreateWithAffinityReqUnpack(buffer, memShmDispatcher), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 usr_info 解包失败
    buffer.length = strlen(name) + sizeof(uint32_t) + sizeof(uint64_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    uint64_t size = 1024;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &size, sizeof(uint64_t));
    ptr += sizeof(uint64_t);

    EXPECT_EQ(UbseMemShmCreateWithAffinityReqUnpack(buffer, memShmDispatcher), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 affinitySocketId 解包失败
    buffer.length = strlen(name) + sizeof(uint32_t) + sizeof(uint64_t) +
                    ubse::mem::controller::UBSE_MAX_USR_INFO_LEN * sizeof(uint8_t) +
                    sizeof(uint64_t); // 缺少 affinitySocketId 数据
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &size, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    for (int i = 0; i < ubse::mem::controller::UBSE_MAX_USR_INFO_LEN; i++) {
        uint8_t userInfo = static_cast<uint8_t>(i);
        memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &userInfo, sizeof(uint8_t));
        ptr += sizeof(uint8_t);
    }
    uint64_t flag = 0x1;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &flag, sizeof(uint64_t));

    EXPECT_EQ(UbseMemShmCreateWithAffinityReqUnpack(buffer, memShmDispatcher), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}
TEST_F(TestUbseMemApiConvert, UbseMemCreateReqUnpackSuccess)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    ubse::adapter_plugins::mmi::UbseMemFdBorrowReq memFdBorrowReq{};

    // 模拟一个有效的 name
    buffer.length = UBS_MEM_MAX_NAME_LENGTH + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uid_t) + sizeof(gid_t) +
                    sizeof(pid_t) + sizeof(mode_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];

    // 填充缓冲区数据
    uint8_t* ptr = buffer.buffer;
    const char* name = "test_mem";
    uint32_t nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    uint64_t size = 1024;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &size, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    uid_t uid = 1000;
    gid_t gid = 1000;
    pid_t pid = 1234;
    mode_t mode = 0644;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &uid, sizeof(uid_t));
    ptr += sizeof(uid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &gid, sizeof(gid_t));
    ptr += sizeof(gid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &pid, sizeof(pid_t));
    ptr += sizeof(pid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &mode, sizeof(mode_t));

    // 调用函数并验证结果
    EXPECT_EQ(UbseMemCreateReqUnpack(buffer, memFdBorrowReq), UBSE_OK);
    EXPECT_EQ(memFdBorrowReq.name, "test_mem");
    EXPECT_EQ(memFdBorrowReq.size, 1024);
    EXPECT_EQ(memFdBorrowReq.owner.uid, 1000);
    EXPECT_EQ(memFdBorrowReq.owner.gid, 1000);
    EXPECT_EQ(memFdBorrowReq.owner.pid, 1234);
    EXPECT_EQ(memFdBorrowReq.owner.mode, 0644);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemCreateWithLenderReqUnpackSuccess)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    ubse::adapter_plugins::mmi::UbseMemFdBorrowReq memFdBorrowReq{};

    // 模拟一个有效的 name
    buffer.length = UBS_MEM_MAX_NAME_LENGTH + sizeof(uid_t) + sizeof(gid_t) + sizeof(pid_t) + sizeof(mode_t) +
                    sizeof(uint32_t) +
                    2 * (sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint32_t));
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];

    // 填充缓冲区数据
    uint8_t* ptr = buffer.buffer;
    const char* name = "test_mem";
    uint32_t nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    uid_t uid = 1000;
    gid_t gid = 1000;
    pid_t pid = 1234;
    mode_t mode = 0644;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &uid, sizeof(uid_t));
    ptr += sizeof(uid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &gid, sizeof(gid_t));
    ptr += sizeof(gid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &pid, sizeof(pid_t));
    ptr += sizeof(pid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &mode, sizeof(mode_t));
    ptr += sizeof(mode_t);
    uint32_t lenderCnt = 2; // 两个 lender
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &lenderCnt, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    // 模拟两个 lender 的数据
    for (int i = 0; i < lenderCnt; ++i) {
        uint32_t slotId = 1;
        uint32_t socketId = 123;
        uint64_t numaId = 1;
        uint64_t lenderSize = 1024;
        uint32_t portId = 456;
        memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &lenderSize, sizeof(uint64_t));
        ptr += sizeof(uint64_t);
        memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &slotId, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &socketId, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &numaId, sizeof(uint64_t));
        ptr += sizeof(uint32_t);
        memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &portId, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
    }

    // 调用函数并验证结果
    EXPECT_EQ(UbseMemCreateWithLenderReqUnpack(buffer, memFdBorrowReq), UBSE_OK);
    EXPECT_EQ(memFdBorrowReq.name, "test_mem");
    EXPECT_EQ(memFdBorrowReq.owner.uid, 1000);
    EXPECT_EQ(memFdBorrowReq.owner.gid, 1000);
    EXPECT_EQ(memFdBorrowReq.owner.pid, 1234);
    EXPECT_EQ(memFdBorrowReq.owner.mode, 0644);
    EXPECT_EQ(memFdBorrowReq.lenderLocs.size(), 2);
    EXPECT_EQ(memFdBorrowReq.lenderSizes.size(), 2);
    EXPECT_EQ(memFdBorrowReq.lenderLocs[0].nodeId, "1");
    EXPECT_EQ(memFdBorrowReq.lenderLocs[0].numaId, 1);
    EXPECT_EQ(memFdBorrowReq.lenderSizes[0], 1024);
    EXPECT_EQ(memFdBorrowReq.lenderLocs[1].nodeId, "1");
    EXPECT_EQ(memFdBorrowReq.lenderLocs[1].numaId, 1);
    EXPECT_EQ(memFdBorrowReq.lenderSizes[1], 1024);
    EXPECT_EQ(memFdBorrowReq.size, 2048);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 name 解包失败
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = UBS_MEM_MAX_NAME_LENGTH + 1; // 超出最大长度
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);

    EXPECT_EQ(UbseMemCreateWithLenderReqUnpack(buffer, memFdBorrowReq), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 lenderCnt 解包失败
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &uid, sizeof(uid_t));
    ptr += sizeof(uid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &gid, sizeof(gid_t));
    ptr += sizeof(gid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &pid, sizeof(pid_t));
    ptr += sizeof(pid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &mode, sizeof(mode_t));
    ptr += sizeof(mode_t);
    uint32_t invalidLenderCnt = 0; // 无效的 lenderCnt
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &invalidLenderCnt, sizeof(uint32_t));

    EXPECT_EQ(UbseMemCreateWithLenderReqUnpack(buffer, memFdBorrowReq), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemCreateWithCandidateReqUnpackSuccess)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    ubse::adapter_plugins::mmi::UbseMemFdBorrowReq memFdBorrowReq{};

    // 模拟一个有效的 name
    buffer.length = UBS_MEM_MAX_NAME_LENGTH + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uid_t) + sizeof(gid_t) +
                    sizeof(pid_t) + sizeof(mode_t) + sizeof(uint32_t) + UBS_MEM_MAX_SLOT_NUM * sizeof(uint32_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];

    // 填充缓冲区数据
    uint8_t* ptr = buffer.buffer;
    const char* name = "test_mem";
    uint32_t nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    uint64_t size = 1024;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &size, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    uid_t uid = 1000;
    gid_t gid = 1000;
    pid_t pid = 1234;
    mode_t mode = 0644;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &uid, sizeof(uid_t));
    ptr += sizeof(uid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &gid, sizeof(gid_t));
    ptr += sizeof(gid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &pid, sizeof(pid_t));
    ptr += sizeof(pid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &mode, sizeof(mode_t));
    ptr += sizeof(mode_t);
    uint32_t slotCnt = 2; // 两个 slot
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &slotCnt, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    for (int i = 0; i < slotCnt; ++i) {
        uint32_t slotId = i + 1;
        memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &slotId, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
    }

    // 调用函数并验证结果
    EXPECT_EQ(UbseMemCreateWithCandidateReqUnpack(buffer, memFdBorrowReq), UBSE_OK);
    EXPECT_EQ(memFdBorrowReq.name, "test_mem");
    EXPECT_EQ(memFdBorrowReq.size, 1024);
    EXPECT_EQ(memFdBorrowReq.owner.uid, 1000);
    EXPECT_EQ(memFdBorrowReq.owner.gid, 1000);
    EXPECT_EQ(memFdBorrowReq.owner.pid, 1234);
    EXPECT_EQ(memFdBorrowReq.owner.mode, 0644);
    EXPECT_EQ(memFdBorrowReq.candidateNodeList.size(), 2);
    EXPECT_EQ(memFdBorrowReq.candidateNodeList[0], "1");
    EXPECT_EQ(memFdBorrowReq.candidateNodeList[1], "2");

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 name 解包失败
    buffer.length = UBS_MEM_MAX_NAME_LENGTH + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uid_t) + sizeof(gid_t) +
                    sizeof(pid_t) + sizeof(mode_t) + sizeof(uint32_t) + UBS_MEM_MAX_SLOT_NUM * sizeof(uint32_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = UBS_MEM_MAX_NAME_LENGTH + 1; // 超出最大长度
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);

    EXPECT_EQ(UbseMemCreateWithCandidateReqUnpack(buffer, memFdBorrowReq), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 size 解包失败
    buffer.length = UBS_MEM_MAX_NAME_LENGTH + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uid_t) + sizeof(gid_t) +
                    sizeof(pid_t) + sizeof(mode_t) + sizeof(uint32_t) + UBS_MEM_MAX_SLOT_NUM * sizeof(uint32_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;

    EXPECT_EQ(UbseMemCreateWithCandidateReqUnpack(buffer, memFdBorrowReq), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}
TEST_F(TestUbseMemApiConvert, UbseMemFdPermissionReqUnpackSuccess)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    ubse::adapter_plugins::mmi::UbseMemFdPermissionReq memFdPermissionReq{};

    // 模拟一个有效的 name
    buffer.length =
        UBS_MEM_MAX_NAME_LENGTH + sizeof(uint32_t) + sizeof(uid_t) + sizeof(gid_t) + sizeof(pid_t) + sizeof(mode_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];

    // 填充缓冲区数据
    uint8_t* ptr = buffer.buffer;
    const char* name = "test_mem";
    uint32_t nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    uid_t uid = 1000;
    gid_t gid = 1000;
    pid_t pid = 1234;
    mode_t mode = 0644;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &uid, sizeof(uid_t));
    ptr += sizeof(uid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &gid, sizeof(gid_t));
    ptr += sizeof(gid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &pid, sizeof(pid_t));
    ptr += sizeof(pid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &mode, sizeof(mode_t));

    // 调用函数并验证结果
    EXPECT_EQ(UbseMemFdPermissionReqUnpack(buffer, memFdPermissionReq), UBSE_OK);
    EXPECT_EQ(memFdPermissionReq.name, "test_mem");
    EXPECT_EQ(memFdPermissionReq.fdOwner.uid, 1000);
    EXPECT_EQ(memFdPermissionReq.fdOwner.gid, 1000);
    EXPECT_EQ(memFdPermissionReq.fdOwner.pid, 1234);
    EXPECT_EQ(memFdPermissionReq.fdOwner.mode, 0644);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 name 解包失败
    buffer.length =
        UBS_MEM_MAX_NAME_LENGTH + sizeof(uint32_t) + sizeof(uid_t) + sizeof(gid_t) + sizeof(pid_t) + sizeof(mode_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = UBS_MEM_MAX_NAME_LENGTH + 1; // 超出最大长度
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);

    EXPECT_EQ(UbseMemFdPermissionReqUnpack(buffer, memFdPermissionReq), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 owner 解包失败
    buffer.length = strlen(name) + sizeof(uint32_t); // 缺少 mode_t
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;

    EXPECT_EQ(UbseMemFdPermissionReqUnpack(buffer, memFdPermissionReq), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemNumaCreateReqUnpackSuccess)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq memNumaBorrowReq{};

    // 模拟一个有效的 name
    buffer.length = UBS_MEM_MAX_NAME_LENGTH + sizeof(uint64_t) + sizeof(uint32_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];

    // 填充缓冲区数据
    uint8_t* ptr = buffer.buffer;
    const char* name = "test_numa";
    uint32_t nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    uint64_t size = 1024;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &size, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    uint32_t distance = 1; // 假设 distance 是一个枚举值
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &distance, sizeof(uint32_t));

    // 调用函数并验证结果
    EXPECT_EQ(UbseMemNumaCreateReqUnpack(buffer, memNumaBorrowReq), UBSE_OK);
    EXPECT_EQ(memNumaBorrowReq.name, "test_numa");
    EXPECT_EQ(memNumaBorrowReq.size, 1024);
    EXPECT_EQ(memNumaBorrowReq.distance, 1);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 name 解包失败
    buffer.length = UBS_MEM_MAX_NAME_LENGTH + sizeof(uint64_t) + sizeof(uint32_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = UBS_MEM_MAX_NAME_LENGTH + 1; // 超出最大长度
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);

    EXPECT_EQ(UbseMemNumaCreateReqUnpack(buffer, memNumaBorrowReq), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 size 解包失败
    buffer.length = strlen(name) + sizeof(uint32_t); // 缺少 size
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);

    EXPECT_EQ(UbseMemNumaCreateReqUnpack(buffer, memNumaBorrowReq), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 distance 解包失败
    buffer.length = strlen(name) + sizeof(uint32_t) + sizeof(uint64_t); // 缺少 distance
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &size, sizeof(uint64_t));

    EXPECT_EQ(UbseMemNumaCreateReqUnpack(buffer, memNumaBorrowReq), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemNumaCreateLenderReqUnpackSuccess)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq memNumaBorrowReq{};

    // 模拟一个有效的 name
    buffer.length = UBS_MEM_MAX_NAME_LENGTH + sizeof(uint32_t) +
                    2 * (sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t));
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];

    // 填充缓冲区数据
    uint8_t* ptr = buffer.buffer;
    const char* name = "test_numa";
    uint32_t nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    uint32_t lenderCnt = 2; // 两个 lender
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &lenderCnt, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    // 模拟两个 lender 的数据
    for (int i = 0; i < lenderCnt; ++i) {
        uint32_t slotId = 1;
        uint32_t socketId = 123;
        uint64_t numaId = 1;
        uint64_t lenderSize = 1024;
        uint32_t portId = 456;
        memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &lenderSize, sizeof(uint64_t));
        ptr += sizeof(uint64_t);
        memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &slotId, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &socketId, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &numaId, sizeof(uint64_t));
        ptr += sizeof(uint32_t);
        memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &portId, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
    }

    // 调用函数并验证结果
    EXPECT_EQ(UbseMemNumaCreateLenderReqUnpack(buffer, memNumaBorrowReq), UBSE_OK);
    EXPECT_EQ(memNumaBorrowReq.name, "test_numa");
    EXPECT_EQ(memNumaBorrowReq.lenderLocs.size(), 2);
    EXPECT_EQ(memNumaBorrowReq.lenderSizes.size(), 2);
    EXPECT_EQ(memNumaBorrowReq.lenderLocs[0].nodeId, "1");
    EXPECT_EQ(memNumaBorrowReq.lenderLocs[0].numaId, 1);
    EXPECT_EQ(memNumaBorrowReq.lenderSizes[0], 1024);
    EXPECT_EQ(memNumaBorrowReq.lenderLocs[1].nodeId, "1");
    EXPECT_EQ(memNumaBorrowReq.lenderLocs[1].numaId, 1);
    EXPECT_EQ(memNumaBorrowReq.lenderSizes[1], 1024);
    EXPECT_EQ(memNumaBorrowReq.size, 2048);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 name 解包失败
    buffer.length = strlen(name) + sizeof(uint32_t) + sizeof(uint32_t) +
                    2 * (sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t));
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = UBS_MEM_MAX_NAME_LENGTH + 1; // 超出最大长度
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);

    EXPECT_EQ(UbseMemNumaCreateLenderReqUnpack(buffer, memNumaBorrowReq), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 lenderCnt 解包失败
    buffer.length = strlen(name) + sizeof(uint32_t); // 缺少 lender 数据
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);

    EXPECT_EQ(UbseMemNumaCreateLenderReqUnpack(buffer, memNumaBorrowReq), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 lender 数据解包失败
    buffer.length = strlen(name) + sizeof(uint32_t) + sizeof(uint32_t); // 缺少 lender 数据
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    lenderCnt = 2; // 两个 lender
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &lenderCnt, sizeof(uint32_t));

    EXPECT_EQ(UbseMemNumaCreateLenderReqUnpack(buffer, memNumaBorrowReq), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemNumaCreateWithCandidateReqUnpackSuccess)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq memNumaBorrowReq{};

    // 模拟一个有效的 name
    buffer.length =
        UBS_MEM_MAX_NAME_LENGTH + sizeof(uint64_t) + sizeof(uint32_t) + UBS_MEM_MAX_SLOT_NUM * sizeof(uint32_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];

    // 填充缓冲区数据
    uint8_t* ptr = buffer.buffer;
    const char* name = "test_numa";
    uint32_t nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    uint64_t size = 1024;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &size, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    uint32_t slotCnt = 2; // 两个 slot
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &slotCnt, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    for (int i = 0; i < slotCnt; ++i) {
        uint32_t slotId = i + 1;
        memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &slotId, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
    }

    // 调用函数并验证结果
    EXPECT_EQ(UbseMemNumaCreateWithCandidateReqUnpack(buffer, memNumaBorrowReq), UBSE_OK);
    EXPECT_EQ(memNumaBorrowReq.name, "test_numa");
    EXPECT_EQ(memNumaBorrowReq.size, 1024);
    EXPECT_EQ(memNumaBorrowReq.candidateNodeList.size(), 2);
    EXPECT_EQ(memNumaBorrowReq.candidateNodeList[0], "1");
    EXPECT_EQ(memNumaBorrowReq.candidateNodeList[1], "2");

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 name 解包失败
    buffer.length =
        strlen(name) + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t) + UBS_MEM_MAX_SLOT_NUM * sizeof(uint32_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = UBS_MEM_MAX_NAME_LENGTH + 1; // 超出最大长度
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);

    EXPECT_EQ(UbseMemNumaCreateWithCandidateReqUnpack(buffer, memNumaBorrowReq), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 size 解包失败
    buffer.length = strlen(name) + sizeof(uint32_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;

    EXPECT_EQ(UbseMemNumaCreateWithCandidateReqUnpack(buffer, memNumaBorrowReq), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 candidate 解包失败
    buffer.length = strlen(name) + sizeof(uint32_t) + sizeof(uint64_t); // 缺少 candidate 数据
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &size, sizeof(uint64_t));

    EXPECT_EQ(UbseMemNumaCreateWithCandidateReqUnpack(buffer, memNumaBorrowReq), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemShmAttachReqUnpackSuccess)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    ubse::adapter_plugins::mmi::UbseMemShareAttachReq memShareAttachReq{};

    // 模拟一个有效的 name
    buffer.length = UBS_MEM_MAX_NAME_LENGTH + sizeof(uid_t) + sizeof(gid_t) + sizeof(pid_t) + sizeof(mode_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];

    // 填充缓冲区数据
    uint8_t* ptr = buffer.buffer;
    const char* name = "test_shm";
    uint32_t nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;
    uid_t uid = 1000;
    gid_t gid = 1000;
    pid_t pid = 1234;
    mode_t mode = 0644;
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &uid, sizeof(uid_t));
    ptr += sizeof(uid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &gid, sizeof(gid_t));
    ptr += sizeof(gid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &pid, sizeof(pid_t));
    ptr += sizeof(pid_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &mode, sizeof(mode_t));

    // 调用函数并验证结果
    EXPECT_EQ(UbseMemShmAttachReqUnpack(buffer, memShareAttachReq), UBSE_OK);
    EXPECT_EQ(memShareAttachReq.name, "test_shm");
    EXPECT_EQ(memShareAttachReq.owner.uid, 1000);
    EXPECT_EQ(memShareAttachReq.owner.gid, 1000);
    EXPECT_EQ(memShareAttachReq.owner.pid, 1234);
    EXPECT_EQ(memShareAttachReq.owner.mode, 0644);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 name 解包失败
    buffer.length = UBS_MEM_MAX_NAME_LENGTH + sizeof(uid_t) + sizeof(gid_t) + sizeof(pid_t) + sizeof(mode_t);
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = UBS_MEM_MAX_NAME_LENGTH + 1; // 超出最大长度
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);

    EXPECT_EQ(UbseMemShmAttachReqUnpack(buffer, memShareAttachReq), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 owner 解包失败
    buffer.length = strlen(name) + sizeof(uint32_t); // 缺少 mode_t
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = strlen(name);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), name, nameLen);
    ptr += nameLen;

    EXPECT_EQ(UbseMemShmAttachReqUnpack(buffer, memShareAttachReq), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemNameUnpackSuccess)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    std::string name;

    // 模拟一个有效的 name
    buffer.length = UBS_MEM_MAX_NAME_LENGTH;
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];

    // 填充缓冲区数据
    uint8_t* ptr = buffer.buffer;
    const char* testName = "test_name";
    uint32_t nameLen = strlen(testName);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), testName, nameLen);

    // 调用函数并验证结果
    EXPECT_EQ(UbseMemNameUnpack(buffer, name), UBSE_OK);
    EXPECT_EQ(name, "test_name");

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);

    // 模拟 name 解包失败
    buffer.length = UBS_MEM_MAX_NAME_LENGTH;
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];
    ptr = buffer.buffer;
    nameLen = UBS_MEM_MAX_NAME_LENGTH + 1; // 超出最大长度
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));

    EXPECT_EQ(UbseMemNameUnpack(buffer, name), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemShmGetReqUnpackSuccess)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    std::string name;

    // 模拟一个有效的 name
    buffer.length = UBS_MEM_MAX_NAME_LENGTH;
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];

    // 填充缓冲区数据
    uint8_t* ptr = buffer.buffer;
    const char* testName = "test_shm";
    uint32_t nameLen = strlen(testName);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), testName, nameLen);

    // 调用函数并验证结果
    EXPECT_EQ(UbseMemShmGetReqUnpack(buffer, name), UBSE_OK);
    EXPECT_EQ(name, "test_shm");

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemShmDetachReqUnpackSuccess)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    ubse::adapter_plugins::mmi::UbseMemShareDetachReq memShareDetachReq;

    // 模拟一个有效的 name
    buffer.length = UBS_MEM_MAX_NAME_LENGTH;
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];

    // 填充缓冲区数据
    uint8_t* ptr = buffer.buffer;
    const char* testName = "test_shm";
    uint32_t nameLen = strlen(testName);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), testName, nameLen);

    // 调用函数并验证结果
    EXPECT_EQ(UbseMemShmDetachReqUnpack(buffer, memShareDetachReq), UBSE_OK);
    EXPECT_EQ(memShareDetachReq.name, "test_shm");

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemShmDeleteReqUnpackSuccess)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    ubse::adapter_plugins::mmi::UbseMemReturnReq memReturnReq;

    // 模拟一个有效的 name
    buffer.length = UBS_MEM_MAX_NAME_LENGTH;
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];

    // 填充缓冲区数据
    uint8_t* ptr = buffer.buffer;
    const char* testName = "test_shm";
    uint32_t nameLen = strlen(testName);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), testName, nameLen);

    // 调用函数并验证结果
    EXPECT_EQ(UbseMemShmDeleteReqUnpack(buffer, memReturnReq), UBSE_OK);
    EXPECT_EQ(memReturnReq.name, "test_shm");

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemShmtatusGetReqUnPackSuccess)
{
    // 模拟一个有效的请求缓冲区
    UbseIpcMessage buffer{};
    std::string name;

    // 模拟一个有效的 name
    buffer.length = UBS_MEM_MAX_NAME_LENGTH;
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];

    // 填充缓冲区数据
    uint8_t* ptr = buffer.buffer;
    const char* testName = "test_shm";
    uint32_t nameLen = strlen(testName);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), testName, nameLen);

    // 调用函数并验证结果
    EXPECT_EQ(UbseMemShmtatusGetReqUnPack(buffer, name), UBSE_OK);
    EXPECT_EQ(name, "test_shm");

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}

TEST_F(TestUbseMemApiConvert, UbseMemShmtatusGetReqUnPackFailure)
{
    // 模拟一个无效的请求缓冲区
    UbseIpcMessage buffer{};
    std::string name;

    // 模拟一个无效的 name
    buffer.length = UBS_MEM_MAX_NAME_LENGTH;
    buffer.buffer = new (std::nothrow) uint8_t[buffer.length];

    // 填充缓冲区数据
    uint8_t* ptr = buffer.buffer;
    uint32_t nameLen = UBS_MEM_MAX_NAME_LENGTH + 1; // 超出最大长度
    memcpy_s(ptr, buffer.length - (ptr - buffer.buffer), &nameLen, sizeof(uint32_t));

    // 调用函数并验证结果
    EXPECT_EQ(UbseMemShmtatusGetReqUnPack(buffer, name), UBSE_ERROR_DESERIALIZE_FAILED);

    // 清理缓冲区
    SafeDeleteArray(buffer.buffer);
}
} // namespace ubse::mem_controller::ut