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

#include "test_ubse_ssu_collector.h"

#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_ssu_adapter_interface.h"
#include "ubse_timer.h"

namespace ubse::ssu::service::ut {

using namespace ubse::adapter_plugins::ssu::def;
using namespace ubse::context;

// ========== 常量辅助 ==========
constexpr uint64_t KB = 1024ULL;
constexpr uint64_t MB = 1024ULL * KB;
constexpr uint64_t GB = 1024ULL * MB;
constexpr uint64_t TB = 1024ULL * GB;

// ========== Helper 辅助函数 ==========

UbseSsuDevInfo TestUbseSsuCollector::MakeDevInfo(const std::string &eid,
                                                  uint64_t totalBytes,
                                                  uint64_t usedBytes)
{
    UbseSsuDevInfo info;
    info.subSystem.eid = eid;
    info.subSystem.subNqn = "nqn.2024-01.org.nvmexpress:uuid:" + eid;
    info.subSystem.jettyId = 1;
    info.serialNumber = "SN_" + eid;
    info.firmware = "1.0";
    info.totalBytes = totalBytes;
    info.usedBytes = usedBytes;
    info.state = UbseSsuState::ONLINE;
    return info;
}

// ========== 辅助：直接填充 collector 缓存 ==========
// MOCKER_CPP_VIRTUAL 对引用类型 out 参数的 invoke 不兼容，
// 因此测试中通过 -fno-access-control 直接填充 cachedDevMap_ 来模拟采集后的缓存

static void PopulateCache(UbseSsuCollector &collector, const std::string &eid,
                          uint64_t totalBytes = 1099511627776ULL,
                          uint64_t usedBytes = 0)
{
    auto dev = std::make_shared<const UbseSsuDevInfo>(
        TestUbseSsuCollector::MakeDevInfo(eid, totalBytes, usedBytes));
    collector.cachedDevMap_[eid] = dev;
}

// ========== SetUp / TearDown ==========

void TestUbseSsuCollector::SetUp()
{
    Test::SetUp();
}

void TestUbseSsuCollector::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
    g_globalStop.store(false);
}

/*
 * 用例描述：Start 正常启动成功，首次采集完成后注册定时器
 * 测试步骤：
 * 1、mock GetDevList 返回 UBSE_OK（空列表）
 * 2、mock UbseTimerHandlerRegister 返回 UBSE_OK
 * 3、调用 Start()
 * 4、再次调用 Start() 验证幂等
 * 预期结果：
 * 1、Start 返回 UBSE_OK
 * 2、重复 Start 也返回 UBSE_OK
 * 3、采集到空列表时缓存为空
 */
TEST_F(TestUbseSsuCollector, Start_Success)
{
    auto &adapter = UbseSsuAdapterInterface::GetInstance();
    MOCKER_CPP_VIRTUAL(adapter, &UbseSsuAdapterInterface::GetDevList)
        .stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister)
        .stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));

    UbseSsuCollector collector;
    EXPECT_EQ(collector.Start(), UBSE_OK);
    EXPECT_TRUE(collector.GetCachedDevList().empty());
    EXPECT_EQ(collector.Start(), UBSE_OK);
}

/*
 * 用例描述：Start 首次采集失败时不阻塞启动，由定时器重试
 * 测试步骤：
 * 1、mock GetDevList 返回 UBSE_ERROR
 * 2、mock UbseTimerHandlerRegister 返回 UBSE_OK
 * 3、调用 Start()
 * 预期结果：
 * 1、Start 返回 UBSE_OK（首次采集失败置位rebuildPending，由定时器采集成功后补建账本）
 */
TEST_F(TestUbseSsuCollector, Start_CollectFail)
{
    auto &adapter = UbseSsuAdapterInterface::GetInstance();
    MOCKER_CPP_VIRTUAL(adapter, &UbseSsuAdapterInterface::GetDevList)
        .stubs().will(returnValue(static_cast<uint32_t>(UBSE_ERROR)));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister)
        .stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));

    UbseSsuCollector collector;
    EXPECT_EQ(collector.Start(), UBSE_OK);
}

/*
 * 用例描述：Start 定时器注册失败时返回错误，timerStarted_回滚
 * 测试步骤：
 * 1、mock GetDevList 返回 UBSE_OK
 * 2、mock UbseTimerHandlerRegister 返回 UBSE_ERROR
 * 3、调用 Start()
 * 预期结果：
 * 1、Start 返回非 UBSE_OK 的错误码
 */
