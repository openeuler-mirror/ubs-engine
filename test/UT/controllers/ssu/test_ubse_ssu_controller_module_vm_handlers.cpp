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

// UbseSsuControllerModule 三个 vsock 推送 handler 的反序列化/边界处理 UT
// 注:本测试聚焦 handler 业务逻辑(反序列化决策 + 缓存副作用),不断言 vsock SendResponse 返回值
//     (vsockServer_ 在 UT 中未 Start,SendResponse 返回 UNREACHABLE 与反序列化逻辑无关)
//     通过 vmCache_ 状态(是否 Update/Clear)验证 handler 走了正确分支

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "config/ubse_conf.h"
#include "message/ubse_ssu_alloc_msg.h"
#include "message/ubse_ssu_query_verify_msg.h"
#include "test_ubse_ssu_controller_module_vm_mock.h"
#include "ubse_error.h"
#include "ubse_serial_util.h"
#include "ubse_ssu_controller_module.h"
#include "ubse_ssu_service_imp.h"

namespace ubse::ssu::controller::ut {

using ubse::ssu::controller::UbseSsuControllerModule;
using ubse::ssu::service::UbseSsuServiceImp;
using ubse::serial::UbseSerialization;
using namespace ubse::ssu::message; // 引入 operator<< (UbseSerialization, UbseSsuAllocResult/ConnectInfo)

// config mock(MockUbseGetBool/GetStr/GetUInt 及 g_vmModeForTest 等状态)见共享头 test_ubse_ssu_controller_module_vm_mock.h
// 本文件场景:g_vmModeForTest=true 且 g_enableTlsForTest=false(UT 环境关闭 TLS,避免证书依赖)

// ============================================================================
// Payload 构造辅助(用 UbseSerialization 按协议格式序列化)
// ============================================================================

static UbseSsuAllocResult MakeAllocResult(const std::string &name)
{
    UbseSsuAllocResult ar{};
    ar.name = name;
    ar.strategy = UbseSsuAllocStrategy::NORMAL;
    UbseSsuNameSpaceInfo ns{};
    ns.tgtEid = std::string(16, 'A');
    ns.tgtNqn = "nqn.target." + name;
    ns.nsUuid = "uuid-" + name;
    ns.namespaceId = 1;
    ns.nsDevPath = "/dev/nvme0n1";
    ns.nsSize = 1024 * 1024 * 1024;
    ns.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;
    ar.nameSpaceList.push_back(std::move(ns));
    return ar;
}

static UbseSsuConnectInfo MakeConnectInfo(const std::string &name, const std::string &eid)
{
    UbseSsuConnectInfo ci{};
    ci.srcEid = "src_" + name;
    ci.tgtEid = eid;
    ci.tgtNqn = "nqn.target." + name;
    ci.hostNqn = "nqn.host." + name;
    ci.nsUuid = "uuid-" + name;
    ci.nsId = 1;
    return ci;
}

// 序列化单条 UbseSsuAllocResult(HandlePushAllocInfo body 格式)
static std::vector<uint8_t> SerializeAlloc(const UbseSsuAllocResult &ar)
{
    UbseSerialization ser;
    ser << ar;
    EXPECT_TRUE(ser.Check()) << "serialize AllocResult failed";
    auto *buf = ser.GetBuffer();
    auto len = ser.GetLength();
    return std::vector<uint8_t>(buf, buf + len);
}

// 序列化 HandlePushConnectInfo body: name + count + connectInfo[count]
static std::vector<uint8_t> SerializeConnectPush(const std::string &name,
                                                  const std::vector<UbseSsuConnectInfo> &list)
{
    UbseSerialization ser;
    ser << name;
    uint32_t count = static_cast<uint32_t>(list.size());
    ser << count;
    for (const auto &ci : list) {
        ser << ci;
    }
    EXPECT_TRUE(ser.Check()) << "serialize ConnectPush failed";
    auto *buf = ser.GetBuffer();
    auto len = ser.GetLength();
    return std::vector<uint8_t>(buf, buf + len);
}

// 序列化 HandleFullSync body: allocCount + alloc[] + connectCount + connect[](每个 connect: name + ciCount + ci[])
static std::vector<uint8_t> SerializeFullSync(
    const std::vector<UbseSsuAllocResult> &allocList,
    const std::vector<std::pair<std::string, std::vector<UbseSsuConnectInfo>>> &connectList)
{
    UbseSerialization ser;
    uint32_t allocCount = static_cast<uint32_t>(allocList.size());
    ser << allocCount;
    for (const auto &ar : allocList) {
        ser << ar;
    }
    uint32_t connectCount = static_cast<uint32_t>(connectList.size());
    ser << connectCount;
    for (const auto &entry : connectList) {
        ser << entry.first;
        uint32_t ciCount = static_cast<uint32_t>(entry.second.size());
        ser << ciCount;
        for (const auto &ci : entry.second) {
            ser << ci;
        }
    }
    EXPECT_TRUE(ser.Check()) << "serialize FullSync failed";
    auto *buf = ser.GetBuffer();
    auto len = ser.GetLength();
    return std::vector<uint8_t>(buf, buf + len);
}

// 构造 UbseIpcMessage(view,不持有所有权;buffer 为 uint8_t*,需 const_cast)
static ubse::vsock::UbseIpcMessage MakeMessage(const std::vector<uint8_t> &payload)
{
    return ubse::vsock::UbseIpcMessage{const_cast<uint8_t *>(payload.data()),
                                       static_cast<uint32_t>(payload.size())};
}

// 构造空 UbseIpcMessage(模拟空 payload)
static ubse::vsock::UbseIpcMessage MakeEmptyMessage()
{
    return ubse::vsock::UbseIpcMessage{nullptr, 0};
}

// ============================================================================
// 测试夹具
// ============================================================================

class TestUbseSsuVmHandlers : public testing::Test {
public:
    void SetUp() override
    {
        g_vmModeForTest = true;  // VM 模式
        g_enableTlsForTest = false; // UT 环境关闭 TLS,避免证书依赖
        g_vmModeConfigRetForTest = UBSE_OK;
        SetupVmConfigMocks();

        ASSERT_EQ(module_.Initialize(), UBSE_OK) << "Initialize (VM mode) failed";
        // UT 环境 executorModule 缺失,vmFlushExecutor_ 为 nullptr(降级路径),不影响 handler 逻辑
        // 直接访问 private vmCache_(依赖 -fno-access-control)验证缓存副作用
        ASSERT_NE(module_.vmCache_, nullptr);
        module_.vmCache_->Clear(); // 确保起始状态为空
    }

