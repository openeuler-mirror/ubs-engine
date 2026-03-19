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

#include <vector>
#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include "securec.h"
#include "ucache_error.h"
#include "ucache_config.h"
#include "borrow_action.h"
#include "ubse_error.h"
#include "mem_borrow.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace std;
using namespace ucache;
using namespace ucache::mem_borrow;
using namespace ucache::borrow_action;
static std::map<std::string, std::vector<std::string>> physicalTopo = {
    {"Node0", {"Node1"}},
    {"Node1", {"Node0"}},
};

class BorrowActionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        MemBorrowTopo::SetTopology(physicalTopo);
        MemBorrowTopo::globalMemBorrowTopo.lendMap = {
            {"Node1", {}},
            {"Node2", {}}
        };

        MemBorrowTopo::globalMemBorrowTopo.borrowMap = {
            {"Node0", {}},
            {"Node1", {}}
        };

        MemBorrowTopo::globalMemBorrowTopo.borrowMemInfoPerNodeMap = {
            {
                "Node0",
                {
                    .numaNodeSize = {
                        {0, 5 * granularity},
                        {1, 5 * granularity},
                        {2, 5 * granularity},
                        {3, 5 * granularity},
                    },
                    .freeMem = 10 * granularity,
                    .totalMem = 20 * granularity
                }
            },
            {
                "Node1",
                {
                    .numaNodeSize = {
                        {0, 5 * granularity},
                        {1, 5 * granularity},
                        {2, 5 * granularity},
                        {3, 5 * granularity},
                    },
                    .freeMem = 10 * granularity,
                    .totalMem = 20 * granularity
                }
            },
        };
        cout << "[Phase SetUp End]" << endl;
    }

    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }

    uint64_t granularity = UcacheConfig::GetInstance().GetBorrowSize();
};

TEST_F(BorrowActionTest, ExecuteBorrowActions_ActionType_Bad)
{
    std::vector<BorrowAction> actionSet {
        {
            .type = static_cast<ActionType>(2),
            .nodeMemBorrowInfo = {
                .totalSize = 2 * granularity,
                .srcNodeId = "Node0",
                .destNodeId = "Node1",
            }
        },
    };

    MOCKER(ucache::borrow_action::ExecuteBorrowMem)
        .stubs()
        .will(returnValue(UCACHE_OK));

    uint32_t ret = ExecuteBorrowActions(actionSet);
    EXPECT_EQ(ret, MEM_ACTION_TYPE_ERROR);
}

TEST_F(BorrowActionTest, ExecuteBorrowActions_ActionType_Borrow_Success)
{
    std::vector<BorrowAction> actionSet {
        {
            .type = ActionType::BORROW,
            .nodeMemBorrowInfo = {
                .totalSize = 2 * granularity,
                .srcNodeId = "Node0",
                .destNodeId = "Node1",
            }
        },
    };

    MOCKER(ucache::borrow_action::ExecuteBorrowMem)
        .stubs()
        .will(returnValue(UCACHE_OK));

    uint32_t ret = ExecuteBorrowActions(actionSet);
    EXPECT_EQ(ret, UCACHE_OK);
}

