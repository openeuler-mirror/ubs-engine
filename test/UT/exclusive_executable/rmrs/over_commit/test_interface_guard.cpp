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
#include <thread>
#include "ubse_def.h"
#include "interface_guard.h"
#include "mockcpp/mokc.h"
#include "over_commit_election_handler.h"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling::over_commit {
using std::cout;
using std::endl;
class TestInterfaceGuard : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};
static void TestOutMemBorrowThread()
{
    auto guard = InterfaceGuard::InvokeOutMemBorrow();
    std::this_thread::sleep_for(std::chrono::seconds(3));
}
static void TestOutMemMigrateThread()
{
    auto guard = InterfaceGuard::InvokeOutMemMigrate();
    std::this_thread::sleep_for(std::chrono::seconds(3));
}
static void TestOutMemReturnThread()
{
    auto guard = InterfaceGuard::InvokeOutMemReturn();
    std::this_thread::sleep_for(std::chrono::seconds(3));
}
TEST_F(TestInterfaceGuard, SingleThreadTest)
{
    // 测试 OutMemBorrow
    {
        auto guard = InterfaceGuard::InvokeOutMemBorrow();
        auto status = InterfaceGuard::GetStatus();
        EXPECT_TRUE(status.isOutMemBorrowRunning);
        EXPECT_FALSE(status.isOutMemMigrateRunning);
        EXPECT_FALSE(status.isOutMemReturnRunning);
    }

    // 测试 OutMemMigrate
    {
        auto guard = InterfaceGuard::InvokeOutMemMigrate();
        auto status = InterfaceGuard::GetStatus();
        EXPECT_FALSE(status.isOutMemBorrowRunning);
        EXPECT_TRUE(status.isOutMemMigrateRunning);
        EXPECT_FALSE(status.isOutMemReturnRunning);
    }

    // 测试 OutMemReturn
    {
        auto guard = InterfaceGuard::InvokeOutMemReturn();
        auto status = InterfaceGuard::GetStatus();
        EXPECT_FALSE(status.isOutMemBorrowRunning);
        EXPECT_FALSE(status.isOutMemMigrateRunning);
        EXPECT_TRUE(status.isOutMemReturnRunning);
    }
}

TEST_F(TestInterfaceGuard, GuardShouldReturn3False)
{
    // 创建3个线程
    std::thread t1(TestOutMemBorrowThread);
    std::thread t2(TestOutMemMigrateThread);
    std::thread t3(TestOutMemReturnThread);
    t1.detach();
    t2.detach();
    t3.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto status = InterfaceGuard::GetStatus();
    EXPECT_TRUE(status.isOutMemReturnRunning);
    EXPECT_TRUE(status.isOutMemMigrateRunning);
    EXPECT_TRUE(status.isOutMemBorrowRunning);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    status = InterfaceGuard::GetStatus();
    EXPECT_TRUE(status.isOutMemReturnRunning);
    EXPECT_TRUE(status.isOutMemMigrateRunning);
    EXPECT_TRUE(status.isOutMemBorrowRunning);
    std::this_thread::sleep_for(std::chrono::seconds(3));
    // 检查结果
    status = InterfaceGuard::GetStatus();
    EXPECT_FALSE(status.isOutMemReturnRunning);
    EXPECT_FALSE(status.isOutMemMigrateRunning);
    EXPECT_FALSE(status.isOutMemBorrowRunning);
}

// 测试 Guard 的析构函数是否正确释放资源
TEST_F(TestInterfaceGuard, DestructorTest)
{
    {
        auto guard = InterfaceGuard::InvokeOutMemBorrow();
        auto status = InterfaceGuard::GetStatus();
        EXPECT_TRUE(status.isOutMemBorrowRunning);
    }

    auto status = InterfaceGuard::GetStatus();
    EXPECT_FALSE(status.isOutMemBorrowRunning);
}

// 测试异常情况下的资源释放
TEST_F(TestInterfaceGuard, ExceptionTest)
{
    try {
        auto guard = InterfaceGuard::InvokeOutMemBorrow();
        throw std::runtime_error("Test exception");
    } catch (const std::exception &e) {
        // 异常在本测试中是预期行为，仅用于触发资源释放逻辑
        // 不需要进一步处理
    }

    auto status = InterfaceGuard::GetStatus();
    EXPECT_FALSE(status.isOutMemBorrowRunning);
}

TEST_F(TestInterfaceGuard, OverCommitSwitchoverHandlerFail)
{
    auto type = UbseElectionEventType::STANDBY_CHANGE_TO_MASTER;
    auto guard = InterfaceGuard::InvokeOutMemBorrow();
    std::string nodeId;
    EXPECT_EQ(1, OverCommitSwitchoverHandler(type, nodeId));
}
TEST_F(TestInterfaceGuard, OverCommitSwitchoverHandlerSuccess)
{
    auto type = UbseElectionEventType::STANDBY_CHANGE_TO_MASTER;
    std::string nodeId;
    EXPECT_EQ(0, OverCommitSwitchoverHandler(type, nodeId));
}
} // namespace mempooling::over_commit
