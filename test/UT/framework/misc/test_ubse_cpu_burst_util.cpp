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

#include "ubse_cpu_burst_util.h"

#include <atomic>
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

namespace ubse::utils {
namespace {
using namespace std::chrono_literals;

constexpr auto HOLD_WINDOW = 100ms; // 缩短的空闲保持/预热窗口，加速看门狗路径验证
constexpr auto WATCHDOG_WAIT = 5s;  // 等待看门狗完成恢复的上限

// 等待谓词成立（看门狗为异步线程，恢复时点非确定性）。
template <typename Pred>
bool WaitFor(Pred pred, std::chrono::milliseconds timeout)
{
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < end) {
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(10ms);
    }
    return pred();
}

// 测试桩：注入配额/亲和性行为并缩短保持窗口，脱离 sudo/systemctl 与真实 sched 依赖。
// UT 二进制以 -fno-access-control 编译，可直接访问基类私有构造与保护/私有成员。
class TestableBurstUtil : public UbseCpuBurstUtil {
public:
    TestableBurstUtil()
    {
        idleHoldMs_ = HOLD_WINDOW;
        preOpenWarmupMs_ = HOLD_WINDOW;
        restoreRetryMs_ = HOLD_WINDOW;
    }

    // 生产代码为 leaky 单例、看门狗随进程存活；UT 实例在栈上，销毁前必须停掉 detach 的看门狗线程，
    // 避免其唤醒后访问已销毁对象。短暂等待确保线程观察到退出标志。
    ~TestableBurstUtil()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            watchdogStop_ = true;
        }
        idleCv_.notify_all();
        std::this_thread::sleep_for(50ms);
    }

    std::atomic<int> raiseCount{0};
    std::atomic<int> restoreCount{0};
    std::atomic<bool> failRaise{false};
    std::atomic<bool> failWiden{false};

    UbseResult RaiseQuota() override
    {
        ++raiseCount;
        return failRaise.load() ? UBSE_ERROR : UBSE_OK;
    }

    UbseResult RestoreQuota() override
    {
        ++restoreCount;
        return UBSE_OK;
    }

    UbseResult WidenAffinity() override
    {
        return failWiden.load() ? UBSE_ERROR : UBSE_OK;
    }
};

class UbseCpuBurstUtilTest : public testing::Test {
};
} // namespace

/*
 * 用例描述
 * Idle 冷开时提配额失败，降级为日常资源拷贝
 * 测试步骤
 * 1. 注入 RaiseQuota 失败
 * 2. 调用 Acquire
 * 预期结果
 * 1. Acquire 返回 false，IsActive 为 false，状态保持 Idle（未触发恢复）
 */
TEST_F(UbseCpuBurstUtilTest, ColdOpenRaiseFailureFallsBackToDaily)
{
    TestableBurstUtil util;
    util.failRaise = true;
    EXPECT_FALSE(util.Acquire());
    EXPECT_FALSE(util.IsActive());
    EXPECT_EQ(util.raiseCount.load(), 1);
    EXPECT_EQ(util.restoreCount.load(), 0);
}

/*
 * 用例描述
 * Idle 冷开时放宽亲和性失败，回滚已提升的配额
 * 测试步骤
 * 1. 注入 WidenAffinity 失败，调用 Acquire
 * 2. 解除注入后再次 Acquire
 * 预期结果
 * 1. 首次 Acquire 返回 false，RestoreQuota 被调用一次（配额回滚）
 * 2. 状态保持 Idle：再次 Acquire 走全新冷开（raiseCount 累加到 2）
 */
TEST_F(UbseCpuBurstUtilTest, ColdOpenWidenFailureRollsBackQuota)
{
    TestableBurstUtil util;
    util.failWiden = true;
    EXPECT_FALSE(util.Acquire());
    EXPECT_FALSE(util.IsActive());
    EXPECT_EQ(util.raiseCount.load(), 1);
    EXPECT_EQ(util.restoreCount.load(), 1);
    util.failWiden = false;
    EXPECT_TRUE(util.Acquire());
    EXPECT_TRUE(util.IsActive());
    EXPECT_EQ(util.raiseCount.load(), 2);
    util.Release();
    ASSERT_TRUE(WaitFor([&util] { return util.restoreCount.load() >= 2; }, WATCHDOG_WAIT));
}