TEST_F(TestUbseSsuCollector, Start_TimerRegisterFail)
{
    auto &adapter = UbseSsuAdapterInterface::GetInstance();
    MOCKER_CPP_VIRTUAL(adapter, &UbseSsuAdapterInterface::GetDevList)
        .stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister)
        .stubs().will(returnValue(static_cast<uint32_t>(UBSE_ERROR)));

    UbseSsuCollector collector;
    EXPECT_NE(collector.Start(), UBSE_OK);
}

/*
 * 用例描述：Stop 在未启动时调用不应有副作用
 * 测试步骤：
 * 1、直接调用 Stop()
 * 预期结果：
 * 1、无崩溃，正常返回
 */
TEST_F(TestUbseSsuCollector, Stop_WithoutStartNoOp)
{
    UbseSsuCollector collector;
    EXPECT_NO_THROW(collector.Stop());
}

/*
 * 用例描述：Stop 正常停止，Unregister 被调用一次，缓存被清空
 * 测试步骤：
 * 1、mock GetDevList 返回 UBSE_OK
 * 2、mock UbseTimerHandlerRegister 返回 UBSE_OK
 * 3、调用 Start()
 * 4、mock UbseTimerHandlerUnregister 预期被调用一次
 * 5、调用 Stop()
 * 预期结果：
 * 1、UbseTimerHandlerUnregister 被调用一次
 * 2、GetCachedDevList 返回空列表
 */
TEST_F(TestUbseSsuCollector, Stop_Normal)
{
    auto &adapter = UbseSsuAdapterInterface::GetInstance();
    MOCKER_CPP_VIRTUAL(adapter, &UbseSsuAdapterInterface::GetDevList)
        .stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister)
        .stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister)
        .expects(once());

    UbseSsuCollector collector;
    collector.Start();
    collector.Stop();
    EXPECT_TRUE(collector.GetCachedDevList().empty());
}

/*
 * 用例描述：Stop 重复调用时，第二次不应再次调用 Unregister
 * 测试步骤：
 * 1、mock 并 Start
 * 2、Stop() 第一次（Unregister 被调用一次）
 * 3、Stop() 第二次（Unregister 不应再被调用）
 * 预期结果：
 * 1、UbseTimerHandlerUnregister 仅被调用一次
 */
TEST_F(TestUbseSsuCollector, Stop_DoubleStopNoOp)
{
    auto &adapter = UbseSsuAdapterInterface::GetInstance();
    MOCKER_CPP_VIRTUAL(adapter, &UbseSsuAdapterInterface::GetDevList)
        .stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister)
        .stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister)
        .expects(once());

    UbseSsuCollector collector;
    collector.Start();
    collector.Stop();
    collector.Stop();
}

/*
 * 用例描述：CollectDeviceList 在全局停止时直接返回，不调用 GetDevList
 * 测试步骤：
 * 1、设置 g_globalStop = true
 * 2、mock GetDevList 预期不被调用
 * 3、mock UbseTimerHandlerRegister 返回 UBSE_OK
 * 4、调用 Start()（内部首次触发 CollectDeviceList）
 * 预期结果：
 * 1、GetDevList 未被调用
 * 2、Start 返回 UBSE_OK
 */
TEST_F(TestUbseSsuCollector, CollectDeviceList_GlobalStop)
{
    g_globalStop.store(true);
    auto &adapter = UbseSsuAdapterInterface::GetInstance();
    MOCKER_CPP_VIRTUAL(adapter, &UbseSsuAdapterInterface::GetDevList)
        .expects(never());
    MOCKER(&ubse::timer::UbseTimerHandlerRegister)
        .stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));

    UbseSsuCollector collector;
    EXPECT_EQ(collector.Start(), UBSE_OK);
    g_globalStop.store(false);
}

/*
 * 用例描述：GetCachedDevList 在缓存为空时返回空列表
 * 测试步骤：
 * 1、直接调用 GetCachedDevList（未填充缓存）
 * 预期结果：
 * 1、返回空 vector
 */
TEST_F(TestUbseSsuCollector, GetCachedDevList_EmptyCache)
{
    UbseSsuCollector collector;
    EXPECT_TRUE(collector.GetCachedDevList().empty());
}

/*
 * 用例描述：GetCachedDevList 在缓存有数据时返回设备列表副本
 * 测试步骤：
 * 1、PopulateCache 插入 eid1（模拟采集完成后的缓存）
 * 2、调用 GetCachedDevList
 * 预期结果：
 * 1、返回 size=1
 * 2、设备 eid 为 "eid1"
 */
