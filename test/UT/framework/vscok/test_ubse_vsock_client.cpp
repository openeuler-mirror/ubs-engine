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
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <securec.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <mockcpp/mockcpp.hpp>

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "src/framework/vscok/ubse_vsock_client.h"

namespace ubse::vsock::ut {
using namespace ubse::config;
using namespace ubse::context;

namespace {
// 与 ubse_vsock_client.cpp 内部的 MAX_RSP_LEN 对齐, 用于覆盖长度校验边界
constexpr uint32_t MAX_RSP_LEN = 1U << 20;
// 假 fd: Disconnect 中的 close 已打桩, 不会误关进程内真实句柄
constexpr int FAKE_SOCK_FD = 1024;

// 模拟对端 TLS 流: 可限制单次返回字节数(复现 TLS 记录分段短读), 也可注入错误码
class FakeSslStream {
public:
    void Reset()
    {
        wire_.clear();
        offset_ = 0;
        limits_.clear();
        readCalls_ = 0;
        failCall_ = 0;
        failSslErr_ = 0;
        failRet_ = -1;
        failErrNo_ = 0;
        pendingErr_ = false;
    }

    void Write(const std::string& bytes)
    {
        wire_ += bytes;
    }

    // 第 i 次 SSL_read 最多返回 limits[i] 字节; 未指定则仅受本次请求长度约束
    void SetChunkLimits(const std::vector<size_t>& limits)
    {
        limits_ = limits;
    }

    // 第 call 次 SSL_read 不返回数据, 直接给出错误码(用于 WANT_READ/EINTR/致命错误)
    void FailOnCall(size_t call, int sslErr, int ret, int errNo)
    {
        failCall_ = call;
        failSslErr_ = sslErr;
        failRet_ = ret;
        failErrNo_ = errNo;
    }

    int Read(void* buf, int num)
    {
        ++readCalls_;
        if (readCalls_ == failCall_) {
            pendingErr_ = true;
            errno = failErrNo_; // errno 由失败的底层调用设置, 此处如实模拟
            return failRet_;
        }
        size_t remain = wire_.size() - offset_;
        if (remain == 0) { // 对端提前关闭: 与 SSL_read 返回 0 且 errno 为 0 语义一致
            pendingErr_ = true;
            failSslErr_ = SSL_ERROR_SYSCALL;
            failErrNo_ = 0;
            errno = 0;
            return 0;
        }
        size_t limit = (readCalls_ <= limits_.size()) ? limits_[readCalls_ - 1] : remain;
        size_t len = std::min({remain, limit, static_cast<size_t>(num)});
        std::copy_n(wire_.data() + offset_, len, static_cast<char*>(buf));
        offset_ += len;
        return static_cast<int>(len);
    }

    // SSL_get_error 不承诺保留 errno(内部要查错误队列): 这里模拟其把 errno 污染成非 EINTR 值,
    // 只有"SSL_read 失败后立即保存 errno"的实现才能通过 EINTR 重试用例
    int GetError()
    {
        if (pendingErr_) {
            pendingErr_ = false;
            errno = ENOENT;
            return failSslErr_;
        }
        return SSL_ERROR_NONE;
    }

