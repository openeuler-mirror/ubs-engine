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

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <securec.h>
#include <csignal>

#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "mockcpp/mockcpp.hpp"
#include "src/framework/vscok/ubse_vsock_transport.h"
#include "src/include/ubse_error.h"

/*
 * 用例说明:Vsock 传输层 UT
 * 覆盖范围:消息编解码(完整包/半包/粘包/异常包/零长度)、SendResponse 正常与异常路径、
 * 连接断开清理(HandleWrite 致命错误)、TLS 证书加载失败路径。
 * 依赖真实 AF_VSOCK 设备的端到端连接与 TLS 握手场景由 IT 层覆盖。
 */
namespace ubse::vsock::ut {
namespace {
constexpr uint32_t TEST_PORT = 6174;
constexpr uint16_t TEST_MODULE_CODE = 0x0007; // UBSE_SSU
constexpr uint16_t TEST_OP_CODE = 0x0100;     // MSG_TYPE_PUSH_ALLOC_INFO
constexpr int WAIT_TIMEOUT_MS = 2000;

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

// handler 中捕获的请求字段(body 需在回调内深拷贝,body 指针在任务结束后释放)
struct CapturedRequest {
    uint16_t moduleCode{0};
    uint16_t opCode{0};
    uint32_t bodyLen{0};
    uint64_t clientRequestId{0};
    uint64_t requestId{0};
    std::string body;
};

int AcceptWouldBlock(int, struct sockaddr*, socklen_t*, int)
{
    errno = EAGAIN;
    return -1;
}
} // namespace

class TestUbseVsockTransport : public testing::Test {
public:
    void SetUp() override
    {
        config_.port = TEST_PORT;
        config_.enableTls = false; // UT 环境无证书,TLS 路径走错误分支覆盖
    }

protected:
    UbseVsockConfig config_;
};

/*
 * 用例描述:
 * enableTls=true 且证书目录不存在时启动失败
 * 测试步骤:
 * 1、构造 enableTls=true、certDir 指向不存在目录的配置
 * 2、调用 UbseVsockTransport::Start()
 * 预期结果:
 * 1、返回 UBSE_ERR_INTERNAL(证书加载失败)
 */
TEST_F(TestUbseVsockTransport, Start_TlsCertDirMissing_ReturnsInternal)
{
    UbseVsockConfig config;
    config.port = TEST_PORT;
    config.enableTls = true;
    config.certDir = "/nonexistent_vsock_cert_dir_for_ut";
    UbseVsockTransport transport(config);
    EXPECT_EQ(UBSE_ERR_INTERNAL, transport.Start());
}

/*
 * 用例描述:
 * Stop 丢弃队列中尚未执行的 TLS 握手任务时,须关闭已 accept 但未握手的 socket
 * 测试步骤:
 * 1、构造 enableTls=true 的传输层,注入队列充足且工作线程被阻塞的任务执行器
 * 2、mock accept4 返回 socketpair 端点,调用 HandleAccept 投递握手任务
 * 3、另起线程调用 Stop,待取消任务入队后放行工作线程
 * 预期结果:
 * 1、握手任务已入队且 socket 仍处于打开状态
 * 2、任务被取消后 socket 已关闭(fcntl 返回 EBADF),sessions_ 为空
 */
TEST_F(TestUbseVsockTransport, Stop_QueuedTlsHandshake_ClosesSocket)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    config_.enableTls = true;
    UbseVsockTransport transport(config_);
    auto executor = task_executor::UbseTaskExecutor::Create("UtTlsCancel", 1, 4);
    ASSERT_NE(nullptr, executor.Get());
    std::promise<void> releaseWorker;
    auto released = releaseWorker.get_future().share();
    executor->SetThreadInitCallback([released]() { released.wait(); });
    ASSERT_TRUE(executor->Start());
    transport.handshakeExecutor_ = executor;
    transport.running_ = true;
    MOCKER(accept4).stubs().will(returnValue(fds[0])).then(invoke(AcceptWouldBlock));
    transport.HandleAccept();
    GlobalMockObject::verify();
    EXPECT_EQ(1U, executor->mRunnableQueue.mRingBuffer_.Size());
    EXPECT_NE(-1, fcntl(fds[0], F_GETFD));

