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

// SSU 模块虚机化特性 IT:
// vsock 端到端推送链路,覆盖分配/连接信息推送与全量同步、VM 模式查询、attach/detach 生命周期、
// 缓存持久化恢复及推送协议异常路径。
// mock ub-device-manager 客户端按真实线路协议(UbseRequestHeader/UbseResponseHeader)
// 通过 AF_VSOCK 回环与真实 UbseVsockServer 交互。

#include "test_ssu_virt_push_it.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <linux/vm_sockets.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include "config/ubse_conf.h"
#include "message/ubse_ssu_alloc_msg.h"
#include "message/ubse_ssu_query_verify_msg.h"
#include "ssu_virt_it_mock_adapter.h"
#include "ssu_virt_it_vsock_client.h"
#include "ubse_error.h"
#include "ubse_serial_util.h"
#include "ubse_ssu_service_imp.h"
#include "ubse_ssu_vm_cache.h"
#include "ubse_thread_pool.h"
#include "src/framework/vscok/ubse_vsock_message.h"
#include "src/framework/vscok/ubse_vsock_server.h"

namespace ubse::ssu::virt::it {

using namespace ubse::ssu::message; // 引入 operator<</>>(UbseSerialization, UbseSsuAllocResult/ConnectInfo)
using ubse::plugin::service::ssu::UbseSsuAllocResult;
using ubse::plugin::service::ssu::UbseSsuConnectInfo;
using ubse::plugin::service::ssu::UbseSsuNameSpaceInfo;
using ubse::ssu::controller::UbseSsuVmCache;
using ubse::ssu::service::UbseSsuServiceImp;
using ubse::vsock::UbseVsockMsgType;

namespace {

// vsock 回环 CID(linux/vm_sockets.h 定义,回环传输要求 vsock_loopback 内核模块)
constexpr uint32_t IT_VSOCK_CID_LOCAL = VMADDR_CID_LOCAL;
// IT 端口探测基址(避开常用服务端口;绑定失败依次后移)
constexpr uint32_t IT_VSOCK_PORT_BASE = 32000;
constexpr int IT_MAX_PORT_ATTEMPTS = 32;
// mock 客户端收发超时(秒)
constexpr int IT_CLIENT_TIMEOUT_SEC = 10;
// IT 身份(与适配器 mock customData 默认值对齐)
constexpr uid_t IT_UID = 2000;
constexpr const char *IT_USER_NAME = "it_user";
constexpr const char *IT_HOST_NQN = "nqn.2024-01.org.nvmexpress:uuid:11111111-1111-1111-1111-111111111111";
// 适配器 eid 为固定 16 字符(与适配器侧构造 namespace 信息的长度约束保持一致)
constexpr size_t IT_EID_LEN = 16;

// ---------------------------------------------------------------------------
// 消息体序列化(与 controller 推送 handler 反序列化格式一致)
// ---------------------------------------------------------------------------

std::vector<uint8_t> SerializeAllocBody(const UbseSsuAllocResult &ar)
{
    ubse::serial::UbseSerialization ser;
    ser << ar;
    EXPECT_TRUE(ser.Check());
    auto *buf = ser.GetBuffer();
    return std::vector<uint8_t>(buf, buf + ser.GetLength());
}

std::vector<uint8_t> SerializeConnectPushBody(const std::string &name,
                                              const std::vector<UbseSsuConnectInfo> &list)
{
    ubse::serial::UbseSerialization ser;
    ser << name;
    uint32_t count = static_cast<uint32_t>(list.size());
    ser << count;
    for (const auto &ci : list) {
        ser << ci;
    }
    EXPECT_TRUE(ser.Check());
    auto *buf = ser.GetBuffer();
    return std::vector<uint8_t>(buf, buf + ser.GetLength());
}

std::vector<uint8_t> SerializeFullSyncBody(
    const std::vector<UbseSsuAllocResult> &allocList,
    const std::vector<std::pair<std::string, std::vector<UbseSsuConnectInfo>>> &connectList)
{
    ubse::serial::UbseSerialization ser;
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
    EXPECT_TRUE(ser.Check());
    auto *buf = ser.GetBuffer();
    return std::vector<uint8_t>(buf, buf + ser.GetLength());
}

// ---------------------------------------------------------------------------
// 推送辅助:发送并等待 ACK,断言成功
// ---------------------------------------------------------------------------

void PushAndExpectAck(ItVsockPushClient &client, UbseVsockMsgType msgType, const std::vector<uint8_t> &body,
                      uint64_t clientRequestId)
{
    ASSERT_TRUE(client.SendPush(msgType, body, clientRequestId));
    uint32_t statusCode = 0xFFFFFFFF;
    uint64_t respRequestId = 0;
    std::vector<uint8_t> respBody;
    ASSERT_TRUE(client.RecvResponse(statusCode, respRequestId, respBody));
    EXPECT_EQ(0u, statusCode) << "push msgType=" << static_cast<uint32_t>(msgType)
                              << ", clientRequestId=" << clientRequestId;
    EXPECT_EQ(clientRequestId, respRequestId);
}

// 探测 AF_VSOCK 回环可用性(绑定 CID_LOCAL 任意端口)
bool IsVsockLoopbackAvailable()
{
    int fd = socket(AF_VSOCK, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return false;
    }
    struct sockaddr_vm addr {};
    addr.svm_family = AF_VSOCK;
    addr.svm_cid = IT_VSOCK_CID_LOCAL;
    addr.svm_port = VMADDR_PORT_ANY;
    bool ok = bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == 0;
    (void)close(fd);
    return ok;
}

// UbseGetStr mock:GetDevList 内部 GetAdminNqn 需要(与 UT 同一约定)
uint32_t MockItUbseGetStr(const std::string &section, const std::string &configKey, std::string &configVal)
{
    (void)section;
    (void)configKey;
    configVal = "nqn.2024-01.org.nvmexpress:uuid:it-admin-nqn";
    return UBSE_OK;
}

UbseSsuAllocResult MakeAllocResult(const std::string &name, const UbseSsuNameSpaceInfo &nsInfo)
{
    UbseSsuAllocResult ar{};
    ar.name = name;
    ar.strategy = ubse::plugin::service::ssu::UbseSsuAllocStrategy::NORMAL;
    // 持久化契约:lbaFormat 必须为合法枚举(512/4096),默认构造值 0 会被 LoadFromDisk 拒绝
    auto validNs = nsInfo;
    if (validNs.lbaFormat != ubse::plugin::service::ssu::UbseSsuLBAFormat::LBA_FORMAT_512 &&
        validNs.lbaFormat != ubse::plugin::service::ssu::UbseSsuLBAFormat::LBA_FORMAT_4K) {
        validNs.lbaFormat = ubse::plugin::service::ssu::UbseSsuLBAFormat::LBA_FORMAT_512;
    }
    ar.nameSpaceList.push_back(std::move(validNs));
    return ar;
}

// 轮询等待连接状态(accept 在 server 事件循环线程异步完成,connect 返回不等于已受理)
bool WaitForConnState(const ubse::vsock::UbseVsockServer &server, ubse::vsock::UbseVsockConnState expected,
                      int timeoutMs)
{
    constexpr int kPollIntervalMs = 10;
    for (int waited = 0; waited < timeoutMs; waited += kPollIntervalMs) {
        if (server.GetConnectionStatus() == expected) {
            return true;
        }
        usleep(kPollIntervalMs * 1000);
    }
    return server.GetConnectionStatus() == expected;
}

// 用 openssl 自签生成 TLS 冒烟证书(2 天有效期,进程级临时目录):
// server.pem/server_key.pem/trust.pem/key_pwd.txt 与 vsock server certDir 契约一致;
// 另生成 client.pem/client_key.pem(无密码)供客户端双向认证出示
bool MakeItTlsCertDir(const std::string &dir)
{
    std::string cmd = "command -v openssl >/dev/null 2>&1 && mkdir -p " + dir + " && cd " + dir + " && "
        "openssl req -x509 -newkey rsa:2048 -nodes -keyout ca_key.pem -out ca.pem -days 2 -subj /CN=ssu-it-ca && "
        "openssl genrsa -aes256 -passout pass:itpwd -out server_key.pem 2048 && "
        "openssl req -new -key server_key.pem -passin pass:itpwd -subj /CN=ssu-it-server -out server.csr && "
        "openssl x509 -req -in server.csr -CA ca.pem -CAkey ca_key.pem -CAcreateserial -out server.pem -days 2 && "
        "openssl genrsa -out client_key.pem 2048 && "
        "openssl req -new -key client_key.pem -subj /CN=ssu-it-client -out client.csr && "
        "openssl x509 -req -in client.csr -CA ca.pem -CAkey ca_key.pem -CAcreateserial -out client.pem -days 2 && "
        "cp ca.pem trust.pem && echo itpwd > key_pwd.txt";
    return system(cmd.c_str()) == 0;
}
} // namespace

// ---------------------------------------------------------------------------
// 夹具实现
// ---------------------------------------------------------------------------

void TestSsuVirtPush::SetUp()
{
    // 1) 环境探测:AF_VSOCK 回环不可用时跳过(目标环境 openEuler ARM64 已加载 vsock_loopback)
    if (!IsVsockLoopbackAvailable()) {
        GTEST_SKIP() << "AF_VSOCK loopback unavailable (requires vsock_loopback kernel module)";
    }

    // 2) 适配器 mock(绕过 dlopen)与可观测状态复位
    SetupItAdapterFuncs();
    ResetItAdapterMockState();
    // GetDevList 内部 GetAdminNqn → UbseGetStr,需 mock 配置
    MOCKER_CPP(&ubse::config::UbseGetStr)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), mockcpp::any())
        .will(invoke(MockItUbseGetStr));

    // 3) 缓存文件临时路径(进程级隔离,避免并发 IT 互相覆盖)
    cacheFilePath_ = "/tmp/ssu_virt_it_" + std::to_string(static_cast<long>(getpid())) + ".json";
    (void)::remove(cacheFilePath_.c_str());

    // 4) ServiceImp 进入 VM 模式(缓存指针在模块组装后注入)
    auto &service = UbseSsuServiceImp::GetInstance();
    service.SetVmMode(true);

    // 5) 进程内组装 SSU 控制器模块(与 InitializeVmMode 相同的装配,复用真实推送 handler;
    //    私有成员注入依赖 IT 目标的 -fno-access-control 编译选项)
    module_ = std::make_unique<UbseSsuControllerModule>();
    auto vmCache = std::make_unique<UbseSsuVmCache>();
    vmCache->SetCacheFilePath(cacheFilePath_);
    service.SetVmCache(vmCache.get());
    module_->vmCache_ = std::move(vmCache);

    // 异步落盘线程池(单条推送 handler 投递 FlushToDisk,与生产路径一致)
    module_->vmFlushExecutor_ = ubse::task_executor::UbseTaskExecutor::Create("itSsuVmFlush", 1, 256);
    ASSERT_NE(module_->vmFlushExecutor_, nullptr);
    ASSERT_TRUE(module_->vmFlushExecutor_->Start());

    // 6) 启动真实 vsock server(TLS 关闭,免证书依赖;端口冲突依次后移)
    ubse::vsock::UbseVsockConfig cfg{};
    cfg.enableTls = false;
    cfg.threadPoolSize = 1; // 与生产一致:单工作线程保证同一会话消息串行
    cfg.threadPoolQueueSize = 256;
    bool started = false;
    for (int attempt = 0; attempt < IT_MAX_PORT_ATTEMPTS && !started; ++attempt) {
        cfg.port = IT_VSOCK_PORT_BASE + static_cast<uint32_t>(getpid() % 512) * IT_MAX_PORT_ATTEMPTS +
                   static_cast<uint32_t>(attempt);
        module_->vsockServer_ = std::make_unique<ubse::vsock::UbseVsockServer>(cfg);
        if (module_->RegisterVsockHandlers() != UBSE_OK) {
            module_->vsockServer_.reset();
            continue;
        }
        started = (module_->vsockServer_->Start() == UBSE_OK);
        if (!started) {
            module_->vsockServer_.reset();
        } else {
            vsockPort_ = cfg.port;
        }
    }
    ASSERT_TRUE(started) << "failed to start vsock server on any candidate port";
}