    size_t ReadCalls() const
    {
        return readCalls_;
    }

private:
    std::string wire_;
    size_t offset_{0};
    std::vector<size_t> limits_;
    size_t readCalls_{0};
    size_t failCall_{0};
    int failSslErr_{0};
    int failRet_{-1};
    int failErrNo_{0};
    bool pendingErr_{false};
};

FakeSslStream g_stream;

int MockSslRead(SSL*, void* buf, int num)
{
    return g_stream.Read(buf, num);
}

int MockSslGetError(const SSL*, int)
{
    return g_stream.GetError();
}

std::string g_written;
int g_connectCalls = 0;

// SSL_write 返回入参 num 表示整包写出, 同时记录报文用于断言线格式
int MockSslWrite(SSL*, const void* buf, int num)
{
    g_written.append(static_cast<const char*>(buf), static_cast<size_t>(num));
    return num;
}

// 打桩 Connect 作探针: 被测代码若自行重连则计数非零, 用例据此断言连接生命周期归调用方
bool MockConnectOk(UbseVsockClient* self)
{
    ++g_connectCalls;
    self->sockFd_ = FAKE_SOCK_FD;
    return true;
}

// 按 MsgHeader 线格式构造响应头, 便于伪造任意 len 的对端帧
std::string MakeHeader(uint32_t len)
{
    MsgHeader hdr{};
    hdr.id = 1;
    hdr.version = DEFAULT_VERSION;
    hdr.type = 1;
    hdr.len = len;
    std::string out(sizeof(hdr), '\0');
    errno_t ret = memcpy_s(out.data(), out.size(), &hdr, sizeof(hdr));
    if (ret != EOK) {
        ADD_FAILURE() << "build message header failed, ret=" << ret;
        return {};
    }
    return out;
}

std::string MakeResponse(const std::string& payload)
{
    return MakeHeader(static_cast<uint32_t>(payload.size())) + payload;
}
} // namespace

class UbseVsockClientTest : public testing::Test {
public:
    void SetUp() override
    {
        GlobalMockObject::reset();
        g_stream.Reset();
        g_written.clear();
        g_connectCalls = 0;
        MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(std::make_shared<UbseConfModule>()));
        MOCKER(SSL_read).stubs().will(invoke(MockSslRead));
        MOCKER(SSL_get_error).stubs().will(invoke(MockSslGetError));
        MOCKER(close).stubs().will(returnValue(0));

        client_ = std::make_unique<UbseVsockClient>();
        client_->sockFd_ = FAKE_SOCK_FD;
        AttachSsl();
    }

    void TearDown() override
    {
        // 用例可能把连接置为断开(sockFd_ = -1), 此时析构不会回收 SSL; 统一改回已连接交由析构释放
        if (client_->ssl_ != nullptr) {
            client_->sockFd_ = FAKE_SOCK_FD;
        }
        client_.reset();
        GlobalMockObject::verify();
        GlobalMockObject::reset();
    }

protected:
    // ssl_ 用真实 SSL 对象承载(仅挂内存 BIO), 只把读路径 SSL_read/SSL_get_error 打桩,
    // 这样 Disconnect 走的是真实的 SSL_shutdown/SSL_free, 且无需伪造指针
    void AttachSsl()
    {
        SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
        ASSERT_NE(ctx, nullptr);
        SSL* ssl = SSL_new(ctx);
        SSL_CTX_free(ctx);
        ASSERT_NE(ssl, nullptr);
        BIO* rbio = BIO_new(BIO_s_mem());
        BIO* wbio = BIO_new(BIO_s_mem());
        ASSERT_NE(rbio, nullptr);
        ASSERT_NE(wbio, nullptr);
        SSL_set_bio(ssl, rbio, wbio);
        client_->ssl_ = ssl;
    }

    std::unique_ptr<UbseVsockClient> client_;
};

TEST_F(UbseVsockClientTest, RecvMessage_HappyPath)
{
    const std::string payload = "signed-data";
    g_stream.Write(MakeResponse(payload));

    UbseSignRsp rsp;
    ASSERT_TRUE(client_->RecvMessage(rsp));
    EXPECT_EQ(rsp.signedData, payload);
    EXPECT_EQ(g_stream.ReadCalls(), 2U);
    EXPECT_EQ(client_->sockFd_, FAKE_SOCK_FD);
}

// 正常路径不变式: 单个合法帧的载荷(含 0 字节与形似帧头的字节)必须逐字节原样返回
TEST_F(UbseVsockClientTest, RecvMessage_PayloadReturnedVerbatim)
{
    std::string payload;
    payload.push_back('\0');
    payload += MakeHeader(999); // 载荷内嵌形似帧头的字节, 不得被当作下一帧剥掉
    payload += "tail";
    g_stream.Write(MakeResponse(payload));

    UbseSignRsp rsp;
    ASSERT_TRUE(client_->RecvMessage(rsp));
    EXPECT_EQ(rsp.signedData.size(), payload.size());
    EXPECT_EQ(rsp.signedData, payload);
}

// TLS 记录分段到达时单次 SSL_read 短读, 头与 payload 都必须按长度收全
TEST_F(UbseVsockClientTest, RecvMessage_ShortReadsAreReassembled)
{
    const std::string payload = "0123456789";
    g_stream.Write(MakeResponse(payload));
    g_stream.SetChunkLimits({7, 9, 3}); // 头 7+9, payload 3+7

    UbseSignRsp rsp;
    ASSERT_TRUE(client_->RecvMessage(rsp));
    EXPECT_EQ(rsp.signedData, payload);
    EXPECT_EQ(g_stream.ReadCalls(), 4U);
}

