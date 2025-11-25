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

#include "test_ubse_mem_api.h"

#include <mockcpp/mockcpp.hpp>

#include "message/ubse_mem_node_debt_info_conversion.h"
#include "ubs_error.h"
#include "ubse_context.h"
#include "ubse_mem_api.h"
#include "ubse_mem_api_convert.h"
#include "ubse_mem_controller_query_api.h"
#include "ubse_mem_obj.h"
#include "ubse_mem_resource.h"

namespace ubse::mem_controller::ut {
using namespace usbe::mem::api;
using namespace api::server;
using namespace context;
using namespace ubse::resource::mem;
using namespace ubse::mem::obj;

void TestUbseMemApi::SetUp()
{
    Test::SetUp();
}
void TestUbseMemApi::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemApi, Register)
{
    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ(UbseMemApi::Register(), UBSE_ERROR_NULLPTR);

    MOCKER_CPP(&UbseApiServerModule::RegisterIpcHandler)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::Register(), UBSE_ERROR);
    EXPECT_EQ(UbseMemApi::Register(), UBSE_OK);
}

TEST_F(TestUbseMemApi, UbseSeverNodeNumaGet)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(UbseMemApi::UbseSeverNodeNumaGet(req, context), static_cast<uint32_t>(UBS_ERR_BUFFER_TOO_SMALL));

    req.length = sizeof(uint32_t);
    EXPECT_EQ(UbseMemApi::UbseSeverNodeNumaGet(req, context), 150);

    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);
    MOCKER_CPP(&UbseNumaInfoListPack).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseSeverNodeNumaGet(req, context), UBSE_ERROR);

    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ(UbseMemApi::UbseSeverNodeNumaGet(req, context), UBSE_ERROR_NULLPTR);

    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseSeverNodeNumaGet(req, context), UBSE_ERROR);
    EXPECT_EQ(UbseMemApi::UbseSeverNodeNumaGet(req, context), UBSE_OK);
    delete req.buffer;
    req.buffer = nullptr;
}

TEST_F(TestUbseMemApi, UbseServerFdGet)
{
    UbseIpcMessage req{};
    req.length = sizeof(uint32_t);
    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);
    UbseRequestContext context{};
    MOCKER_CPP(&mem::controller::UbseMemFdGet).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseServerFdGet(req, context), UBSE_ERROR);

    MOCKER_CPP(&api::server::UbseMemFdDescPack).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseServerFdGet(req, context), UBSE_ERROR);

    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ(UbseMemApi::UbseServerFdGet(req, context), UBSE_ERROR_NULLPTR);

    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseServerFdGet(req, context), UBSE_ERROR);
    EXPECT_EQ(UbseMemApi::UbseServerFdGet(req, context), UBSE_OK);
    delete req.buffer;
    req.buffer = nullptr;
}

TEST_F(TestUbseMemApi, UbseServerFdList)
{
    UbseIpcMessage req{};
    req.length = sizeof(uint32_t);
    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);
    UbseRequestContext context{};
    MOCKER_CPP(&mem::controller::UbseMemFdList).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseServerFdList(req, context), UBSE_ERROR);

    MOCKER_CPP(&api::server::UbseMemFdDescListPackedSize)
        .stubs()
        .will(returnValue(static_cast<uint32_t>(IPC_ERROR_SERIALIZATION_FAILED)))
        .then(returnValue(static_cast<uint32_t>(IPC_SUCCESS)));
    EXPECT_EQ(UbseMemApi::UbseServerFdList(req, context), static_cast<uint32_t>(IPC_ERROR_SERIALIZATION_FAILED));

    MOCKER_CPP(&api::server::UbseMemFdDescListPack)
        .stubs()
        .will(returnValue(static_cast<uint32_t>(IPC_ERROR_SERIALIZATION_FAILED)))
        .then(returnValue(static_cast<uint32_t>(IPC_SUCCESS)));
    EXPECT_EQ(UbseMemApi::UbseServerFdList(req, context), static_cast<uint32_t>(IPC_ERROR_SERIALIZATION_FAILED));

    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ(UbseMemApi::UbseServerFdList(req, context), UBSE_ERROR_NULLPTR);

    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseServerFdList(req, context), UBSE_ERROR);
    EXPECT_EQ(UbseMemApi::UbseServerFdList(req, context), UBSE_OK);
    delete req.buffer;
    req.buffer = nullptr;
}