TEST_F(BorrowActionTest, ExecuteBorrowActions_ActionType_Borrow_Failed)
{
    std::vector<BorrowAction> actionSet {
        {
            .type = ActionType::BORROW,
            .nodeMemBorrowInfo = {
                .totalSize = 2 * granularity,
                .srcNodeId = "Node0",
                .destNodeId = "Node1",
            }
        },
    };

    MOCKER(ucache::borrow_action::ExecuteBorrowMem)
        .stubs()
        .will(returnValue(UCACHE_ERR));
    uint32_t ret = ExecuteBorrowActions(actionSet);
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(BorrowActionTest, ExecuteBorrowActions_ActionType_Return_Success)
{
    std::vector<BorrowAction> actionSet {
        {
            .type = ActionType::RETURN,
            .nodeMemBorrowInfo = {
                .totalSize = 2 * granularity,
                .srcNodeId = "Node0",
                .destNodeId = "Node1",
            }
        },
    };

    MOCKER(ucache::borrow_action::ExecuteReturnMem)
        .stubs()
        .will(returnValue(UCACHE_OK));

    uint32_t ret = ExecuteBorrowActions(actionSet);
    EXPECT_EQ(ret, UCACHE_OK);
}

TEST_F(BorrowActionTest, ExecuteBorrowActions_ActionType_Return_Failed)
{
    std::vector<BorrowAction> actionSet {
        {
            .type = ActionType::RETURN,
            .nodeMemBorrowInfo = {
                .totalSize = 2 * granularity,
                .srcNodeId = "Node0",
                .destNodeId = "Node1",
            }
        },
    };

    MOCKER(ucache::borrow_action::ExecuteReturnMem)
        .stubs()
        .will(returnValue(UCACHE_ERR));

    uint32_t ret = ExecuteBorrowActions(actionSet);
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(BorrowActionTest, ExecuteBorrowMem_InvalidArgumentTest)
{
    std::string from = "3";
    std::string to = "1";
    MOCKER_CPP(&ucache::mem_borrow::MemBorrowTopo::GetBorrowableNumaInfo,
               uint32_t(*)(const std::string &node, int &socketId, int &numaId))
        .stubs()
        .will(returnValue(EXEC_MEM_BORROW_ERROR));
    uint32_t ret = ExecuteBorrowMem(from, to);
    EXPECT_EQ(ret, EXEC_MEM_BORROW_ERROR);
}

TEST_F(BorrowActionTest, ExecuteBorrowMem_SafeStoInt_failed)
{
    std::string from = "abc";
    std::string to = "1";
    MOCKER_CPP(&ucache::mem_borrow::MemBorrowTopo::GetBorrowableNumaInfo,
               uint32_t(*)(const std::string &node, int &socketId, int &numaId))
        .stubs()
        .will(returnValue(UCACHE_OK));

    uint32_t ret = ExecuteBorrowMem(from, to);
    EXPECT_EQ(ret, INVALID_ARGUMENT);
}

TEST_F(BorrowActionTest, ExecuteBorrowMemTest)
{
    std::string from = "Node1";
    std::string to = "Node2";
    MOCKER_CPP(&ucache::mem_borrow::MemBorrowTopo::GetBorrowableNumaInfo,
               uint32_t(*)(const std::string &node, int &socketId, int &numaId))
        .stubs()
        .will(returnValue(UCACHE_OK));
    uint32_t ret = ExecuteBorrowMem(from, to);
    EXPECT_EQ(ret, INVALID_ARGUMENT);

    ret = ExecuteBorrowMem("1", to);
    EXPECT_EQ(ret, EXEC_MEM_BORROW_ERROR);

    MOCKER(UbseMemNumaCreateWithLender).stubs().will(returnValue(UBSE_ERR_INVALID_ARG));
    ret = ExecuteBorrowMem("1", to);
    EXPECT_EQ(ret, UBSE_API_ERROR);

    MOCKER(memcpy_s).stubs().will(returnValue(1));
    ret = ExecuteBorrowMem("1", to);
    EXPECT_EQ(ret, EXEC_MEM_BORROW_ERROR);
}

TEST_F(BorrowActionTest, ExecuteReturnMemTest)
{
    std::string from = "Node1";
    std::string to = "Node2";
    MOCKER_CPP(&ucache::mem_borrow::MemBorrowTopo::GetReturnableMem,
               uint32_t(*)(const std::string &node, int &socketId, int &numaId))
        .stubs()
        .will(returnValue(UCACHE_OK));
    MOCKER_CPP(&ucache::mem_borrow::MemBorrowTopo::DelNumaNodeBorrowSize,
               uint32_t(*)(const std::string &from, const std::string &to, std::string &memName, int &numaId))
        .stubs()
        .will(returnValue(UCACHE_OK));
    uint32_t ret = ExecuteReturnMem(from, to);
    EXPECT_EQ(ret, UCACHE_OK);

    MOCKER(UbseMemNumaDelete).stubs().will(returnValue(UBSE_ERR_INVALID_ARG));
    ret = ExecuteReturnMem(from, to);
    EXPECT_EQ(ret, EXEC_MEM_RETURN_ERROR);
}