/*
 * 用例描述
 * Holding 态复用与看门狗超时恢复
 * 测试步骤
 * 1. Acquire 成功后 Release（进入空闲保持）
 * 2. 保持窗口内再次 Acquire
 * 3. 等待看门狗超时恢复后再次 Acquire
 * 预期结果
 * 1. Release 后 IsActive 为 false
 * 2. 保持窗口内复用成功，且不重复提配额（raiseCount 保持 1）
 * 3. 看门狗恢复（restoreCount=1）后，Acquire 走全新冷开（raiseCount=2）
 */
TEST_F(UbseCpuBurstUtilTest, HoldWindowReuseThenWatchdogRestore)
{
    TestableBurstUtil util;
    ASSERT_TRUE(util.Acquire());
    EXPECT_TRUE(util.IsActive());
    EXPECT_EQ(util.raiseCount.load(), 1);
    util.Release();
    EXPECT_FALSE(util.IsActive());
    EXPECT_TRUE(util.Acquire());
    EXPECT_EQ(util.raiseCount.load(), 1); // 复用，无新冷开
    EXPECT_TRUE(util.IsActive());
    util.Release();
    ASSERT_TRUE(WaitFor([&util] { return util.restoreCount.load() >= 1; }, WATCHDOG_WAIT));
    EXPECT_TRUE(util.Acquire()); // 恢复完成、状态回 Idle：重新冷开
    EXPECT_TRUE(util.IsActive());
    EXPECT_EQ(util.raiseCount.load(), 2);
    util.Release();
    ASSERT_TRUE(WaitFor([&util] { return util.restoreCount.load() >= 2; }, WATCHDOG_WAIT));
}

/*
 * 用例描述
 * Active 期间收到 PreOpen：置标记并由 Release 保留预热窗口
 * 测试步骤
 * 1. Acquire 后调用 PreOpen
 * 2. Release 后立即 Acquire
 * 3. 等待预热窗口耗尽、看门狗恢复
 * 预期结果
 * 1. PreOpen 返回 true
 * 2. Release 后预热窗口内复用成功，无新冷开
 * 3. 预热窗口耗尽后看门狗完成恢复
 */
TEST_F(UbseCpuBurstUtilTest, PreOpenDuringActiveKeepsWarmupAfterRelease)
{
    TestableBurstUtil util;
    ASSERT_TRUE(util.Acquire());
    EXPECT_TRUE(util.PreOpen());
    util.Release();
    EXPECT_TRUE(util.Acquire()); // 预热窗口内复用
    EXPECT_EQ(util.raiseCount.load(), 1);
    EXPECT_TRUE(util.IsActive());
    util.Release();
    ASSERT_TRUE(WaitFor([&util] { return util.restoreCount.load() >= 1; }, WATCHDOG_WAIT));
}

/*
 * 用例描述
 * Idle 冷开预开：成功后进入预开等待（不占拷贝引用），失败时降级
 * 测试步骤
 * 1. 注入 RaiseQuota 失败，调用 PreOpen
 * 2. 解除注入后调用 PreOpen
 * 3. 首块拷贝到达（Acquire），Release 后等待恢复
 * 预期结果
 * 1. 预开失败返回 false，IsActive 为 false
 * 2. 预开成功返回 true，且 IsActive 为 false（预开不占拷贝引用）
 * 3. Acquire 复用预开资源成功（无新冷开），Release 后看门狗完成恢复
 */
TEST_F(UbseCpuBurstUtilTest, PreOpenColdOpenAndFailureFallback)
{
    TestableBurstUtil util;
    util.failRaise = true;
    EXPECT_FALSE(util.PreOpen());
    EXPECT_FALSE(util.IsActive());
    util.failRaise = false;
    EXPECT_TRUE(util.PreOpen());
    EXPECT_FALSE(util.IsActive());
    EXPECT_EQ(util.raiseCount.load(), 2); // 失败 1 次 + 预开冷开 1 次
    EXPECT_TRUE(util.Acquire());          // 复用预开资源，首块免资源切换
    EXPECT_TRUE(util.IsActive());
    EXPECT_EQ(util.raiseCount.load(), 2);
    util.Release();
    ASSERT_TRUE(WaitFor([&util] { return util.restoreCount.load() >= 1; }, WATCHDOG_WAIT));
}
} // namespace ubse::utils
