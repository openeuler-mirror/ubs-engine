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

#include "test_ubse_ssu_scheduler.h"
#include <securec.h>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace ubse::ssu::scheduler::ut {

using namespace ubse::adapter_plugins::ssu::def;

UbseSsuDevInfo TestUbseSsuScheduler::MakeDev(const std::string &eid, uint64_t totalBytes, uint64_t usedBytes,
                                             uint32_t nsCount, UbseSsuState state)
{
    UbseSsuDevInfo dev;
    dev.subSystem.eid = eid;
    dev.totalBytes = totalBytes;
    dev.usedBytes = usedBytes;
    dev.state = state;
    for (uint32_t i = 0; i < nsCount; ++i) {
        UbseSsuDevNameSpace ns;
        ns.namespaceId = i + 1;
        dev.nameSpaces.push_back(ns);
    }
    return dev;
}

UbseSsuDevInfo TestUbseSsuScheduler::MakeDevWithTenant(const std::string &eid, uint64_t totalBytes, uint64_t usedBytes,
                                                       const std::string &tenant, uint32_t nsCount, UbseSsuState state)
{
    // 租户隔离的设备 nsCount至少要 >= 1（严格三态隔离要求全部NS的tenant一致）
    EXPECT_GE(nsCount, 1u);
    auto dev = MakeDev(eid, totalBytes, usedBytes, nsCount, state);
    for (auto &ns : dev.nameSpaces) {
        strncpy_s(ns.customData.tenant, sizeof(ns.customData.tenant), tenant.c_str(), tenant.size());
    }
    return dev;
}

void TestUbseSsuScheduler::SetUp()
{
    Test::SetUp();
}

void TestUbseSsuScheduler::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * PreCheckHandler 在 allocSize 为 0 时拒绝请求
 * 测试步骤：
 * 1、构造 allocSize=0、nsNum=2 的请求
 * 2、调用 PreCheckHandler::Handle
 * 预期结果：
 * 1、返回 false
 * 2、ctx.result.ret == INVALID_PARAM
 */
TEST_F(TestUbseSsuScheduler, PreCheckAllocSizeZero)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 0;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);

    PreCheckHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INVALID_PARAM);
}

/*
 * 用例描述：
 * PreCheckHandler 在 nsNum 为 0 时拒绝请求
 * 测试步骤：
 * 1、构造 allocSize=1024、nsNum=0 的请求
 * 2、调用 PreCheckHandler::Handle
 * 预期结果：
 * 1、返回 false
 * 2、ctx.result.ret == INVALID_PARAM
 */
TEST_F(TestUbseSsuScheduler, PreCheckNsNumZero)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 0;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);

    PreCheckHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INVALID_PARAM);
}

/*
 * 用例描述：
 * PreCheckHandler 在设备列表为空时返回空间不足
 * 测试步骤：
 * 1、构造空设备列表
 * 2、调用 PreCheckHandler::Handle
 * 预期结果：
 * 1、返回 false
 * 2、ctx.result.ret == INSUFFICIENT_SPACE
 */
TEST_F(TestUbseSsuScheduler, PreCheckEmptyDevices)
{
    std::vector<UbseSsuDevInfo> devs;
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);

    PreCheckHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INSUFFICIENT_SPACE);
}

/*
 * 用例描述：
 * PreCheckHandler 在所有设备都离线时返回空间不足
 * 测试步骤：
 * 1、构造 2 个 OFFLINE 设备
 * 2、调用 PreCheckHandler::Handle
 * 预期结果：
 * 1、返回 false
 * 2、ctx.result.ret == INSUFFICIENT_SPACE
 */
TEST_F(TestUbseSsuScheduler, PreCheckAllOfflineDevices)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 0, 0, UbseSsuState::OFFLINE),
                                        MakeDev("eid-2", 1024, 0, 0, UbseSsuState::OFFLINE)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);

    PreCheckHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INSUFFICIENT_SPACE);
}

/*
 * 用例描述：
 * PreCheckHandler 在已使用命名空间数量达上限的设备会被过滤掉
 * 测试步骤：
 * 1、构造 3 个设备，其中 1 个 nameSpaces 数量为 128
 * 2、调用 PreCheckHandler::Handle
 * 预期结果：
 * 1、返回 false
 * 2、ctx.result.ret == INSUFFICIENT_SPACE（剩余 2 个设备 < nsNum 3）
 */
TEST_F(TestUbseSsuScheduler, PreCheckFilterMaxNsDevices)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 512, 128), MakeDev("eid-2", 1024, 0),
                                        MakeDev("eid-3", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1536; // 需同时为lbaSize(512)和nsNum(3)的整数倍，以通过PreCheck整除校验
    req.nsNum = 3;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);

    PreCheckHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INSUFFICIENT_SPACE);
}

/*
 * 用例描述：
 * PreCheckHandler 在过滤后剩余设备数量不足 nsNum 时返回失败
 * 测试步骤：
 * 1、构造 1 个在线设备，请求 nsNum=2
 * 2、调用 PreCheckHandler::Handle
 * 预期结果：
 * 1、返回 false
 * 2、ctx.result.ret == INSUFFICIENT_SPACE
 */
TEST_F(TestUbseSsuScheduler, PreCheckInsufficientFilteredDevices)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);

    PreCheckHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INSUFFICIENT_SPACE);
    EXPECT_NE(ctx.result.msg.find("devices available"), std::string::npos);
}

/*
 * 用例描述：
 * PreCheckHandler 正常情况下完成设备过滤并按规则排序
 * 测试步骤：
 * 1、构造 3 个在线设备，剩余空间和已用命名空间数量各异
 * 2、调用 PreCheckHandler::Handle
 * 预期结果：
 * 1、返回 true
 * 2、ctx.selectedDevs 按 freeBytes 降序、nsCount 升序排序
 */
TEST_F(TestUbseSsuScheduler, PreCheckSuccessSortByFreeAndNsCount)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-low", 1024, 512, 2),  // free 512, nsCount 2
                                        MakeDev("eid-high", 1024, 0, 1),   // free 1024, nsCount 1
                                        MakeDev("eid-mid", 2048, 1024, 0), // free 1024, nsCount 0
                                        MakeDev("eid-offline", 1024, 0, 0, UbseSsuState::OFFLINE)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);

    PreCheckHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.selectedDevs.size(), 3u);
    // 期望：eid-mid (free 1024, nsCount 0) > eid-high (free 1024, nsCount 1) > eid-low (free 512, nsCount 2)
    EXPECT_EQ(ctx.selectedDevs[0].eid, "eid-mid");
    EXPECT_EQ(ctx.selectedDevs[1].eid, "eid-high");
    EXPECT_EQ(ctx.selectedDevs[2].eid, "eid-low");
}

/*
 * 用例描述：
 * PreCheckHandler 在 STRIPED 模式下 allocSize 不能被 nsNum 整除时返回参数非法
 * 测试步骤：
 * 1、构造 3 个在线设备
 * 2、STRIPED 模式请求 allocSize=1024、nsNum=3（1024 % 3 != 0）
 * 3、调用 PreCheckHandler::Handle
 * 预期结果：
 * 1、返回 false
 * 2、ctx.result.ret == INVALID_PARAM
 * 3、ctx.selectedDevs 为空
 */
