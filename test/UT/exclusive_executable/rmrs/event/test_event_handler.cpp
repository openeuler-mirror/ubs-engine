/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mockcpp/mokc.h>
#include <iostream>

#include "ubse_storage.h"
#include "event_handler.h"
#include "fault_memid_helper.h"
#include "fault_node_module.h"
#include "mem_manager.h"
#include "mp_error.h"
#include "over_commit_fault_node_module.h"
#include "smap_query_process_send.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling::event::handler {

using namespace std;
using namespace ubse::ras;
using namespace ubse::storage;
// 测试类
class TestEventHandler : public ::testing::Test {
public:
    void SetUp() override
    {
        cout << "[TestEventHandler SetUp Begin]" << endl;
        cout << "[TestEventHandler SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestEventHandler TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestEventHandler TearDown End]" << endl;
    }
};

TEST_F(TestEventHandler, HandleAlarmUceEventFailed)
{
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({12345})";
    MpResult res = EventHandler::HandleAlarmUceEvent(eventId, eventMessage);
    ASSERT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestEventHandler, HandleAlarmUceEventFailed1)
{
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"import_nodeid": "Node1"})";
    MpResult res = EventHandler::HandleAlarmUceEvent(eventId, eventMessage);
    ASSERT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestEventHandler, HandleAlarmUceEventFailed2)
{
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"import_nodeid":"Node1","importMemID":12345})";
    MpResult res = EventHandler::HandleAlarmUceEvent(eventId, eventMessage);
    ASSERT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestEventHandler, HandleAlarmRebootEventFailed)
{
    MOCKER_CPP(&FaultNodeModule::DetermineNodeType, MpResult(*)(const std::string nodeId, NodeType& nodeType))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({})";
    MpResult res = EventHandler::HandleAlarmRebootEvent(eventId, eventMessage);
    ASSERT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestEventHandler, HandleAlarmRebootEventFailed1)
{
    MOCKER_CPP(&FaultNodeModule::DetermineNodeType, MpResult(*)(const std::string nodeId, NodeType& nodeType))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({})";
    MpResult res = EventHandler::HandleAlarmRebootEvent(eventId, eventMessage);
    ASSERT_EQ(res, MEM_POOLING_ERROR);
}

MpResult TestDetermineNodeType(const std::string nodeId, NodeType& nodeType)
{
    nodeType = NodeType::BORROW_IN;
    return MEM_POOLING_OK;
}

TEST_F(TestEventHandler, HandleAlarmUceEventCheckModeFailed)
{
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"Node1","importMemID":1})";
    MpResult ret = EventHandler::HandleAlarmUceEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

uint32_t RackStorageQueryDataReturnOverCommit(const std::string& keyPrefix, const std::string& key, void* ctx,
                                              ubse::storage::UbseStorageDealDataFunc func)
{
    auto ptr = static_cast<int*>(ctx);
    *ptr = 0;
    return 0;
}
uint32_t RackStorageQueryDataReturnMemFragment(const std::string& keyPrefix, const std::string& key, void* ctx,
                                               ubse::storage::UbseStorageDealDataFunc func)
{
    auto ptr = static_cast<int*>(ctx);
    *ptr = 1;
    return 0;
}

TEST_F(TestEventHandler, HandleAlarmUceEventOverCommitBranchFaultMemIdManageFailed)
{
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"Node1","importMemID":1})";
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, ubse::storage::UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(RackStorageQueryDataReturnOverCommit));
    MpResult ret = EventHandler::HandleAlarmUceEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestEventHandler, HandleAlarmUceEventMemFragmentBranchFaultMemIdManageFailed)
{
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"Node1","importMemID":1})";
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, ubse::storage::UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(RackStorageQueryDataReturnMemFragment));
    MpResult ret = EventHandler::HandleAlarmUceEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

MpResult DetermineNodeTypeMockBorrowIn(FaultNodeModule& thisPtr, const std::string nodeId, NodeType& nodeType)
{
    nodeType = NodeType::BORROW_IN;
    return MEM_POOLING_OK;
}