TEST_F(TestUbseMemApi, UbseServerNumaList)
{
    UbseIpcMessage req{};
    req.length = sizeof(uint32_t);
    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);
    UbseRequestContext context{};
    MOCKER_CPP(&mem::controller::UbseMemNumaList).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseServerNumaList(req, context), UBSE_ERROR);

    MOCKER_CPP(&api::server::UbseMemNumaDescListPack).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseServerNumaList(req, context), UBSE_ERROR);

    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ(UbseMemApi::UbseServerNumaList(req, context), UBSE_ERROR_NULLPTR);

    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseServerNumaList(req, context), UBSE_ERROR);
    EXPECT_EQ(UbseMemApi::UbseServerNumaList(req, context), UBSE_OK);
    delete req.buffer;
    req.buffer = nullptr;
}

TEST_F(TestUbseMemApi, UbseServerNumaGet)
{
    UbseIpcMessage req{};
    req.length = sizeof(uint32_t);
    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);
    UbseRequestContext context{};
    MOCKER_CPP(&mem::controller::UbseMemNumaGet).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseServerNumaGet(req, context), UBSE_ERROR);

    MOCKER_CPP(&api::server::UbseMemNumaDescPack).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseServerNumaGet(req, context), UBSE_ERROR);

    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ(UbseMemApi::UbseServerNumaGet(req, context), UBSE_ERROR_NULLPTR);

    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseServerNumaGet(req, context), UBSE_ERROR);
    EXPECT_EQ(UbseMemApi::UbseServerNumaGet(req, context), UBSE_OK);
    delete req.buffer;
    req.buffer = nullptr;
}

TEST_F(TestUbseMemApi, UbseNodeBorrowInfoHandle)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(UbseMemApi::UbseNodeBorrowInfoHandle(req, context), UBSE_ERROR_NULLPTR);
    req.length = sizeof(uint32_t);
    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);
    EXPECT_EQ(UbseMemApi::UbseNodeBorrowInfoHandle(req, context), UBSE_ERROR_NULL_INFO);

    MOCKER_CPP(&resource::mem::UbseGetMemDebtInfo).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    EXPECT_EQ(UbseMemApi::UbseNodeBorrowInfoHandle(req, context), UBSE_ERROR_NULLPTR);

    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseNodeBorrowInfoHandle(req, context), UBSE_ERROR);
    EXPECT_EQ(UbseMemApi::UbseNodeBorrowInfoHandle(req, context), UBSE_OK);
    delete req.buffer;
    req.buffer = nullptr;
}

void SetupTestObjects(UbseMemFdBorrowReq &fdBorrowReq, UbseMemNumaBorrowReq &numaBorrowReq,
                      UbseMemFdBorrowImportObj &fdImportObj, UbseMemFdBorrowExportObj &fdExportObj,
                      UbseMemNumaBorrowImportObj &numaImportObj, UbseMemNumaBorrowExportObj &numaExportObj,
                      NodeMemDebtInfoMap &nodeDebtInfoMap)
{
    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};

    // 初始化 fdBorrowReq
    fdBorrowReq.name = "test";
    fdBorrowReq.requestNodeId = "1";
    fdBorrowReq.importNodeId = "1";
    fdBorrowReq.size = 128;
    fdBorrowReq.udsInfo = udsInfo;

    // 初始化 numaBorrowReq
    numaBorrowReq.name = "test";
    numaBorrowReq.requestNodeId = "1";
    numaBorrowReq.importNodeId = "1";
    numaBorrowReq.size = 128;
    numaBorrowReq.udsInfo = udsInfo;

    // 构造 fdImportObj
    fdImportObj.req = fdBorrowReq;
    UbseMemDebtNumaInfo fdImportNmaInfo{.nodeId = "1", .socketId = 0, .numaId = 0, .size = 128};
    UbseMemDebtNumaInfo fdExportNmaInfo{.nodeId = "2", .socketId = 0, .numaId = 0, .size = 128};
    fdImportObj.algoResult.exportNumaInfos.emplace_back(fdExportNmaInfo);
    fdImportObj.algoResult.importNumaInfos.emplace_back(fdImportNmaInfo);
    fdImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;

    // 构造 fdExportObj
    fdExportObj.req = fdBorrowReq;
    fdExportObj.algoResult = fdImportObj.algoResult;
    fdExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;

    // 构造 numaImportObj
    numaImportObj.req = numaBorrowReq;
    UbseMemDebtNumaInfo numaImportNmaInfo{.nodeId = "1", .socketId = 0, .numaId = 0, .size = 128};
    UbseMemDebtNumaInfo numaExportNmaInfo{.nodeId = "2", .socketId = 0, .numaId = 0, .size = 128};
    numaImportObj.algoResult.exportNumaInfos.emplace_back(numaExportNmaInfo);
    numaImportObj.algoResult.importNumaInfos.emplace_back(numaImportNmaInfo);
    numaImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;

    // 构造 numaExportObj
    numaExportObj.req = numaBorrowReq;
    numaExportObj.algoResult = numaImportObj.algoResult;
    numaExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;

    // 填充 nodeDebtInfoMap
    nodeDebtInfoMap[fdExportObj.algoResult.exportNumaInfos[0].nodeId].fdExportObjMap[fdBorrowReq.name] = fdExportObj;
    nodeDebtInfoMap[fdBorrowReq.importNodeId].fdImportObjMap[fdBorrowReq.name] = fdImportObj;

    nodeDebtInfoMap[numaExportObj.algoResult.importNumaInfos[0].nodeId].numaExportObjMap[numaBorrowReq.name] =
        numaExportObj;
    nodeDebtInfoMap[numaBorrowReq.importNodeId].numaImportObjMap[numaBorrowReq.name] = numaImportObj;
}