TEST_F(TestUbseSsuScheduler, PreCheckStripedAllocSizeNotDivisible)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 0), MakeDev("eid-2", 1024, 0),
                                        MakeDev("eid-3", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 3;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::STRIPED;
    UbseSsuAllocationContext ctx(devs, req);

    PreCheckHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INVALID_PARAM);
    EXPECT_TRUE(ctx.selectedDevs.empty());
}

/*
 * 用例描述：
 * PreCheckHandler 在 STRIPED 模式下 nsNum < 2 时返回参数非法
 * （条带化至少需要2个成员NS，RAID0下限；nsNum=1时任何RAID级别都无法挂载）
 * 测试步骤：
 * 1、构造 1 个在线设备
 * 2、STRIPED 模式请求 allocSize=1024、nsNum=1
 * 3、调用 PreCheckHandler::Handle
 * 预期结果：
 * 1、返回 false
 * 2、ctx.result.ret == INVALID_PARAM
 * 3、ctx.selectedDevs 为空
 */
TEST_F(TestUbseSsuScheduler, PreCheckStripedNsNumLessThanTwo)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 1;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::STRIPED;
    UbseSsuAllocationContext ctx(devs, req);

    PreCheckHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INVALID_PARAM);
    EXPECT_TRUE(ctx.selectedDevs.empty());
}

/*
 * 用例描述：
 * PreCheckHandler 在 LINEAR 模式下 allocSize 不能被 nsNum 整除时仍应通过校验
 * （线性分配按各设备剩余空间权重分配，不要求均分）
 * 测试步骤：
 * 1、构造 3 个在线设备
 * 2、LINEAR 模式请求 allocSize=1024、nsNum=3（1024 % 3 != 0）
 * 3、调用 PreCheckHandler::Handle
 * 预期结果：
 * 1、返回 true
 * 2、ctx.selectedDevs 包含 3 个设备
 */
TEST_F(TestUbseSsuScheduler, PreCheckLinearAllocSizeNotDivisible)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 0), MakeDev("eid-2", 1024, 0),
                                        MakeDev("eid-3", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 3;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);

    PreCheckHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.selectedDevs.size(), 3u);
}

/*
 * 用例描述：
 * PreCheckHandler 在 STRIPED 模式下 allocSize 能被 nsNum 和 lbaSize 整除、
 * 但 singleNsSize 不能被 lbaSize 整除时返回参数非法
 * 测试步骤：
 * 1、构造 2 个在线设备
 * 2、STRIPED 模式请求 allocSize=2560、nsNum=2、lbaSize=512
 *    （2560%2==0、2560%512==0，但 singleNsSize=1280，1280%512!=0）
 * 3、调用 PreCheckHandler::Handle
 * 预期结果：
 * 1、返回 false
 * 2、ctx.result.ret == INVALID_PARAM
 * 3、ctx.selectedDevs 为空
 */
TEST_F(TestUbseSsuScheduler, PreCheckStripedSingleNsSizeNotLbaMultiple)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 4096, 0), MakeDev("eid-2", 4096, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 2560;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::STRIPED;
    UbseSsuAllocationContext ctx(devs, req);

    PreCheckHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INVALID_PARAM);
    EXPECT_TRUE(ctx.selectedDevs.empty());
}

/*
 * 用例描述：
 * AlgorithmHandler 在 lbaSize 为 0 时返回参数非法
 * 测试步骤：
 * 1、手动准备 ctx.selectedDevs
 * 2、lbaSize=0、strategy=LINEAR 调用 AlgorithmHandler::Handle
 * 预期结果：
 * 1、返回 false
 * 2、ctx.result.ret == INVALID_PARAM
 */
TEST_F(TestUbseSsuScheduler, AlgorithmLinearLbaSizeZero)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 1;
    req.lbaSize = 0;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0, ""});

    UbseSsuAllocateAlgorithmHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INVALID_PARAM);
}

/*
 * 用例描述：
 * AlgorithmHandler 在 LINEAR 模式、可用空间不足时返回空间不足
 * 测试步骤：
 * 1、手动准备 ctx.selectedDevs（2 个设备，每个剩余 256 字节）
 * 2、请求 2048 字节
 * 3、调用 AlgorithmHandler::Handle
 * 预期结果：
 * 1、返回 false
 * 2、ctx.result.ret == INSUFFICIENT_SPACE
 */
TEST_F(TestUbseSsuScheduler, AlgorithmLinearInsufficientSpace)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 768), MakeDev("eid-2", 1024, 768)};
    UbseSsuAllocRequest req;
    req.allocSize = 2048;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 256, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-2", 256, 512, 0, ""});

    UbseSsuAllocateAlgorithmHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INSUFFICIENT_SPACE);
}

/*
 * 用例描述：
 * AlgorithmHandler 在 LINEAR 模式下成功完成均衡分配
 * 测试步骤：
 * 1、ctx.selectedDevs 为 2 个设备，各有 1024 字节
 * 2、请求 1024 字节、扇区 512
 * 3、调用 AlgorithmHandler::Handle
 * 预期结果：
 * 1、返回 true
 * 2、ctx.result.ret == OK
 * 3、eidNsSizeList.size() == nsNum
 * 4、eidNsSizeList 总和等于请求大小
 */
TEST_F(TestUbseSsuScheduler, AlgorithmLinearSuccess)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 0), MakeDev("eid-2", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0, ""});

    UbseSsuAllocateAlgorithmHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::OK);
    EXPECT_EQ(ctx.result.nsNum, 2u);
    ASSERT_EQ(ctx.result.eidNsSizeList.size(), 2u);

    uint64_t totalAllocated = 0;
    for (const auto &p : ctx.result.eidNsSizeList) {
        totalAllocated += p.second;
        EXPECT_EQ(p.second % 512, 0u);
    }
    EXPECT_EQ(totalAllocated, 1024u);
}

/*
 * 用例描述：
 * AlgorithmHandler 在 LINEAR 模式下请求大小按扇区向上对齐
 * 测试步骤：
 * 1、ctx.selectedDevs 为 2 个设备，各有 1024 字节
 * 2、请求 600 字节、扇区 512
 * 3、调用 AlgorithmHandler::Handle
 * 预期结果：
 * 1、返回 true
 * 2、eidNsSizeList 总和等于 1024（向上对齐）
 */
TEST_F(TestUbseSsuScheduler, AlgorithmLinearAlignUpToSector)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 0), MakeDev("eid-2", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 600;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0, ""});

    UbseSsuAllocateAlgorithmHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::OK);
    uint64_t totalAllocated = 0;
    for (const auto &p : ctx.result.eidNsSizeList) {
        totalAllocated += p.second;
    }
    EXPECT_EQ(totalAllocated, 1024u);
}

/*
 * 用例描述：
 * AlgorithmHandler 在 STRIPED 模式下成功完成条带化分配
 * 测试步骤：
 * 1、ctx.selectedDevs 为 2 个设备，各有 1024 字节
 * 2、请求 1024 字节、nsNum=2
 * 3、调用 AlgorithmHandler::Handle
 * 预期结果：
 * 1、返回 true
 * 2、每个 eid 分配大小为 512
 */
TEST_F(TestUbseSsuScheduler, AlgorithmStripedSuccess)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 2048, 0), MakeDev("eid-2", 2048, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.strategy = UbseSsuAllocStrategy::STRIPED;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0, ""});

    UbseSsuAllocateAlgorithmHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::OK);
    ASSERT_EQ(ctx.result.eidNsSizeList.size(), 2u);
    for (const auto &p : ctx.result.eidNsSizeList) {
        EXPECT_EQ(p.second, 512u);
    }
}

