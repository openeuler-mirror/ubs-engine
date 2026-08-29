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
#include "mem_manager.h"

using namespace mempooling;

class TestFaultNumaReservedLock : public ::testing::Test {
public:
    void SetUp() override
    {
        // Release any leftovers from previous tests
        auto& lock = FaultNumaReservedLock::Instance();
        while (lock.IsReserved(1)) {
            lock.Release(1);
        }
        while (lock.IsReserved(2)) {
            lock.Release(2);
        }
        while (lock.IsReserved(3)) {
            lock.Release(3);
        }
    }
    void TearDown() override
    {
        auto& lock = FaultNumaReservedLock::Instance();
        lock.Release(1);
        lock.Release(2);
        lock.Release(3);
    }
};

TEST_F(TestFaultNumaReservedLock, TryReserve_Succeed)
{
    auto& lock = FaultNumaReservedLock::Instance();
    EXPECT_TRUE(lock.TryReserve(1));
    EXPECT_TRUE(lock.IsReserved(1));
    lock.Release(1);
}

TEST_F(TestFaultNumaReservedLock, TryReserve_Duplicate_Fail)
{
    auto& lock = FaultNumaReservedLock::Instance();
    EXPECT_TRUE(lock.TryReserve(1));
    EXPECT_FALSE(lock.TryReserve(1));
    EXPECT_TRUE(lock.IsReserved(1));
    lock.Release(1);
}

TEST_F(TestFaultNumaReservedLock, Release_AllowsRereserve)
{
    auto& lock = FaultNumaReservedLock::Instance();
    EXPECT_TRUE(lock.TryReserve(1));
    lock.Release(1);
    EXPECT_FALSE(lock.IsReserved(1));
    EXPECT_TRUE(lock.TryReserve(1));
    lock.Release(1);
}

TEST_F(TestFaultNumaReservedLock, IsReserved_NotReserved_False)
{
    auto& lock = FaultNumaReservedLock::Instance();
    EXPECT_FALSE(lock.IsReserved(2));
}

TEST_F(TestFaultNumaReservedLock, DifferentNumas_Independent)
{
    auto& lock = FaultNumaReservedLock::Instance();
    EXPECT_TRUE(lock.TryReserve(1));
    EXPECT_TRUE(lock.TryReserve(2));
    EXPECT_TRUE(lock.IsReserved(1));
    EXPECT_TRUE(lock.IsReserved(2));
    lock.Release(1);
    EXPECT_FALSE(lock.IsReserved(1));
    EXPECT_TRUE(lock.IsReserved(2));
    lock.Release(2);
}

class TestFaultNumaLock : public ::testing::Test {
public:
    void SetUp() override
    {
        // Ensure no leftover locks
        auto& lock = FaultNumaLock::Instance();
        lock.ReleaseExclusive(1);
        lock.ReleaseExclusive(2);
        lock.ReleaseShared(1);
        lock.ReleaseShared(2);
        lock.ReleaseSelf(1);
        lock.ReleaseSelf(2);
    }
    void TearDown() override
    {
        auto& lock = FaultNumaLock::Instance();
        lock.ReleaseExclusive(1);
        lock.ReleaseExclusive(2);
        lock.ReleaseShared(1);
        lock.ReleaseShared(2);
        lock.ReleaseSelf(1);
        lock.ReleaseSelf(2);
    }
};

TEST_F(TestFaultNumaLock, TryAcquireShared_NoExclusive_Succeed)
{
    auto& lock = FaultNumaLock::Instance();
    EXPECT_EQ(lock.TryAcquireShared(1), MEM_POOLING_OK);
    lock.ReleaseShared(1);
}

TEST_F(TestFaultNumaLock, TryAcquireExclusive_NoOtherLock_Succeed)
{
    auto& lock = FaultNumaLock::Instance();
    EXPECT_EQ(lock.TryAcquireExclusive(1), MEM_POOLING_OK);
    EXPECT_EQ(lock.TryAcquireShared(1), MEM_POOLING_HANDLING_FAULT);
    lock.ReleaseExclusive(1);
}

TEST_F(TestFaultNumaLock, TryAcquireShared_WhenExclusiveHeld_Fail)
{
    auto& lock = FaultNumaLock::Instance();
    EXPECT_EQ(lock.TryAcquireExclusive(1), MEM_POOLING_OK);
    EXPECT_EQ(lock.TryAcquireShared(1), MEM_POOLING_HANDLING_FAULT);
    lock.ReleaseExclusive(1);
}

TEST_F(TestFaultNumaLock, TryAcquireExclusive_WhenExclusiveHeld_Fail)
{
    auto& lock = FaultNumaLock::Instance();
    EXPECT_EQ(lock.TryAcquireExclusive(1), MEM_POOLING_OK);
    EXPECT_EQ(lock.TryAcquireExclusive(1), MEM_POOLING_HANDLING_FAULT);
    lock.ReleaseExclusive(1);
}