// payload 只取 header.len 字节: 同一 TLS 记录里紧邻的下一帧必须留给下一次收包, 不得错位
TEST_F(UbseVsockClientTest, RecvMessage_ConsecutiveFramesAreAligned)
{
    g_stream.Write(MakeResponse("hello") + MakeResponse("world"));

    UbseSignRsp rsp;
    ASSERT_TRUE(client_->RecvMessage(rsp));
    EXPECT_EQ(rsp.signedData, "hello");
    ASSERT_TRUE(client_->RecvMessage(rsp));
    EXPECT_EQ(rsp.signedData, "world");
    EXPECT_EQ(g_stream.ReadCalls(), 4U);
}

TEST_F(UbseVsockClientTest, RecvMessage_ZeroLenRejected)
{
    g_stream.Write(MakeHeader(0) + "x");

    UbseSignRsp rsp;
    EXPECT_FALSE(client_->RecvMessage(rsp));
    EXPECT_EQ(g_stream.ReadCalls(), 1U); // 长度非法, 不再尝试读 payload
    EXPECT_TRUE(rsp.signedData.empty());
    EXPECT_EQ(client_->sockFd_, FAKE_SOCK_FD); // 连接不复位, 交由调用方重试
}

// 伪造超长 len 不得放大读取/分配: 头解析后立即拒绝
TEST_F(UbseVsockClientTest, RecvMessage_OversizeLenRejected)
{
    g_stream.Write(MakeHeader(MAX_RSP_LEN + 1));

    UbseSignRsp rsp;
    EXPECT_FALSE(client_->RecvMessage(rsp));
    EXPECT_EQ(g_stream.ReadCalls(), 1U);
    EXPECT_TRUE(rsp.signedData.empty());
    EXPECT_EQ(client_->sockFd_, FAKE_SOCK_FD); // 连接不复位, 交由调用方重试
}

TEST_F(UbseVsockClientTest, RecvMessage_MaxLenAccepted)
{
    const std::string payload(MAX_RSP_LEN, 'a');
    g_stream.Write(MakeResponse(payload));

    UbseSignRsp rsp;
    ASSERT_TRUE(client_->RecvMessage(rsp));
    EXPECT_EQ(rsp.signedData.size(), payload.size());
    EXPECT_EQ(rsp.signedData, payload);
}

TEST_F(UbseVsockClientTest, RecvMessage_RetryOnWantRead)
{
    const std::string payload = "abc";
    g_stream.Write(MakeResponse(payload));
    g_stream.FailOnCall(1, SSL_ERROR_WANT_READ, -1, 0); // 首次读返回 WANT_READ, 应续读而非整帧失败

    UbseSignRsp rsp;
    ASSERT_TRUE(client_->RecvMessage(rsp));
    EXPECT_EQ(rsp.signedData, payload);
    EXPECT_EQ(client_->sockFd_, FAKE_SOCK_FD);
}

TEST_F(UbseVsockClientTest, RecvMessage_RetryOnEintr)
{
    const std::string payload = "abcd";
    g_stream.Write(MakeResponse(payload));
    g_stream.FailOnCall(2, SSL_ERROR_SYSCALL, -1, EINTR); // payload 首段读被信号打断

    UbseSignRsp rsp;
    ASSERT_TRUE(client_->RecvMessage(rsp));
    EXPECT_EQ(rsp.signedData, payload);
}

TEST_F(UbseVsockClientTest, RecvMessage_FatalReadErrorReturnsFalse)
{
    g_stream.Write(MakeResponse("abcd"));
    g_stream.FailOnCall(1, SSL_ERROR_SSL, -1, 0);

    UbseSignRsp rsp;
    EXPECT_FALSE(client_->RecvMessage(rsp));
    EXPECT_TRUE(rsp.signedData.empty());
    EXPECT_EQ(client_->sockFd_, FAKE_SOCK_FD); // 连接不复位, 交由调用方重试
}