/*
 * 用例描述：
 * AlgorithmHandler 在 STRIPED 模式下，单设备空间不足时返回空间不足
 * 测试步骤：
 * 1、ctx.selectedDevs 为 2 个设备（第二个仅剩 256）
 * 2、请求 1024
 * 3、调用 AlgorithmHandler::Handle
 * 预期结果：
 * 1、返回 false
 * 2、ctx.result.ret == INSUFFICIENT_SPACE
 */
TEST_F(TestUbseSsuScheduler, AlgorithmStripedInsufficientSpace)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 0), MakeDev("eid-2", 256, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.strategy = UbseSsuAllocStrategy::STRIPED;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-2", 256, 512, 0, ""});

    UbseSsuAllocateAlgorithmHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INSUFFICIENT_SPACE);
}

/*
 * 用例描述：
 * AlgorithmHandler 在 STRIPED 模式下，allocSize 可被 nsNum 整除时均分分配
 * 测试步骤：
 * 1、ctx.selectedDevs 为 2 个设备，各有 4096 字节
 * 2、请求 1024、nsNum=2（singleNsSize=512）
 * 3、调用 AlgorithmHandler::Handle
 * 预期结果：
 * 1、返回 true
 * 2、每个 eid 分配大小为 512
 */
TEST_F(TestUbseSsuScheduler, AlgorithmStripedEvenlyDivisible)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 4096, 0), MakeDev("eid-2", 4096, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.strategy = UbseSsuAllocStrategy::STRIPED;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 4096, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-2", 4096, 512, 0, ""});

    UbseSsuAllocateAlgorithmHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::OK);
    ASSERT_EQ(ctx.result.eidNsSizeList.size(), 2u);
    for (const auto &p : ctx.result.eidNsSizeList) {
        EXPECT_EQ(p.second, 512u);
    }
}

/*
 * 用例描述：
 * AlgorithmHandler 成功分配后，ctx.selectedDevs 仅保留被选中的设备
 * 测试步骤：
 * 1、ctx.selectedDevs 为 3 个设备
 * 2、请求 1024、nsNum=2、扇区 512
 * 3、调用 AlgorithmHandler::Handle
 * 预期结果：
 * 1、返回 true
 * 2、ctx.selectedDevs.size() == 2（仅保留被分配的 eid）
 */
TEST_F(TestUbseSsuScheduler, AlgorithmSelectedDevsFilteredAfterSuccess)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 0), MakeDev("eid-2", 1024, 0),
                                        MakeDev("eid-3", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-3", 1024, 512, 0, ""});

    UbseSsuAllocateAlgorithmHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.selectedDevs.size(), 2u);
    for (const auto &d : ctx.selectedDevs) {
        EXPECT_TRUE(d.eid == "eid-1" || d.eid == "eid-2");
    }
}

/*
 * 用例描述：
 * UbseSsuScheduler 默认构造函数会添加 PreCheck 与 Algorithm 过滤器
 * 测试步骤：
 * 1、构造 UbseSsuScheduler
 * 2、构造合法请求
 * 3、Execute 调用
 * 预期结果：
 * 1、返回 OK
 * 2、ctx.result.eidNsSizeList 大小为 nsNum
 */
TEST_F(TestUbseSsuScheduler, SchedulerExecuteSuccess)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 2048, 0), MakeDev("eid-2", 2048, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);

    UbseSsuScheduler scheduler;
    auto ret = scheduler.Execute(ctx);

    EXPECT_EQ(ret, UbseSsuAllocRetCode::OK);
    EXPECT_EQ(ctx.result.nsNum, 2u);
    ASSERT_EQ(ctx.result.eidNsSizeList.size(), 2u);
}

/*
 * 用例描述：
 * UbseSsuScheduler 在预检查失败时不会执行后续过滤器
 * 测试步骤：
 * 1、构造 UbseSsuScheduler
 * 2、构造非法请求（allocSize=0）
 * 3、Execute 调用
 * 预期结果：
 * 1、返回 INVALID_PARAM
 * 2、ctx.selectedDevs 为空（PreCheck 未填充，Algorithm 未执行）
 */
TEST_F(TestUbseSsuScheduler, SchedulerExecuteParamInvalid)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 2048, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 0;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);

    UbseSsuScheduler scheduler;
    auto ret = scheduler.Execute(ctx);

    EXPECT_EQ(ret, UbseSsuAllocRetCode::INVALID_PARAM);
    EXPECT_TRUE(ctx.selectedDevs.empty());
}

/*
 * 用例描述：
 * UbseSsuScheduler 在 STRIPED 模式下可正常完成分配
 * 测试步骤：
 * 1、构造 UbseSsuScheduler
 * 2、构造 STRIPED 请求
 * 3、Execute 调用
 * 预期结果：
 * 1、返回 OK
 * 2、eidNsSizeList 数量为 nsNum
 */
TEST_F(TestUbseSsuScheduler, SchedulerExecuteStripedSuccess)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 4096, 0), MakeDev("eid-2", 4096, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::STRIPED;
    UbseSsuAllocationContext ctx(devs, req);

    UbseSsuScheduler scheduler;
    auto ret = scheduler.Execute(ctx);

    EXPECT_EQ(ret, UbseSsuAllocRetCode::OK);
    ASSERT_EQ(ctx.result.eidNsSizeList.size(), 2u);
}

/*
 * 用例描述：
 * UbseSsuScheduler::AddFilter 可以向 POST_CHECK 组添加自定义过滤器
 * 测试步骤：
 * 1、自定义一个 UbseFilter，将 ctx.result.msg 标记为"CUSTOM"
 * 2、向 POST_CHECK 组添加
 * 3、Execute 调用
 * 预期结果：
 * 1、返回 OK
 * 2、ctx.result.msg 包含 "CUSTOM"
 */
TEST_F(TestUbseSsuScheduler, SchedulerAddCustomFilter)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 2048, 0), MakeDev("eid-2", 2048, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);

    UbseSsuScheduler scheduler;
    scheduler.AddFilter(UbseSsuFilterGroupId::POST_CHECK, std::make_shared<CustomFilter>());
    auto ret = scheduler.Execute(ctx);

    EXPECT_EQ(ret, UbseSsuAllocRetCode::OK);
    EXPECT_NE(ctx.result.msg.find("CUSTOM"), std::string::npos);
}

/*
 * 用例描述：
 * UbseSsuScheduler::AddFilter 传入 nullptr 时不会使调度器崩溃
 * 测试步骤：
 * 1、构造 UbseSsuScheduler
 * 2、AddFilter 传入 nullptr
 * 3、Execute 调用
 * 预期结果：
 * 1、返回 OK（仅默认 filter 生效）
 */
TEST_F(TestUbseSsuScheduler, SchedulerAddNullFilterIgnored)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 2048, 0), MakeDev("eid-2", 2048, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);

    UbseSsuScheduler scheduler;
    scheduler.AddFilter(UbseSsuFilterGroupId::POST_CHECK, nullptr);
    auto ret = scheduler.Execute(ctx);

    EXPECT_EQ(ret, UbseSsuAllocRetCode::OK);
}

