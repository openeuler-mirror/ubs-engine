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

#include <gtest/gtest.h>

#include <securec.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "src/framework/vscok/ubse_vsock_server.h"
#include "src/include/ubse_error.h"

/*
 * 用例说明:Vsock Server 服务层 UT
 * 覆盖范围:配置校验、RegisterHandler(重复/空/并发)、请求路由、
 * 服务层 SendResponse/Start/Stop 错误路径、连接状态查询。
 * 传输层用例(消息编解码/会话管理/响应写入)见 test_ubse_vsock_transport.cpp;
 * 需要真实 AF_VSOCK 设备的连接/会话/TLS 握手由 IT 层覆盖。
 */
namespace ubse::vsock::ut {
namespace {
constexpr uint32_t TEST_PORT = 6174;
constexpr uint16_t TEST_MODULE_CODE = 0x0007; // UBSE_SSU
constexpr uint16_t TEST_OP_CODE = 0x0100;     // MSG_TYPE_PUSH_ALLOC_INFO

// 构造 UbseRequestHeader + body 的完整请求字节流
std::vector<uint8_t> MakeRequestBytes(uint16_t moduleCode, uint16_t opCode, const std::string& body,
                                      uint64_t clientRequestId)
{
    UbseRequestHeader header = {};
    header.moduleCode = moduleCode;
    header.opCode = opCode;
    header.bodyLen = static_cast<uint32_t>(body.size());
    header.clientRequestId = clientRequestId;
    std::vector<uint8_t> bytes(sizeof(header) + body.size());
    if (memcpy_s(bytes.data(), bytes.size(), &header, sizeof(header)) != EOK) {
        return {};
    }
    if (!body.empty() && memcpy_s(bytes.data() + sizeof(header), body.size(), body.data(), body.size()) != EOK) {
        return {};
    }
    return bytes;
}

// handler 中捕获的请求字段(body 需在 handler 内深拷贝,body 指针在任务结束后释放)
struct CapturedRequest {
    uint16_t moduleCode{0};
    uint16_t opCode{0};
    uint32_t bodyLen{0};
    uint64_t clientRequestId{0};
    uint64_t requestId{0};
    std::string body;
};
} // namespace

class TestUbseVsockServer : public testing::Test {
public:
    void SetUp() override
    {
        config_.port = TEST_PORT;
        config_.enableTls = false; // UT 环境无证书,TLS 路径走错误分支覆盖
        server_ = std::make_unique<UbseVsockServer>(config_);
    }

