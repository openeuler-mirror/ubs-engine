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

#include "test_ubse_mem_controller_api_agent.h"

#include <mockcpp/mockcpp.hpp>

#include "message/ubse_mem_fd_borrow_req_simpo.h"
#include "message/ubse_mem_numa_borrow_req_simpo.h"
#include "message/ubse_mem_return_req_simpo.h"
#include "message/ubse_mem_addr_borrow_req_simpo.h"
#include "message/ubse_mem_share_attach_req_simpo.h"
#include "message/ubse_mem_share_borrow_req_simpo.h"
#include "message/test_ubse_mem_share_detach_req_simpo.h"
#include "ubse_conf.h"
#include "ubse_context.h"
#include "ubse_conf_module.h"
#include "ubse_election.h"
#include "ubse_mem_util.h"
#include "ubse_error.h"
#include "ubse_mem_controller_api_agent.h"
#include "ubse_mem_controller_handler.h"
#include "ubse_mem_controller_numa_api.h"
#include "ubse_thread_pool_module.h"
#include "ubse_serial_util.h"
#include "ubse_api_server_module.h"
#include "ubse_node_controller.h"
#include "ubse_mem_controller_api_agent.cpp"

namespace ubse::mem_controller::ut {
using namespace context;
using namespace ubse::mem::controller::agent;
using namespace ubse::serial;
using namespace ubse::election;
using namespace ubse::nodeController;
using namespace ubse::config;

void TestUbseMemControllerApiAgent::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerApiAgent::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerApiAgent, Init)
{
    GTEST_SKIP();
    std::shared_ptr<UbseConfModule> nullConfModule = nullptr;
    std::shared_ptr<UbseConfModule> configModule = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>)
        .stubs()
        .will(returnValue(configModule))
        .then(returnValue(configModule));
    EXPECT_EQ(ubse::mem::controller::agent::Init(), UBSE_ERROR_NULLPTR);
    MOCKER_CPP(&UbseConfModule::GetULongConf).stubs().will(returnValue(UBSE_OK));

    std::shared_ptr<task_executor::UbseTaskExecutorModule> nullModule = nullptr;
    std::shared_ptr<task_executor::UbseTaskExecutorModule> module =
        std::make_shared<task_executor::UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<task_executor::UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ(ubse::mem::controller::agent::Init(), UBSE_ERROR_NULLPTR);

    MOCKER_CPP(&task_executor::UbseTaskExecutorModule::Create)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(ubse::mem::controller::agent::Init(), UBSE_ERROR);

    MOCKER_CPP(&mem::controller::agent::UbseMemOperationRespHandler::RegUbseMemOperationRespHandlerToServer)
        .stubs()
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(ubse::mem::controller::agent::Init(), UBSE_OK);
}