/*
 * 用例描述：
 * UbseSsuTenantIsolationFilter 在请求tenant为空时过滤掉所有带租户标识的设备
 * 测试步骤：
 * 1、构造tenant不同的设备列表（均带租户标识）
 * 2、请求tenant为空
 * 3、调用UbseSsuTenantIsolationFilter::Handle
 * 预期结果：
 * 1、返回false
 * 2、ctx.result.ret == INSUFFICIENT_SPACE
 * 3、selectedDevs为空（带租户标识的设备均被过滤掉）
 */
TEST_F(TestUbseSsuScheduler, TenantIsolationEmptyReqTenantFiltersTenantDevices)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-2", 1024, 0, "tenant-B")};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0, "tenant-B"});

    UbseSsuTenantIsolationFilter filter;
    bool ok = filter.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INSUFFICIENT_SPACE);
    EXPECT_EQ(ctx.selectedDevs.size(), 0u);
}

/*
 * 用例描述：
 * UbseSsuTenantIsolationFilter 在请求tenant为空时保留未关联租户的设备
 * 测试步骤：
 * 1、构造3个设备：tenant-A、空tenant、tenant-B
 * 2、请求tenant为空，nsNum=1
 * 3、调用UbseSsuTenantIsolationFilter::Handle
 * 预期结果：
 * 1、返回true
 * 2、selectedDevs仅保留空tenant的设备
 */
TEST_F(TestUbseSsuScheduler, TenantIsolationEmptyReqTenantKeepsNonTenantDevices)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-2", 1024, 0, ""),
                                        MakeDevWithTenant("eid-3", 1024, 0, "tenant-B")};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 1;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-3", 1024, 512, 0, "tenant-B"});

    UbseSsuTenantIsolationFilter filter;
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.selectedDevs.size(), 1u);
    EXPECT_EQ(ctx.selectedDevs[0].eid, "eid-2");
    EXPECT_TRUE(ctx.selectedDevs[0].tenant.empty());
}

/*
 * 用例描述：
 * UbseSsuTenantIsolationFilter 过滤掉Tenant不匹配的设备
 * 测试步骤：
 * 1、构造3个设备：tenant-A、tenant-A、tenant-B
 * 2、请求Tenant为tenant-A
 * 3、调用UbseSsuTenantIsolationFilter::Handle
 * 预期结果：
 * 1、返回true
 * 2、selectedDevs仅保留tenant-A的2个设备
 */
TEST_F(TestUbseSsuScheduler, TenantIsolationFiltersMismatchedDevices)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-2", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-3", 1024, 0, "tenant-B")};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-3", 1024, 512, 0, "tenant-B"});

    UbseSsuTenantIsolationFilter filter;
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.selectedDevs.size(), 2u);
    for (const auto &dev : ctx.selectedDevs) {
        EXPECT_EQ(dev.tenant, "tenant-A");
    }
}

/*
 * 用例描述：
 * UbseSsuTenantIsolationFilter 在tenant不匹配设备不足时返回失败
 * 测试步骤：
 * 1、构造2个设备：tenant-A、tenant-B
 * 2、请求tenant为tenant-A，nsNum=2
 * 3、调用UbseSsuTenantIsolationFilter::Handle
 * 预期结果：
 * 1、返回false
 * 2、ctx.result.ret == INSUFFICIENT_SPACE
 */
TEST_F(TestUbseSsuScheduler, TenantIsolationInsufficientMatchedDevices)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-2", 1024, 0, "tenant-B")};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0, "tenant-B"});

    UbseSsuTenantIsolationFilter filter;
    bool ok = filter.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INSUFFICIENT_SPACE);
}

/*
 * 用例描述：
 * UbseSsuTenantIsolationFilter 保留tenant为空的设备（未关联tenant的设备可分配给任何租户）
 * 测试步骤：
 * 1、构造3个设备：tenant-A、空tenant、tenant-B
 * 2、请求tenant为tenant-A，nsNum=2
 * 3、调用UbseSsuTenantIsolationFilter::Handle
 * 预期结果：
 * 1、返回true
 * 2、selectedDevs保留tenant-A和空tenant的设备
 */
TEST_F(TestUbseSsuScheduler, TenantIsolationKeepsEmptyTenantDevices)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-2", 1024, 0, ""),
                                        MakeDevWithTenant("eid-3", 1024, 0, "tenant-B")};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-3", 1024, 512, 0, "tenant-B"});

    UbseSsuTenantIsolationFilter filter;
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.selectedDevs.size(), 2u);
    EXPECT_EQ(ctx.selectedDevs[0].eid, "eid-1");
    EXPECT_EQ(ctx.selectedDevs[1].eid, "eid-2");
}

/*
 * 用例描述：
 * UbseSsuTenantIsolationFilter 租户请求排除非租户专用设备（有NS且全为空tenant），
 * 确保租户与非租户的namespace不会落在同一个SSU设备上
 * 测试步骤：
 * 1、构造2个设备：eid-1非租户专用（已有1个非租户NS）、eid-2干净（无NS）
 * 2、请求tenant-A，nsNum=1
 * 3、调用UbseSsuTenantIsolationFilter::Handle
 * 预期结果：
 * 1、返回true
 * 2、selectedDevs仅保留干净的eid-2
 */
TEST_F(TestUbseSsuScheduler, TenantIsolationTenantReqExcludesNonTenantOwnedDevices)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 512, 1), MakeDev("eid-2", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 1;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 512, 512, 1, ""});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0, ""});

    UbseSsuTenantIsolationFilter filter;
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    ASSERT_EQ(ctx.selectedDevs.size(), 1u);
    EXPECT_EQ(ctx.selectedDevs[0].eid, "eid-2");
}

/*
 * 用例描述：
 * UbseSsuTenantIsolationFilter 非租户请求保留非租户专用设备（有NS且全为空tenant）
 * 测试步骤：
 * 1、构造2个设备：eid-1非租户专用（已有1个非租户NS）、eid-2租户专用
 * 2、请求tenant为空，nsNum=1
 * 3、调用UbseSsuTenantIsolationFilter::Handle
 * 预期结果：
 * 1、返回true
 * 2、selectedDevs仅保留非租户专用的eid-1
 */
TEST_F(TestUbseSsuScheduler, TenantIsolationEmptyReqTenantKeepsNonTenantOwnedDevices)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 512, 1), MakeDevWithTenant("eid-2", 1024, 0, "tenant-A")};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 1;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 512, 512, 1, ""});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 1, "tenant-A"});

    UbseSsuTenantIsolationFilter filter;
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    ASSERT_EQ(ctx.selectedDevs.size(), 1u);
    EXPECT_EQ(ctx.selectedDevs[0].eid, "eid-1");
}

/*
 * 用例描述：
 * UbseSsuScheduler 集成严格三态隔离：非租户请求先占用的设备（有NS且全为空tenant）
 * 对后续租户请求不可见，租户的namespace不会与非租户的namespace落在同一设备
 * 测试步骤：
 * 1、构造2个设备：eid-1非租户专用（已有1个非租户NS）、eid-2干净
 * 2、请求tenant-A，nsNum=1
 * 3、调用UbseSsuScheduler::Execute
 * 预期结果：
 * 1、返回OK
 * 2、分配结果落在干净的eid-2上（旧实现会把eid-1误判为未隔离设备并混入租户NS）
 */