void TestSsuVirtPush::TearDown()
{
    if (module_ != nullptr) {
        if (module_->vsockServer_ != nullptr) {
            module_->vsockServer_->Stop();
            module_->vsockServer_.reset();
        }
        if (module_->vmFlushExecutor_ != nullptr) {
            module_->vmFlushExecutor_->Stop();
            module_->vmFlushExecutor_ = nullptr;
        }
    }
    // 恢复 ServiceImp host 模式状态,避免污染其他测试
    auto &service = UbseSsuServiceImp::GetInstance();
    service.SetVmCache(nullptr);
    service.SetVmMode(false);
    if (module_ != nullptr && module_->vmCache_ != nullptr) {
        module_->vmCache_->Clear();
    }
    module_.reset();
    (void)::remove(cacheFilePath_.c_str());
    GlobalMockObject::verify();
    ReleaseItAdapterDlHandle();
}

UbseSsuSpaceReq TestSsuVirtPush::MakeSpaceReq(const std::string &name)
{
    UbseSsuSpaceReq req{};
    req.name = name;
    req.nqn = IT_HOST_NQN;
    // attach/detach 身份校验:identity 与适配器 mock customData 默认值(IT_UID/IT_USER_NAME)对齐
    req.identity.userName = IT_USER_NAME;
    req.identity.uid = IT_UID;
    return req;
}