TEST_F(TestUbseMemControllerApiAgent, UbseMemNumaBorrow)
{
    UbseRoleInfo currentNode;
    currentNode.nodeRole = election::ELECTION_ROLE_MASTER;
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().with(outBound(currentNode)).will(returnValue(UBSE_OK));
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetMasterInfo)
        .stubs()
        .with(outBound(masterInfo))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    UbseMemNumaBorrowReq req{};
    UbseMemOperationResp resp{};
    EXPECT_NE(mem::controller::UbseMemNumaBorrow(req, resp), UBSE_ERROR);

    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(mem::controller::UbseMemNumaBorrow(req, resp), UBSE_ERROR_NULLPTR);

    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::UbseMemNumaBorrowReqSimpoPtr, UbseBaseMessagePtr>;
    std::chrono::seconds timeout(1);
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&GetWaitTimeout).stubs().will(returnValue(timeout));
    bool (task_executor::UbseTaskExecutor::*func)(const std::function<void()> &task) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(mem::controller::UbseMemNumaBorrow(req, resp), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, UbseMemReturn)
{
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetMasterInfo)
        .stubs()
        .with(outBound(masterInfo))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    UbseMemReturnReq req{};
    UbseMemOperationResp resp{};
    EXPECT_EQ(UbseMemReturn(req, MemOperationType::NUMA_RETURN, resp), UBSE_ERROR);

    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(UbseMemReturn(req, MemOperationType::NUMA_RETURN, resp), UBSE_ERROR_NULLPTR);

    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::UbseMemReturnReqSimpoPtr, UbseBaseMessagePtr>;
    std::chrono::seconds timeout(1);
    Ref<ubse::task_executor::UbseTaskExecutor> taskExecutorPtr = new ubse::task_executor::UbseTaskExecutor("task", 0, 0);
    MOCKER_CPP(&ubse::mem::util::GetExecutor).stubs().will(returnValue(taskExecutorPtr));
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&GetWaitTimeout).stubs().will(returnValue(timeout));
    bool (task_executor::UbseTaskExecutor::*func)(const std::function<void()> &task) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(UbseMemReturn(req, MemOperationType::NUMA_RETURN, resp), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, DeleteMemoryHandler)
{
    UbseIpcMessage request{};
    UbseRequestContext context{};
    UbseSerialization serialization;
    serialization << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    request.buffer = serialization.GetBuffer();
    request.length = serialization.GetLength();
    EXPECT_EQ(DeleteMemoryHandler(request, context), UBSE_ERROR);

    UbseSerialization serialization1;
    serialization1 << "test" << "xx";
    request.buffer = serialization1.GetBuffer();
    request.length = serialization1.GetLength();
    UbseRoleInfo currentNodeInfo;
    currentNodeInfo.nodeId = "1";
    MOCKER(&UbseGetCurrentNodeInfo)
        .stubs().with(outBound(currentNodeInfo)).will(returnValue(UBSE_ERROR));
    EXPECT_EQ(DeleteMemoryHandler(request, context), UBSE_ERROR_NULLPTR);

    MOCKER(&UbseGetCurrentNodeInfo).reset();
    MOCKER(&UbseGetCurrentNodeInfo)
        .stubs().with(outBound(currentNodeInfo)).will(returnValue(UBSE_OK));
    MOCKER(UbseMemReturn).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    MOCKER(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(DeleteMemoryHandler(request, context), UBSE_OK);
}

TEST_F(TestUbseMemControllerApiAgent, UbseMemNumaBorrow1)
{
    MOCKER(&election::UbseGetCurrentNodeInfo)
        .stubs().will(returnValue(UBSE_OK));
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetMasterInfo)
        .stubs()
        .with(outBound(masterInfo))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    UbseMemNumaBorrowReq req{};
    UbseMemOperationResp resp{};
    EXPECT_EQ(UbseMemNumaBorrow(req, resp), UBSE_ERROR);

    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(UbseMemNumaBorrow(req, resp), UBSE_ERROR_NULLPTR);

    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::UbseMemNumaBorrowReqSimpoPtr, UbseBaseMessagePtr>;
    std::chrono::seconds timeout(1);
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&GetWaitTimeout).stubs().will(returnValue(timeout));
    bool (task_executor::UbseTaskExecutor::*func)(const std::function<void()> &task) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(UbseMemNumaBorrow(req, resp), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, UbseMemReturn1)
{
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = "1";
    MOCKER(&election::UbseGetCurrentNodeInfo)
        .stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&election::UbseGetMasterInfo)
        .stubs()
        .with(outBound(masterInfo))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    UbseMemReturnReq req{};
    UbseMemOperationResp resp{};
    EXPECT_EQ(UbseMemReturn(req, MemOperationType::NUMA_RETURN, resp), UBSE_ERROR);

    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(UbseMemReturn(req, MemOperationType::NUMA_RETURN, resp), UBSE_ERROR_NULLPTR);

    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::UbseMemReturnReqSimpoPtr, UbseBaseMessagePtr>;
    std::chrono::seconds timeout(1);
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_OK));
    Ref<ubse::task_executor::UbseTaskExecutor> taskExecutorPtr = new ubse::task_executor::UbseTaskExecutor("task", 0, 0);
    MOCKER_CPP(&ubse::mem::util::GetExecutor).stubs().will(returnValue(taskExecutorPtr));
    MOCKER_CPP(&GetWaitTimeout).stubs().will(returnValue(timeout));
    bool (task_executor::UbseTaskExecutor::*func)(const std::function<void()> &task) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(UbseMemReturn(req, MemOperationType::NUMA_RETURN, resp), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, UbseMemAddrBorrow)
{
    MOCKER(&election::UbseGetCurrentNodeInfo)
        .stubs().will(returnValue(UBSE_OK));
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetMasterInfo)
        .stubs()
        .with(outBound(masterInfo))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    UbseMemAddrBorrowReq req{};
    UbseMemOperationResp resp{};
    EXPECT_EQ(UbseMemAddrBorrow(req, resp), UBSE_ERROR);

    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(UbseMemAddrBorrow(req, resp), UBSE_ERROR_NULLPTR);

    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::UbseMemAddrBorrowReqSimpoPtr, UbseBaseMessagePtr>;
    std::chrono::seconds timeout(1);
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&GetWaitTimeout).stubs().will(returnValue(timeout));
    bool (task_executor::UbseTaskExecutor::*func)(const std::function<void()> &task) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(UbseMemAddrBorrow(req, resp), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, UbseMemAddrBorrow1)
{
    MOCKER(&election::UbseGetCurrentNodeInfo)
        .stubs().will(returnValue(UBSE_OK));
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetMasterInfo)
        .stubs()
        .with(outBound(masterInfo))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    UbseMemAddrBorrowReq req{};
    req.wrDelayComp = 2;
    UbseMemOperationResp resp{};
    EXPECT_EQ(ubse::mem::controller::agent::UbseMemAddrBorrow(req, resp), UBSE_ERROR);
    req.wrDelayComp = 1;
    EXPECT_EQ(ubse::mem::controller::agent::UbseMemAddrBorrow(req, resp), UBSE_ERROR);
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(ubse::mem::controller::agent::UbseMemAddrBorrow(req, resp), UBSE_ERROR_NULLPTR);

    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::UbseMemAddrBorrowReqSimpoPtr, UbseBaseMessagePtr>;
    std::chrono::seconds timeout(1);
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&GetWaitTimeout).stubs().will(returnValue(timeout));
    bool (task_executor::UbseTaskExecutor::*func)(const std::function<void()> &task) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(ubse::mem::controller::agent::UbseMemAddrBorrow(req, resp), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, UbseMemShareBorrow)
{
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetMasterInfo)
        .stubs()
        .with(outBound(masterInfo))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    UbseMemShareBorrowReq req{};
    UbseMemOperationResp resp{};
    EXPECT_EQ(ubse::mem::controller::agent::UbseMemShareBorrow(req, resp), UBSE_ERROR);
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(ubse::mem::controller::agent::UbseMemShareBorrow(req, resp), UBSE_ERROR_NULLPTR);

    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::UbseMemShareBorrowReqSimpoPtr, UbseBaseMessagePtr>;
    std::chrono::seconds timeout(1);
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&GetWaitTimeout).stubs().will(returnValue(timeout));
    bool (task_executor::UbseTaskExecutor::*func)(const std::function<void()> &task) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(ubse::mem::controller::agent::UbseMemShareBorrow(req, resp), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, UbseMemShareAttach)
{
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetMasterInfo)
        .stubs()
        .with(outBound(masterInfo))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    UbseMemShareAttachReq req{};
    UbseMemOperationResp resp{};
    EXPECT_EQ(ubse::mem::controller::agent::UbseMemShareAttach(req, resp), UBSE_ERROR);
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(ubse::mem::controller::agent::UbseMemShareAttach(req, resp), UBSE_ERROR_NULLPTR);

    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::UbseMemShareAttachReqSimpoPtr, UbseBaseMessagePtr>;
    std::chrono::seconds timeout(1);
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&GetWaitTimeout).stubs().will(returnValue(timeout));
    bool (task_executor::UbseTaskExecutor::*func)(const std::function<void()> &task) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(ubse::mem::controller::agent::UbseMemShareAttach(req, resp), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, UbseMemShareDetach)
{
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetMasterInfo)
        .stubs()
        .with(outBound(masterInfo))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    UbseMemShareDetachReq req{};
    UbseMemOperationResp resp{};
    EXPECT_EQ(ubse::mem::controller::agent::UbseMemShareDetach(req, resp), UBSE_ERROR);
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(ubse::mem::controller::agent::UbseMemShareDetach(req, resp), UBSE_ERROR_NULLPTR);

    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::UbseMemShareDetachReqSimpoPtr, UbseBaseMessagePtr>;
    std::chrono::seconds timeout(1);
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&GetWaitTimeout).stubs().will(returnValue(timeout));
    bool (task_executor::UbseTaskExecutor::*func)(const std::function<void()> &task) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(ubse::mem::controller::agent::UbseMemShareDetach(req, resp), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, FillLinkInfo_Success)
{
    std::vector<std::string> link = {"1", "2", "3"};
    UbseMemNumaBorrowReq numaBorrowReq{};
    auto ret = FillLinkInfo(link, numaBorrowReq);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(numaBorrowReq.linkInfo.lenderNode, "1");
    EXPECT_EQ(numaBorrowReq.linkInfo.lenderSocketId, 2);
    EXPECT_EQ(numaBorrowReq.linkInfo.lenderPort, 3);
}

