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

#include <algorithm>

namespace ubse::ssu::scheduler::ut {

using namespace ubse::adapter_plugins::ssu::def;

UbseSsuDevInfo TestUbseSsuScheduler::MakeDev(const std::string &eid,
                                                                              uint64_t totalBytes, uint64_t usedBytes,
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
    req.addressingType = UbseSsuAddressingType::LINEAR;
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
    req.addressingType = UbseSsuAddressingType::LINEAR;
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
    req.addressingType = UbseSsuAddressingType::LINEAR;
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
    req.addressingType = UbseSsuAddressingType::LINEAR;
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
    req.allocSize = 1024;
    req.nsNum = 3;
    req.lbaSize = 512;
    req.addressingType = UbseSsuAddressingType::LINEAR;
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
    req.addressingType = UbseSsuAddressingType::LINEAR;
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
    req.addressingType = UbseSsuAddressingType::LINEAR;
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
 * AlgorithmHandler 在 lbaSize 为 0 时返回参数非法
 * 测试步骤：
 * 1、手动准备 ctx.selectedDevs
 * 2、lbaSize=0、addressingType=LINEAR 调用 AlgorithmHandler::Handle
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
    req.addressingType = UbseSsuAddressingType::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0});

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
    req.addressingType = UbseSsuAddressingType::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 256, 512, 0});
    ctx.selectedDevs.push_back({"eid-2", 256, 512, 0});

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
    req.addressingType = UbseSsuAddressingType::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0});

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
    req.addressingType = UbseSsuAddressingType::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0});

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
 * 2、请求 1024 字节、chunk=512、nsNum=2
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
    req.chunkSize = 512;
    req.addressingType = UbseSsuAddressingType::STRIPED;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0});

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
 * AlgorithmHandler 在 STRIPED 模式下 chunkSize=0 时返回参数非法
 * 测试步骤：
 * 1、ctx.selectedDevs 为 2 个设备
 * 2、chunkSize=0
 * 3、调用 AlgorithmHandler::Handle
 * 预期结果：
 * 1、返回 false
 * 2、ctx.result.ret == INVALID_PARAM
 */
TEST_F(TestUbseSsuScheduler, AlgorithmStripedChunkSizeZero)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 1024, 0), MakeDev("eid-2", 1024, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 1024;
    req.nsNum = 2;
    req.chunkSize = 0;
    req.addressingType = UbseSsuAddressingType::STRIPED;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0});

    UbseSsuAllocateAlgorithmHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INVALID_PARAM);
}

/*
 * 用例描述：
 * AlgorithmHandler 在 STRIPED 模式下，单设备空间不足时返回空间不足
 * 测试步骤：
 * 1、ctx.selectedDevs 为 2 个设备（第二个仅剩 256）
 * 2、请求 1024、chunk=512
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
    req.chunkSize = 512;
    req.addressingType = UbseSsuAddressingType::STRIPED;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0});
    ctx.selectedDevs.push_back({"eid-2", 256, 512, 0});

    UbseSsuAllocateAlgorithmHandler handler;
    bool ok = handler.Handle(ctx);

    EXPECT_FALSE(ok);
    EXPECT_EQ(ctx.result.ret, UbseSsuAllocRetCode::INSUFFICIENT_SPACE);
}

/*
 * 用例描述：
 * AlgorithmHandler 在 STRIPED 模式下，allocSize 非条带整数倍时向上对齐
 * 测试步骤：
 * 1、ctx.selectedDevs 为 2 个设备，各有 2048 字节
 * 2、请求 600、chunk=512、nsNum=2（stripeSize=1024，向上对齐到 1024）
 * 3、调用 AlgorithmHandler::Handle
 * 预期结果：
 * 1、返回 true
 * 2、每个 eid 分配大小为 512（向上对齐后 stripeSize=1024 -> 512/设备）
 */
TEST_F(TestUbseSsuScheduler, AlgorithmStripedAlignUpToStripe)
{
    std::vector<UbseSsuDevInfo> devs = {MakeDev("eid-1", 4096, 0), MakeDev("eid-2", 4096, 0)};
    UbseSsuAllocRequest req;
    req.allocSize = 600;
    req.nsNum = 2;
    req.chunkSize = 512;
    req.addressingType = UbseSsuAddressingType::STRIPED;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 4096, 512, 0});
    ctx.selectedDevs.push_back({"eid-2", 4096, 512, 0});

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
    req.addressingType = UbseSsuAddressingType::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);
    ctx.selectedDevs.push_back({"eid-1", 1024, 512, 0});
    ctx.selectedDevs.push_back({"eid-2", 1024, 512, 0});
    ctx.selectedDevs.push_back({"eid-3", 1024, 512, 0});

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
    req.addressingType = UbseSsuAddressingType::LINEAR;
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
    req.addressingType = UbseSsuAddressingType::LINEAR;
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
    req.chunkSize = 512;
    req.addressingType = UbseSsuAddressingType::STRIPED;
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
    req.addressingType = UbseSsuAddressingType::LINEAR;
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
    req.addressingType = UbseSsuAddressingType::LINEAR;
    UbseSsuAllocationContext ctx(devs, req);

    UbseSsuScheduler scheduler;
    scheduler.AddFilter(UbseSsuFilterGroupId::POST_CHECK, nullptr);
    auto ret = scheduler.Execute(ctx);

    EXPECT_EQ(ret, UbseSsuAllocRetCode::OK);
}

} // namespace ubse::ssu::scheduler::ut