/*
 * 用例描述:mock ub-device-manager 推送分配信息 → VM 接收 ACK → 缓存更新 →
 *           VM 模式查询 → attach → detach 全链路通过
 * 测试步骤:
 * 1、注册适配器可见 namespace(identity 匹配),客户端连接 vsock server
 * 2、推送单条分配信息,等待 ACK
 * 3、ServiceImp VM 模式按名查询该分配信息
 * 4、调用 AttachSpace 后调用 DetachSpace
 * 预期结果:
 * 1、连接后 server 状态为 CONNECTED,ACK statusCode=0 且 clientRequestId 回填
 * 2、GetAllocInfoByName 返回推送数据(identity 匹配)
 * 3、attach 返回 UBSE_OK 且设备路径与推送一致,IsAttached 为 true
 * 4、detach 返回 UBSE_OK,IsAttached 为 false
 */
TEST_F(TestSsuVirtPush, PushAllocInfo_FullCycle)
{
    ItVsockPushClient client;
    ASSERT_TRUE(client.Connect(IT_VSOCK_CID_LOCAL, vsockPort_));
    // accept 由 server 事件循环线程异步完成,轮询等待状态迁移
    EXPECT_TRUE(WaitForConnState(*module_->vsockServer_, ubse::vsock::UbseVsockConnState::CONNECTED, 2000));

    const std::string name = "it_alloc_cycle";
    const std::string eid(IT_EID_LEN, 'A');
    auto nsInfo = RegisterAdapterNs(eid, "nqn.target." + name, 1, std::string(16, 'P'));
    PushAndExpectAck(client, UbseVsockMsgType::MSG_TYPE_PUSH_ALLOC_INFO, SerializeAllocBody(MakeAllocResult(name, nsInfo)),
                     1001);

    // 缓存已更新
    UbseSsuAllocResult cached{};
    ASSERT_EQ(UBSE_OK, module_->vmCache_->GetAllocInfo(name, cached));
    EXPECT_EQ(nsInfo.nsUuid, cached.nameSpaceList[0].nsUuid);

    // VM 模式查询(identity 匹配)
    UbseSsuAllocIdentityInfo identity{};
    identity.userName = IT_USER_NAME;
    identity.uid = IT_UID;
    UbseSsuAllocResult queried{};
    ASSERT_EQ(UBSE_OK, UbseSsuServiceImp::GetInstance().GetAllocInfoByName(name, queried, identity));
    EXPECT_EQ(name, queried.name);
    ASSERT_EQ(1u, queried.nameSpaceList.size());
    EXPECT_EQ(nsInfo.nsDevPath, queried.nameSpaceList[0].nsDevPath);

    // attach → detach 全链路
    std::vector<std::string> nsDevPaths;
    ASSERT_EQ(UBSE_OK, UbseSsuServiceImp::GetInstance().AttachSpace(MakeSpaceReq(name), nsDevPaths));
    ASSERT_EQ(1u, nsDevPaths.size());
    EXPECT_EQ(nsInfo.nsDevPath, nsDevPaths[0]);
    EXPECT_TRUE(module_->vmCache_->IsAttached(name));
    EXPECT_EQ(1, g_itAttachCallCount.load());

    ASSERT_EQ(UBSE_OK, UbseSsuServiceImp::GetInstance().DetachSpace(MakeSpaceReq(name)));
    EXPECT_FALSE(module_->vmCache_->IsAttached(name));
    EXPECT_EQ(1, g_itDetachCallCount.load());
}