    std::thread stopper([&transport]() { transport.Stop(); });
    // 等待 STOP 插到队首后再放行工作线程,确保握手任务被取消而非执行。
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(WAIT_TIMEOUT_MS);
    while (executor->mRunnableQueue.mRingBuffer_.Size() != 2 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_EQ(2U, executor->mRunnableQueue.mRingBuffer_.Size());
    releaseWorker.set_value();
    stopper.join();

    errno = 0;
    const int fdStatus = fcntl(fds[0], F_GETFD);
    EXPECT_EQ(-1, fdStatus);
    EXPECT_EQ(EBADF, errno);
    EXPECT_TRUE(transport.sessions_.empty());
    if (fdStatus != -1) {
        close(fds[0]); // 旧代码复现失败时也回收测试 socket。
    }
    close(fds[1]);
}

/*
 * 用例描述:
 * TLS 握手任务因队列满未能入队时,须立即关闭该连接 socket
 * 测试步骤:
 * 1、构造 enableTls=true 的传输层,注入容量为 1 且工作线程被阻塞的任务执行器
 * 2、先投递一个任务占满唯一队列槽位
 * 3、mock accept4 返回 socketpair 端点,调用 HandleAccept
 * 预期结果:
 * 1、握手任务入队失败,socket 被关闭(fcntl 返回 EBADF)
 * 2、释放工作线程并调用 Stop 后资源无重复关闭
 */
TEST_F(TestUbseVsockTransport, HandleAccept_TlsQueueFull_ClosesSocket)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    config_.enableTls = true;
    UbseVsockTransport transport(config_);
    auto executor = task_executor::UbseTaskExecutor::Create("UtTlsReject", 1, 1);
    ASSERT_NE(nullptr, executor.Get());
    std::promise<void> releaseWorker;
    auto released = releaseWorker.get_future().share();
    executor->SetThreadInitCallback([released]() { released.wait(); });
    ASSERT_TRUE(executor->Start());
    transport.handshakeExecutor_ = executor;
    transport.running_ = true;
    EXPECT_TRUE(executor->Execute([]() {})); // 填满唯一队列槽位。
    MOCKER(accept4).stubs().will(returnValue(fds[0])).then(invoke(AcceptWouldBlock));
    transport.HandleAccept();
    GlobalMockObject::verify();
    errno = 0;
    const int fdStatus = fcntl(fds[0], F_GETFD);
    EXPECT_EQ(-1, fdStatus);
    EXPECT_EQ(EBADF, errno);
    releaseWorker.set_value();
    transport.Stop();
    if (fdStatus != -1) {
        close(fds[0]);
    }
    close(fds[1]);
}

/*
 * 用例描述:
 * 完整消息(头+体)解码成功并提交线程池
 * 测试步骤:
 * 1、构造传输层(不 Start),注入线程池与请求回调,构造会话
 * 2、向 readBuffer 写入完整请求
 * 3、调用 TryDecodeMessage 并等待回调执行
 * 预期结果:
 * 1、TryDecodeMessage 返回 true
 * 2、回调收到正确的 moduleCode/opCode/body/clientRequestId
 * 3、readBuffer 被消费清空,requestId→fd 映射已记录
 */