TEST_F(TestUbseSsuCollector, GetCachedDevList_AfterPopulate)
{
    UbseSsuCollector collector;
    PopulateCache(collector, "eid1");
    auto devList = collector.GetCachedDevList();
    ASSERT_EQ(devList.size(), 1);
    EXPECT_EQ(devList[0].subSystem.eid, "eid1");
}

/*
 * 用例描述：GetDevListWithReservations 在没有预留调整量时返回原始 usedBytes
 * 测试步骤：
 * 1、PopulateCache 插入 eid1，usedBytes=0
 * 2、不调用任何预留操作
 * 3、调用 GetDevListWithReservations
 * 预期结果：
 * 1、返回 size=1
 * 2、usedBytes = 0
 */
TEST_F(TestUbseSsuCollector, GetDevListWithReservations_NoReservation)
{
    UbseSsuCollector collector;
    PopulateCache(collector, "eid1");
    auto result = collector.GetDevListWithReservations();
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].usedBytes, 0);
}

/*
 * 用例描述：GetDevListWithReservations 在有预扣除时，usedBytes 累加预扣除量
 * 测试步骤：
 * 1、PopulateCache 插入 eid1，usedBytes=100
 * 2、AddReserveSpace 预扣除 200
 * 3、调用 GetDevListWithReservations
 * 预期结果：
 * 1、返回 size=1
 * 2、usedBytes = 300（100 + 200）
 */
TEST_F(TestUbseSsuCollector, GetDevListWithReservations_WithReserve)
{
    UbseSsuCollector collector;
    PopulateCache(collector, "eid1", 1ULL * TB, 100);
    collector.AddReserveSpace({{"eid1", 200}});
    auto result = collector.GetDevListWithReservations();
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].usedBytes, 300);
}

/*
 * 用例描述：GetDevListWithReservations 在有预释放时，usedBytes 扣减释放量
 * 测试步骤：
 * 1、PopulateCache 插入 eid1，usedBytes=500
 * 2、AddReleasedSpace 预释放 200
 * 3、调用 GetDevListWithReservations
 * 预期结果：
 * 1、返回 size=1
 * 2、usedBytes = 300（500 - 200）
 */
TEST_F(TestUbseSsuCollector, GetDevListWithReservations_WithRelease)
{
    UbseSsuCollector collector;
    PopulateCache(collector, "eid1", 1ULL * TB, 500);
    collector.AddReleasedSpace({{"eid1", 200}});
    auto result = collector.GetDevListWithReservations();
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].usedBytes, 300);
}

/*
 * 用例描述：GetDevListWithReservations 当预释放超过 usedBytes 时，结果被 clamp 到 0
 * 测试步骤：
 * 1、PopulateCache 插入 eid1，usedBytes=100
 * 2、AddReleasedSpace 预释放 500（超过当前 usedBytes）
 * 3、调用 GetDevListWithReservations
 * 预期结果：
 * 1、返回 size=1
 * 2、usedBytes = 0（clamp 到 0）
 */
TEST_F(TestUbseSsuCollector, GetDevListWithReservations_NegativeClamp)
{
    UbseSsuCollector collector;
    PopulateCache(collector, "eid1", 1ULL * TB, 100);
    collector.AddReleasedSpace({{"eid1", 500}});
    auto result = collector.GetDevListWithReservations();
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].usedBytes, 0);
}

/*
 * 用例描述：ReleaseReservation 完全回滚预扣除，usedBytes 恢复原始值
 * 测试步骤：
 * 1、PopulateCache 插入 eid1，usedBytes=100
 * 2、AddReserveSpace 预扣除 200
 * 3、ReleaseReservation 释放 200
 * 4、调用 GetDevListWithReservations
 * 预期结果：
 * 1、返回 size=1
 * 2、usedBytes = 100（回滚后恢复原始值）
 */
TEST_F(TestUbseSsuCollector, ReleaseReservation_Rollback)
{
    UbseSsuCollector collector;
    PopulateCache(collector, "eid1", 1ULL * TB, 100);
    collector.AddReserveSpace({{"eid1", 200}});
    collector.ReleaseReservation({{"eid1", 200}});
    auto result = collector.GetDevListWithReservations();
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].usedBytes, 100);
}

/*
 * 用例描述：OnReserveBegin/End 成对调用不应崩溃
 * 测试步骤：
 * 1、调用 OnReserveBegin()
 * 2、调用 OnReserveEnd()
 * 预期结果：
 * 1、无崩溃
 */