/*
 * 用例描述:mock ub-device-manager 推送连接信息 → VM 接收 ACK → VM 模式查询连接信息
 * 测试步骤:
 * 1、推送分配信息(为 identity 校验提供缓存分配条目)
 * 2、推送同名连接信息,等待 ACK
 * 3、ServiceImp VM 模式调用 GetConnectInfo
 * 预期结果:
 * 1、两次推送均 ACK statusCode=0
 * 2、GetConnectInfo 返回推送的连接信息列表
 */
TEST_F(TestSsuVirtPush, PushConnectInfo_FullCycle)
{
    ItVsockPushClient client;
    ASSERT_TRUE(client.Connect(IT_VSOCK_CID_LOCAL, vsockPort_));

    const std::string name = "it_connect";
    const std::string eid(IT_EID_LEN, 'B');
    auto nsInfo = RegisterAdapterNs(eid, "nqn.target." + name, 1, std::string(16, 'Q'));
    PushAndExpectAck(client, UbseVsockMsgType::MSG_TYPE_PUSH_ALLOC_INFO, SerializeAllocBody(MakeAllocResult(name, nsInfo)),
                     2001);

    std::vector<UbseSsuConnectInfo> pushedList{MakeItConnectInfo(name, eid)};
    PushAndExpectAck(client, UbseVsockMsgType::MSG_TYPE_PUSH_CONNECT_INFO, SerializeConnectPushBody(name, pushedList),
                     2002);

    UbseSsuAllocIdentityInfo identity{};
    identity.userName = IT_USER_NAME;
    identity.uid = IT_UID;
    std::vector<UbseSsuConnectInfo> result;
    ASSERT_EQ(UBSE_OK, UbseSsuServiceImp::GetInstance().GetConnectInfo(name, nullptr, result, identity));
    ASSERT_EQ(1u, result.size());
    EXPECT_EQ(pushedList[0].hostNqn, result[0].hostNqn);
    EXPECT_EQ(pushedList[0].nsUuid, result[0].nsUuid);
}