MpResult DetermineNodeTypeMockBorrowOut(FaultNodeModule& thisPtr, const std::string nodeId, NodeType& nodeType)
{
    nodeType = NodeType::BORROW_OUT;
    return MEM_POOLING_OK;
}

MpResult DetermineNodeTypeOverCommitMockBorrowIn(FaultNodeModule* thisPtr, const std::string nodeId, NodeType& nodeType)
{
    nodeType = NodeType::BORROW_IN;
    return MEM_POOLING_OK;
}

MpResult DetermineNodeTypeOverCommitMockBorrowOut(FaultNodeModule* thisPtr, const std::string nodeId,
                                                  NodeType& nodeType)
{
    nodeType = NodeType::BORROW_OUT;
    return MEM_POOLING_OK;
}

TEST_F(TestEventHandler, HandleAlarmRebootEventBorrowOutOverCommitSuccess)
{
    ALARM_FAULT_TYPE eventId = 0;
    std::string nodeId = "node1";
    std::string eventMessage = R"({"importNodeID":"Node1","importMemID":1})";
    MOCKER_CPP(&FaultNodeModule::DetermineNodeTypeOverCommit,
               MpResult(*)(FaultNodeModule * thisPtr, const std::string nodeId, NodeType& nodeType))
        .stubs()
        .will(invoke(DetermineNodeTypeOverCommitMockBorrowOut));
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, ubse::storage::UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(RackStorageQueryDataReturnOverCommit));
    MOCKER_CPP(&OverCommitFaultNodeModule::ProcessBorrowOutNodeFault, MpResult(*)(const std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MpResult ret = EventHandler::HandleAlarmRebootEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestEventHandler, HandlePanicEventDetermineNodeTypeFailed)
{
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"Node1","importMemID":1})";
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, ubse::storage::UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(RackStorageQueryDataReturnOverCommit));
    MOCKER_CPP(&FaultNodeModule::DetermineNodeTypeOverCommit,
               MpResult(*)(FaultNodeModule & thisPtr, const std::string nodeId, NodeType& nodeType))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MpResult ret = EventHandler::HandlePanicEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestEventHandler, HandlePanicEventBorrowOutOverCommitFailed)
{
    ALARM_FAULT_TYPE eventId = 0;
    std::string nodeId = "node1";
    std::string eventMessage = R"({"importNodeID":"Node1","importMemID":1})";
    MOCKER_CPP(&FaultNodeModule::DetermineNodeTypeOverCommit,
               MpResult(*)(FaultNodeModule * thisPtr, const std::string nodeId, NodeType& nodeType))
        .stubs()
        .will(invoke(DetermineNodeTypeOverCommitMockBorrowOut));
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, ubse::storage::UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(RackStorageQueryDataReturnOverCommit));
    MpResult ret = EventHandler::HandlePanicEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestEventHandler, HandleAlarmKernelRebootEventDetermineNodeTypeFailed)
{
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"Node1","importMemID":1})";
    MpResult ret = EventHandler::HandleAlarmKernelRebootEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestEventHandler, HandleAlarmKernelRebootEventBorrowOutOverCommitFailed)
{
    ALARM_FAULT_TYPE eventId = 0;
    std::string nodeId = "node1";
    std::string eventMessage = R"({"importNodeID":"Node1","importMemID":1})";
    MOCKER_CPP(&FaultNodeModule::DetermineNodeTypeOverCommit,
               MpResult(*)(FaultNodeModule * thisPtr, const std::string nodeId, NodeType& nodeType))
        .stubs()
        .will(invoke(DetermineNodeTypeOverCommitMockBorrowOut));
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, ubse::storage::UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(RackStorageQueryDataReturnOverCommit));
    MpResult ret = EventHandler::HandleAlarmKernelRebootEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

uint32_t SetVirtualScene(const std::string& keyPrefix, const std::string& key, void* ctx, UbseStorageDealDataFunc func)
{
    *static_cast<int*>(ctx) = 1;
    return MEM_POOLING_OK;
}