TEST_F(TestUbseMemControllerApiAgent, FillLinkInfo_InvalidSocketId)
{
    std::vector<std::string> link = {"1", "invalid", "3"};
    UbseMemNumaBorrowReq numaBorrowReq{};
    auto ret = FillLinkInfo(link, numaBorrowReq);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApiAgent, FillLinkInfo_InvalidPort)
{
    std::vector<std::string> link = {"1", "2", "invalid"};
    UbseMemNumaBorrowReq numaBorrowReq{};
    auto ret = FillLinkInfo(link, numaBorrowReq);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApiAgent, IsSocketExist_Found)
{
    uint32_t socketId = 36;
    ubse::nodeController::UbseNodeInfo nodeInfo{};
    UbseCpuLocation location{.nodeId = "1", .chipId = 1};
    UbseCpuInfo cpuInfo;
    cpuInfo.socketId = socketId;
    nodeInfo.cpuInfos[location] = cpuInfo;
    UbseCpuLocation resultLocation{};
    EXPECT_TRUE(IsSocketExist(socketId, nodeInfo, resultLocation));
    EXPECT_EQ(resultLocation.nodeId, "1");
    EXPECT_EQ(resultLocation.chipId, 1);
}

TEST_F(TestUbseMemControllerApiAgent, IsSocketExist_NotFound)
{
    uint32_t socketId = 99;
    ubse::nodeController::UbseNodeInfo nodeInfo{};
    UbseCpuLocation location{.nodeId = "1", .chipId = 1};
    UbseCpuInfo cpuInfo;
    cpuInfo.socketId = 36;
    nodeInfo.cpuInfos[location] = cpuInfo;
    UbseCpuLocation resultLocation{};
    EXPECT_FALSE(IsSocketExist(socketId, nodeInfo, resultLocation));
}