TEST_F(TestUbseSsuScheduler, SchedulerTenantStrictIsolationIntegration)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 2048, 0, 1), MakeDev("eid-2", 2048, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 1;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);

    UbseSsuScheduler scheduler;
    auto ret = scheduler.Execute(ctx);

    EXPECT_EQ(ret, UbseSsuAllocRetCode::OK);
    ASSERT_EQ(ctx.result.eidNsSizeList.size(), 1u);
    EXPECT_EQ(ctx.result.eidNsSizeList[0].first, "eid-2");
}

/*
 * 用例描述：
 * UbseSsuScheduler 集成tenant隔离策略，仅分配匹配tenant的设备
 * 测试步骤：
 * 1、构造4个设备：2个tenant-A、2个tenant-B
 * 2、请求tenant为tenant-A，nsNum=2
 * 3、调用UbseSsuScheduler::Execute
 * 预期结果：
 * 1、返回OK
 * 2、分配结果中的设备均属于tenant-A
 */
TEST_F(TestUbseSsuScheduler, SchedulerTenantIsolationIntegration)
{
    std::vector<UbseSsuDevInfo> devs = {
        MakeDevWithTenant("eid-1", 2048, 0, "tenant-A"), MakeDevWithTenant("eid-2", 2048, 0, "tenant-A"),
        MakeDevWithTenant("eid-3", 2048, 0, "tenant-B"), MakeDevWithTenant("eid-4", 2048, 0, "tenant-B")};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);

    UbseSsuScheduler scheduler;
    auto ret = scheduler.Execute(ctx);

    EXPECT_EQ(ret, UbseSsuAllocRetCode::OK);
    ASSERT_EQ(ctx.result.eidNsSizeList.size(), 2u);
    for (const auto &p : ctx.result.eidNsSizeList) {
        EXPECT_TRUE(p.first == "eid-1" || p.first == "eid-2");
    }
}

/*
 * 用例描述：
 * NORMAL策略允许一个设备上分配多个NS（nsNum可大于设备数）
 * 测试步骤：
 * 1、构造2个设备，请求nsNum=4、allocSize=2048（每NS 512字节）
 * 2、调用AlgorithmHandler::Handle（strategy=NORMAL）
 * 预期结果：
 * 1、返回true、ret==OK
 * 2、eidNsSizeList.size()==4，总和==2048，每个NS为512字节的整数倍
 * 3、同eid设备出现多次（一个设备分配多个NS）
 */
TEST_F(TestUbseSsuScheduler, AlgorithmNormalAllowMultiNsPerDev)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 2048, 0), MakeDev("eid-2", 2048, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 2048;
    req.nsNum = 4;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::NORMAL;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 2048, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-2", 2048, 512, 0, ""});

    UbseSsuAllocateAlgorithmHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::OK);
    EXPECT_EQ(ctx.result.nsNum, 4u);
    ASSERT_EQ(ctx.result.eidNsSizeList.size(), 4u);

    uint64_t totalAllocated = 0;
    std::unordered_set<std::string> eidSet;
    for (const auto &p : ctx.result.eidNsSizeList) {
        eidSet.insert(p.first);
        totalAllocated += p.second;
        EXPECT_EQ(p.second % 512, 0u);
    }
    EXPECT_EQ(totalAllocated, 2048u);
    // 4个NS落在2个设备上，必然有设备承载多个NS
    EXPECT_LT(eidSet.size(), 4u);
}

/*
 * 用例描述：
 * NORMAL策略NS等大：均分向上对齐到扇区，每个NS大小一致
 * 测试步骤：
 * 1、单设备（eid-1）5120字节，请求nsNum=3、allocSize=2560（perNs=853→向上对齐512=1024）
 * 2、调用AlgorithmHandler::Handle（strategy=NORMAL）
 * 预期结果：
 * 1、返回true、ret==OK
 * 2、3个NS均为1024字节（等大），总和==3072（向上对齐）
 */
TEST_F(TestUbseSsuScheduler, AlgorithmNormalEqualNsSize)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 5120, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 2560;
    req.nsNum = 3;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::NORMAL;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 5120, 512, 0, ""});

    UbseSsuAllocateAlgorithmHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::OK);
    ASSERT_EQ(ctx.result.eidNsSizeList.size(), 3u);
    EXPECT_EQ(ctx.result.eidNsSizeList[0].second, 1024u);
    EXPECT_EQ(ctx.result.eidNsSizeList[1].second, 1024u);
    EXPECT_EQ(ctx.result.eidNsSizeList[2].second, 1024u);
    uint64_t totalAllocated = 0;
    for (const auto &p : ctx.result.eidNsSizeList) {
        totalAllocated += p.second;
    }
    EXPECT_EQ(totalAllocated, 3072u);
}

/*
 * 用例描述：
 * NORMAL策略空间不足时返回失败（设备总剩余空间不足）
 * 测试步骤：
 * 1、单设备（eid-1）1024字节，请求nsNum=4、allocSize=4096
 * 2、调用AlgorithmHandler::Handle（strategy=NORMAL）
 * 预期结果：
 * 1、返回false
 * 2、ctx.result.ret == INSUFFICIENT_SPACE
 */
TEST_F(TestUbseSsuScheduler, AlgorithmNormalInsufficientSpace)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 4096;
    req.nsNum = 4;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::NORMAL;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0, ""});

    UbseSsuAllocateAlgorithmHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INSUFFICIENT_SPACE);
}

/*
 * 用例描述：
 * NORMAL策略权重分配：每设备NS数与其剩余空间成正比（参考线性负载均衡），大设备多放
 * 测试步骤：
 * 1、3个设备：6000/3000/1000字节，请求nsNum=4、allocSize=4096（base=1024）
 * 2、调用AlgorithmHandler::Handle（strategy=NORMAL）
 * 预期结果：
 * 1、返回true、ret==OK
 * 2、eidNsSizeList.size()==4，权重配额：eid-1得2个、eid-2得1个，剩余1个补给eid-1（共3个）
 */
TEST_F(TestUbseSsuScheduler, AlgorithmNormalWeightedByFreeBytes)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 6000, 0), MakeDev("eid-2", 3000, 0),
                                        MakeDev("eid-3", 1000, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 4096;
    req.nsNum = 4;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::NORMAL;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 6000, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-2", 3000, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-3", 1000, 512, 0, ""});

    UbseSsuAllocateAlgorithmHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::OK);
    ASSERT_EQ(ctx.result.eidNsSizeList.size(), 4u);
    std::unordered_map<std::string, uint32_t> eidCnt;
    uint64_t totalAllocated = 0;
    for (const auto &p : ctx.result.eidNsSizeList) {
        eidCnt[p.first]++;
        totalAllocated += p.second;
        EXPECT_EQ(p.second % 512, 0u);
    }
    EXPECT_EQ(totalAllocated, 4096u);
    // 权重：6000:3000:1000 → 配额2:1:0，剩余1个补给最大设备，共3:1:0
    EXPECT_EQ(eidCnt["eid-1"], 3u);
    EXPECT_EQ(eidCnt["eid-2"], 1u);
    EXPECT_EQ(eidCnt.count("eid-3"), 0u);
}

/*
 * 用例描述：
 * NORMAL策略下设备数可少于nsNum，PreCheck不拦截（同设备多NS）
 * 测试步骤：
 * 1、1个在线设备，请求nsNum=4（strategy=NORMAL）
 * 2、调用UbseSsuScheduler::Execute
 * 预期结果：
 * 1、返回OK
 * 2、eidNsSizeList.size()==4
 */