    void TearDown() override
    {
        if (server_ != nullptr) {
            server_->Stop();
        }
    }

protected:
    UbseVsockConfig config_;
    std::unique_ptr<UbseVsockServer> server_;
};

/*
 * 用例描述:
 * 默认配置(端口 6174、TLS 开启、证书目录非空)合法
 * 测试步骤:
 * 1、构造默认 UbseVsockConfig
 * 2、调用 IsValid()
 * 预期结果:
 * 1、IsValid() 返回 true
 */
TEST_F(TestUbseVsockServer, Config_DefaultIsValid)
{
    UbseVsockConfig config;
    EXPECT_TRUE(config.IsValid());
    EXPECT_EQ(UBSE_VSOCK_DEFAULT_PORT, config.port);
    EXPECT_TRUE(config.enableTls);
    EXPECT_FALSE(config.certDir.empty());
}

/*
 * 用例描述:
 * 端口为 0 时配置非法
 * 测试步骤:
 * 1、构造 port=0 的配置
 * 2、调用 IsValid()
 * 预期结果:
 * 1、IsValid() 返回 false
 */
TEST_F(TestUbseVsockServer, Config_ZeroPort_IsInvalid)
{
    UbseVsockConfig config;
    config.port = 0;
    EXPECT_FALSE(config.IsValid());
}

/*
 * 用例描述:
 * enableTls=true 且 certDir 为空时配置非法
 * 测试步骤:
 * 1、构造 enableTls=true、certDir="" 的配置
 * 2、调用 IsValid()
 * 预期结果:
 * 1、IsValid() 返回 false
 */
TEST_F(TestUbseVsockServer, Config_TlsEnabledEmptyCertDir_IsInvalid)
{
    UbseVsockConfig config;
    config.enableTls = true;
    config.certDir = "";
    EXPECT_FALSE(config.IsValid());
}

/*
 * 用例描述:
 * enableTls=false 时 certDir 允许为空(明文模式无证书依赖)
 * 测试步骤:
 * 1、构造 enableTls=false、certDir="" 的配置
 * 2、调用 IsValid()
 * 预期结果:
 * 1、IsValid() 返回 true
 */
TEST_F(TestUbseVsockServer, Config_TlsDisabledEmptyCertDir_IsValid)
{
    UbseVsockConfig config;
    config.enableTls = false;
    config.certDir = "";
    EXPECT_TRUE(config.IsValid());
}

/*
 * 用例描述:
 * 正常注册 handler 成功
 * 测试步骤:
 * 1、调用 RegisterHandler 注册 (moduleCode, opCode, handler)
 * 预期结果:
 * 1、返回 UBSE_OK
 */
TEST_F(TestUbseVsockServer, RegisterHandler_Valid_ReturnsOk)
{
    auto ret = server_->RegisterHandler(TEST_MODULE_CODE, TEST_OP_CODE,
                                        [](const UbseIpcMessage&, const UbseRequestContext&) { return UBSE_OK; });
    EXPECT_EQ(UBSE_OK, ret);
}

/*
 * 用例描述:
 * 重复注册相同 (moduleCode, opCode) 被拒绝
 * 测试步骤:
 * 1、首次注册 handler
 * 2、再次注册相同 (moduleCode, opCode)
 * 预期结果:
 * 1、首次注册返回 UBSE_OK
 * 2、重复注册返回 UBSE_ERR_EXISTED
 */
TEST_F(TestUbseVsockServer, RegisterHandler_Duplicate_ReturnsExisted)
{
    auto handler = [](const UbseIpcMessage&, const UbseRequestContext&) {
        return UBSE_OK;
    };
    EXPECT_EQ(UBSE_OK, server_->RegisterHandler(TEST_MODULE_CODE, TEST_OP_CODE, handler));
    EXPECT_EQ(UBSE_ERR_EXISTED, server_->RegisterHandler(TEST_MODULE_CODE, TEST_OP_CODE, handler));
}

/*
 * 用例描述:
 * 注册空 handler 被拒绝
 * 测试步骤:
 * 1、调用 RegisterHandler 传入 nullptr
 * 预期结果:
 * 1、返回 UBSE_ERR_INVALID_ARG
 */
TEST_F(TestUbseVsockServer, RegisterHandler_NullHandler_ReturnsInvalidArg)
{
    EXPECT_EQ(UBSE_ERR_INVALID_ARG, server_->RegisterHandler(TEST_MODULE_CODE, TEST_OP_CODE, nullptr));
}

/*
 * 用例描述:
 * 多线程并发注册不同 (moduleCode, opCode) 无竞态
 * 测试步骤:
 * 1、8 个线程并发注册 opCode 不同的 handler
 * 2、检查注册结果与注册表完整性
 * 预期结果:
 * 1、全部注册返回 UBSE_OK
 * 2、再次注册相同 opCode 均返回 UBSE_ERR_EXISTED
 */
TEST_F(TestUbseVsockServer, RegisterHandler_ConcurrentDifferentHandlers_AllRegistered)
{
    constexpr int threadCount = 8;
    std::vector<std::thread> threads;
    std::atomic<uint32_t> okCount{0};
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([this, &okCount, i]() {
            auto ret =
                server_->RegisterHandler(TEST_MODULE_CODE, static_cast<uint16_t>(TEST_OP_CODE + i),
                                         [](const UbseIpcMessage&, const UbseRequestContext&) { return UBSE_OK; });
            if (ret == UBSE_OK) {
                okCount.fetch_add(1);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(static_cast<uint32_t>(threadCount), okCount.load());
    for (int i = 0; i < threadCount; ++i) {
        EXPECT_EQ(UBSE_ERR_EXISTED,
                  server_->RegisterHandler(TEST_MODULE_CODE, static_cast<uint16_t>(TEST_OP_CODE + i),
                                           [](const UbseIpcMessage&, const UbseRequestContext&) { return UBSE_OK; }));
    }
}

/*
 * 用例描述:
 * 服务层将请求路由到已注册的 handler 并透传消息内容
 * 测试步骤:
 * 1、注册 handler 捕获消息与上下文
 * 2、构造 UbseRequestMessage + UbseRequestContext 调用 HandleRequest
 * 预期结果:
 * 1、handler 被调用,moduleCode/opCode/body/requestId 透传一致
 */
TEST_F(TestUbseVsockServer, HandleRequest_RoutesToRegisteredHandler)
{
    CapturedRequest captured;
    std::mutex captureMutex;
    EXPECT_EQ(UBSE_OK, server_->RegisterHandler(
                           TEST_MODULE_CODE, TEST_OP_CODE,
                           [&captured, &captureMutex](const UbseIpcMessage& msg, const UbseRequestContext& ctx) {
                               std::lock_guard<std::mutex> lock(captureMutex);
                               captured.moduleCode = ctx.moduleCode;
                               captured.opCode = ctx.opCode;
                               captured.requestId = ctx.requestId;
                               captured.bodyLen = msg.length;
                               captured.body.assign(reinterpret_cast<const char*>(msg.buffer), msg.length);
                               return UBSE_OK;
                           }));
    const std::string body = "push-alloc-info-payload";
    auto bytes = MakeRequestBytes(TEST_MODULE_CODE, TEST_OP_CODE, body, 0x1122334455);
    UbseRequestMessage request = {};
    if (memcpy_s(&request.header, sizeof(request.header), bytes.data(), sizeof(request.header)) != EOK) {
        FAIL() << "copy header failed";
    }
    request.body = bytes.data() + sizeof(request.header);
    request.freeFunc = nullptr;
    UbseRequestContext context = {};
    context.requestId = 42;
    context.moduleCode = TEST_MODULE_CODE;
    context.opCode = TEST_OP_CODE;
    server_->HandleRequest(request, context);
    EXPECT_EQ(TEST_MODULE_CODE, captured.moduleCode);
    EXPECT_EQ(TEST_OP_CODE, captured.opCode);
    EXPECT_EQ(42U, captured.requestId);
    EXPECT_EQ(body.size(), captured.bodyLen);
    EXPECT_EQ(body, captured.body);
}

/*
 * 用例描述:
 * 未注册的 (moduleCode, opCode) 请求不会路由到其他 handler
 * 测试步骤:
 * 1、注册 opCode=A 的 handler
 * 2、以 opCode=B 调用 HandleRequest
 * 预期结果:
 * 1、handler 不被调用,流程正常返回(错误响应因传输层未启动而失败,不影响断言)
 */
TEST_F(TestUbseVsockServer, HandleRequest_UnregisteredOp_HandlerNotCalled)
{
    std::atomic<bool> called{false};
    EXPECT_EQ(UBSE_OK, server_->RegisterHandler(TEST_MODULE_CODE, TEST_OP_CODE,
                                                [&called](const UbseIpcMessage&, const UbseRequestContext&) {
                                                    called.store(true);
                                                    return UBSE_OK;
                                                }));
    auto bytes = MakeRequestBytes(TEST_MODULE_CODE, TEST_OP_CODE + 1, "body", 1);
    UbseRequestMessage request = {};
    if (memcpy_s(&request.header, sizeof(request.header), bytes.data(), sizeof(request.header)) != EOK) {
        FAIL() << "copy header failed";
    }
    request.body = bytes.data() + sizeof(request.header);
    request.freeFunc = nullptr;
    UbseRequestContext context = {};
    context.requestId = 43;
    context.moduleCode = TEST_MODULE_CODE;
    context.opCode = TEST_OP_CODE + 1;
    server_->HandleRequest(request, context);
    EXPECT_FALSE(called.load());
}

/*
 * 用例描述:
 * 传输层未启动时 SendResponse 返回错误
 * 测试步骤:
 * 1、不调用 Start,直接调用 SendResponse
 * 预期结果:
 * 1、返回非 UBSE_OK(连接不可达)
 */
TEST_F(TestUbseVsockServer, SendResponse_NotStarted_ReturnsError)
{
    uint8_t body[4] = {1, 2, 3, 4};
    UbseIpcMessage response = {};
    response.buffer = body;
    response.length = sizeof(body);
    EXPECT_NE(UBSE_OK, server_->SendResponse(UBSE_OK, 1, response));
}

/*
 * 用例描述:
 * 非法端口配置启动返回参数错误
 * 测试步骤:
 * 1、构造 port=0 的配置并创建 server
 * 2、调用 Start()
 * 预期结果:
 * 1、返回 UBSE_ERR_INVALID_ARG
 */
TEST_F(TestUbseVsockServer, Start_InvalidPort_ReturnsInvalidArg)
{
    UbseVsockConfig config;
    config.port = 0;
    config.enableTls = false;
    UbseVsockServer server(config);
    EXPECT_EQ(UBSE_ERR_INVALID_ARG, server.Start());
}

/*
 * 用例描述:
 * 未启动时调用 Stop 无异常
 * 测试步骤:
 * 1、构造 server 后直接调用 Stop()
 * 预期结果:
 * 1、无崩溃、无资源残留
 */
TEST_F(TestUbseVsockServer, Stop_NotStarted_NoCrash)
{
    server_->Stop();
    server_->Stop(); // 幂等:重复 Stop 同样安全
    SUCCEED();
}

/*
 * 用例描述:
 * 初始连接状态为 WAITING_CONNECT
 * 测试步骤:
 * 1、构造 server(未启动)
 * 2、调用 GetConnectionStatus()
 * 预期结果:
 * 1、返回 UbseVsockConnState::WAITING_CONNECT
 */
TEST_F(TestUbseVsockServer, GetConnectionStatus_InitialState_ReturnsWaitingConnect)
{
    EXPECT_EQ(UbseVsockConnState::WAITING_CONNECT, server_->GetConnectionStatus());
}
} // namespace ubse::vsock::ut