TEST_F(TestUbseMemControllerApiAgent, CheckRemoteExist_Success)
{
    UbsePortInfo portInfo;
    portInfo.remoteChipId = "1";
    portInfo.remoteSlotId = "1";
    std::vector<std::string> secondLink = {"1", "36", "1"};

    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos;
    ubse::nodeController::UbseNodeInfo remoteNode;
    remoteNode.nodeId = "2";
    remoteNode.slotId = 1;
    UbseCpuLocation remoteLocation{.nodeId = "1", .chipId = 1};
    UbseCpuInfo remoteCpuInfo;
    remoteCpuInfo.socketId = 36;
    remoteNode.cpuInfos[remoteLocation] = remoteCpuInfo;
    nodeInfos["2"] = remoteNode;

    auto ret = CheckRemoteExist(portInfo, secondLink, nodeInfos);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApiAgent, CheckRemoteExist_InvalidChipId)
{
    UbsePortInfo portInfo;
    portInfo.remoteChipId = "invalid";
    portInfo.remoteSlotId = "1";
    std::vector<std::string> secondLink = {"1", "36", "1"};

    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos;
    auto ret = CheckRemoteExist(portInfo, secondLink, nodeInfos);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, CheckRemoteExist_WrongSecondLinkSize)
{
    UbsePortInfo portInfo;
    portInfo.remoteChipId = "1";
    portInfo.remoteSlotId = "1";
    std::vector<std::string> secondLink = {"1", "36"};

    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos;
    ubse::nodeController::UbseNodeInfo remoteNode;
    remoteNode.nodeId = "2";
    remoteNode.slotId = 1;
    UbseCpuLocation remoteLocation{.nodeId = "1", .chipId = 1};
    UbseCpuInfo remoteCpuInfo;
    remoteCpuInfo.socketId = 36;
    remoteNode.cpuInfos[remoteLocation] = remoteCpuInfo;
    nodeInfos["2"] = remoteNode;

    auto ret = CheckRemoteExist(portInfo, secondLink, nodeInfos);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, CheckRemoteExist_SocketMismatch)
{
    UbsePortInfo portInfo;
    portInfo.remoteChipId = "1";
    portInfo.remoteSlotId = "1";
    std::vector<std::string> secondLink = {"1", "99", "1"};

    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos;
    ubse::nodeController::UbseNodeInfo remoteNode;
    remoteNode.nodeId = "2";
    remoteNode.slotId = 1;
    UbseCpuLocation remoteLocation{.nodeId = "1", .chipId = 1};
    UbseCpuInfo remoteCpuInfo;
    remoteCpuInfo.socketId = 36;
    remoteNode.cpuInfos[remoteLocation] = remoteCpuInfo;
    nodeInfos["2"] = remoteNode;

    auto ret = CheckRemoteExist(portInfo, secondLink, nodeInfos);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, CheckLinkExist_NodeNotFound)
{
    std::vector<std::string> firstLink = {"99", "36", "0"};
    std::vector<std::string> secondLink = {"2", "36", "1"};

    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos;
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfos));
    auto ret = CheckLinkExist(firstLink, secondLink);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, CheckLinkExist_SocketNotFound)
{
    std::vector<std::string> firstLink = {"1", "99", "0"};
    std::vector<std::string> secondLink = {"2", "36", "1"};

    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos;
    ubse::nodeController::UbseNodeInfo node1;
    node1.nodeId = "node1";
    node1.slotId = 1;
    UbseCpuLocation location1{.nodeId = "node1", .chipId = 1};
    UbseCpuInfo cpuInfo1;
    cpuInfo1.socketId = 36;
    node1.cpuInfos[location1] = cpuInfo1;
    nodeInfos["node1"] = node1;

    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfos));
    auto ret = CheckLinkExist(firstLink, secondLink);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, CheckLinkExist_PortNotFound)
{
    std::vector<std::string> firstLink = {"1", "36", "99"};
    std::vector<std::string> secondLink = {"2", "36", "1"};

    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos;
    ubse::nodeController::UbseNodeInfo node1;
    node1.nodeId = "node1";
    node1.slotId = 1;
    UbseCpuLocation location1{.nodeId = "node1", .chipId = 1};
    UbseCpuInfo cpuInfo1;
    cpuInfo1.socketId = 36;
    node1.cpuInfos[location1] = cpuInfo1;
    nodeInfos["node1"] = node1;

    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfos));
    auto ret = CheckLinkExist(firstLink, secondLink);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, CheckLinkExist_PortDown)
{
    std::vector<std::string> firstLink = {"1", "36", "0"};
    std::vector<std::string> secondLink = {"2", "36", "1"};

    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos;
    ubse::nodeController::UbseNodeInfo node1;
    node1.nodeId = "node1";
    node1.slotId = 1;
    UbseCpuLocation location1{.nodeId = "node1", .chipId = 1};
    UbseCpuInfo cpuInfo1;
    cpuInfo1.socketId = 36;
    UbsePortInfo portInfo1;
    portInfo1.portId = "0";
    portInfo1.portStatus = PortStatus::DOWN;
    cpuInfo1.portInfos["0"] = portInfo1;
    node1.cpuInfos[location1] = cpuInfo1;
    nodeInfos["node1"] = node1;

    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfos));
    auto ret = CheckLinkExist(firstLink, secondLink);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, DealLinkInfo_InvalidFormat)
{
    std::string linkInfo = "invalid";
    UbseMemNumaBorrowReq numaBorrowReq{};
    UbseRoleInfo currentNodeInfo;
    currentNodeInfo.nodeId = "1";
    std::string errorMsg;
    auto ret = DealLinkInfo(linkInfo, numaBorrowReq, currentNodeInfo, errorMsg);
    EXPECT_EQ(ret, UBSE_ERROR);
    EXPECT_NE(errorMsg.find("Invalid"), std::string::npos);
}