TEST_F(TestFaultNumaLock, MultipleShared_Concurrently)
{
    auto& lock = FaultNumaLock::Instance();
    EXPECT_EQ(lock.TryAcquireShared(1), MEM_POOLING_OK);
    EXPECT_EQ(lock.TryAcquireShared(1), MEM_POOLING_OK);
    EXPECT_EQ(lock.TryAcquireShared(1), MEM_POOLING_OK);
    lock.ReleaseShared(1);
    lock.ReleaseShared(1);
    lock.ReleaseShared(1);
}

TEST_F(TestFaultNumaLock, DifferentNumas_IndependentExclusive)
{
    auto& lock = FaultNumaLock::Instance();
    EXPECT_EQ(lock.TryAcquireExclusive(1), MEM_POOLING_OK);
    EXPECT_EQ(lock.TryAcquireExclusive(2), MEM_POOLING_OK);
    lock.ReleaseExclusive(1);
    lock.ReleaseExclusive(2);
}

TEST_F(TestFaultNumaLock, WhenExclusiveNotLocked_True)
{
    auto& lock = FaultNumaLock::Instance();
    EXPECT_EQ(lock.TryAcquireShared(1), MEM_POOLING_OK);
    lock.ReleaseShared(1);
}

TEST_F(TestFaultNumaLock, ReleaseExclusive_NotLocked_NoOp)
{
    auto& lock = FaultNumaLock::Instance();
    lock.ReleaseExclusive(1); // Should not crash
    EXPECT_EQ(lock.TryAcquireShared(1), MEM_POOLING_OK);
    lock.ReleaseShared(1);
}

TEST_F(TestFaultNumaLock, ReleaseShared_NotLocked_NoOp)
{
    auto& lock = FaultNumaLock::Instance();
    lock.ReleaseShared(1); // Should not crash
}

TEST_F(TestFaultNumaLock, TryAcquireSelf_NoOtherLock_Succeed)
{
    auto& lock = FaultNumaLock::Instance();
    EXPECT_EQ(lock.TryAcquireSelf(1), MEM_POOLING_OK);
    lock.ReleaseSelf(1);
}

TEST_F(TestFaultNumaLock, MultipleSelf_Concurrently)
{
    auto& lock = FaultNumaLock::Instance();
    EXPECT_EQ(lock.TryAcquireSelf(1), MEM_POOLING_OK);
    EXPECT_EQ(lock.TryAcquireSelf(1), MEM_POOLING_OK);
    EXPECT_EQ(lock.TryAcquireSelf(1), MEM_POOLING_OK);
    lock.ReleaseSelf(1);
    lock.ReleaseSelf(1);
    lock.ReleaseSelf(1);
}

TEST_F(TestFaultNumaLock, TryAcquireSelf_WhenExclusiveHeld_Fail)
{
    auto& lock = FaultNumaLock::Instance();
    EXPECT_EQ(lock.TryAcquireExclusive(1), MEM_POOLING_OK);
    EXPECT_EQ(lock.TryAcquireSelf(1), MEM_POOLING_HANDLING_FAULT);
    lock.ReleaseExclusive(1);
}

TEST_F(TestFaultNumaLock, TryAcquireSelf_WhenSharedHeld_Fail)
{
    auto& lock = FaultNumaLock::Instance();
    EXPECT_EQ(lock.TryAcquireShared(1), MEM_POOLING_OK);
    EXPECT_EQ(lock.TryAcquireSelf(1), MEM_POOLING_ERROR_CONCURRENCY_CONFLICT);
    lock.ReleaseShared(1);
}

TEST_F(TestFaultNumaLock, TryAcquireExclusive_WhenSelfHeld_Fail)
{
    auto& lock = FaultNumaLock::Instance();
    EXPECT_EQ(lock.TryAcquireSelf(1), MEM_POOLING_OK);
    EXPECT_EQ(lock.TryAcquireExclusive(1), MEM_POOLING_ERROR_CONCURRENCY_CONFLICT);
    lock.ReleaseSelf(1);
}

TEST_F(TestFaultNumaLock, TryAcquireShared_WhenSelfHeld_Fail)
{
    auto& lock = FaultNumaLock::Instance();
    EXPECT_EQ(lock.TryAcquireSelf(1), MEM_POOLING_OK);
    EXPECT_EQ(lock.TryAcquireShared(1), MEM_POOLING_ERROR_CONCURRENCY_CONFLICT);
    lock.ReleaseSelf(1);
}

TEST_F(TestFaultNumaLock, DifferentNumas_IndependentSelf)
{
    auto& lock = FaultNumaLock::Instance();
    EXPECT_EQ(lock.TryAcquireSelf(1), MEM_POOLING_OK);
    EXPECT_EQ(lock.TryAcquireSelf(2), MEM_POOLING_OK);
    lock.ReleaseSelf(1);
    lock.ReleaseSelf(2);
}

TEST_F(TestFaultNumaLock, ReleaseSelf_NotLocked_NoOp)
{
    auto& lock = FaultNumaLock::Instance();
    lock.ReleaseSelf(1); // Should not crash
    EXPECT_EQ(lock.TryAcquireShared(1), MEM_POOLING_OK);
    lock.ReleaseShared(1);
}
