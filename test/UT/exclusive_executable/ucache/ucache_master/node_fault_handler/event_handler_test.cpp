/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#define private public
#include "event_handler.h"

#include <chrono>
#include <thread>
#include "ubse_logger.h"
#include "data_collect.h"
#include "ucache_config.h"
#include "ucache_error.h"
#include "ucache_master.h"

using namespace std;
using namespace ucache;
using namespace ubse::log;
using namespace ucache::data_collect;
using namespace ucache::fault_handler;

namespace ucache::fault_handler {
static void GetMemIdsFromNumaMemMap(std::map<int, std::map<std::string, uint64_t>>& numaMemMap,
                                    std::vector<std::string>& memNameList);
static void GetMemIdFromLendMap(const std::string& nodeId,
                                std::map<std::string, std::vector<NodeMemBorrowInfo>>& lendMap,
                                std::vector<std::string>& memNameList);
static void GetMemIdFromBorrowMap(const std::string& nodeId,
                                  std::map<std::string, std::vector<NodeMemBorrowInfo>>& borrowMap,
                                  std::vector<std::string>& memNameList);
static uint32_t ReturnMemofFaultyNode(const std::string& nodeId);
static uint32_t ReturnMemofMemId(const std::string& nodeId, const uint64_t memId, const EventCondition eventCondition);
} // namespace ucache::fault_handler

class EventHandlerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }

    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

TEST_F(EventHandlerTest, GetMemIdsFromNumaMemMapTest)
{
    std::vector<std::string> memNameList{};
    std::map<int, std::map<std::string, uint64_t>> numaMemMap = {
        {0, {{"1111", 65536}, {"1112", 65536}}}, {1, {{"2222", 32768}, {"2223", 32768}}}, {2, {}}};
    ucache::fault_handler::GetMemIdsFromNumaMemMap(numaMemMap, memNameList);
}

TEST_F(EventHandlerTest, GetMemIdFromLendMapTest)
{
    std::string nodeId1 = "Node0";
    std::string nodeId2 = "Node10";
    std::vector<std::string> memNameList{};
    std::map<std::string, std::vector<NodeMemBorrowInfo>> lendMap1 = {
        {"Node0",
         {{.totalSize = 65536,
           .srcNodeId = "Node0",
           .destNodeId = "Node1",
           .numaNodeBorrowSize =
               {
                   {0, {{"0001", 12345}, {"0002", 23456}}},
                   {1, {{"1111", 34567}, {"1112", 45678}}},
               },
           .dstNumaId = 1},
          {.totalSize = 32768,
           .srcNodeId = "Node0",
           .destNodeId = "Node3",
           .numaNodeBorrowSize = {},
           .dstNumaId = 1}}}};
    std::map<std::string, std::vector<NodeMemBorrowInfo>> lendMap2 = {{"Node0", {}}};
    ucache::fault_handler::GetMemIdFromLendMap(nodeId2, lendMap1, memNameList);
    ucache::fault_handler::GetMemIdFromLendMap(nodeId1, lendMap2, memNameList);
    ucache::fault_handler::GetMemIdFromLendMap(nodeId1, lendMap1, memNameList);
}

TEST_F(EventHandlerTest, GetMemIdFromBorrowMapTest)
{
    std::string nodeId1 = "Node0";
    std::string nodeId2 = "Node10";
    std::vector<std::string> memNameList{};
    std::map<std::string, std::vector<NodeMemBorrowInfo>> borrowMap1 = {
        {"Node0",
         {{.totalSize = 65536,
           .srcNodeId = "Node0",
           .destNodeId = "Node1",
           .numaNodeBorrowSize =
               {
                   {0, {{"0001", 12345}, {"0002", 23456}}},
                   {1, {{"1111", 34567}, {"1112", 45678}}},
               },
           .dstNumaId = 1},
          {.totalSize = 32768,
           .srcNodeId = "Node0",
           .destNodeId = "Node3",
           .numaNodeBorrowSize = {},
           .dstNumaId = 1}}}};
    std::map<std::string, std::vector<NodeMemBorrowInfo>> borrowMap2 = {{"Node0", {}}};
    ucache::fault_handler::GetMemIdFromBorrowMap(nodeId2, borrowMap1, memNameList);
    ucache::fault_handler::GetMemIdFromBorrowMap(nodeId1, borrowMap2, memNameList);
    ucache::fault_handler::GetMemIdFromBorrowMap(nodeId1, borrowMap1, memNameList);
}