TEST_F(TestUbseMemControllerApiAgent, DealLinkInfo_WrongSplitSize)
{
    std::string linkInfo = "0/0/1-1/0";
    UbseMemNumaBorrowReq numaBorrowReq{};
    UbseRoleInfo currentNodeInfo;
    currentNodeInfo.nodeId = "1";
    std::string errorMsg;
    auto ret = DealLinkInfo(linkInfo, numaBorrowReq, currentNodeInfo, errorMsg);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, DealLinkInfo_LinkNotExist)
{
    std::string linkInfo = "0/0/1-1/0/1";
    UbseMemNumaBorrowReq numaBorrowReq{};
    UbseRoleInfo currentNodeInfo;
    currentNodeInfo.nodeId = "1";
    std::string errorMsg;

    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos;
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfos));
    MOCKER(CheckLinkExist).stubs().will(returnValue(UBSE_ERROR));

    auto ret = DealLinkInfo(linkInfo, numaBorrowReq, currentNodeInfo, errorMsg);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, DealLinkInfo_NodeNotMatched)
{
    std::string linkInfo = "0/36/0-1/36/1";
    UbseMemNumaBorrowReq numaBorrowReq{};
    UbseRoleInfo currentNodeInfo;
    currentNodeInfo.nodeId = "99";
    std::string errorMsg;

    MOCKER(&UbseNodeController::GetAllNodes).reset();
    MOCKER(CheckLinkExist).reset();
    MOCKER(CheckLinkExist).stubs().will(returnValue(UBSE_OK));

    auto ret = DealLinkInfo(linkInfo, numaBorrowReq, currentNodeInfo, errorMsg);
    EXPECT_EQ(ret, UBSE_ERROR);
    EXPECT_NE(errorMsg.find("not matched"), std::string::npos);
}

