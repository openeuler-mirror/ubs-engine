/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/*
 * DT Fuzz harness: SDK -> 守护进程 raw IPC 全链路攻击面（恶意服务端）
 *
 * 攻击面说明：
 *   ubse_invoke_call() 是所有 SDK（含 ubs_virt_agent_sdk）访问守护进程的唯一通道：
 *   UDS connect -> SerializeRequestMessage -> send -> recv(响应头+body) -> CopyResponseBody。
 *   若守护进程被攻破或响应被篡改（伪造 bodyLen、截断 body、直接断连），
 *   客户端的响应解析/内存分配路径必须安全。本 harness 在进程内拉起一个
 *   "恶意 daemon"线程，按 fuzz 输入构造畸形响应，对客户端做全链路攻击测试。
 *
 * 输入布局：
 *   byte[0..1]  module_code (uint16 LE)
 *   byte[2..3]  op_code (uint16 LE)
 *   byte[4]     攻击模式（见 kAttackMode 说明）
 *   byte[5..8]  statusCode (uint32 LE)
 *   byte[9..]   请求体 / 响应体（同一 payload 复用）
 *
 * 攻击模式：
 *   0 = 正常响应（body 如实返回，基线）
 *   1 = bodyLen 谎报 0xFFFFFFFF，不发 body 直接断连（长度校验）
 *   2 = bodyLen 恰好 10MB 边界，不发 body 直接断连（边界分配）
 *   3 = bodyLen 谎报多 8 字节，只发一半 body（截断攻击）
 *   4 = 不回任何字节直接断连（连接重置）
 */

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ubse_ipc_client.h"
#include "ubse_ipc_log.h"
#include "ubse_ipc_message.h"
#include "ubse_logger_manager.h"

namespace {

constexpr const char* SOCK_PATH = "/tmp/ft_ubse_ipc_fuzz.sock";
constexpr uint32_t MAX_MSG_SIZE = 10 * 1024 * 1024; // 与 UBSE_MESSAGE_SIZE 一致

volatile uint64_t g_sink = 0;

// fuzz 线程写、daemon 线程读的响应参数
struct FuzzRespSpec {
    uint32_t statusCode = 0;
    uint8_t attackMode = 0;
    std::vector<uint8_t> body;
};
FuzzRespSpec g_respSpec;
std::mutex g_respMtx;

int g_listenFd = -1;

// 阻塞读满 n 字节；对端关闭/出错返回 false
bool ReadFull(int fd, void* buf, size_t n)
{
    auto* p = static_cast<uint8_t*>(buf);
    size_t got = 0;
    while (got < n) {
        const ssize_t r = recv(fd, p + got, n - got, 0);
        if (r <= 0) {
            return false;
        }
        got += static_cast<size_t>(r);
    }
    return true;
}

void WriteAll(int fd, const void* buf, size_t n)
{
    const auto* p = static_cast<const uint8_t*>(buf);
    size_t sent = 0;
    while (sent < n) {
        const ssize_t r = send(fd, p + sent, n - sent, MSG_NOSIGNAL);
        if (r <= 0) {
            return;
        }
        sent += static_cast<size_t>(r);
    }
}

// 恶意 daemon：accept 循环，读取请求后按 g_respSpec 构造响应
void DaemonAcceptLoop()
{
    while (true) {
        const int connFd = accept(g_listenFd, nullptr, nullptr);
        if (connFd < 0) {
            if (errno == EINTR) {
                continue;
            }
            return; // listen socket 异常，退出线程
        }

        // 读取请求：bool + UbseRequestHeader(packed 16B) + body
        uint8_t flag = 0;
        UbseRequestHeader reqHeader{};
        if (ReadFull(connFd, &flag, sizeof(flag)) && ReadFull(connFd, &reqHeader, sizeof(reqHeader)) &&
            reqHeader.bodyLen > 0 && reqHeader.bodyLen <= MAX_MSG_SIZE) {
            std::vector<uint8_t> reqBody(reqHeader.bodyLen);
            (void)ReadFull(connFd, reqBody.data(), reqBody.size()); // 尽力消费请求体
        }

        // 快照本次响应参数
        uint32_t statusCode = 0;
        uint8_t attackMode = 0;
        std::vector<uint8_t> body;
        {
            std::lock_guard<std::mutex> lock(g_respMtx);
            statusCode = g_respSpec.statusCode;
            attackMode = g_respSpec.attackMode;
            body = g_respSpec.body;
        }

        // 构造响应：bool(true) + UbseResponseHeader(packed 16B) + body
        const bool isResp = true;
        switch (attackMode) {
            case 1: {
                // 谎报超大 bodyLen，不发 body 直接断连
                UbseResponseHeader h{statusCode, 0xFFFFFFFF, reqHeader.clientRequestId};
                WriteAll(connFd, &isResp, sizeof(isResp));
                WriteAll(connFd, &h, sizeof(h));
                break;
            }
            case 2: {
                // bodyLen 恰好为 10MB 上限，不发 body 直接断连
                UbseResponseHeader h{statusCode, MAX_MSG_SIZE, reqHeader.clientRequestId};
                WriteAll(connFd, &isResp, sizeof(isResp));
                WriteAll(connFd, &h, sizeof(h));
                break;
            }
            case 3: {
                // bodyLen 谎报多 8 字节，只发一半 body（截断攻击）
                UbseResponseHeader h{statusCode, static_cast<uint32_t>(body.size()) + 8, reqHeader.clientRequestId};
                WriteAll(connFd, &isResp, sizeof(isResp));
                WriteAll(connFd, &h, sizeof(h));
                WriteAll(connFd, body.data(), body.size() / 2);
                break;
            }
            case 4:
                // 不回任何字节直接断连
                break;
            default: {
                // 正常响应（基线）
                UbseResponseHeader h{statusCode, static_cast<uint32_t>(body.size()), reqHeader.clientRequestId};
                WriteAll(connFd, &isResp, sizeof(isResp));
                WriteAll(connFd, &h, sizeof(h));
                if (!body.empty()) {
                    WriteAll(connFd, body.data(), body.size());
                }
                break;
            }
        }
        close(connFd);
    }
}

// 同步完成 socket/bind/listen，再拉起 daemon 线程，避免首次调用竞态
void StartDaemonOnce()
{
    // 静音 IPC 层日志：IPC_LOG_* 未注册 sink 时无条件直写 stdout（每次调用数条，
    // 30M 次会拖慢 20 倍并产生 ~20GB 日志）。日志不属于被测攻击面，注册空 sink 丢弃。
    ubse::ipc::UbseIpcLog::SetLogFunc([](uint32_t, const char*) {});

    unlink(SOCK_PATH);
    g_listenFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_listenFd < 0) {
        return;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);
    if (bind(g_listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 || listen(g_listenFd, 16) < 0) {
        close(g_listenFd);
        g_listenFd = -1;
        return;
    }
    std::thread(DaemonAcceptLoop).detach();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 10) {
        return 0;
    }
    // 静音框架日志：fuzz 高频调用会打海量 INFO/ERROR 日志（拖慢 20 倍、30M 次可产生 ~20GB 日志）。
    // 日志不属于被测攻击面，仅保留 CRIT。每次调用都设置，防止客户端首次调用时 Init 重置级别。
    ubse::log::UbseLoggerManager::Instance()->SetLogLevel(ubse::log::UbseLogLevel::CRIT);
    static std::once_flag daemonOnce;
    std::call_once(daemonOnce, StartDaemonOnce);
    if (g_listenFd < 0) {
        return 0; // daemon 未就绪（socket 创建失败），跳过
    }