TEST_F(TestUbseVsockTransport, TryDecodeMessage_FullPacket_SubmitsToPool)
{
    UbseVsockTransport transport(config_);
    int fds[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    auto session = std::make_unique<UbseVsockTransport::Session>();
    session->fd = fds[0];
    const int fd = fds[0];
    transport.sessions_[fd] = std::move(session);
    transport.threadPool_ = task_executor::UbseTaskExecutor::Create("UtVsockExecutor", 2, 16);
    ASSERT_NE(nullptr, transport.threadPool_.Get());
    ASSERT_TRUE(transport.threadPool_->Start());

    std::mutex captureMutex;
    std::vector<CapturedRequest> captured;
    transport.requestHandler_ = [&captured, &captureMutex](const UbseRequestMessage& req,
                                                           const UbseRequestContext& ctx) {
        std::lock_guard<std::mutex> lock(captureMutex);
        CapturedRequest item;
        item.moduleCode = req.header.moduleCode;
        item.opCode = req.header.opCode;
        item.bodyLen = req.header.bodyLen;
        item.clientRequestId = req.header.clientRequestId;
        item.requestId = ctx.requestId;
        item.body.assign(reinterpret_cast<const char*>(req.body), req.header.bodyLen);
        captured.push_back(item);
    };

    const std::string body = "alloc-info-body";
    auto bytes = MakeRequestBytes(TEST_MODULE_CODE, TEST_OP_CODE, body, 0xAABBCCDD);
    auto& sessionRef = transport.sessions_[fd];
    sessionRef->readBuffer.assign(bytes.begin(), bytes.end());
    EXPECT_TRUE(transport.TryDecodeMessage(fd));

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(WAIT_TIMEOUT_MS);
    while (captured.empty() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_EQ(1U, captured.size());
    EXPECT_EQ(TEST_MODULE_CODE, captured[0].moduleCode);
    EXPECT_EQ(TEST_OP_CODE, captured[0].opCode);
    EXPECT_EQ(body.size(), captured[0].bodyLen);
    EXPECT_EQ(body, captured[0].body);
    EXPECT_EQ(0xAABBCCDDU, captured[0].clientRequestId);
    EXPECT_GT(captured[0].requestId, 0U);
    EXPECT_TRUE(transport.sessions_[fd]->readBuffer.empty());
    {
        std::lock_guard<std::mutex> lock(transport.requestMapMutex_);
        ASSERT_EQ(1U, transport.requestIdToFd_.size());
        EXPECT_EQ(fd, transport.requestIdToFd_.begin()->second.fd);
        EXPECT_EQ(0xAABBCCDDU, transport.requestIdToFd_.begin()->second.clientRequestId);
    }
    transport.threadPool_->Stop();
    transport.threadPool_ = nullptr;
    transport.sessions_.clear();
    close(fds[0]);
    close(fds[1]);
}

/*
 * 用例描述:
 * 半包(消息头不完整)等待更多数据
 * 测试步骤:
 * 1、readBuffer 只写入 8 字节(不足 16 字节消息头)
 * 2、调用 TryDecodeMessage
 * 预期结果:
 * 1、返回 true(未判定为恶意数据)
 * 2、不产生请求回调,readBuffer 保留等待续传
 */
TEST_F(TestUbseVsockTransport, TryDecodeMessage_HalfHeader_Waits)
{
    UbseVsockTransport transport(config_);
    int fds[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    auto session = std::make_unique<UbseVsockTransport::Session>();
    session->fd = fds[0];
    const int fd = fds[0];
    transport.sessions_[fd] = std::move(session);
    std::atomic<uint32_t> callCount{0};
    transport.requestHandler_ = [&callCount](const UbseRequestMessage&, const UbseRequestContext&) {
        callCount.fetch_add(1);
    };

    std::vector<uint8_t> partial(sizeof(UbseRequestHeader) / 2, 0x5A);
    transport.sessions_[fd]->readBuffer = partial;
    EXPECT_TRUE(transport.TryDecodeMessage(fd));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(0U, callCount.load());
    EXPECT_EQ(partial.size(), transport.sessions_[fd]->readBuffer.size());
    transport.sessions_.clear();
    close(fds[0]);
    close(fds[1]);
}

/*
 * 用例描述:
 * 半包(消息体不完整)等待更多数据
 * 测试步骤:
 * 1、写入完整消息头(bodyLen=16)+ 4 字节 body
 * 2、调用 TryDecodeMessage
 * 预期结果:
 * 1、返回 true,不产生请求回调,readBuffer 保留
 */
TEST_F(TestUbseVsockTransport, TryDecodeMessage_BodyIncomplete_Waits)
{
    UbseVsockTransport transport(config_);
    int fds[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    auto session = std::make_unique<UbseVsockTransport::Session>();
    session->fd = fds[0];
    const int fd = fds[0];
    transport.sessions_[fd] = std::move(session);
    std::atomic<uint32_t> callCount{0};
    transport.requestHandler_ = [&callCount](const UbseRequestMessage&, const UbseRequestContext&) {
        callCount.fetch_add(1);
    };

    auto bytes = MakeRequestBytes(TEST_MODULE_CODE, TEST_OP_CODE, std::string(16, 'x'), 1);
    bytes.resize(sizeof(UbseRequestHeader) + 4); // 截断 body
    transport.sessions_[fd]->readBuffer = bytes;
    EXPECT_TRUE(transport.TryDecodeMessage(fd));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(0U, callCount.load());
    EXPECT_EQ(bytes.size(), transport.sessions_[fd]->readBuffer.size());
    transport.sessions_.clear();
    close(fds[0]);
    close(fds[1]);
}

/*
 * 用例描述:
 * bodyLen 超过最大限制判定为恶意数据并关闭连接
 * 测试步骤:
 * 1、写入 bodyLen=UBSE_MESSAGE_SIZE+1 的消息头
 * 2、调用 TryDecodeMessage
 * 预期结果:
 * 1、返回 false(调用方据此关闭连接)
 */
TEST_F(TestUbseVsockTransport, TryDecodeMessage_OversizedBodyLen_ClosesConnection)
{
    UbseVsockTransport transport(config_);
    int fds[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    auto session = std::make_unique<UbseVsockTransport::Session>();
    session->fd = fds[0];
    const int fd = fds[0];
    transport.sessions_[fd] = std::move(session);

    UbseRequestHeader header = {};
    header.moduleCode = TEST_MODULE_CODE;
    header.opCode = TEST_OP_CODE;
    header.bodyLen = UBSE_MESSAGE_SIZE + 1;
    header.clientRequestId = 1;
    std::vector<uint8_t> bytes(sizeof(header));
    if (memcpy_s(bytes.data(), bytes.size(), &header, sizeof(header)) != EOK) {
        FAIL() << "copy header failed";
    }
    transport.sessions_[fd]->readBuffer = bytes;
    EXPECT_FALSE(transport.TryDecodeMessage(fd));
    transport.sessions_.clear();
    close(fds[0]);
    close(fds[1]);
}

/*
 * 用例描述:
 * bodyLen=0 的空消息可正常解码
 * 测试步骤:
 * 1、写入 bodyLen=0 的消息头
 * 2、调用 TryDecodeMessage 并等待回调
 * 预期结果:
 * 1、返回 true,回调收到 bodyLen=0 的请求
 */
TEST_F(TestUbseVsockTransport, TryDecodeMessage_ZeroBodyLen_CreatesRequest)
{
    UbseVsockTransport transport(config_);
    int fds[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    auto session = std::make_unique<UbseVsockTransport::Session>();
    session->fd = fds[0];
    const int fd = fds[0];
    transport.sessions_[fd] = std::move(session);
    transport.threadPool_ = task_executor::UbseTaskExecutor::Create("UtVsockExecutor", 2, 16);
    ASSERT_NE(nullptr, transport.threadPool_.Get());
    ASSERT_TRUE(transport.threadPool_->Start());
    std::mutex captureMutex;
    std::vector<CapturedRequest> captured;
    transport.requestHandler_ = [&captured, &captureMutex](const UbseRequestMessage& req,
                                                           const UbseRequestContext& ctx) {
        std::lock_guard<std::mutex> lock(captureMutex);
        CapturedRequest item;
        item.moduleCode = req.header.moduleCode;
        item.opCode = req.header.opCode;
        item.bodyLen = req.header.bodyLen;
        item.requestId = ctx.requestId;
        captured.push_back(item);
    };

    auto bytes = MakeRequestBytes(TEST_MODULE_CODE, TEST_OP_CODE, "", 7);
    transport.sessions_[fd]->readBuffer = bytes;
    EXPECT_TRUE(transport.TryDecodeMessage(fd));
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(WAIT_TIMEOUT_MS);
    while (captured.empty() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_EQ(1U, captured.size());
    EXPECT_EQ(0U, captured[0].bodyLen);
    EXPECT_EQ(TEST_MODULE_CODE, captured[0].moduleCode);
    transport.threadPool_->Stop();
    transport.threadPool_ = nullptr;
    transport.sessions_.clear();
    close(fds[0]);
    close(fds[1]);
}

/*
 * 用例描述:
 * 粘包场景:一次读缓冲中包含多条完整消息,全部解码
 * 测试步骤:
 * 1、readBuffer 写入两条完整消息
 * 2、调用 TryDecodeMessage 并等待回调
 * 预期结果:
 * 1、返回 true,两条消息均被解码分发,readBuffer 清空
 */
TEST_F(TestUbseVsockTransport, TryDecodeMessage_MultipleMessages_AllDecoded)
{
    UbseVsockTransport transport(config_);
    int fds[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    auto session = std::make_unique<UbseVsockTransport::Session>();
    session->fd = fds[0];
    const int fd = fds[0];
    transport.sessions_[fd] = std::move(session);
    transport.threadPool_ = task_executor::UbseTaskExecutor::Create("UtVsockExecutor", 2, 16);
    ASSERT_NE(nullptr, transport.threadPool_.Get());
    ASSERT_TRUE(transport.threadPool_->Start());
    std::mutex captureMutex;
    std::vector<uint64_t> clientRequestIds;
    transport.requestHandler_ = [&clientRequestIds, &captureMutex](const UbseRequestMessage& req,
                                                                   const UbseRequestContext&) {
        std::lock_guard<std::mutex> lock(captureMutex);
        clientRequestIds.push_back(req.header.clientRequestId);
    };

    auto first = MakeRequestBytes(TEST_MODULE_CODE, TEST_OP_CODE, "msg-one", 0x1111);
    auto second = MakeRequestBytes(TEST_MODULE_CODE, TEST_OP_CODE + 1, "msg-two", 0x2222);
    first.insert(first.end(), second.begin(), second.end());
    transport.sessions_[fd]->readBuffer = first;
    EXPECT_TRUE(transport.TryDecodeMessage(fd));
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(WAIT_TIMEOUT_MS);
    while (clientRequestIds.size() < 2 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_EQ(2U, clientRequestIds.size());
    EXPECT_TRUE(transport.sessions_[fd]->readBuffer.empty());
    transport.threadPool_->Stop();
    transport.threadPool_ = nullptr;
    transport.sessions_.clear();
    close(fds[0]);
    close(fds[1]);
}

/*
 * 用例描述:
 * SendResponse 将响应头+响应体写入目标会话写缓冲并回填 clientRequestId
 * 测试步骤:
 * 1、构造传输层(不 Start),手工置位运行标记/epoll fd/会话/requestId 映射
 * 2、调用 SendResponse
 * 预期结果:
 * 1、返回 UBSE_OK
 * 2、writeBuffer 长度为 sizeof(UbseResponseHeader)+bodyLen
 * 3、响应头 statusCode/clientRequestId 正确,requestId 映射被清理
 */
TEST_F(TestUbseVsockTransport, SendResponse_SessionExists_AppendsWriteBuffer)
{
    UbseVsockTransport transport(config_);
    int fds[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    auto session = std::make_unique<UbseVsockTransport::Session>();
    session->fd = fds[0];
    const int fd = fds[0];
    transport.sessions_[fd] = std::move(session);
    transport.epollFd_ = epoll_create1(0);
    ASSERT_NE(-1, transport.epollFd_);
    // 模拟 HandleAccept 行为:会话 fd 先注册到 epoll
    struct epoll_event ev = {};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fd;
    ASSERT_EQ(0, epoll_ctl(transport.epollFd_, EPOLL_CTL_ADD, fd, &ev));
    transport.running_ = true;
    constexpr uint64_t requestId = 100;
    constexpr uint64_t clientRequestId = 0x1234;
    transport.requestIdToFd_[requestId] = UbseVsockTransport::RequestInfo{fd, clientRequestId};

    uint8_t body[4] = {1, 2, 3, 4};
    UbseResponseMessage resp = {};
    resp.header.statusCode = UBSE_OK;
    resp.header.bodyLen = sizeof(body);
    resp.body = body;
    EXPECT_EQ(UBSE_OK, transport.SendResponse(requestId, resp));

    auto& sessionRef = transport.sessions_[fd];
    ASSERT_EQ(sizeof(UbseResponseHeader) + sizeof(body), sessionRef->writeBuffer.size());
    UbseResponseHeader outHeader = {};
    if (memcpy_s(&outHeader, sizeof(outHeader), sessionRef->writeBuffer.data(), sizeof(outHeader)) != EOK) {
        FAIL() << "copy response header failed";
    }
    EXPECT_EQ(UBSE_OK, outHeader.statusCode);
    EXPECT_EQ(sizeof(body), outHeader.bodyLen);
    EXPECT_EQ(clientRequestId, outHeader.clientRequestId);
    EXPECT_EQ(0, memcmp(sessionRef->writeBuffer.data() + sizeof(outHeader), body, sizeof(body)));
    {
        std::lock_guard<std::mutex> lock(transport.requestMapMutex_);
        EXPECT_TRUE(transport.requestIdToFd_.empty()); // 映射已清理
    }
    transport.running_ = false;
    close(transport.epollFd_);
    transport.epollFd_ = -1;
    transport.sessions_.clear();
    close(fds[0]);
    close(fds[1]);
}

/*
 * 用例描述:
 * requestId 不存在时 SendResponse 返回错误
 * 测试步骤:
 * 1、构造传输层(不 Start),置位运行标记与 epoll fd
 * 2、调用 SendResponse 传入不存在的 requestId
 * 预期结果:
 * 1、返回 UBSE_ERR_DAEMON_UNREACHABLE
 */
TEST_F(TestUbseVsockTransport, SendResponse_UnknownRequestId_ReturnsError)
{
    UbseVsockTransport transport(config_);
    transport.epollFd_ = epoll_create1(0);
    ASSERT_NE(-1, transport.epollFd_);
    transport.running_ = true;
    UbseResponseMessage resp = {};
    resp.header.statusCode = UBSE_OK;
    resp.header.bodyLen = 0;
    EXPECT_EQ(UBSE_ERR_DAEMON_UNREACHABLE, transport.SendResponse(9999, resp));
    transport.running_ = false;
    close(transport.epollFd_);
    transport.epollFd_ = -1;
}

/*
 * 用例描述:
 * 会话已被关闭(fd 可能被新连接复用)时 SendResponse 安全返回错误并清理映射
 * 测试步骤:
 * 1、构造传输层(不 Start),置位运行标记与 epoll fd,登记 requestId 映射但不创建会话
 * 2、调用 SendResponse
 * 预期结果:
 * 1、返回 UBSE_ERR_DAEMON_UNREACHABLE
 * 2、requestId 映射被清理,无崩溃
 */
TEST_F(TestUbseVsockTransport, SendResponse_SessionGone_ReturnsErrorAndCleansMap)
{
    UbseVsockTransport transport(config_);
    int fds[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    transport.epollFd_ = epoll_create1(0);
    ASSERT_NE(-1, transport.epollFd_);
    transport.running_ = true;
    constexpr uint64_t requestId = 200;
    transport.requestIdToFd_[requestId] = UbseVsockTransport::RequestInfo{fds[0], 0x5678};

    UbseResponseMessage resp = {};
    resp.header.statusCode = UBSE_OK;
    resp.header.bodyLen = 0;
    EXPECT_EQ(UBSE_ERR_DAEMON_UNREACHABLE, transport.SendResponse(requestId, resp));
    {
        std::lock_guard<std::mutex> lock(transport.requestMapMutex_);
        EXPECT_TRUE(transport.requestIdToFd_.empty());
    }
    transport.running_ = false;
    close(transport.epollFd_);
    transport.epollFd_ = -1;
    close(fds[0]);
    close(fds[1]);
}

/*
 * 用例描述:
 * 对端已关闭时 HandleWrite 检测到致命写错误并关闭连接、清理映射
 * 测试步骤:
 * 1、构造传输层(不 Start),创建会话并写入待发送数据,登记 requestId 映射
 * 2、关闭对端 fd 触发 EPIPE
 * 3、调用 HandleWrite
 * 预期结果:
 * 1、会话被清理(sessions_ 中不再存在该 fd)
 * 2、该 fd 的 requestId 映射被清理
 */
TEST_F(TestUbseVsockTransport, HandleWrite_PeerClosed_ClosesConnection)
{
    signal(SIGPIPE, SIG_IGN); // UT 进程屏蔽 SIGPIPE,由错误码路径验证关闭行为
    UbseVsockTransport transport(config_);
    int fds[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    auto session = std::make_unique<UbseVsockTransport::Session>();
    session->fd = fds[0];
    const int fd = fds[0];
    session->writeBuffer.assign(sizeof(UbseResponseHeader), 0xAB);
    transport.sessions_[fd] = std::move(session);
    transport.epollFd_ = epoll_create1(0);
    ASSERT_NE(-1, transport.epollFd_);
    struct epoll_event ev = {};
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    ev.data.fd = fd;
    ASSERT_EQ(0, epoll_ctl(transport.epollFd_, EPOLL_CTL_ADD, fd, &ev));
    transport.running_ = true;
    transport.requestIdToFd_[300] = UbseVsockTransport::RequestInfo{fd, 0x9ABC};

    close(fds[1]); // 模拟对端断开,后续 write 返回 EPIPE
    transport.HandleWrite(fd);

    EXPECT_EQ(0U, transport.sessions_.count(fd)); // 会话已清理
    {
        std::lock_guard<std::mutex> lock(transport.requestMapMutex_);
        EXPECT_TRUE(transport.requestIdToFd_.empty()); // 映射已清理
    }
    transport.running_ = false;
    close(transport.epollFd_);
    transport.epollFd_ = -1;
    // fds[0] 已由 CloseConnection 关闭,无需重复 close
}

/*
 * 用例描述:
 * 线程池队列满时请求被拒绝,错误响应写入会话写缓冲以避免客户端挂等超时
 * 测试步骤:
 * 1、构造 1 线程 + 队列容量 1 的线程池,首个 handler 阻塞占用工作线程
 * 2、第二个请求入队,第三个请求触发队列满
 * 3、检查会话写缓冲与 requestId 映射
 * 预期结果:
 * 1、写缓冲出现 UBSE_ERR_INTERNAL 错误响应,bodyLen=0,clientRequestId 回填为被拒请求的 ID
 * 2、被拒请求的映射已清理,已受理的两个请求映射保留
 */
TEST_F(TestUbseVsockTransport, SubmitToThreadPool_QueueFull_WritesErrorResponse)
{
    UbseVsockTransport transport(config_);
    int fds[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    auto session = std::make_unique<UbseVsockTransport::Session>();
    session->fd = fds[0];
    const int fd = fds[0];
    transport.sessions_[fd] = std::move(session);
    transport.epollFd_ = epoll_create1(0);
    ASSERT_NE(-1, transport.epollFd_);
    struct epoll_event ev = {};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fd;
    ASSERT_EQ(0, epoll_ctl(transport.epollFd_, EPOLL_CTL_ADD, fd, &ev));

    transport.threadPool_ = task_executor::UbseTaskExecutor::Create("UtVsockExecutor", 1, 1);
    ASSERT_NE(nullptr, transport.threadPool_.Get());
    ASSERT_TRUE(transport.threadPool_->Start());

    std::atomic<bool> workerStarted{false};
    std::atomic<bool> releaseWorker{false};
    std::atomic<uint32_t> handledCount{0};
    transport.requestHandler_ = [&workerStarted, &releaseWorker, &handledCount](const UbseRequestMessage&,
                                                                                const UbseRequestContext&) {
        workerStarted.store(true);
        while (!releaseWorker.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        handledCount.fetch_add(1);
    };

    // 请求一:占用唯一工作线程
    auto first = MakeRequestBytes(TEST_MODULE_CODE, TEST_OP_CODE, "msg-one", 0x1111);
    transport.sessions_[fd]->readBuffer = first;
    EXPECT_TRUE(transport.TryDecodeMessage(fd));
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(WAIT_TIMEOUT_MS);
    while (!workerStarted.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_TRUE(workerStarted.load()); // 工作线程已取走请求一,队列为空

    // 请求二:占满队列(容量 1)
    auto second = MakeRequestBytes(TEST_MODULE_CODE, TEST_OP_CODE, "msg-two", 0x2222);
    transport.sessions_[fd]->readBuffer = second;
    EXPECT_TRUE(transport.TryDecodeMessage(fd));

    // 请求三:队列满,被拒绝并回写错误响应
    constexpr uint64_t rejectedClientRequestId = 0x3333;
    auto third = MakeRequestBytes(TEST_MODULE_CODE, TEST_OP_CODE, "msg-three", rejectedClientRequestId);
    transport.sessions_[fd]->readBuffer = third;
    EXPECT_TRUE(transport.TryDecodeMessage(fd));

    auto& writeBuffer = transport.sessions_[fd]->writeBuffer;
    ASSERT_EQ(sizeof(UbseResponseHeader), writeBuffer.size());
    UbseResponseHeader outHeader = {};
    if (memcpy_s(&outHeader, sizeof(outHeader), writeBuffer.data(), sizeof(outHeader)) != EOK) {
        FAIL() << "copy response header failed";
    }
    EXPECT_EQ(UBSE_ERR_INTERNAL, outHeader.statusCode);
    EXPECT_EQ(0U, outHeader.bodyLen);
    EXPECT_EQ(rejectedClientRequestId, outHeader.clientRequestId);
    {
        std::lock_guard<std::mutex> lock(transport.requestMapMutex_);
        // 已受理的请求一/二映射保留,被拒的请求三映射已清理
        EXPECT_EQ(2U, transport.requestIdToFd_.size());
    }

    releaseWorker.store(true);
    // 等待已受理的两个请求处理完再 Stop,避免 Stop 任务入队时队列满导致固定 2s 等待
    deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(WAIT_TIMEOUT_MS);
    while (handledCount.load() < 2 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_EQ(2U, handledCount.load());
    transport.threadPool_->Stop();
    transport.threadPool_ = nullptr;
    transport.sessions_.clear();
    close(transport.epollFd_);
    transport.epollFd_ = -1;
    close(fds[0]);
    close(fds[1]);
}

/*
 * 用例描述:
 * 读事件处理完成后重试待写数据(SSL_write 因 WANT_READ 阻塞时,EPOLLOUT 边沿永不触发的修复)
 * 测试步骤:
 * 1、构造传输层(不 Start),创建会话注册到 epoll,writeBuffer 预置待发送响应头
 * 2、调用 HandleRead(readBuffer 为空,读循环立即 EAGAIN)
 * 预期结果:
 * 1、待写数据被重试写出:对端 socket 收到响应头字节
 * 2、写完成后 writeBuffer 清空
 */
TEST_F(TestUbseVsockTransport, HandleRead_PendingWrite_RetriedAfterRead)
{
    UbseVsockTransport transport(config_);
    int fds[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    // 模拟 HandleAccept 行为:会话 fd 置为非阻塞,配合读循环 EAGAIN 语义
    int flags = fcntl(fds[0], F_GETFL, 0);
    ASSERT_NE(-1, flags);
    ASSERT_EQ(0, fcntl(fds[0], F_SETFL, flags | O_NONBLOCK));
    auto session = std::make_unique<UbseVsockTransport::Session>();
    session->fd = fds[0];
    const int fd = fds[0];
    transport.sessions_[fd] = std::move(session);
    transport.epollFd_ = epoll_create1(0);
    ASSERT_NE(-1, transport.epollFd_);
    struct epoll_event ev = {};
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    ev.data.fd = fd;
    ASSERT_EQ(0, epoll_ctl(transport.epollFd_, EPOLL_CTL_ADD, fd, &ev));
    transport.running_ = true;

    // 预置待写响应(模拟 SendResponse 已追加数据但尚未写出)
    constexpr uint64_t clientRequestId = 0x7777;
    UbseResponseHeader header = {};
    header.statusCode = UBSE_OK;
    header.bodyLen = 0;
    header.clientRequestId = clientRequestId;
    const auto* headerBytes = reinterpret_cast<const uint8_t*>(&header); // NOLINT
    transport.sessions_[fd]->writeBuffer.assign(headerBytes, headerBytes + sizeof(header));

    transport.HandleRead(fd); // 读循环 EAGAIN 后重试待写数据

    uint8_t recvBuf[sizeof(UbseResponseHeader)] = {0};
    ASSERT_EQ(static_cast<ssize_t>(sizeof(UbseResponseHeader)), recv(fds[1], recvBuf, sizeof(recvBuf), MSG_DONTWAIT));
    UbseResponseHeader outHeader = {};
    if (memcpy_s(&outHeader, sizeof(outHeader), recvBuf, sizeof(outHeader)) != EOK) {
        FAIL() << "copy response header failed";
    }
    EXPECT_EQ(UBSE_OK, outHeader.statusCode);
    EXPECT_EQ(clientRequestId, outHeader.clientRequestId);
    EXPECT_TRUE(transport.sessions_[fd]->writeBuffer.empty()); // 写完成,缓冲已清空

    transport.running_ = false;
    close(transport.epollFd_);
    transport.epollFd_ = -1;
    transport.sessions_.clear();
    close(fds[0]);
    close(fds[1]);
}

/*
 * 用例描述:
 * epoll_wait 致命错误时事件循环自行停止并标记断连,Stop() 仍能完成清理(静默死亡修复)
 * 测试步骤:
 * 1、构造传输层(不 Start),置位运行标记、无效 epoll fd,启动事件循环线程并创建会话
 * 2、等待事件循环因 EBADF 自行退出
 * 3、调用 Stop()
 * 预期结果:
 * 1、running_ 变为 false,连接状态迁移为 DISCONNECTED(不再表现为"仍在运行")
 * 2、Stop 不因 running_ 已为 false 而跳过:事件循环线程被 join,会话被清理
 */
TEST_F(TestUbseVsockTransport, EventLoop_FatalError_SelfStopsAndStopCleansUp)
{
    UbseVsockTransport transport(config_);
    int fds[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    auto session = std::make_unique<UbseVsockTransport::Session>();
    session->fd = fds[0];
    transport.sessions_[fds[0]] = std::move(session);
    transport.running_ = true;
    transport.epollFd_ = -1; // epoll_wait 返回 EBADF,触发致命错误路径
    transport.eventLoopThread_ = std::thread(&UbseVsockTransport::EventLoop, &transport);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(WAIT_TIMEOUT_MS);
    while (transport.running_.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_FALSE(transport.running_.load());                                          // 事件循环已自行停止
    EXPECT_EQ(UbseVsockConnState::DISCONNECTED, transport.GetConnectionStatus());    // 状态可见异常

    transport.Stop(); // 不被跳过:事件循环线程 joinable 识别自退出场景
    EXPECT_FALSE(transport.eventLoopThread_.joinable());                              // 线程已被 join 清理
    EXPECT_TRUE(transport.sessions_.empty());                                         // 会话已被清理
    EXPECT_EQ(UbseVsockConnState::WAITING_CONNECT, transport.GetConnectionStatus()); // Stop 复位状态
    close(fds[1]); // fds[0] 已由 Stop 关闭
}
/*
 * 用例描述:
 * 最后一个连接断开后状态迁移:CONNECTED → DISCONNECTED → WAITING_CONNECT
 * 测试步骤:
 * 1、构造传输层(不 Start),注入单个会话,置连接状态为 CONNECTED
 * 2、调用 CloseConnection
 * 预期结果:
 * 1、会话被清理,连接状态最终回到 WAITING_CONNECT(等待 ub-device-manager 重连)
 */
TEST_F(TestUbseVsockTransport, CloseConnection_LastSession_TransitionsToWaitingConnect)
{
    UbseVsockTransport transport(config_);
    int fds[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    auto session = std::make_unique<UbseVsockTransport::Session>();
    session->fd = fds[0];
    const int fd = fds[0];
    transport.sessions_[fd] = std::move(session);
    transport.connState_ = UbseVsockConnState::CONNECTED; // 模拟 HandleAccept 已受理连接

    transport.CloseConnection(fd);

    EXPECT_EQ(0U, transport.sessions_.count(fd)); // 会话已清理
    EXPECT_EQ(UbseVsockConnState::WAITING_CONNECT, transport.GetConnectionStatus());
    close(fds[1]); // fds[0] 已由 CloseConnection 关闭
}

/*
 * 用例描述:
 * 多连接场景仅断开其中一个连接时保持 CONNECTED(仅最后一个连接断开才迁移状态)
 * 测试步骤:
 * 1、注入两个会话,置连接状态为 CONNECTED
 * 2、CloseConnection 关闭第一个会话
 * 预期结果:
 * 1、状态保持 CONNECTED
 * 2、关闭第二个会话后状态才迁移为 WAITING_CONNECT
 */
TEST_F(TestUbseVsockTransport, CloseConnection_NotLastSession_KeepsConnected)
{
    UbseVsockTransport transport(config_);
    int fds1[2] = {-1, -1};
    int fds2[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds1));
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds2));
    for (int fd : {fds1[0], fds2[0]}) {
        auto session = std::make_unique<UbseVsockTransport::Session>();
        session->fd = fd;
        transport.sessions_[fd] = std::move(session);
    }
    transport.connState_ = UbseVsockConnState::CONNECTED;

    transport.CloseConnection(fds1[0]);
    EXPECT_EQ(UbseVsockConnState::CONNECTED, transport.GetConnectionStatus()); // 仍有存活连接

    transport.CloseConnection(fds2[0]);
    EXPECT_EQ(UbseVsockConnState::WAITING_CONNECT, transport.GetConnectionStatus());
    close(fds1[1]);
    close(fds2[1]);
}

/*
 * 用例描述:
 * 对端发送完整请求后立即关闭连接,该请求仍被提交处理(不随连接关闭被丢弃)
 * 测试步骤:
 * 1、构造传输层(不 Start),创建非阻塞会话,注入线程池与请求回调
 * 2、对端发送一条完整请求后 shutdown(SHUT_WR)
 * 3、调用 HandleRead(读到数据后 read 返回 0,needClose=true)
 * 预期结果:
 * 1、回调收到该请求(needClose 时也先 TryDecodeMessage 提交完整消息)
 * 2、连接被关闭,状态回到 WAITING_CONNECT
 */
TEST_F(TestUbseVsockTransport, HandleRead_PeerCloseAfterFullMessage_MessageSubmitted)
{
    UbseVsockTransport transport(config_);
    int fds[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    int flags = fcntl(fds[0], F_GETFL, 0);
    ASSERT_NE(-1, flags);
    ASSERT_EQ(0, fcntl(fds[0], F_SETFL, flags | O_NONBLOCK));
    auto session = std::make_unique<UbseVsockTransport::Session>();
    session->fd = fds[0];
    const int fd = fds[0];
    transport.sessions_[fd] = std::move(session);
    transport.threadPool_ = task_executor::UbseTaskExecutor::Create("UtVsockExecutor", 2, 16);
    ASSERT_NE(nullptr, transport.threadPool_.Get());
    ASSERT_TRUE(transport.threadPool_->Start());

    std::mutex captureMutex;
    std::vector<CapturedRequest> captured;
    transport.requestHandler_ = [&captured, &captureMutex](const UbseRequestMessage& req,
                                                           const UbseRequestContext&) {
        std::lock_guard<std::mutex> lock(captureMutex);
        CapturedRequest item;
        item.moduleCode = req.header.moduleCode;
        item.clientRequestId = req.header.clientRequestId;
        item.body.assign(reinterpret_cast<const char*>(req.body), req.header.bodyLen);
        captured.push_back(item);
    };

    // 对端发送完整请求后立即关闭写端
    const std::string body = "last-message-before-close";
    auto bytes = MakeRequestBytes(TEST_MODULE_CODE, TEST_OP_CODE, body, 0xCAFEBABE);
    ASSERT_EQ(static_cast<ssize_t>(bytes.size()), write(fds[1], bytes.data(), bytes.size()));
    ASSERT_EQ(0, shutdown(fds[1], SHUT_WR));

    transport.HandleRead(fd);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(WAIT_TIMEOUT_MS);
    while (captured.empty() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_EQ(1U, captured.size());
    EXPECT_EQ(TEST_MODULE_CODE, captured[0].moduleCode);
    EXPECT_EQ(0xCAFEBABEU, captured[0].clientRequestId);
    EXPECT_EQ(body, captured[0].body);
    EXPECT_EQ(0U, transport.sessions_.count(fd)); // 连接已关闭
    EXPECT_EQ(UbseVsockConnState::WAITING_CONNECT, transport.GetConnectionStatus());

    transport.threadPool_->Stop();
    transport.threadPool_ = nullptr;
    transport.sessions_.clear();
    close(fds[1]); // fds[0] 已由 CloseConnection 关闭
}
} // namespace ubse::vsock::ut