    void TearDown() override
    {
        module_.UnInitialize();
        GlobalMockObject::verify();
        // 恢复 ServiceImp 单例状态与共享 mock 状态
        UbseSsuServiceImp::GetInstance().SetVmCache(nullptr);
        UbseSsuServiceImp::GetInstance().SetVmMode(false);
        g_vmModeForTest = false;
        g_enableTlsForTest = false;
        g_vmModeConfigRetForTest = UBSE_OK;
    }

protected:
    // 调用 private handler(依赖 -fno-access-control)
    uint32_t CallHandlePushAllocInfo(const ubse::vsock::UbseIpcMessage &req)
    {
        ubse::vsock::UbseRequestContext ctx{};
        ctx.requestId = ++requestIdSeq_;
        return module_.HandlePushAllocInfo(req, ctx);
    }
    uint32_t CallHandlePushConnectInfo(const ubse::vsock::UbseIpcMessage &req)
    {
        ubse::vsock::UbseRequestContext ctx{};
        ctx.requestId = ++requestIdSeq_;
        return module_.HandlePushConnectInfo(req, ctx);
    }
    uint32_t CallHandleFullSync(const ubse::vsock::UbseIpcMessage &req)
    {
        ubse::vsock::UbseRequestContext ctx{};
        ctx.requestId = ++requestIdSeq_;
        return module_.HandleFullSync(req, ctx);
    }