TEST_F(TestUbseSsuScheduler, SchedulerExecuteNormalMultiNsPerDev)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 4096, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 4096;
    req.nsNum = 4;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::NORMAL;
    UbseSsuAllocationContext ctx(devs, req);

    UbseSsuScheduler scheduler;
    auto ret = scheduler.Execute(ctx);

    EXPECT_EQ(ret, UbseSsuAllocRetCode::OK);
    ASSERT_EQ(ctx.result.eidNsSizeList.size(), 4u);
    for (const auto &p : ctx.result.eidNsSizeList) {
        EXPECT_EQ(p.first, "eid-1");
        EXPECT_EQ(p.second % 512, 0u);
    }
}

/*
 * 用例描述：
 * UbseSsuTenantQuotaFilter 单租户配额用尽时剔除未归属设备（软限制：仅保留本租户已归属设备）
 * 测试步骤：
 * 1、构造4个在线设备：2个tenant-A、1个tenant-B、1个未归属
 * 2、注入配额（perTenantPct=50），请求tenant-A，nsNum=2
 * 3、调用UbseSsuTenantQuotaFilter::Handle
 * 预期结果：
 * 1、返回true
 * 2、selectedDevs仅保留tenant-A的2个设备（未归属设备被剔除）
 */
TEST_F(TestUbseSsuScheduler, TenantQuotaPerTenantReachedStripsUnclaimed)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-2", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-3", 1024, 0, "tenant-B"),
                                        MakeDev("eid-4", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-4", 1024, 512, 0, ""});

    UbseSsuTenantQuotaFilter filter(50);
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    ASSERT_EQ(ctx.selectedDevs.size(), 2u);
    for (const auto &dev : ctx.selectedDevs) {
        EXPECT_EQ(dev.tenant, "tenant-A");
    }
}

/*
 * 用例描述：
 * UbseSsuTenantQuotaFilter 预算充足时不裁剪未归属设备
 * 测试步骤：
 * 1、构造4个在线设备：1个tenant-A、1个tenant-B、2个未归属
 * 2、注入配额（perTenantPct=75），请求tenant-A
 *    （owned=1，limit=ceil(4*75/100)=3，预算allowedNew=2，未归属候选2<=预算不裁剪）
 * 3、调用UbseSsuTenantQuotaFilter::Handle
 * 预期结果：
 * 1、返回true
 * 2、selectedDevs保持不变（tenant-A设备与未归属设备均保留）
 */
TEST_F(TestUbseSsuScheduler, TenantQuotaNotReachedKeepsUnclaimed)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-2", 1024, 0, "tenant-B"),
                                        MakeDev("eid-3", 1024, 0), MakeDev("eid-4", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-3", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-4", 1024, 512, 0, ""});

    UbseSsuTenantQuotaFilter filter(75);
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.selectedDevs.size(), 3u);
}

/*
 * 用例描述：
 * UbseSsuTenantQuotaFilter 请求nsNum超过配额上限时前置快筛直接拒绝
 * 测试步骤：
 * 1、构造4个在线设备：1个tenant-A、3个未归属
 * 2、注入配额（perTenantPct=25），请求tenant-A，nsNum=2
 *    （limit=ceil(4*25/100)=1，LINEAR一设备一NS，nsNum=2>limit=1不可能在配额内满足）
 * 3、调用UbseSsuTenantQuotaFilter::Handle
 * 预期结果：
 * 1、返回false
 * 2、ctx.result.ret == INSUFFICIENT_SPACE
 */
TEST_F(TestUbseSsuScheduler, TenantQuotaReachedInsufficientOwnedDevices)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 0, "tenant-A"),
                                        MakeDev("eid-2", 1024, 0), MakeDev("eid-3", 1024, 0),
                                        MakeDev("eid-4", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-3", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-4", 1024, 512, 0, ""});

    UbseSsuTenantQuotaFilter filter(25);
    bool ok = filter.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INSUFFICIENT_SPACE);
}

/*
 * 用例描述：
 * UbseSsuTenantQuotaFilter 非租户请求（tenant为空）直接跳过，不受配额限制
 * 测试步骤：
 * 1、构造2个带租户标识的设备
 * 2、请求tenant为空，注入极低配额（1,1）
 * 3、调用UbseSsuTenantQuotaFilter::Handle
 * 预期结果：
 * 1、返回true
 * 2、selectedDevs保持不变
 */
TEST_F(TestUbseSsuScheduler, TenantQuotaEmptyRequestTenantNoop)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-2", 1024, 0, "tenant-B")};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 1;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 1, "tenant-B"});

    UbseSsuTenantQuotaFilter filter(1);
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.selectedDevs.size(), 2u);
}

/*
 * 用例描述：
 * UbseSsuTenantQuotaFilter 配额100（默认关闭）时租户圈占全部设备也不会误伤
 * 测试步骤：
 * 1、构造2个在线设备：均归属tenant-A（无未归属设备）
 * 2、注入配额（100,100），请求tenant-A，nsNum=2
 * 3、调用UbseSsuTenantQuotaFilter::Handle
 * 预期结果：
 * 1、返回true（超限判定命中但无未归属设备可剔除，软限制为no-op）
 * 2、selectedDevs保持2个设备
 */
TEST_F(TestUbseSsuScheduler, TenantQuotaPct100OwningAllDevicesNoop)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-2", 1024, 0, "tenant-A")};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 1, "tenant-A"});

    UbseSsuTenantQuotaFilter filter(100);
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.selectedDevs.size(), 2u);
}

/*
 * 用例描述：
 * UbseSsuTenantQuotaFilter 已达MAX_NS_PER_DEV的已归属设备计入配额分子
 * 测试步骤：
 * 1、构造4个在线设备：tenant-A的128NS设备（被Preprocess过滤出selectedDevs但已被占用）、
 *    tenant-A的普通设备、2个未归属
 * 2、注入配额（perTenantPct=50），请求tenant-A，nsNum=1
 * 3、调用UbseSsuTenantQuotaFilter::Handle
 * 预期结果：
 * 1、返回true（owned=2>=limit=2超限，剔除未归属设备，仅剩tenant-A普通设备）
 */
TEST_F(TestUbseSsuScheduler, TenantQuotaCountsFullNsDevices)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 0, "tenant-A", 128),
                                        MakeDevWithTenant("eid-2", 1024, 0, "tenant-A", 1),
                                        MakeDev("eid-3", 1024, 0), MakeDev("eid-4", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 1;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    // eid-1已达MAX_NS_PER_DEV，PreprocessDevices会将其过滤，selectedDevs不含eid-1
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-3", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-4", 1024, 512, 0, ""});

    UbseSsuTenantQuotaFilter filter(50);
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    ASSERT_EQ(ctx.selectedDevs.size(), 1u);
    EXPECT_EQ(ctx.selectedDevs[0].tenant, "tenant-A");
}

/*
 * 用例描述：
 * UbseSsuTenantQuotaFilter 离线设备不计入配额分母与分子
 * 测试步骤：
 * 1、构造4个设备：tenant-A在线1个、tenant-A离线1个、2个未归属在线
 * 2、注入配额（perTenantPct=67），请求tenant-A，nsNum=1
 *    （在线分母3：limit=ceil(3*67/100)=3、owned=1、预算allowedNew=2，未归属候选2<=预算不裁剪；
 *     若误计离线设备则分母4、limit=ceil(4*67/100)=3、owned=2、allowedNew=1，
 *     裁剪后仅剩2个设备，size断言可捕获误计）
 * 3、调用UbseSsuTenantQuotaFilter::Handle
 * 预期结果：
 * 1、返回true
 * 2、selectedDevs保持3个设备（不裁剪未归属设备）
 */