TEST_F(TestUbseMemControllerApiAgent, FillSrcNuma_Success)
{
    UbseMemNumaBorrowReq numaBorrowReq{};
    numaBorrowReq.srcSocket = 36;
    UbseRoleInfo currentNodeInfo;
    currentNodeInfo.nodeId = "1";

    ubse::nodeController::UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "1";
    ubse::nodeController::UbseNumaLocation numaLocation{.numaId = 0};
    ubse::nodeController::UbseNumaInfo numaInfo;
    numaInfo.socketId = 36;
    nodeInfo.numaInfos[numaLocation] = numaInfo;

    MOCKER(&UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));
    auto ret = FillSrcNuma(numaBorrowReq, currentNodeInfo);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(numaBorrowReq.srcNuma, 0);
}

TEST_F(TestUbseMemControllerApiAgent, FillSrcNuma_NodeNotFound)
{
    UbseMemNumaBorrowReq numaBorrowReq{};
    numaBorrowReq.srcSocket = 36;
    UbseRoleInfo currentNodeInfo;
    currentNodeInfo.nodeId = "1";

    ubse::nodeController::UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "";

    MOCKER(&UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));
    auto ret = FillSrcNuma(numaBorrowReq, currentNodeInfo);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, FillSrcNuma_NumaNotFound)
{
    UbseMemNumaBorrowReq numaBorrowReq{};
    numaBorrowReq.srcSocket = 99;
    UbseRoleInfo currentNodeInfo;
    currentNodeInfo.nodeId = "1";

    ubse::nodeController::UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "1";
    ubse::nodeController::UbseNumaLocation numaLocation{.numaId = 0};
    ubse::nodeController::UbseNumaInfo numaInfo;
    numaInfo.socketId = 36;
    nodeInfo.numaInfos[numaLocation] = numaInfo;

    MOCKER(&UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));
    auto ret = FillSrcNuma(numaBorrowReq, currentNodeInfo);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, RegSpecifyLinkInfoCreateHandler_ModuleNull)
{
    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(nullModule));
    EXPECT_NO_THROW(RegSpecifyLinkInfoCreateHandler());
}