/*
 * 用例描述:全量同步清空旧缓存并批量重建,且同步落盘后新缓存实例可从磁盘恢复
 * 测试步骤:
 * 1、直接向缓存写入一条陈旧分配信息(stale_alloc)
 * 2、推送 FULL_SYNC_RESP(2 条分配信息 + 1 组连接信息)
 * 3、校验缓存状态与磁盘持久化文件
 * 预期结果:
 * 1、ACK statusCode=0,陈旧条目被清空
 * 2、缓存与推送数据一致(全量覆盖语义)
 * 3、handler 同步落盘后,新 UbseSsuVmCache 实例 LoadFromDisk 恢复一致数据
 */
TEST_F(TestSsuVirtPush, PushFullSync_AfterCacheClear)
{
    ItVsockPushClient client;
    ASSERT_TRUE(client.Connect(IT_VSOCK_CID_LOCAL, vsockPort_));

    // 陈旧数据:全量同步后必须被清空
    UbseSsuAllocResult stale{};
    stale.name = "stale_alloc";
    module_->vmCache_->UpdateAllocInfo(stale);

    const std::string nameA = "it_sync_a";
    const std::string nameB = "it_sync_b";
    std::vector<UbseSsuAllocResult> allocList{MakeAllocResult(nameA, UbseSsuNameSpaceInfo{}),
                                              MakeAllocResult(nameB, UbseSsuNameSpaceInfo{})};
    std::vector<std::pair<std::string, std::vector<UbseSsuConnectInfo>>> connectList{
        {nameA, {MakeItConnectInfo(nameA, "eid_sync_a")}}};
    PushAndExpectAck(client, UbseVsockMsgType::MSG_TYPE_FULL_SYNC_RESP, SerializeFullSyncBody(allocList, connectList),
                     3001);

    // 全量覆盖:陈旧条目清空,新数据正确填充
    UbseSsuAllocResult result{};
    EXPECT_NE(UBSE_OK, module_->vmCache_->GetAllocInfo("stale_alloc", result));
    ASSERT_EQ(UBSE_OK, module_->vmCache_->GetAllocInfo(nameA, result));
    EXPECT_EQ(nameA, result.name);
    ASSERT_EQ(UBSE_OK, module_->vmCache_->GetAllocInfo(nameB, result));
    std::vector<UbseSsuConnectInfo> ciResult;
    ASSERT_EQ(UBSE_OK, module_->vmCache_->GetConnectInfo(nameA, ciResult));
    EXPECT_EQ(1u, ciResult.size());

    // 全量同步走同步落盘:新缓存实例应能从磁盘恢复
    UbseSsuVmCache recoveredCache;
    recoveredCache.SetCacheFilePath(cacheFilePath_);
    EXPECT_TRUE(recoveredCache.LoadFromDisk());
    UbseSsuAllocResult recovered{};
    ASSERT_EQ(UBSE_OK, recoveredCache.GetAllocInfo(nameA, recovered));
    EXPECT_EQ(nameA, recovered.name);
    EXPECT_NE(UBSE_OK, recoveredCache.GetAllocInfo("stale_alloc", recovered));
}