TEST_F(TestUbseMemApi, UbseNodeBorrowInfoHandleInnerMethod)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    req.length = sizeof(uint32_t);
    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);

    UbseMemFdBorrowReq fdBorrowReq;
    UbseMemNumaBorrowReq numaBorrowReq;
    UbseMemFdBorrowImportObj fdImportObj;
    UbseMemFdBorrowExportObj fdExportObj;
    UbseMemNumaBorrowImportObj numaImportObj;
    UbseMemNumaBorrowExportObj numaExportObj;
    NodeMemDebtInfoMap ubse_node_mem_debt_info;
    SetupTestObjects(fdBorrowReq, numaBorrowReq, fdImportObj, fdExportObj, numaImportObj, numaExportObj,
                     ubse_node_mem_debt_info);

    MOCKER_CPP(&resource::mem::UbseGetMemDebtInfo)
        .stubs()
        .with(any(), outBound(ubse_node_mem_debt_info))
        .will(returnValue(UBSE_OK));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();

    EXPECT_EQ(UbseMemApi::UbseNodeBorrowInfoHandle(req, context), UBSE_ERROR_NULLPTR);
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseNodeBorrowInfoHandle(req, context), UBSE_OK);
    delete req.buffer;
    req.buffer = nullptr;
}

TEST_F(TestUbseMemApi, UbseBorrowDetailsInfoHandle)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(UbseMemApi::UbseBorrowDetailsInfoHandle(req, context), UBSE_ERROR_NULLPTR);
    req.length = sizeof(uint32_t);
    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);
    EXPECT_EQ(UbseMemApi::UbseBorrowDetailsInfoHandle(req, context), UBSE_ERROR_NULL_INFO);

    MOCKER_CPP(&resource::mem::UbseGetMemDebtInfo).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    EXPECT_EQ(UbseMemApi::UbseBorrowDetailsInfoHandle(req, context), UBSE_ERROR_NULLPTR);

    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseBorrowDetailsInfoHandle(req, context), UBSE_ERROR);
    EXPECT_EQ(UbseMemApi::UbseBorrowDetailsInfoHandle(req, context), UBSE_OK);
    delete req.buffer;
    req.buffer = nullptr;
}

TEST_F(TestUbseMemApi, UbseBorrowDetailsInfoHandleInner)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    req.length = sizeof(uint32_t);
    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);

    UbseMemFdBorrowReq fdBorrowReq;
    UbseMemNumaBorrowReq numaBorrowReq;
    UbseMemFdBorrowImportObj fdImportObj;
    UbseMemFdBorrowExportObj fdExportObj;
    UbseMemNumaBorrowImportObj numaImportObj;
    UbseMemNumaBorrowExportObj numaExportObj;
    NodeMemDebtInfoMap ubse_node_mem_debt_info;
    SetupTestObjects(fdBorrowReq, numaBorrowReq, fdImportObj, fdExportObj, numaImportObj, numaExportObj,
                     ubse_node_mem_debt_info);

    MOCKER_CPP(&resource::mem::UbseGetMemDebtInfo)
        .stubs()
        .with(any(), outBound(ubse_node_mem_debt_info))
        .will(returnValue(UBSE_OK));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();

    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseBorrowDetailsInfoHandle(req, context), UBSE_OK);
    delete req.buffer;
    req.buffer = nullptr;
}

TEST_F(TestUbseMemApi, UbseCheckMemoryStatus)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(UbseMemApi::UbseCheckMemoryStatus(req, context), UBSE_ERROR_NULLPTR);
    req.length = sizeof(uint32_t);
    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);
    EXPECT_EQ(UbseMemApi::UbseCheckMemoryStatus(req, context), UBSE_ERROR_NULLPTR);

    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseCheckMemoryStatus(req, context), UBSE_ERROR);
    EXPECT_EQ(UbseMemApi::UbseCheckMemoryStatus(req, context), UBSE_OK);
    delete req.buffer;
    req.buffer = nullptr;
}
} // namespace ubse::mem_controller::ut