TEST_F(TestUbseMemControllerApiAgent, RegSpecifyLinkInfoCreateHandler_RegisterFail)
{
    std::shared_ptr<UbseApiServerModule> apiModule = std::make_shared<UbseApiServerModule>();
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>).reset();
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiModule));
    MOCKER(&UbseApiServerModule::RegisterIpcHandler).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NO_THROW(RegSpecifyLinkInfoCreateHandler());
}

TEST_F(TestUbseMemControllerApiAgent, RegSpecifyLinkInfoCreateHandler_Success)
{
    std::shared_ptr<UbseApiServerModule> apiModule = std::make_shared<UbseApiServerModule>();
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>).reset();
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiModule));
    MOCKER(&UbseApiServerModule::RegisterIpcHandler).reset();
    MOCKER(&UbseApiServerModule::RegisterIpcHandler).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(RegSpecifyLinkInfoCreateHandler());
}

TEST_F(TestUbseMemControllerApiAgent, GetWaitTimeout_ReturnsValue)
{
    std::chrono::seconds timeout(60);
    EXPECT_NO_THROW(GetWaitTimeout());
}

TEST_F(TestUbseMemControllerApiAgent, SwitchReturnType_FdReturn)
{
    SendParam sendParam("1", 1, 1);
    SwitchReturnType(sendParam, MemOperationType::FD_RETURN);
    EXPECT_EQ(sendParam.GetOpCode(), static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_FD_RETURN));
}

TEST_F(TestUbseMemControllerApiAgent, SwitchReturnType_NumaReturn)
{
    SendParam sendParam("1", 1, 1);
    SwitchReturnType(sendParam, MemOperationType::NUMA_RETURN);
    EXPECT_EQ(sendParam.GetOpCode(), static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_NUMA_RETURN));
}

TEST_F(TestUbseMemControllerApiAgent, SwitchReturnType_SharedReturn)
{
    SendParam sendParam("1", 1, 1);
    SwitchReturnType(sendParam, MemOperationType::SHARED_RETURN);
    EXPECT_EQ(sendParam.GetOpCode(), static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_SHARE_RETURN));
}

TEST_F(TestUbseMemControllerApiAgent, SwitchReturnType_AddrReturn)
{
    SendParam sendParam("1", 1, 1);
    SwitchReturnType(sendParam, MemOperationType::ADDR_RETURN);
    EXPECT_EQ(sendParam.GetOpCode(), static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_ADDR_RETURN));
}

TEST_F(TestUbseMemControllerApiAgent, SwitchReturnType_Default)
{
    SendParam sendParam("1", 1, 1);
    SwitchReturnType(sendParam, static_cast<MemOperationType>(99));
    EXPECT_EQ(sendParam.GetOpCode(), static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_NUMA_RETURN));
}