TEST_F(TestUbseSsuCollector, OnReserveBeginEnd)
{
    UbseSsuCollector collector;
    EXPECT_NO_THROW(collector.OnReserveBegin());
    EXPECT_NO_THROW(collector.OnReserveEnd());
}

/*
 * 用例描述：OnReserveBegin/End 支持多次嵌套调用
 * 测试步骤：
 * 1、连续调用两次 OnReserveBegin()
 * 2、连续调用两次 OnReserveEnd()
 * 预期结果：
 * 1、无崩溃
 */
TEST_F(TestUbseSsuCollector, OnReserveBeginEnd_MultipleNested)
{
    UbseSsuCollector collector;
    collector.OnReserveBegin();
    collector.OnReserveBegin();
    collector.OnReserveEnd();
    collector.OnReserveEnd();
}

/*
 * 用例描述：GetCachedDevMap 在缓存为空时返回空 map
 * 测试步骤：
 * 1、直接调用 GetCachedDevMap（未填充缓存）
 * 预期结果：
 * 1、返回空 unordered_map
 */
TEST_F(TestUbseSsuCollector, GetCachedDevMap_EmptyCache)
{
    UbseSsuCollector collector;
    EXPECT_TRUE(collector.GetCachedDevMap().empty());
}

/*
 * 用例描述：GetCachedDevMap 返回缓存的设备 map 副本
 * 测试步骤：
 * 1、PopulateCache 插入 eid1
 * 2、调用 GetCachedDevMap
 * 预期结果：
 * 1、返回 size=1
 * 2、可通过 eid "eid1" 查找到对应的设备信息
 */
TEST_F(TestUbseSsuCollector, GetCachedDevMap_AfterPopulate)
{
    UbseSsuCollector collector;
    PopulateCache(collector, "eid1");
    auto devMap = collector.GetCachedDevMap();
    ASSERT_EQ(devMap.size(), 1);
    EXPECT_NE(devMap.find("eid1"), devMap.end());
    EXPECT_EQ(devMap["eid1"]->subSystem.eid, "eid1");
}

/*
 * 用例描述：GetCachedDevList 支持多设备缓存返回
 * 测试步骤：
 * 1、PopulateCache 插入 eid1（2TB, 100GB）和 eid2（4TB, 200GB）
 * 2、调用 GetCachedDevList
 * 预期结果：
 * 1、返回 size=2
 * 2、两个设备的 totalBytes 和 usedBytes 与插入值一致
 */
TEST_F(TestUbseSsuCollector, GetCachedDevList_MultipleDevices)
{
    UbseSsuCollector collector;
    PopulateCache(collector, "eid1", 2ULL * TB, 100ULL * GB);
    PopulateCache(collector, "eid2", 4ULL * TB, 200ULL * GB);
    auto devList = collector.GetCachedDevList();
    ASSERT_EQ(devList.size(), 2);
    // unordered_map 不保证顺序，按 eid 索引验证
    auto getByEid = [&](const std::string &eid) -> const UbseSsuDevInfo & {
        for (const auto &d : devList) {
            if (d.subSystem.eid == eid) return d;
        }
        return devList[0];
    };
    EXPECT_EQ(getByEid("eid1").totalBytes, 2ULL * TB);
    EXPECT_EQ(getByEid("eid2").totalBytes, 4ULL * TB);
    EXPECT_EQ(getByEid("eid1").usedBytes, 100ULL * GB);
    EXPECT_EQ(getByEid("eid2").usedBytes, 200ULL * GB);
}

/*
 * 用例描述：GetDevListWithReservations 多设备场景下，仅对指定 eid 做预留调整
 * 测试步骤：
 * 1、PopulateCache 插入 eid1（usedBytes=100）和 eid2（usedBytes=200）
 * 2、AddReserveSpace 仅对 eid1 预扣除 50
 * 3、调用 GetDevListWithReservations
 * 预期结果：
 * 1、返回 size=2
 * 2、eid1 的 usedBytes = 150（100 + 50）
 * 3、eid2 的 usedBytes = 200（无预留）
 */
TEST_F(TestUbseSsuCollector, GetDevListWithReservations_MultiDevice)
{
    UbseSsuCollector collector;
    PopulateCache(collector, "eid1", 1ULL * TB, 100);
    PopulateCache(collector, "eid2", 2ULL * TB, 200);
    collector.AddReserveSpace({{"eid1", 50}});
    auto result = collector.GetDevListWithReservations();
    ASSERT_EQ(result.size(), 2);
    auto getByEid = [&](const std::string &eid) -> const UbseSsuDevInfo & {
        for (const auto &d : result) {
            if (d.subSystem.eid == eid) return d;
        }
        return result[0];
    };
    EXPECT_EQ(getByEid("eid1").usedBytes, 150);
    EXPECT_EQ(getByEid("eid2").usedBytes, 200);
}

