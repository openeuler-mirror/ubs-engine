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

#include "test_ubse_ssu_reservation_mgr.h"
#include <thread>
#include <vector>

namespace ubse::ssu::service::ut {

void TestUbseSsuReservationMgr::SetUp()
{
    Test::SetUp();
}

void TestUbseSsuReservationMgr::TearDown()
{
    mgr_.Clear();
    Test::TearDown();
}

/*
 * 用例描述：AddReserveSpace 为新 eid 添加预扣除容量
 * 测试步骤：
 * 1、添加 eid-1 的预扣除 1024 字节
 * 预期结果：
 * 1、GetPendingAdjustment("eid-1") 返回 1024
 */
TEST_F(TestUbseSsuReservationMgr, AddReserveNewEid)
{
    mgr_.AddReserveSpace({{"eid-1", 1024}});

    EXPECT_EQ(mgr_.GetPendingAdjustment("eid-1"), 1024);
}

/*
 * 用例描述：AddReserveSpace 为已有 eid 累计预扣除容量
 * 测试步骤：
 * 1、添加 eid-1 预扣除 1024
 * 2、再次添加 eid-1 预扣除 512
 * 预期结果：
 * 1、GetPendingAdjustment("eid-1") 返回 1536
 */
TEST_F(TestUbseSsuReservationMgr, AddReserveExistingEid)
{
    mgr_.AddReserveSpace({{"eid-1", 1024}});
    mgr_.AddReserveSpace({{"eid-1", 512}});

    EXPECT_EQ(mgr_.GetPendingAdjustment("eid-1"), 1536);
}

/*
 * 用例描述：AddReleasedSpace 为新 eid 添加预释放容量
 * 测试步骤：
 * 1、添加 eid-1 的预释放 2048 字节
 * 预期结果：
 * 1、GetPendingAdjustment("eid-1") 返回 -2048
 */
TEST_F(TestUbseSsuReservationMgr, AddReleasedNewEid)
{
    mgr_.AddReleasedSpace({{"eid-1", 2048}});

    EXPECT_EQ(mgr_.GetPendingAdjustment("eid-1"), -2048);
}

/*
 * 用例描述：AddReleasedSpace 为已有 eid 累计预释放容量
 * 测试步骤：
 * 1、添加 eid-1 预释放 2048
 * 2、再次添加 eid-1 预释放 1024
 * 预期结果：
 * 1、GetPendingAdjustment("eid-1") 返回 -3072
 */
TEST_F(TestUbseSsuReservationMgr, AddReleasedExistingEid)
{
    mgr_.AddReleasedSpace({{"eid-1", 2048}});
    mgr_.AddReleasedSpace({{"eid-1", 1024}});

    EXPECT_EQ(mgr_.GetPendingAdjustment("eid-1"), -3072);
}

/*
 * 用例描述：ReleaseReservation 完全释放预扣除（容量相等）
 * 测试步骤：
 * 1、添加 eid-1 预扣除 1024
 * 2、释放 eid-1 的 1024 预扣除
 * 预期结果：
 * 1、GetPendingAdjustment("eid-1") 返回 0
 */
TEST_F(TestUbseSsuReservationMgr, ReleaseReservationFull)
{
    mgr_.AddReserveSpace({{"eid-1", 1024}});
    mgr_.ReleaseReservation({{"eid-1", 1024}});

    EXPECT_EQ(mgr_.GetPendingAdjustment("eid-1"), 0);
}

/*
 * 用例描述：ReleaseReservation 部分释放预扣除
 * 测试步骤：
 * 1、添加 eid-1 预扣除 2048
 * 2、释放 eid-1 的 1024 预扣除
 * 预期结果：
 * 1、GetPendingAdjustment("eid-1") 返回 1024
 */
TEST_F(TestUbseSsuReservationMgr, ReleaseReservationPartial)
{
    mgr_.AddReserveSpace({{"eid-1", 2048}});
    mgr_.ReleaseReservation({{"eid-1", 1024}});

    EXPECT_EQ(mgr_.GetPendingAdjustment("eid-1"), 1024);
}

/*
 * 用例描述：ReleaseReservation 对不存在的 eid 应无影响
 * 测试步骤：
 * 1、添加 eid-1 预扣除 1024
 * 2、释放 eid-2 的 512 预扣除（不存在）
 * 预期结果：
 * 1、eid-1 调整量仍为 1024
 * 2、eid-2 调整量为 0
 */
TEST_F(TestUbseSsuReservationMgr, ReleaseReservationNotExistEid)
{
    mgr_.AddReserveSpace({{"eid-1", 1024}});
    mgr_.ReleaseReservation({{"eid-2", 512}});

    EXPECT_EQ(mgr_.GetPendingAdjustment("eid-1"), 1024);
    EXPECT_EQ(mgr_.GetPendingAdjustment("eid-2"), 0);
}

/*
 * 用例描述：GetPendingAdjustment 同时有预扣除和预释放时返回净调整量（正数）
 * 测试步骤：
 * 1、添加 eid-1 预扣除 2048
 * 2、添加 eid-1 预释放 512
 * 预期结果：
 * 1、GetPendingAdjustment("eid-1") 返回 1536 (2048 - 512)
 */
TEST_F(TestUbseSsuReservationMgr, GetAdjustmentNetReserve)
{
    mgr_.AddReserveSpace({{"eid-1", 2048}});
    mgr_.AddReleasedSpace({{"eid-1", 512}});

    EXPECT_EQ(mgr_.GetPendingAdjustment("eid-1"), 1536);
}

/*
 * 用例描述：GetPendingAdjustment 释放大于预扣除时返回负数
 * 测试步骤：
 * 1、添加 eid-1 预扣除 1024
 * 2、添加 eid-1 预释放 2048
 * 预期结果：
 * 1、GetPendingAdjustment("eid-1") 返回 -1024
 */
TEST_F(TestUbseSsuReservationMgr, GetAdjustmentNetReleased)
{
    mgr_.AddReserveSpace({{"eid-1", 1024}});
    mgr_.AddReleasedSpace({{"eid-1", 2048}});

    EXPECT_EQ(mgr_.GetPendingAdjustment("eid-1"), -1024);
}

/*
 * 用例描述：Clear 清空有数据的管理器，所有调整量归零
 * 测试步骤：
 * 1、添加多个 eid 的预扣除和预释放
 * 2、Clear
 * 预期结果：
 * 1、所有 eid 的调整量为 0
 */
TEST_F(TestUbseSsuReservationMgr, ClearWithData)
{
    mgr_.AddReserveSpace({{"eid-1", 1024}, {"eid-2", 2048}});
    mgr_.AddReleasedSpace({{"eid-1", 512}});
    mgr_.Clear();

    EXPECT_EQ(mgr_.GetPendingAdjustment("eid-1"), 0);
    EXPECT_EQ(mgr_.GetPendingAdjustment("eid-2"), 0);
}

/*
 * 用例描述：MixedReserveAndReleased 多 eid 混合预扣除和预释放，验证各 eid 净调整量
 * 测试步骤：
 * 1、添加 eid-1 预扣除 1024、eid-2 预扣除 2048
 * 2、添加 eid-1 预释放 512
 * 预期结果：
 * 1、eid-1 净调整量为 512（1024 - 512）
 * 2、eid-2 净调整量为 2048（仅有预扣除）
 */
TEST_F(TestUbseSsuReservationMgr, MixedReserveAndReleased)
{
    mgr_.AddReserveSpace({{"eid-1", 1024}, {"eid-2", 2048}});
    mgr_.AddReleasedSpace({{"eid-1", 512}});

    EXPECT_EQ(mgr_.GetPendingAdjustment("eid-1"), 512);
    EXPECT_EQ(mgr_.GetPendingAdjustment("eid-2"), 2048);
}

/*
 * 用例描述：ConcurrentSafety 多线程并发访问不应有数据竞争
 * 测试步骤：
 * 1、4 个线程同时对多个 eid 进行 AddReserve/AddReleased/ReleaseReservation/GetAdjustment
 * 2、主线程等待所有线程完成
 * 预期结果：
 * 1、所有操作正常完成，无崩溃
 * 2、最终 Clear 成功
 */
TEST_F(TestUbseSsuReservationMgr, ConcurrentSafety)
{
    std::vector<std::thread> threads;
    constexpr int THREAD_COUNT = 4;

    for (int t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([this, t]() {
            std::string eid = "eid-" + std::to_string(t);
            for (int i = 0; i < 100; ++i) {
                mgr_.AddReserveSpace({{eid, 1024}});
                mgr_.AddReleasedSpace({{eid, 512}});
                (void)mgr_.GetPendingAdjustment(eid);
                if (i % 10 == 0) {
                    mgr_.ReleaseReservation({{eid, 512}});
                }
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    // 验证每个 eid 的净调整量：reserve 100*1024 - release 100*512 - releaseReservation 10*512 = 46080
    for (int t = 0; t < THREAD_COUNT; ++t) {
        EXPECT_EQ(mgr_.GetPendingAdjustment("eid-" + std::to_string(t)), 46080);
    }

    // 操作完成后应能正常 Clear
    EXPECT_NO_FATAL_FAILURE(mgr_.Clear());
    for (int t = 0; t < THREAD_COUNT; ++t) {
        EXPECT_EQ(mgr_.GetPendingAdjustment("eid-" + std::to_string(t)), 0);
    }
}

} // namespace ubse::ssu::service::ut