TEST_F(TestUbseMemControllerApiAgent, UbseMemFdBorrow_SignFail)
{
    UbseMemFdBorrowReq req{};
    req.name = "test_name";
    req.requestNodeId = "1";
    req.importNodeId = "2";
    req.size = 1024;
    UbseMemOperationResp resp{};

    MOCKER(IsHighSafety).stubs().will(returnValue(true));
    MOCKER(&mem::controller::UbseMemSignVerifier::Sign).stubs().will(returnValue(UBSE_ERROR));

    auto ret = UbseMemFdBorrow(req, resp);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApiAgent, UbseMemFdBorrow_FutureMgrNull)
{
    UbseMemFdBorrowReq req{};
    req.name = "test_name";
    req.requestNodeId = "1";
    req.importNodeId = "2";
    req.size = 1024;
    UbseMemOperationResp resp{};

    MOCKER(IsHighSafety).reset();
    MOCKER(IsHighSafety).stubs().will(returnValue(false));
    std::shared_ptr<UbseFutureMgr> nullFutureMgr = nullptr;
    MOCKER(&UbseFutureMgr::CreateInstance).stubs().will(returnValue(nullFutureMgr));

    auto ret = UbseMemFdBorrow(req, resp);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, DeleteMemoryHandlerSuccess)
{
    // 准备测试数据
    UbseIpcMessage request{};
    UbseRequestContext context{};
    context.requestId = 12345;

    // 构造有效的请求数据
    UbseSerialization serialization;
    serialization << "valid_name" << "NUMA_RETURN";
    request.buffer = serialization.GetBuffer();
    request.length = serialization.GetLength();

    // 模拟依赖项
    UbseRoleInfo currentNodeInfo;
    currentNodeInfo.nodeId = "1";
    MOCKER(&UbseGetCurrentNodeInfo)
        .stubs().with(outBound(currentNodeInfo)).will(returnValue(UBSE_OK));

    // 模拟内存返回成功
    UbseMemOperationResp mockResp{};
    mockResp.errorCode = UBSE_OK;
    mockResp.errMsg = "Success";
    MOCKER(UbseMemReturn).stubs().will(returnValue(UBSE_OK));

    // 模拟API模块
    std::shared_ptr<UbseApiServerModule> apiModule = std::make_shared<UbseApiServerModule>();
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiModule));
    MOCKER(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));

    // 执行测试
    EXPECT_EQ(DeleteMemoryHandler(request, context), UBSE_OK);
}

TEST_F(TestUbseMemControllerApiAgent, DeleteMemoryHandlerDeserializationFail)
{
    // 准备测试数据
    UbseIpcMessage request{};
    UbseRequestContext context{};

    // 构造无效的请求数据
    request.buffer = nullptr;
    request.length = 0;

    // 执行测试
    EXPECT_EQ(DeleteMemoryHandler(request, context), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, DeleteMemoryHandlerGetCurrentNodeFail)
{
    // 准备测试数据
    UbseIpcMessage request{};
    UbseRequestContext context{};

    // 构造有效的请求数据
    UbseSerialization serialization;
    serialization << "valid_name" << "NUMA_RETURN";
    request.buffer = serialization.GetBuffer();
    request.length = serialization.GetLength();

    // 模拟获取当前节点信息失败
    MOCKER(&UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));

    // 执行测试
    EXPECT_EQ(DeleteMemoryHandler(request, context), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemControllerApiAgent, DeleteMemoryHandlerMemoryReturnFail)
{
    // 准备测试数据
    UbseIpcMessage request{};
    UbseRequestContext context{};

    // 构造有效的请求数据
    UbseSerialization serialization;
    serialization << "valid_name" << "NUMA_RETURN";
    request.buffer = serialization.GetBuffer();
    request.length = serialization.GetLength();

    // 模拟依赖项
    UbseRoleInfo currentNodeInfo;
    currentNodeInfo.nodeId = "1";
    MOCKER(&UbseGetCurrentNodeInfo)
        .stubs().with(outBound(currentNodeInfo)).will(returnValue(UBSE_OK));

    // 模拟内存返回失败
    MOCKER(UbseMemReturn).stubs().will(returnValue(UBSE_ERROR));

    // 执行测试
    EXPECT_EQ(DeleteMemoryHandler(request, context), UBSE_ERROR);
}
}