/*
 * 用例描述:同一连接上连续推送多条消息,全部 ACK 且缓存逐条更新
 * 测试步骤:
 * 1、单连接连续推送 5 条分配信息与 2 组连接信息
 * 2、逐条等待 ACK
 * 3、校验缓存中的列表与连接信息
 * 预期结果:
 * 1、7 条消息全部 ACK statusCode=0
 * 2、ListAllocInfo 返回全部 5 条分配信息
 * 3、两组连接信息均可按名查询
 */
TEST_F(TestSsuVirtPush, PushMultipleMsgs_Sequential)
{
    ItVsockPushClient client;
    ASSERT_TRUE(client.Connect(IT_VSOCK_CID_LOCAL, vsockPort_));

    constexpr uint32_t kAllocCount = 5;
    uint64_t requestId = 4000;
    for (uint32_t i = 0; i < kAllocCount; ++i) {
        const std::string name = "it_multi_" + std::to_string(i);
        PushAndExpectAck(client, UbseVsockMsgType::MSG_TYPE_PUSH_ALLOC_INFO,
                         SerializeAllocBody(MakeAllocResult(name, UbseSsuNameSpaceInfo{})), requestId++);
    }
    for (uint32_t i = 0; i < 2; ++i) {
        const std::string name = "it_multi_" + std::to_string(i);
        PushAndExpectAck(client, UbseVsockMsgType::MSG_TYPE_PUSH_CONNECT_INFO,
                         SerializeConnectPushBody(name, {MakeItConnectInfo(name, "eid_multi")}), requestId++);
    }

    std::vector<UbseSsuAllocResult> all;
    ASSERT_EQ(UBSE_OK, module_->vmCache_->ListAllocInfo(all));
    EXPECT_EQ(kAllocCount, all.size());

    std::vector<UbseSsuConnectInfo> ciResult;
    EXPECT_EQ(UBSE_OK, module_->vmCache_->GetConnectInfo("it_multi_0", ciResult));
    EXPECT_EQ(1u, ciResult.size());
    EXPECT_EQ(UBSE_OK, module_->vmCache_->GetConnectInfo("it_multi_1", ciResult));
    EXPECT_EQ(1u, ciResult.size());
}

/*
 * 用例描述:推送与 VM 模式查询/attach 并发执行无冲突,推送覆盖与 attach 标记互不破坏
 * 测试步骤:
 * 1、注册适配器可见 namespace 并推送初始分配信息
 * 2、线程 A 持续推送覆盖同名分配信息(不同 nsSize),线程 B 持续 VM 模式查询
 * 3、主线程并发执行一次 AttachSpace
 * 预期结果:
 * 1、并发期间所有查询返回 UBSE_OK,attach 返回 UBSE_OK
 * 2、并发结束后缓存反映最后一次推送内容,IsAttached 保持 true
 * 3、无崩溃、无死锁
 */