TEST_F(TestUbseSsuScheduler, TenantQuotaExcludesOfflineDevices)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 0, "tenant-A", 1),
                                        MakeDevWithTenant("eid-2", 1024, 0, "tenant-A", 1, UbseSsuState::OFFLINE),
                                        MakeDev("eid-3", 1024, 0), MakeDev("eid-4", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 1;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-3", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-4", 1024, 512, 0, ""});

    UbseSsuTenantQuotaFilter filter(67);
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.selectedDevs.size(), 3u);
}

/*
 * 用例描述：
 * UbseSsuTenantQuotaFilter 配额换算向上取整，正配额至少允许圈占1台设备；
 * 预算不足时按剩余空间降序保留未归属候选
 * 测试步骤：
 * 1、构造3个在线设备：均未归属，剩余空间分别为100/300/200
 * 2、注入配额（perTenantPct=30），请求tenant-A，nsNum=1
 *    （limit=ceil(3*30/100)=1，owned=0，预算allowedNew=1，未归属候选3>预算，
 *     按剩余空间降序保留最大的eid-2；若按floor取整则limit=0、allowedNew=0，
 *     未归属设备全被剔除后size=0直接失败）
 * 3、调用UbseSsuTenantQuotaFilter::Handle
 * 预期结果：
 * 1、返回true
 * 2、selectedDevs仅剩eid-2（租户可正常开垦第1台设备，且保留的是剩余空间最大者）
 */
TEST_F(TestUbseSsuScheduler, TenantQuotaCeilRoundingAllowsFirstDevice)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 0), MakeDev("eid-2", 1024, 0),
                                        MakeDev("eid-3", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 1;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 100, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-2", 300, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-3", 200, 512, 0, ""});

    UbseSsuTenantQuotaFilter filter(30);
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    ASSERT_EQ(ctx.selectedDevs.size(), 1u);
    EXPECT_EQ(ctx.selectedDevs[0].eid, "eid-2");
}

/*
 * 用例描述：
 * UbseSsuTenantQuotaFilter 注入非法配额百分比时归一化为100（关闭配额）
 * 测试步骤：
 * 1、构造4个在线设备：1个tenant-A、3个未归属
 * 2、注入非法配额0，请求tenant-A（若按0处理limit=0必超限）
 * 3、调用UbseSsuTenantQuotaFilter::Handle
 * 预期结果：
 * 1、返回true（归一化为100后limit=4，owned=1未超限）
 * 2、selectedDevs保持4个设备
 */
TEST_F(TestUbseSsuScheduler, TenantQuotaInvalidPctNormalized)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 0, "tenant-A"),
                                        MakeDev("eid-2", 1024, 0), MakeDev("eid-3", 1024, 0),
                                        MakeDev("eid-4", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 1;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-3", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-4", 1024, 512, 0, ""});

    UbseSsuTenantQuotaFilter filter(0);
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.selectedDevs.size(), 4u);
}

/*
 * 用例描述：
 * UbseSsuTenantQuotaFilter 首次大请求（nsNum=设备总数）不能一次性突破配额圈占全部设备
 * 测试步骤：
 * 1、构造4个在线设备：均未归属
 * 2、注入配额（perTenantPct=50），请求tenant-A，nsNum=4
 *    （limit=ceil(4*50/100)=2，LINEAR一设备一NS，nsNum=4>limit=2，前置快筛直接拒绝）
 * 3、调用UbseSsuTenantQuotaFilter::Handle
 * 预期结果：
 * 1、返回false
 * 2、ctx.result.ret == INSUFFICIENT_SPACE（若无配额机制，本次请求将圈占全部4台设备）
 */
TEST_F(TestUbseSsuScheduler, TenantQuotaFirstRequestLargeNsNumBlocked)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 0), MakeDev("eid-2", 1024, 0),
                                        MakeDev("eid-3", 1024, 0), MakeDev("eid-4", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 4096;
    req.nsNum = 4;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-3", 1024, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-4", 1024, 512, 0, ""});

    UbseSsuTenantQuotaFilter filter(50);
    bool ok = filter.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INSUFFICIENT_SPACE);
}

/*
 * 用例描述：
 * UbseSsuTenantQuotaFilter 预算不足时按剩余空间降序保留未归属候选，且本租户已归属设备不受裁剪影响
 * 测试步骤：
 * 1、构造5个在线设备：1个tenant-A、4个未归属（剩余空间400/800/200/100）
 * 2、注入配额（perTenantPct=60），请求tenant-A，nsNum=3
 *    （limit=ceil(5*60/100)=3，owned=1，预算allowedNew=2，未归属候选4>预算，
 *     按剩余空间降序保留eid-2(800)/eid-3(400)，剔除eid-4(200)/eid-5(100)）
 * 3、调用UbseSsuTenantQuotaFilter::Handle
 * 预期结果：
 * 1、返回true
 * 2、selectedDevs为eid-1(已归属)+eid-2+eid-3共3个设备，数量满足nsNum=3
 */
TEST_F(TestUbseSsuScheduler, TenantQuotaBudgetTrimKeepsLargestFree)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 0, "tenant-A"),
                                        MakeDev("eid-2", 1024, 0), MakeDev("eid-3", 1024, 0),
                                        MakeDev("eid-4", 1024, 0), MakeDev("eid-5", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 3;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-2", 800, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-3", 400, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-4", 200, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-5", 100, 512, 0, ""});

    UbseSsuTenantQuotaFilter filter(60);
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    ASSERT_EQ(ctx.selectedDevs.size(), 3u);
    std::unordered_set<std::string> eids;
    for (const auto &dev : ctx.selectedDevs) {
        eids.insert(dev.eid);
    }
    EXPECT_TRUE(eids.count("eid-1") > 0);
    EXPECT_TRUE(eids.count("eid-2") > 0);
    EXPECT_TRUE(eids.count("eid-3") > 0);
    EXPECT_EQ(eids.count("eid-4"), 0u);
    EXPECT_EQ(eids.count("eid-5"), 0u);
}

/*
 * 用例描述：
 * UbseSsuTenantQuotaFilter NORMAL策略豁免前置快筛：nsNum超过limit不会被误杀，
 * packing可在预算内保留的候选设备上完成（同设备多NS）
 * 测试步骤：
 * 1、构造4个在线未归属设备（剩余空间400/300/200/100）
 * 2、注入配额（perTenantPct=50），请求tenant-A，NORMAL策略nsNum=4
 *    （limit=ceil(4*50/100)=2，owned=0，预算allowedNew=2，未归属候选4>预算，
 *     裁剪保留剩余空间最大的eid-1(400)/eid-2(300)）
 * 3、调用UbseSsuTenantQuotaFilter::Handle
 * 预期结果：
 * 1、返回true（NORMAL允许同设备多NS，4个NS可在2台设备上packing完成）
 * 2、selectedDevs为eid-1+eid-2共2个设备
 */