/*
 * 用例描述：CollectDeviceList 采集到空列表时，若缓存非空则保留原有缓存
 * 测试步骤：
 * 1、PopulateCache 插入 eid1
 * 2、mock GetDevList 返回 UBSE_OK（空列表）
 * 3、调用 Start() 触发 CollectDeviceList
 * 预期结果：
 * 1、缓存 size 仍为 1（空列表不覆盖非空缓存）
 */
TEST_F(TestUbseSsuCollector, Collect_EmptyListDoesNotClearNonEmptyCache)
{
    UbseSsuCollector collector;
    PopulateCache(collector, "eid1", 1ULL * TB, 100);
    ASSERT_EQ(collector.GetCachedDevList().size(), 1);

    auto &adapter = UbseSsuAdapterInterface::GetInstance();
    MOCKER_CPP_VIRTUAL(adapter, &UbseSsuAdapterInterface::GetDevList)
        .stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister)
        .stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));

    EXPECT_EQ(collector.Start(), UBSE_OK);
    EXPECT_EQ(collector.GetCachedDevList().size(), 1);
}

/*
 * 用例描述：无进行中的预留操作时，CollectDeviceList 会清除预留数据
 * 测试步骤：
 * 1、AddReserveSpace 添加预扣除 200
 * 2、验证 reservationMgr_ 中存在该预留
 * 3、mock GetDevList 返回 UBSE_OK（空列表）
 * 4、调用 CollectDeviceList()（pendingOps_ == 0 且 cachedDevMap_ 为空，不走提前返回，执行 Clear）
 * 预期结果：
 * 1、CollectDeviceList 返回 UBSE_OK
 * 2、reservationMgr_ 中的预留数据被 Clear 清除
 */
TEST_F(TestUbseSsuCollector, CollectClearsReservationWhenNoPendingOps)
{
    UbseSsuCollector collector;
    collector.AddReserveSpace({{"eid1", 200}});

    // 验证预留已存在
    EXPECT_EQ(collector.reservationMgr_.GetPendingAdjustment("eid1"), 200);

    auto &adapter = UbseSsuAdapterInterface::GetInstance();
    MOCKER_CPP_VIRTUAL(adapter, &UbseSsuAdapterInterface::GetDevList)
        .stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));

    auto ret = collector.CollectDeviceList();
    EXPECT_EQ(ret, UBSE_OK);

    // 验证预留已被 Clear 清除
    EXPECT_EQ(collector.reservationMgr_.GetPendingAdjustment("eid1"), 0);
}

/*
 * 用例描述：有进行中的预留操作时，CollectDeviceList 跳过 reservationMgr_.Clear()
 * 测试步骤：
 * 1、PopulateCache 插入 eid1（模拟已有缓存）
 * 2、AddReserveSpace 添加预扣除 200
 * 3、调用 OnReserveBegin()，pendingOps_ > 0
 * 4、mock GetDevList 返回 UBSE_OK（空列表，不会覆盖缓存）
 * 5、调用 CollectDeviceList()
 * 6、调用 OnReserveEnd()
 * 预期结果：
 * 1、CollectDeviceList 返回 UBSE_OK
 * 2、预留数据仍保留，GetDevListWithReservations 中 usedBytes 仍为 100 + 200 = 300
 */
TEST_F(TestUbseSsuCollector, PendingOpsPreventsClearDuringReservation)
{
    UbseSsuCollector collector;
    PopulateCache(collector, "eid1", 1ULL * TB, 100);
    collector.AddReserveSpace({{"eid1", 200}});

    // CollectDeviceList 之前验证预留生效
    EXPECT_EQ(collector.reservationMgr_.GetPendingAdjustment("eid1"), 200);

    collector.OnReserveBegin();

    auto &adapter = UbseSsuAdapterInterface::GetInstance();
    MOCKER_CPP_VIRTUAL(adapter, &UbseSsuAdapterInterface::GetDevList)
        .stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));

    auto ret = collector.CollectDeviceList();
    EXPECT_EQ(ret, UBSE_OK);

    // 验证预留数据未被 Clear 清除
    EXPECT_EQ(collector.reservationMgr_.GetPendingAdjustment("eid1"), 200);

    collector.OnReserveEnd();
}

} // namespace ubse::ssu::service::ut