TEST_F(TestSsuVirtPush, PushDuringAttach_NoConflict)
{
    ItVsockPushClient client;
    ASSERT_TRUE(client.Connect(IT_VSOCK_CID_LOCAL, vsockPort_));

    const std::string name = "it_conflict";
    const std::string eid(IT_EID_LEN, 'C');
    auto nsInfo = RegisterAdapterNs(eid, "nqn.target." + name, 1, std::string(16, 'R'));
    PushAndExpectAck(client, UbseVsockMsgType::MSG_TYPE_PUSH_ALLOC_INFO, SerializeAllocBody(MakeAllocResult(name, nsInfo)),
                     5001);

    std::atomic<bool> stop{false};
    std::atomic<int> queryOkCount{0};
    // 线程 A:持续推送覆盖同名分配信息
    auto pusher = std::thread([this, &client, &name, &nsInfo, &stop]() {
        uint64_t requestId = 5100;
        for (int i = 0; i < 50 && !stop.load(); ++i) {
            auto updated = nsInfo;
            updated.nsSize = nsInfo.nsSize + static_cast<uint64_t>(i) + 1;
            (void)client.SendPush(UbseVsockMsgType::MSG_TYPE_PUSH_ALLOC_INFO,
                                  SerializeAllocBody(MakeAllocResult(name, updated)), requestId++);
        }
    });
    // 线程 B:持续 VM 模式查询(identity 匹配)
    auto querier = std::thread([this, &name, &stop, &queryOkCount]() {
        UbseSsuAllocIdentityInfo identity{};
        identity.userName = IT_USER_NAME;
        identity.uid = IT_UID;
        while (!stop.load()) {
            UbseSsuAllocResult queried{};
            if (UbseSsuServiceImp::GetInstance().GetAllocInfoByName(name, queried, identity) == UBSE_OK) {
                queryOkCount.fetch_add(1);
            }
        }
    });

    // 主线程并发 attach
    std::vector<std::string> nsDevPaths;
    uint32_t attachRet = UbseSsuServiceImp::GetInstance().AttachSpace(MakeSpaceReq(name), nsDevPaths);

    pusher.join();
    stop.store(true);
    querier.join();

    EXPECT_EQ(UBSE_OK, attachRet);
    EXPECT_GT(queryOkCount.load(), 0);

    // 最终一致性:缓存反映最后一次推送,attach 标记未被推送覆盖破坏
    UbseSsuAllocResult finalResult{};
    ASSERT_EQ(UBSE_OK, module_->vmCache_->GetAllocInfo(name, finalResult));
    ASSERT_EQ(1u, finalResult.nameSpaceList.size());
    EXPECT_EQ(nsInfo.nsUuid, finalResult.nameSpaceList[0].nsUuid);
    EXPECT_TRUE(module_->vmCache_->IsAttached(name));
    EXPECT_EQ(1, g_itAttachCallCount.load());
}

/*
 * 用例描述:非法推送消息返回 NACK(statusCode 非 0),缓存不受污染
 * 测试步骤:
 * 1、推送随机非法 payload(反序列化失败)
 * 2、推送空 payload
 * 3、查询缓存列表
 * 预期结果:
 * 1、两次推送均收到 NACK(statusCode != 0)
 * 2、缓存保持为空
 */
TEST_F(TestSsuVirtPush, PushInvalidPayload_Nack_CacheUnchanged)
{
    ItVsockPushClient client;
    ASSERT_TRUE(client.Connect(IT_VSOCK_CID_LOCAL, vsockPort_));

    // 非法 payload:随机字节无法反序列化
    std::vector<uint8_t> garbage(64);
    for (size_t i = 0; i < garbage.size(); ++i) {
        garbage[i] = static_cast<uint8_t>(i * 7 + 3);
    }
    ASSERT_TRUE(client.SendPush(UbseVsockMsgType::MSG_TYPE_PUSH_ALLOC_INFO, garbage, 6001));
    uint32_t statusCode = 0;
    uint64_t respRequestId = 0;
    std::vector<uint8_t> respBody;
    ASSERT_TRUE(client.RecvResponse(statusCode, respRequestId, respBody));
    EXPECT_NE(0u, statusCode);
    EXPECT_EQ(6001u, respRequestId);

    // 空 payload
    ASSERT_TRUE(client.SendPush(UbseVsockMsgType::MSG_TYPE_PUSH_ALLOC_INFO, {}, 6002));
    ASSERT_TRUE(client.RecvResponse(statusCode, respRequestId, respBody));
    EXPECT_NE(0u, statusCode);

    // 缓存不受污染
    std::vector<UbseSsuAllocResult> all;
    ASSERT_EQ(UBSE_OK, module_->vmCache_->ListAllocInfo(all));
    EXPECT_TRUE(all.empty());
}