uint32_t SetOverCommitScene(const std::string& keyPrefix, const std::string& key, void* ctx,
                            UbseStorageDealDataFunc func)
{
    *static_cast<int*>(ctx) = 0;
    return MEM_POOLING_OK;
}

MpResult setNodeTypeIn(FaultNodeModule* obj, const std::string nodeId, NodeType& nodeType)
{
    nodeType = NodeType::BORROW_IN;
    return MEM_POOLING_OK;
}

MpResult setNodeTypeOut(FaultNodeModule* obj, const std::string nodeId, NodeType& nodeType)
{
    nodeType = NodeType::BORROW_OUT;
    return MEM_POOLING_OK;
}

TEST_F(TestEventHandler, CheckModeFailure)
{
    MOCKER_CPP(&MpConfiguration::GetSceneType, MpSceneType(*)()).stubs().will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, UbseStorageDealDataFunc func))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"1","importMemID":1})";
    MpResult ret = EventHandler::HandlePanicEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestEventHandler, PanicVirtualSceneBorrowInSuccess)
{
    MOCKER_CPP(&MpConfiguration::GetSceneType, MpSceneType(*)()).stubs().will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(SetVirtualScene));
    MOCKER_CPP(&mempooling::FaultNodeModule::FragmentHandleFault,
               MpResult(*)(FaultNodeModule * This, const std::string))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"3","importMemID":3})";
    MpResult ret = EventHandler::HandlePanicEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestEventHandler, PanicVirtualSceneBorrowOutFailed)
{
    MOCKER_CPP(&MpConfiguration::GetSceneType, MpSceneType(*)()).stubs().will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(SetVirtualScene));
    MOCKER_CPP(&mempooling::FaultNodeModule::DetermineNodeType,
               MpResult(*)(FaultNodeModule * This, const std::string, NodeType&))
        .stubs()
        .will(invoke(setNodeTypeOut));
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"4","importMemID":4})";
    MpResult ret = EventHandler::HandlePanicEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestEventHandler, PanicOverCommitSceneBorrowInSuccess)
{
    MOCKER_CPP(&MpConfiguration::GetSceneType, MpSceneType(*)()).stubs().will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(SetOverCommitScene));
    MOCKER_CPP(&mempooling::FaultNodeModule::DetermineNodeTypeOverCommit,
               MpResult(*)(FaultNodeModule * This, const std::string, NodeType&))
        .stubs()
        .will(invoke(setNodeTypeIn));
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"5","importMemID":5})";
    MpResult ret = EventHandler::HandlePanicEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestEventHandler, PanicOverCommitSceneBorrowOutSuccess)
{
    MOCKER_CPP(&MpConfiguration::GetSceneType, MpSceneType(*)()).stubs().will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(SetOverCommitScene));
    MOCKER_CPP(&mempooling::FaultNodeModule::DetermineNodeTypeOverCommit,
               MpResult(*)(FaultNodeModule * This, const std::string, NodeType&))
        .stubs()
        .will(invoke(setNodeTypeOut));
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"6","importMemID":6})";
    MpResult ret = EventHandler::HandlePanicEvent(eventId, eventMessage);
    ASSERT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestEventHandler, KernelRebootVirtualSceneBorrowInSuccess)
{
    MOCKER_CPP(&MpConfiguration::GetSceneType, MpSceneType(*)()).stubs().will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(SetVirtualScene));
    MOCKER_CPP(&mempooling::FaultNodeModule::FragmentHandleFault,
               MpResult(*)(FaultNodeModule * This, const std::string))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"7","importMemID":7})";
    MpResult ret = EventHandler::HandleAlarmKernelRebootEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestEventHandler, KernelRebootVirtualSceneBorrowOutFailed)
{
    MOCKER_CPP(&MpConfiguration::GetSceneType, MpSceneType(*)()).stubs().will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(SetVirtualScene));
    MOCKER_CPP(&mempooling::FaultNodeModule::DetermineNodeType,
               MpResult(*)(FaultNodeModule * This, const std::string, NodeType&))
        .stubs()
        .will(invoke(setNodeTypeOut));
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"8","importMemID":8})";
    MpResult ret = EventHandler::HandleAlarmKernelRebootEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestEventHandler, KernelRebootOverCommitSceneBorrowInSuccess)
{
    MOCKER_CPP(&MpConfiguration::GetSceneType, MpSceneType(*)()).stubs().will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(SetOverCommitScene));
    MOCKER_CPP(&mempooling::FaultNodeModule::DetermineNodeTypeOverCommit,
               MpResult(*)(FaultNodeModule * This, const std::string, NodeType&))
        .stubs()
        .will(invoke(setNodeTypeIn));
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"9","importMemID":9})";
    MpResult ret = EventHandler::HandleAlarmKernelRebootEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestEventHandler, KernelRebootOverCommitSceneBorrowOutSuccess)
{
    MOCKER_CPP(&MpConfiguration::GetSceneType, MpSceneType(*)()).stubs().will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(SetOverCommitScene));
    MOCKER_CPP(&mempooling::FaultNodeModule::DetermineNodeTypeOverCommit,
               MpResult(*)(FaultNodeModule * This, const std::string, NodeType&))
        .stubs()
        .will(invoke(setNodeTypeOut));
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"1","importMemID":1})";
    MpResult ret = EventHandler::HandleAlarmKernelRebootEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestEventHandler, RebootVirtualSceneBorrowInSuccess)
{
    MOCKER_CPP(&MpConfiguration::GetSceneType, MpSceneType(*)()).stubs().will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(SetVirtualScene));
    MOCKER_CPP(&mempooling::FaultNodeModule::FragmentHandleFault,
               MpResult(*)(FaultNodeModule * This, const std::string))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"2","importMemID":2})";
    MpResult ret = EventHandler::HandleAlarmRebootEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestEventHandler, RebootVirtualSceneBorrowOutFailed)
{
    MOCKER_CPP(&MpConfiguration::GetSceneType, MpSceneType(*)()).stubs().will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(SetVirtualScene));
    MOCKER_CPP(&mempooling::FaultNodeModule::DetermineNodeType,
               MpResult(*)(FaultNodeModule * This, const std::string, NodeType&))
        .stubs()
        .will(invoke(setNodeTypeOut));
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"3","importMemID":3})";
    MpResult ret = EventHandler::HandleAlarmRebootEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestEventHandler, RebootOverCommitSceneBorrowInSuccess)
{
    MOCKER_CPP(&MpConfiguration::GetSceneType, MpSceneType(*)()).stubs().will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(SetOverCommitScene));
    MOCKER_CPP(&mempooling::FaultNodeModule::DetermineNodeTypeOverCommit,
               MpResult(*)(FaultNodeModule * This, const std::string, NodeType&))
        .stubs()
        .will(invoke(setNodeTypeIn));
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"4","importMemID":4})";
    MpResult ret = EventHandler::HandleAlarmRebootEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestEventHandler, RebootOverCommitSceneBorrowOutSuccess)
{
    MOCKER_CPP(&MpConfiguration::GetSceneType, MpSceneType(*)()).stubs().will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(SetOverCommitScene));
    MOCKER_CPP(&mempooling::FaultNodeModule::DetermineNodeTypeOverCommit,
               MpResult(*)(FaultNodeModule * This, const std::string, NodeType&))
        .stubs()
        .will(invoke(setNodeTypeOut));
    MOCKER_CPP(&mempooling::OverCommitFaultNodeModule::ProcessBorrowOutNodeFault, MpResult(*)(const std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    ALARM_FAULT_TYPE eventId = 0;
    std::string eventMessage = R"({"importNodeID":"5","importMemID":5})";
    MpResult ret = EventHandler::HandleAlarmRebootEvent(eventId, eventMessage);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

} // namespace mempooling::event::handler