TEST_F(EventHandlerTest, ReturnMemofMemIdTest)
{
    std::string nodeId = "Node0";
    uint64_t memId = 0;
    MOCKER(Deserialize::GetBorrowMemInfo).stubs().will(returnValue(UCACHE_ERR)).then(returnValue(UCACHE_OK));
    uint32_t ret = ucache::fault_handler::ReturnMemofMemId(nodeId, memId, EventCondition::UCE_MEMID_FAILURE);
    EXPECT_EQ(ret, UCACHE_ERR);
    ret = ucache::fault_handler::ReturnMemofMemId(nodeId, memId, EventCondition::UCE_MEMID_FAILURE);
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(EventHandlerTest, AlarmRebootEventHandlerTest)
{
    ALARM_FAULT_TYPE event = 1001;
    std::string faultInfo = "reboot";
    EventHandler obj;
    MOCKER(ucache::fault_handler::ReturnMemofFaultyNode).stubs().will(returnValue(UCACHE_OK));
    obj.gMasterStopFlag.store(true);
    uint32_t ret = EventHandler::AlarmRebootEventHandler(event, faultInfo);
    EXPECT_EQ(ret, UCACHE_OK);
    EXPECT_EQ(obj.gNodeFaultFlag.load(), false);
    EXPECT_EQ(obj.gMasterStopFlag.load(), false);
}

TEST_F(EventHandlerTest, AlarmPanicEventHandlerTest)
{
    ALARM_FAULT_TYPE event = 1002;
    std::string faultInfo = "panic";
    EventHandler obj;
    MOCKER(ucache::fault_handler::ReturnMemofFaultyNode).stubs().will(returnValue(UCACHE_OK));
    obj.gMasterStopFlag.store(true);
    uint32_t ret = EventHandler::AlarmPanicEventHandler(event, faultInfo);
    EXPECT_EQ(ret, UCACHE_OK);
    EXPECT_EQ(obj.gNodeFaultFlag.load(), false);
    EXPECT_EQ(obj.gMasterStopFlag.load(), false);
}

TEST_F(EventHandlerTest, AlarmKernelRebootEventHandlerTest)
{
    ALARM_FAULT_TYPE event = 1003;
    std::string faultInfo = "kernel reboot";
    EventHandler obj;
    MOCKER(ucache::fault_handler::ReturnMemofFaultyNode).stubs().will(returnValue(UCACHE_OK));
    obj.gMasterStopFlag.store(true);
    uint32_t ret = EventHandler::AlarmKernelRebootEventHandler(event, faultInfo);
    EXPECT_EQ(ret, UCACHE_OK);
    EXPECT_EQ(obj.gNodeFaultFlag.load(), false);
    EXPECT_EQ(obj.gMasterStopFlag.load(), false);
}

TEST_F(EventHandlerTest, AlarmUceEventHandlerTest)
{
    ALARM_FAULT_TYPE event = 1004;
    std::string faultInfo1 = "uce";
    EventHandler obj;
    obj.gMasterStopFlag.store(true);

    uint32_t ret = EventHandler::AlarmUceEventHandler(event, faultInfo1);
    EXPECT_EQ(ret, UCACHE_ERR);
    EXPECT_EQ(obj.gNodeFaultFlag.load(), true);
    EXPECT_EQ(obj.gMasterStopFlag.load(), true);

    std::string faultInfo2 = "{\n"
                             "    \"mem\": \"Node0\",\n"
                             "    \"size\": \"65536\"\n"
                             "}";
    ret = EventHandler::AlarmUceEventHandler(event, faultInfo2);
    EXPECT_EQ(ret, UCACHE_ERR);
    EXPECT_EQ(obj.gNodeFaultFlag.load(), true);
    EXPECT_EQ(obj.gMasterStopFlag.load(), true);

    std::string faultInfo3 = "{\n"
                             "    \"importNodeID\": \"Node0\",\n"
                             "    \"importMemID\": \"65536\"\n"
                             "}";
    ret = EventHandler::AlarmUceEventHandler(event, faultInfo3);
    EXPECT_EQ(ret, UCACHE_ERR);
    EXPECT_EQ(obj.gNodeFaultFlag.load(), true);
    EXPECT_EQ(obj.gMasterStopFlag.load(), true);
}