/*
 * 用例描述:TLS 握手冒烟:
 * 自签证书双向认证下 vsock server 与客户端完成 TLS1.3 握手并加密收发推送消息
 * 测试步骤:
 * 1、openssl 自签生成 CA/服务端/客户端证书(2 天有效期,进程级临时目录,缺失 openssl 则跳过)
 * 2、启动独立 vsock server(enableTls=true),注册 PUSH_ALLOC_INFO 应答 handler
 * 3、客户端连接后 EnableTls 完成双向认证握手,推送一条消息并等待 ACK
 * 预期结果:
 * 1、TLS 握手成功,推送收到 ACK(statusCode=0 且 clientRequestId 回填)
 * 2、握手完成后连接状态迁移为 CONNECTED
 */
TEST(TestSsuVirtTls, TlsHandshake_PushAllocInfo_Ack)
{
    if (!IsVsockLoopbackAvailable()) {
        GTEST_SKIP() << "AF_VSOCK loopback unavailable (requires vsock_loopback kernel module)";
    }
    const std::string certDir = "/tmp/ssu_virt_it_tls_" + std::to_string(static_cast<long>(getpid()));
    (void)system(("rm -rf " + certDir).c_str());
    if (!MakeItTlsCertDir(certDir)) {
        GTEST_SKIP() << "openssl unavailable or self-signed cert generation failed";
    }

    // 独立 vsock server(enableTls=true,不经过 SSU 模块,聚焦 TLS 握手与加密链路)
    ubse::vsock::UbseVsockConfig cfg{};
    cfg.enableTls = true;
    cfg.certDir = certDir;
    cfg.threadPoolSize = 1;
    cfg.threadPoolQueueSize = 16;
    std::unique_ptr<ubse::vsock::UbseVsockServer> server;
    uint32_t port = 0;
    for (int attempt = 0; attempt < IT_MAX_PORT_ATTEMPTS && server == nullptr; ++attempt) {
        cfg.port = IT_VSOCK_PORT_BASE + static_cast<uint32_t>(getpid() % 512) * IT_MAX_PORT_ATTEMPTS +
                   static_cast<uint32_t>(attempt);
        auto candidate = std::make_unique<ubse::vsock::UbseVsockServer>(cfg);
        if (candidate->Start() != UBSE_OK) {
            continue; // 端口冲突依次后移
        }
        server = std::move(candidate);
        port = cfg.port;
    }
    ASSERT_NE(nullptr, server) << "failed to start TLS vsock server on any candidate port";

    // 应答 handler:回空 body ACK(与推送协议 ACK 形态一致)
    auto *serverPtr = server.get();
    ASSERT_EQ(UBSE_OK, server->RegisterHandler(
                           0x0007, static_cast<uint16_t>(UbseVsockMsgType::MSG_TYPE_PUSH_ALLOC_INFO),
                           [serverPtr](const ubse::vsock::UbseIpcMessage &, const ubse::vsock::UbseRequestContext &ctx) {
                               ubse::vsock::UbseIpcMessage empty {};
                               empty.buffer = nullptr;
                               empty.length = 0;
                               return serverPtr->SendResponse(UBSE_OK, ctx.requestId, empty);
                           }));

    ItVsockPushClient client;
    ASSERT_TRUE(client.Connect(IT_VSOCK_CID_LOCAL, port));
    ASSERT_TRUE(client.EnableTls(certDir)) << "TLS handshake failed, err="
                                           << ERR_error_string(ERR_get_error(), nullptr);

    // 加密链路上完成一次推送 → ACK 交互
    const std::vector<uint8_t> body = {1, 2, 3, 4};
    ASSERT_TRUE(client.SendPush(UbseVsockMsgType::MSG_TYPE_PUSH_ALLOC_INFO, body, 9001));
    uint32_t statusCode = 0xFFFFFFFF;
    uint64_t respRequestId = 0;
    std::vector<uint8_t> respBody;
    ASSERT_TRUE(client.RecvResponse(statusCode, respRequestId, respBody));
    EXPECT_EQ(0u, statusCode);
    EXPECT_EQ(9001u, respRequestId);
    // 握手完成后连接状态迁移为 CONNECTED
    EXPECT_TRUE(WaitForConnState(*server, ubse::vsock::UbseVsockConnState::CONNECTED, 2000));

    client.Close();
    server->Stop();
    (void)system(("rm -rf " + certDir).c_str());
}

} // namespace ubse::ssu::virt::it