    const uint16_t moduleCode = static_cast<uint16_t>(data[0] | (data[1] << 8));
    const uint16_t opCode = static_cast<uint16_t>(data[2] | (data[3] << 8));
    const uint8_t attackMode = static_cast<uint8_t>(data[4] % 5);
    uint32_t statusCode = 0;
    statusCode |= static_cast<uint32_t>(data[5]);
    statusCode |= static_cast<uint32_t>(data[6]) << 8;
    statusCode |= static_cast<uint32_t>(data[7]) << 16;
    statusCode |= static_cast<uint32_t>(data[8]) << 24;
    const uint8_t* payload = data + 9;
    const uint32_t payloadLen = static_cast<uint32_t>(size - 9);

    {
        std::lock_guard<std::mutex> lock(g_respMtx);
        g_respSpec.statusCode = statusCode;
        g_respSpec.attackMode = attackMode;
        g_respSpec.body.assign(payload, payload + payloadLen);
    }

    ubse_socket_path_set(SOCK_PATH);
    ubse_api_buffer_t request{const_cast<uint8_t*>(payload), payloadLen};
    ubse_api_buffer_t response{};
    const uint32_t ret = ubse_invoke_call(moduleCode, opCode, &request, &response);
    g_sink ^= ret;
    if (response.buffer != nullptr) {
        // 消费响应内容，防止优化
        uint64_t acc = 0;
        for (uint32_t i = 0; i < response.length; ++i) {
            acc = acc * 31 + response.buffer[i];
        }
        g_sink ^= acc;
        ubse_api_buffer_free(&response);
    }
    return 0;
}

// 语料目录为空时生成种子：各攻击模式的最小输入
extern "C" int FtGenSeeds(const char* corpusDir)
{
    static const struct {
        const char* name;
        std::vector<uint8_t> bytes;
    } seeds[] = {
        // 布局: [module:2][op:2][mode:1][status:4][body...]
        {"p00_normal", {0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 'a', 'b', 'c'}},
        {"p01_oversize_len", {0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x41}},
        {"p02_boundary_10m", {0x01, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x42}},
        {"p03_truncated_body", {0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x41, 0x42, 0x43, 0x44}},
        {"p04_reset_conn", {0x01, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00}},
        {"p05_empty_body", {0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"p06_status_err", {0x01, 0x00, 0x02, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x31, 0x32}},
    };
    int written = 0;
    for (const auto& s : seeds) {
        const std::string path = std::string(corpusDir) + "/" + s.name;
        FILE* f = fopen(path.c_str(), "wb");
        if (f == nullptr) {
            continue;
        }
        if (fwrite(s.bytes.data(), 1, s.bytes.size(), f) == s.bytes.size()) {
            ++written;
        }
        fclose(f);
    }
    return written;
}