TEST_F(TestUbseSsuScheduler, TenantQuotaNormalPackingWithinBudget)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 0), MakeDev("eid-2", 1024, 0),
                                        MakeDev("eid-3", 1024, 0), MakeDev("eid-4", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 4;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::NORMAL;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 400, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-2", 300, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-3", 200, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-4", 100, 512, 0, ""});

    UbseSsuTenantQuotaFilter filter(50);
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    ASSERT_EQ(ctx.selectedDevs.size(), 2u);
    EXPECT_EQ(ctx.selectedDevs[0].eid, "eid-1");
    EXPECT_EQ(ctx.selectedDevs[1].eid, "eid-2");
}

/*
 * 用例描述：
 * UbseSsuTenantQuotaFilter 设备下线使分母收缩、owned > limit 存量超限时，
 * LINEAR请求nsNum落在"己方设备数+预算"内（limit < nsNum <= owned）应放行，
 * 请求可全部落在己方已归属设备上（0新圈占），与"仅拦截新圈占"语义一致
 * 测试步骤：
 * 1、构造6个设备：4个tenant-A在线已归属、2个离线（在线分母4）
 * 2、注入配额（perTenantPct=50），请求tenant-A，LINEAR nsNum=3
 *    （limit=ceil(4*50/100)=2，owned=4>limit存量超限，allowedNew=0，
 *     servable=owned+allowedNew=4>=nsNum=3；旧实现nsNum>limit直接误拒）
 * 3、调用UbseSsuTenantQuotaFilter::Handle
 * 预期结果：
 * 1、返回true
 * 2、selectedDevs保持4个己方设备（unclaimed=0<=allowedNew不干预）
 */
TEST_F(TestUbseSsuScheduler, TenantQuotaPreFilterAllowsOwnedOvershootWhenDenominatorShrunk)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-2", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-3", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-4", 1024, 0, "tenant-A"),
                                        MakeDev("eid-5", 1024, 0, 0, UbseSsuState::OFFLINE),
                                        MakeDev("eid-6", 1024, 0, 0, UbseSsuState::OFFLINE)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 3;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-3", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-4", 1024, 512, 1, "tenant-A"});

    UbseSsuTenantQuotaFilter filter(50);
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.selectedDevs.size(), 4u);
}

/*
 * 用例描述：
 * UbseSsuTenantQuotaFilter 存量超限（owned > limit）时，LINEAR请求nsNum超过
 * "己方设备数+预算"（即超过owned，己方设备也无法覆盖）仍应前置快筛拒绝
 * 测试步骤：
 * 1、构造6个设备：4个tenant-A在线已归属、2个离线（在线分母4）
 * 2、注入配额（perTenantPct=50），请求tenant-A，LINEAR nsNum=5
 *    （limit=2，owned=4，allowedNew=0，servable=4<nsNum=5）
 * 3、调用UbseSsuTenantQuotaFilter::Handle
 * 预期结果：
 * 1、返回false
 * 2、ctx.result.ret == INSUFFICIENT_SPACE
 */
TEST_F(TestUbseSsuScheduler, TenantQuotaPreFilterRejectsNsNumBeyondOwnedOvershoot)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-2", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-3", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-4", 1024, 0, "tenant-A"),
                                        MakeDev("eid-5", 1024, 0, 0, UbseSsuState::OFFLINE),
                                        MakeDev("eid-6", 1024, 0, 0, UbseSsuState::OFFLINE)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 5;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-3", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-4", 1024, 512, 1, "tenant-A"});

    UbseSsuTenantQuotaFilter filter(50);
    bool ok = filter.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INSUFFICIENT_SPACE);
}

/*
 * 用例描述：
 * UbseSsuTenantQuotaFilter 存量超限（owned > limit）且候选含未归属设备时，
 * nsNum在"己方设备数+预算"内的请求放行快筛后，预算为0应裁剪掉全部未归属设备、
 * 仅保留己方设备满足数量（旧实现nsNum>limit会在快筛处误拒本可在己方设备内满足的请求）
 * 测试步骤：
 * 1、构造5个设备：4个tenant-A在线已归属、1个在线未归属（在线分母5）
 * 2、注入配额（perTenantPct=50），请求tenant-A，LINEAR nsNum=4
 *    （limit=ceil(5*50/100)=3，owned=4>limit存量超限，allowedNew=0，
 *     servable=4>=nsNum=4快筛放行；未归属候选1>allowedNew=0触发裁剪）
 * 3、调用UbseSsuTenantQuotaFilter::Handle
 * 预期结果：
 * 1、返回true
 * 2、selectedDevs仅剩4个己方设备（干净设备eid-5被剔除）
 */
TEST_F(TestUbseSsuScheduler, TenantQuotaOwnedOvershootTrimsUnclaimedKeepsOwned)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-2", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-3", 1024, 0, "tenant-A"),
                                        MakeDevWithTenant("eid-4", 1024, 0, "tenant-A"),
                                        MakeDev("eid-5", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 4;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-3", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-4", 1024, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-5", 1024, 512, 0, ""});

    UbseSsuTenantQuotaFilter filter(50);
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    ASSERT_EQ(ctx.selectedDevs.size(), 4u);
    std::unordered_set<std::string> eids;
    for (const auto &dev : ctx.selectedDevs) {
        eids.insert(dev.eid);
    }
    EXPECT_EQ(eids.count("eid-5"), 0u);
}

/*
 * 用例描述：
 * UbseSsuTenantQuotaFilter 预算裁剪拼接（ownedDevs+unclaimedDevs）后必须整体重排，
 * 保证selectedDevs仍满足"剩余空间降序、nsCount升序"的全局序
 * （AllocateStriped按"前nsNum个"直接选成员并以filterDevs[nsNum-1]为组内最小值校验容量，
 * AllocateLinear负载均衡也仅使用前nsNum个设备，均依赖降序假设）
 * 测试步骤：
 * 1、构造3个在线设备：eid-1 tenant-A已归属（剩余200）、eid-2未归属（剩余800）、eid-3未归属（剩余600）
 * 2、注入配额（perTenantPct=50），请求tenant-A，LINEAR nsNum=2
 *    （limit=ceil(3*50/100)=2，owned=1，预算allowedNew=1，未归属候选2>预算，
 *     裁剪保留剩余空间最大的eid-2；owned eid-1(200)拼在unclaimed eid-2(800)之前，无重排时非降序）
 * 3、调用UbseSsuTenantQuotaFilter::Handle
 * 预期结果：
 * 1、返回true
 * 2、selectedDevs为eid-2(800)、eid-1(200)全局降序（eid-3被裁剪剔除）
 */
TEST_F(TestUbseSsuScheduler, TenantQuotaBudgetTrimRestoresGlobalOrder)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDevWithTenant("eid-1", 1024, 824, "tenant-A"),
                                        MakeDev("eid-2", 1024, 224), MakeDev("eid-3", 1024, 424)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.lbaSize = 512;
    req.strategy = UbseSsuAllocStrategy::LINEAR;
    req.tenant = "tenant-A";
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 200, 512, 1, "tenant-A"});
    ctx.selectedDevs.push_back({"eid-2", 800, 512, 0, ""});
    ctx.selectedDevs.push_back({"eid-3", 600, 512, 0, ""});

    UbseSsuTenantQuotaFilter filter(50);
    bool ok = filter.Handle(ctx);

    EXPECT_TRUE(ok);
    ASSERT_EQ(ctx.selectedDevs.size(), 2u);
    EXPECT_EQ(ctx.selectedDevs[0].eid, "eid-2");
    EXPECT_EQ(ctx.selectedDevs[1].eid, "eid-1");
}

} // namespace ubse::ssu::scheduler::ut