    UbseSsuControllerModule module_;
    uint64_t requestIdSeq_{0};
};

// ============================================================================
// A. HandlePushAllocInfo(分配信息推送)
// 覆盖:正常反序列化 / 空 payload / name empty / 反序列化失败
// ============================================================================

/*
 * 用例描述:合法 payload 推送单条分配信息,缓存被更新
 * 测试步骤:
 * 1、序列化一条 UbseSsuAllocResult(name=alloc_ok)
 * 2、调用 HandlePushAllocInfo
 * 预期结果:
 * 1、vmCache_ 中存在 name=alloc_ok 的分配信息(AC:正常反序列化分支)
 */
TEST_F(TestUbseSsuVmHandlers, HandlePushAllocInfo_ValidPayload_UpdatesCache)
{
    auto payload = SerializeAlloc(MakeAllocResult("alloc_ok"));
    auto req = MakeMessage(payload);

    (void)CallHandlePushAllocInfo(req);

    UbseSsuAllocResult result;
    EXPECT_EQ(module_.vmCache_->GetAllocInfo("alloc_ok", result), UBSE_OK);
    EXPECT_EQ(result.name, "alloc_ok");
}

/*
 * 用例描述:空 payload(buffer=nullptr, length=0)推送,缓存不更新
 * 测试步骤:
 * 1、构造空 UbseIpcMessage
 * 2、调用 HandlePushAllocInfo
 * 预期结果:
 * 1、缓存仍为空(空 payload 提前 return,未 Update)
 */
TEST_F(TestUbseSsuVmHandlers, HandlePushAllocInfo_EmptyPayload_NotUpdate)
{
    auto req = MakeEmptyMessage();

    (void)CallHandlePushAllocInfo(req);

    std::vector<UbseSsuAllocResult> list;
    EXPECT_EQ(module_.vmCache_->ListAllocInfo(list), UBSE_OK);
    EXPECT_TRUE(list.empty());
}

/*
 * 用例描述:反序列化得到的 name 为空字符串,缓存不更新
 * 测试步骤:
 * 1、序列化 name="" 的 UbseSsuAllocResult
 * 2、调用 HandlePushAllocInfo
 * 预期结果:
 * 1、缓存仍为空(name empty 提前 return,未 Update)
 */
TEST_F(TestUbseSsuVmHandlers, HandlePushAllocInfo_NameEmpty_NotUpdate)
{
    UbseSsuAllocResult ar = MakeAllocResult(""); // name 为空
    auto payload = SerializeAlloc(ar);
    auto req = MakeMessage(payload);

    (void)CallHandlePushAllocInfo(req);

    std::vector<UbseSsuAllocResult> list;
    EXPECT_EQ(module_.vmCache_->ListAllocInfo(list), UBSE_OK);
    EXPECT_TRUE(list.empty());
}

/*
 * 用例描述:payload 为损坏字节(非合法序列化格式),反序列化失败,缓存不更新
 * 测试步骤:
 * 1、构造随机字节 payload
 * 2、调用 HandlePushAllocInfo
 * 预期结果:
 * 1、缓存仍为空(Check() 失败提前 return,未 Update)
 */
TEST_F(TestUbseSsuVmHandlers, HandlePushAllocInfo_DeserializeFailed_NotUpdate)
{
    std::vector<uint8_t> garbage{0x01, 0x02, 0x03, 0xff};
    auto req = MakeMessage(garbage);

    (void)CallHandlePushAllocInfo(req);

    std::vector<UbseSsuAllocResult> list;
    EXPECT_EQ(module_.vmCache_->ListAllocInfo(list), UBSE_OK);
    EXPECT_TRUE(list.empty());
}

// ============================================================================
// B. HandlePushConnectInfo(连接信息推送)
// 覆盖:正常反序列化 / count 超限 / 空 payload / name empty / 中途失败
// ============================================================================

/*
 * 用例描述:合法 payload 推送单条连接信息,缓存被更新
 * 测试步骤:
 * 1、序列化 name + count=1 + connectInfo[1]
 * 2、调用 HandlePushConnectInfo
 * 预期结果:
 * 1、vmCache_ 中存在 name 对应的连接信息(正常反序列化分支)
 */
TEST_F(TestUbseSsuVmHandlers, HandlePushConnectInfo_ValidPayload_UpdatesCache)
{
    auto payload =
        SerializeConnectPush("conn_ok", {MakeConnectInfo("conn_ok", std::string(16, 'B'))});
    auto req = MakeMessage(payload);

    (void)CallHandlePushConnectInfo(req);

    std::vector<UbseSsuConnectInfo> list;
    EXPECT_EQ(module_.vmCache_->GetConnectInfo("conn_ok", list), UBSE_OK);
    EXPECT_FALSE(list.empty());
    EXPECT_EQ(list[0].srcEid, "src_conn_ok");
}

/*
 * 用例描述:count 字段超过上限(4096),缓存不更新
 * 测试步骤:
 * 1、构造 payload: name + count=5000(超 4096 上限)
 * 2、调用 HandlePushConnectInfo
 * 预期结果:
 * 1、缓存仍为空(超限提前 return,未 Update)
 */
TEST_F(TestUbseSsuVmHandlers, HandlePushConnectInfo_CountExceedsLimit_NotUpdate)
{
    UbseSerialization ser;
    std::string name = "conn_overlimit";
    ser << name;
    uint32_t count = 5000; // > MAX_PUSH_CONNECT_INFO_NUM(4096)
    ser << count;
    EXPECT_TRUE(ser.Check());
    auto *buf = ser.GetBuffer();
    auto len = ser.GetLength();
    std::vector<uint8_t> payload(buf, buf + len);
    auto req = MakeMessage(payload);

    (void)CallHandlePushConnectInfo(req);

    std::vector<UbseSsuConnectInfo> list;
    EXPECT_EQ(module_.vmCache_->GetConnectInfo("conn_overlimit", list), UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

/*
 * 用例描述:空 payload 推送,缓存不更新
 * 测试步骤:
 * 1、构造空 UbseIpcMessage
 * 2、调用 HandlePushConnectInfo
 * 预期结果:
 * 1、缓存仍为空
 */
TEST_F(TestUbseSsuVmHandlers, HandlePushConnectInfo_EmptyPayload_NotUpdate)
{
    auto req = MakeEmptyMessage();

    (void)CallHandlePushConnectInfo(req);

    std::vector<UbseSsuConnectInfo> list;
    EXPECT_EQ(module_.vmCache_->GetConnectInfo("any", list), UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

/*
 * 用例描述:反序列化得到的 name 为空,缓存不更新
 * 测试步骤:
 * 1、序列化 name="" + count=0
 * 2、调用 HandlePushConnectInfo
 * 预期结果:
 * 1、缓存仍为空(name empty 提前 return)
 */
TEST_F(TestUbseSsuVmHandlers, HandlePushConnectInfo_NameEmpty_NotUpdate)
{
    auto payload = SerializeConnectPush("", {}); // name 空
    auto req = MakeMessage(payload);

    (void)CallHandlePushConnectInfo(req);

    std::vector<UbseSsuConnectInfo> list;
    EXPECT_EQ(module_.vmCache_->GetConnectInfo("any", list), UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

/*
 * 用例描述:payload 中途反序列化失败(声称 count=2 但只提供 1 条合法 connectInfo 后截断)
 * 测试步骤:
 * 1、构造 payload: name + count=2 + connectInfo[1](数据不足)
 * 2、调用 HandlePushConnectInfo
 * 预期结果:
 * 1、缓存仍为空(Check() 失败提前 return,未 Update)
 */
TEST_F(TestUbseSsuVmHandlers, HandlePushConnectInfo_DeserializeMidFail_NotUpdate)
{
    UbseSerialization ser;
    std::string name = "conn_midfail";
    ser << name;
    uint32_t count = 2; // 声称 2 条
    ser << count;
    ser << MakeConnectInfo("conn_midfail", std::string(16, 'C')); // 只给 1 条
    EXPECT_TRUE(ser.Check());
    auto *buf = ser.GetBuffer();
    auto len = ser.GetLength();
    std::vector<uint8_t> payload(buf, buf + len);
    auto req = MakeMessage(payload);

    (void)CallHandlePushConnectInfo(req);

    std::vector<UbseSsuConnectInfo> list;
    EXPECT_EQ(module_.vmCache_->GetConnectInfo("conn_midfail", list), UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

// ============================================================================
// C. HandleFullSync(全量同步)
// 覆盖:正常重建 / 空 payload / allocCount 超限 / 中途失败(关键不变式:部分失败不 Clear)
// ============================================================================

/*
 * 用例描述:合法 payload 全量同步,先 Clear 再批量 Update,缓存重建为新数据
 * 测试步骤:
 * 1、预先向 vmCache_ 填充旧数据(old_alloc)
 * 2、序列化 FullSync payload 含新数据(new_alloc)
 * 3、调用 HandleFullSync
 * 预期结果:
 * 1、旧数据 old_alloc 被清除(GetAllocInfo 返回 NOT_FOUND)
 * 2、新数据 new_alloc 存在
 */
TEST_F(TestUbseSsuVmHandlers, HandleFullSync_ValidPayload_ClearsAndRebuilds)
{
    // 预填充旧数据
    module_.vmCache_->UpdateAllocInfo(MakeAllocResult("old_alloc"));
    UbseSsuAllocResult oldResult;
    ASSERT_EQ(module_.vmCache_->GetAllocInfo("old_alloc", oldResult), UBSE_OK);

    auto payload = SerializeFullSync({MakeAllocResult("new_alloc")}, {});
    auto req = MakeMessage(payload);

    (void)CallHandleFullSync(req);

    UbseSsuAllocResult result;
    EXPECT_NE(module_.vmCache_->GetAllocInfo("old_alloc", result), UBSE_OK); // 旧数据被 Clear
    EXPECT_EQ(module_.vmCache_->GetAllocInfo("new_alloc", result), UBSE_OK); // 新数据存在
    EXPECT_EQ(result.name, "new_alloc");
}

/*
 * 用例描述:空 payload 全量同步,缓存不被 Clear(反序列化失败提前 return,不触达 Clear)
 * 测试步骤:
 * 1、预先填充 vmCache_
 * 2、构造空 payload 调用 HandleFullSync
 * 预期结果:
 * 1、旧数据保留(关键不变式:反序列化失败不 Clear)
 */
TEST_F(TestUbseSsuVmHandlers, HandleFullSync_EmptyPayload_NotClear)
{
    module_.vmCache_->UpdateAllocInfo(MakeAllocResult("keep_alloc"));

    auto req = MakeEmptyMessage();
    (void)CallHandleFullSync(req);

    UbseSsuAllocResult result;
    EXPECT_EQ(module_.vmCache_->GetAllocInfo("keep_alloc", result), UBSE_OK); // 旧数据保留
    EXPECT_EQ(result.name, "keep_alloc");
}

/*
 * 用例描述:allocCount 超过上限(8192),缓存不被 Clear(超限提前 return)
 * 测试步骤:
 * 1、预先填充 vmCache_
 * 2、构造 payload: allocCount=9000(超 8192 上限)
 * 3、调用 HandleFullSync
 * 预期结果:
 * 1、旧数据保留(不变式:超限不 Clear)
 */
TEST_F(TestUbseSsuVmHandlers, HandleFullSync_AllocCountExceedsLimit_NotClear)
{
    module_.vmCache_->UpdateAllocInfo(MakeAllocResult("keep_alloc2"));

    UbseSerialization ser;
    uint32_t allocCount = 9000; // > MAX_FULL_SYNC_ALLOC_NUM(8192)
    ser << allocCount;
    EXPECT_TRUE(ser.Check());
    auto *buf = ser.GetBuffer();
    auto len = ser.GetLength();
    std::vector<uint8_t> payload(buf, buf + len);
    auto req = MakeMessage(payload);

    (void)CallHandleFullSync(req);

    UbseSsuAllocResult result;
    EXPECT_EQ(module_.vmCache_->GetAllocInfo("keep_alloc2", result), UBSE_OK); // 旧数据保留
}

/*
 * 用例描述:payload 中途反序列化失败(声称 allocCount=2 但只提供 1 条后截断)
 * 测试步骤:
 * 1、预先填充 vmCache_
 * 2、构造 payload: allocCount=2 + alloc[1](数据不足)
 * 3、调用 HandleFullSync
 * 预期结果:
 * 1、旧数据保留(关键不变式:部分失败不 Clear,因为 Clear 在 Check() 通过后执行)
 */
TEST_F(TestUbseSsuVmHandlers, HandleFullSync_DeserializeMidFail_NotClear)
{
    module_.vmCache_->UpdateAllocInfo(MakeAllocResult("keep_alloc3"));

    UbseSerialization ser;
    uint32_t allocCount = 2; // 声称 2 条
    ser << allocCount;
    ser << MakeAllocResult("partial_alloc"); // 只给 1 条
    EXPECT_TRUE(ser.Check());
    auto *buf = ser.GetBuffer();
    auto len = ser.GetLength();
    std::vector<uint8_t> payload(buf, buf + len);
    auto req = MakeMessage(payload);

    (void)CallHandleFullSync(req);

    UbseSsuAllocResult result;
    EXPECT_EQ(module_.vmCache_->GetAllocInfo("keep_alloc3", result), UBSE_OK); // 旧数据保留(关键不变式)
    EXPECT_NE(module_.vmCache_->GetAllocInfo("partial_alloc", result), UBSE_OK); // 部分数据未提交
}

/*
 * 用例描述:FullSync 中 alloc 条目 name 为空,缓存不被 Clear(脏数据整体拒绝,不部分提交)
 * 测试步骤:
 * 1、预先填充 vmCache_
 * 2、构造 payload: alloc[0] name 非空、alloc[1] name 为空
 * 3、调用 HandleFullSync
 * 预期结果:
 * 1、旧数据保留(name empty 整体拒绝,不 Clear)
 * 2、两条 alloc 均未提交(空 name 条目若落盘,LoadFromDisk 会拒绝缓存文件导致 VM 重启丢数据)
 */
TEST_F(TestUbseSsuVmHandlers, HandleFullSync_AllocNameEmpty_NotClear)
{
    module_.vmCache_->UpdateAllocInfo(MakeAllocResult("keep_alloc4"));

    auto payload = SerializeFullSync({MakeAllocResult("good_alloc"), MakeAllocResult("")}, {});
    auto req = MakeMessage(payload);

    (void)CallHandleFullSync(req);

    UbseSsuAllocResult result;
    EXPECT_EQ(module_.vmCache_->GetAllocInfo("keep_alloc4", result), UBSE_OK); // 旧数据保留
    EXPECT_NE(module_.vmCache_->GetAllocInfo("good_alloc", result), UBSE_OK); // 整体拒绝,未提交
}

/*
 * 用例描述:FullSync 中 connect 条目 name 为空,缓存不被 Clear(脏数据整体拒绝,不部分提交)
 * 测试步骤:
 * 1、预先填充 vmCache_
 * 2、构造 payload: 合法 alloc + connect[0] name 为空
 * 3、调用 HandleFullSync
 * 预期结果:
 * 1、旧数据保留(name empty 整体拒绝,不 Clear)
 * 2、alloc 与 connect 均未提交
 */
TEST_F(TestUbseSsuVmHandlers, HandleFullSync_ConnectNameEmpty_NotClear)
{
    module_.vmCache_->UpdateAllocInfo(MakeAllocResult("keep_alloc5"));

    auto payload = SerializeFullSync({MakeAllocResult("good_alloc5")},
                                     {{"", {MakeConnectInfo("", "eid-" + std::string(16, 'B'))}}});
    auto req = MakeMessage(payload);

    (void)CallHandleFullSync(req);

    UbseSsuAllocResult result;
    EXPECT_EQ(module_.vmCache_->GetAllocInfo("keep_alloc5", result), UBSE_OK); // 旧数据保留
    EXPECT_NE(module_.vmCache_->GetAllocInfo("good_alloc5", result), UBSE_OK); // 整体拒绝,未提交
}

} // namespace ubse::ssu::controller::ut