// 旧实现按单次 SSL_read 的实际字节数切 payload, 对端中途断开会静默返回被截断的签名
TEST_F(UbseVsockClientTest, RecvMessage_TruncatedPayloadReturnsFalse)
{
    const std::string payload = "abcdef";
    g_stream.Write(MakeHeader(payload.size()) + payload.substr(0, 2));

    UbseSignRsp rsp;
    EXPECT_FALSE(client_->RecvMessage(rsp));
    EXPECT_EQ(g_stream.ReadCalls(), 3U); // 头 1 次 + payload 2 次(第 2 次读到 EOF)
    EXPECT_TRUE(rsp.signedData.empty());
    EXPECT_EQ(client_->sockFd_, FAKE_SOCK_FD); // 连接不复位, 交由调用方重试
}

// 连接已复位时直接失败, 不得触碰 SSL 对象
TEST_F(UbseVsockClientTest, RecvMessage_NotConnectedReturnsFalse)
{
    g_stream.Write(MakeResponse("payload"));
    client_->sockFd_ = -1;

    UbseSignRsp rsp;
    EXPECT_FALSE(client_->RecvMessage(rsp));
    EXPECT_EQ(g_stream.ReadCalls(), 0U);
    EXPECT_TRUE(rsp.signedData.empty());
}

// 半包写视为发送失败, 连接不复位, 交由调用方重试
TEST_F(UbseVsockClientTest, SendMessage_ShortWriteReturnsFalse)
{
    const std::string payload = "abcdef";
    MOCKER(SSL_write).stubs().will(returnValue(static_cast<int>(payload.size()) - 1));

    EXPECT_FALSE(client_->SendMessage(1, 2, payload.data(), static_cast<uint32_t>(payload.size())));
    EXPECT_EQ(client_->sockFd_, FAKE_SOCK_FD);
}

// 正常路径不变式: 请求线格式(头字段 + 原始载荷)必须逐字节一致, 对端看到的报文不变
TEST_F(UbseVsockClientTest, SendMessage_WireFormatUnchanged)
{
    MOCKER(SSL_write).stubs().will(invoke(MockSslWrite));
    const std::string payload = "abcdef";

    ASSERT_TRUE(client_->SendMessage(7, 3, payload.data(), static_cast<uint32_t>(payload.size())));
    ASSERT_EQ(g_written.size(), sizeof(MsgHeader) + payload.size());
    MsgHeader hdr{};
    ASSERT_EQ(memcpy_s(&hdr, sizeof(hdr), g_written.data(), sizeof(hdr)), EOK);
    EXPECT_EQ(hdr.id, 7U);
    EXPECT_EQ(hdr.version, DEFAULT_VERSION);
    EXPECT_EQ(hdr.type, 3U);
    EXPECT_EQ(hdr.len, payload.size());
    EXPECT_EQ(g_written.substr(sizeof(MsgHeader)), payload);
}

// 连接可用时发包收包一次完成, 且不得自行重建连接
TEST_F(UbseVsockClientTest, UbseVsockSend_HappyPath)
{
    MOCKER(SSL_write).stubs().will(invoke(MockSslWrite));
    g_stream.Write(MakeResponse("sig"));

    UbseSignReq req{1, 2, "abcdef"};
    UbseSignRsp rsp;
    EXPECT_EQ(client_->UbseVsockSend(req, rsp), UBSE_OK);
    EXPECT_EQ(rsp.signedData, "sig");
    EXPECT_EQ(client_->sockFd_, FAKE_SOCK_FD);
    EXPECT_EQ(g_written.size(), sizeof(MsgHeader) + req.payload.size());
}

// 连接生命周期归调用方(一次 Connect + 循环重试发送): 连接已复位时不自行重连, 直接失败
TEST_F(UbseVsockClientTest, UbseVsockSend_NotConnectedFailsWithoutReconnect)
{
    client_->sockFd_ = -1; // 上一次收包失败后的复位状态
    MOCKER(&UbseVsockClient::Connect).stubs().will(invoke(MockConnectOk));

    UbseSignReq req{1, 2, "abcdef"};
    UbseSignRsp rsp;
    EXPECT_EQ(client_->UbseVsockSend(req, rsp), UBSE_ERROR);
    EXPECT_EQ(g_connectCalls, 0);
    EXPECT_TRUE(g_written.empty());
}
} // namespace ubse::vsock::ut
