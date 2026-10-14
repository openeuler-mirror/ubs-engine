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

// vsock 本机仿真层(LD_PRELOAD):
// 将 AF_VSOCK socket 系列调用透明重定向到 TCP loopback(AF_INET 127.0.0.1),
// 使依赖 vsock 回环(vsock_loopback 内核模块)的 IT 用例可在无该模块的环境
// (如 WSL2 内核)中真实执行,不修改任何产品与测试代码。
//
// 拦截范围(仅 AF_VSOCK 域,其余域原样透传):
// - socket(AF_VSOCK, ...) → socket(AF_INET, ...)
// - bind(sockaddr_vm)     → bind(sockaddr_in, INADDR_LOOPBACK)
//     svm_cid = VMADDR_CID_ANY / VMADDR_CID_LOCAL 均映射 127.0.0.1
//     svm_port = VMADDR_PORT_ANY 或 > 65535 映射为 0(内核分配临时端口)
// - connect(sockaddr_vm)  → connect(sockaddr_in, 127.0.0.1)
// 后续 accept4/epoll/read/write/setsockopt 等对已建立的 AF_INET fd 原生生效,无需拦截。
//
// 使用方式:
//   LD_PRELOAD=<libvsock_sim.so路径> <IT二进制>

#define _GNU_SOURCE

#include <dlfcn.h>

#include <arpa/inet.h>
#include <linux/vm_sockets.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cstdint>
#include <cstring>

using SocketFn = int (*)(int, int, int);
using BindFn = int (*)(int, const struct sockaddr *, socklen_t);
using ConnectFn = int (*)(int, const struct sockaddr *, socklen_t);

template <typename Fn>
static Fn ResolveReal(const char *symbol)
{
    // RTLD_NEXT:跳过本预加载库,定位 libc 中的真实实现
    void *fn = dlsym(RTLD_NEXT, symbol);
    return reinterpret_cast<Fn>(fn);
}

// sockaddr_vm → sockaddr_in 转换(vsock CID/端口语义到 TCP loopback 的映射)
static bool ConvertVsockAddr(const struct sockaddr *addr, socklen_t len, struct sockaddr_in &in)
{
    if (addr == nullptr || addr->sa_family != AF_VSOCK || len < sizeof(struct sockaddr_vm)) {
        return false;
    }
    const auto *vm = reinterpret_cast<const struct sockaddr_vm *>(addr); // NOLINT
    std::memset(&in, 0, sizeof(in));
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    // vsock 端口为 32 位,TCP 端口为 16 位:
    // VMADDR_PORT_ANY(0xFFFFFFFF)与超范围端口映射为 0(临时端口),其余直传
    if (vm->svm_port == VMADDR_PORT_ANY || vm->svm_port > UINT16_MAX) {
        in.sin_port = 0;
    } else {
        in.sin_port = htons(static_cast<uint16_t>(vm->svm_port));
    }
    return true;
}

extern "C" {

int socket(int domain, int type, int protocol)
{
    static SocketFn realSocket = ResolveReal<SocketFn>("socket");
    if (domain == AF_VSOCK) {
        domain = AF_INET;
        protocol = 0;
    }
    return realSocket(domain, type, protocol);
}

int bind(int fd, const struct sockaddr *addr, socklen_t len)
{
    static BindFn realBind = ResolveReal<BindFn>("bind");
    struct sockaddr_in in {};
    if (ConvertVsockAddr(addr, len, in)) {
        return realBind(fd, reinterpret_cast<const struct sockaddr *>(&in), sizeof(in)); // NOLINT
    }
    return realBind(fd, addr, len);
}

int connect(int fd, const struct sockaddr *addr, socklen_t len)
{
    static ConnectFn realConnect = ResolveReal<ConnectFn>("connect");
    struct sockaddr_in in {};
    if (ConvertVsockAddr(addr, len, in)) {
        return realConnect(fd, reinterpret_cast<const struct sockaddr *>(&in), sizeof(in)); // NOLINT
    }
    return realConnect(fd, addr, len);
}

} // extern "C"
