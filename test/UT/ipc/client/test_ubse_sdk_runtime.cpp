/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_sdk_runtime.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <thread>

#include <mockcpp/mockcpp.hpp>

#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_message.h"
#include "ubse_ipc_utils.h"
#include "src/framework/ipc/client/ubse_uds_client.h"

// 白盒链接声明: ubse_ipc_client.cpp 内部非static符号(与源文件定义一致)
void HandleRequest(const UbseRequestMessage& request, UbseResponseMessage& resp);
bool UbseSdkRuntimeEnter();
void UbseSdkRuntimeExit();
// 排空超时上限(秒), 可写白盒: UT直接改写以覆盖drain超时分支
extern uint32_t g_sdkDrainTimeoutSec;

namespace ubse::ut::ipc {
using namespace ubse::ipc;

namespace {
constexpr auto WAIT_SLICE = std::chrono::milliseconds(200);

// 测试辅助: 持有一个活跃引用, 作用域结束自动释放(即使断言失败也不泄漏)
struct GateHold {
    bool held = false;
    GateHold()
    {
        held = UbseSdkRuntimeEnter();
    }
    ~GateHold()
    {
        Release();
    }
    void Release()
    {
        if (held) {
            held = false;
            UbseSdkRuntimeExit();
        }
    }
};

uint32_t InvokeOnce()
{
    ubse_api_buffer_t request = {nullptr, 0};
    ubse_api_buffer_t response = {nullptr, 0};
    return ubse_invoke_call(0, 0, &request, &response);
}

// TLS护栏用例信号: C回调无法捕获, 用文件级原子量传递
std::atomic<bool> g_tlsCallbackEntered{false};

// 回调上下文内调用finalize(契约禁止), 应被TLS护栏拒绝
int32_t FaultHandlerCallingFinalize(const char*, uint64_t, ubs_mem_fault_type_t)
{
    g_tlsCallbackEntered = true;
    ubse_sdk_runtime_finalize(); // 被拒: 状态不迁移, runtime保持READY
    return static_cast<int32_t>(UBSE_OK);
}

// 普通故障回调(finalize后被清理的对象)
int32_t NoopFaultHandler(const char*, uint64_t, ubs_mem_fault_type_t)
{
    return static_cast<int32_t>(UBSE_OK);
}
} // namespace

void TestUbseSdkRuntime::SetUp()
{
    Test::SetUp();
    // 用例间复位: finalize清残留handler表并归FINALIZED, initialize拉回READY
    ubse_sdk_runtime_finalize();
    (void)ubse_sdk_runtime_initialize();
}

void TestUbseSdkRuntime::TearDown()
{
    GlobalMockObject::verify();
    // 复位白盒注入的排空上限与runtime状态, 避免影响同二进制内其他用例
    g_sdkDrainTimeoutSec = DEFAULT_TOTAL_TIMEOUT / 1000 + 60;
    (void)ubse_sdk_runtime_initialize();
    Test::TearDown();
}

// 初始READY态initialize为无操作成功
TEST_F(TestUbseSdkRuntime, RuntimeInitialize_ReadyNoOp)
{
    EXPECT_EQ(ubse_sdk_runtime_initialize(), UBSE_OK);
}

// finalize后所有业务入口被门禁拒绝(无需mock, 拒绝发生在连接之前)
TEST_F(TestUbseSdkRuntime, RuntimeFinalize_RejectsAllEntries)
{
    ubse_sdk_runtime_finalize();
    EXPECT_EQ(InvokeOnce(), UBSE_ERR_RESOURCE_BUSY);
    EXPECT_EQ(ubse_long_link_connect(), UBSE_ERR_RESOURCE_BUSY);
    EXPECT_EQ(ubse_shm_fault_register(nullptr), UBSE_ERR_RESOURCE_BUSY);
    EXPECT_EQ(ubse_fd_fault_register(nullptr), UBSE_ERR_RESOURCE_BUSY);
    EXPECT_EQ(ubse_numa_fault_register(nullptr), UBSE_ERR_RESOURCE_BUSY);
}

// finalize幂等: FINALIZED下重复调用为无操作, 状态不回卷
TEST_F(TestUbseSdkRuntime, RuntimeFinalize_Idempotent)
{
    ubse_sdk_runtime_finalize();
    ubse_sdk_runtime_finalize(); // 幂等无操作, 不崩溃不挂起
    EXPECT_EQ(InvokeOnce(), UBSE_ERR_RESOURCE_BUSY);
}

// finalize后initialize重拉(fork子进程路径), 业务门禁恢复放行
TEST_F(TestUbseSdkRuntime, RuntimeReinitialize_AfterFinalize)
{
    ubse_sdk_runtime_finalize();
    EXPECT_EQ(ubse_sdk_runtime_initialize(), UBSE_OK);

    MOCKER_CPP(&UbseUDSClient::ConnectToServer).stubs().will(returnValue(UBSE_OK));
    auto buffer = new uint8_t[10];
    UbseResponseMessage responseData{{0, 10}, buffer};
    MOCKER_CPP(&UbseUDSClient::Send).stubs().with(_, outBound(responseData)).will(returnValue(UBSE_OK));
    ubse_api_buffer_t request = {nullptr, 0};
    ubse_api_buffer_t response = {nullptr, 0};
    EXPECT_EQ(ubse_invoke_call(0, 0, &request, &response), UBSE_OK);
    ubse_api_buffer_free(&response);
    delete[] buffer;
}

// 排空等待: finalize阻塞至in-flight活跃引用全部释放后才完成
TEST_F(TestUbseSdkRuntime, RuntimeFinalize_WaitsForInFlightDrain)
{
    GateHold hold;
    ASSERT_TRUE(hold.held);
    std::atomic<bool> done{false};
    std::thread finalizer([&done]() {
        ubse_sdk_runtime_finalize();
        done = true;
    });
    std::this_thread::sleep_for(WAIT_SLICE);
    EXPECT_FALSE(done.load()); // 排空中, finalize未完成
    hold.Release();            // 释放活跃引用(等价于in-flight调用返回)
    finalizer.join();
    EXPECT_TRUE(done.load());
    EXPECT_EQ(InvokeOnce(), UBSE_ERR_RESOURCE_BUSY); // 已到FINALIZED
}

// 契约: 不支持并发finalize, 输家在FINALIZING下立即软失败返回, 不允许等待赢家
TEST_F(TestUbseSdkRuntime, RuntimeFinalize_ConcurrentCallReturnsImmediately)
{
    GateHold hold;
    ASSERT_TRUE(hold.held);
    std::atomic<bool> firstDone{false};
    std::atomic<bool> secondDone{false};
    std::thread first([&firstDone]() {
        ubse_sdk_runtime_finalize();
        firstDone = true;
    });
    std::thread second([&secondDone]() {
        ubse_sdk_runtime_finalize();
        secondDone = true;
    });
    std::this_thread::sleep_for(WAIT_SLICE);
    // 线程调度顺序不确定, 但契约要求: 恰好一个是赢家(排空中未完成), 另一个立即返回
    EXPECT_NE(firstDone.load(), secondDone.load());
    hold.Release();
    first.join();
    second.join();
    EXPECT_TRUE(firstDone.load());  // 排空完成后赢家收尾
    EXPECT_TRUE(secondDone.load()); // 输家早已返回
}

// FINALIZING期间initialize被拒绝(需等finalize完成后重试)
TEST_F(TestUbseSdkRuntime, RuntimeInitialize_RejectedWhileFinalizing)
{
    GateHold hold;
    ASSERT_TRUE(hold.held);
    std::atomic<bool> done{false};
    std::thread finalizer([&done]() {
        ubse_sdk_runtime_finalize();
        done = true;
    });
    std::this_thread::sleep_for(WAIT_SLICE); // 确保已进入FINALIZING
    EXPECT_EQ(ubse_sdk_runtime_initialize(), UBSE_SDK_RUNTIME_INIT_REJECTED);
    hold.Release();
    finalizer.join();
    EXPECT_TRUE(done.load());
    EXPECT_EQ(ubse_sdk_runtime_initialize(), UBSE_OK); // finalize完成后重拉成功
}

// TLS护栏: 回调上下文内调用finalize被拒绝, runtime状态不被破坏
TEST_F(TestUbseSdkRuntime, RuntimeFinalize_RejectedInsideCallback)
{
    MOCKER_CPP(&UbseUDSClient::RegisterLongLinkNotify).stubs().will(returnValue(UBSE_OK));
    ASSERT_EQ(ubse_shm_fault_register(FaultHandlerCallingFinalize), UBSE_OK);

    // 构造合法故障消息触发回调(SerializeMemFault分配的buffer为new[]内存)
    UbseMemFault fault{};
    fault.memName = "tls_guard";
    fault.handleId = 42;
    uint8_t* body = nullptr;
    size_t bodyLen = 0;
    ASSERT_EQ(SerializeMemFault(fault, body, bodyLen), UBSE_OK);
    UbseRequestMessage request{{UBSE_LONG_LINK_REGISTER, UBSE_LONGLINK_FAULT_SHM, static_cast<uint32_t>(bodyLen)},
                               body};
    UbseResponseMessage resp{};
    HandleRequest(request, resp);
    delete[] body;
    EXPECT_TRUE(g_tlsCallbackEntered.load()); // 回调确实被派发
    EXPECT_EQ(resp.header.statusCode, UBSE_OK);

    // finalize被拒则状态仍为READY: invoke过闸后被mock的连接失败拦截(而非BUSY)
    MOCKER_CPP(&UbseUDSClient::ConnectToServer).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    EXPECT_EQ(InvokeOnce(), UBSE_ERR_IPC_CONNECTION_FAILED);
}

// 拆除清理: finalize停长连接并清空已注册的故障handler表
TEST_F(TestUbseSdkRuntime, RuntimeFinalize_ClearsRegisteredHandlers)
{
    MOCKER_CPP(&UbseUDSClient::RegisterLongLinkNotify).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ubse_shm_fault_register(NoopFaultHandler), UBSE_OK);

    ubse_sdk_runtime_finalize();
    // handler表已被清理: 同一事件再无handler应答
    UbseRequestMessage request{};
    request.header.moduleCode = UBSE_LONG_LINK_REGISTER;
    request.header.opCode = UBSE_LONGLINK_FAULT_SHM;
    UbseResponseMessage resp{};
    HandleRequest(request, resp);
    EXPECT_EQ(resp.header.statusCode, UBSE_ERR_DAEMON_UNREACHABLE);
}

// atfork防御(契约路径): finalize后fork, 子进程initialize重拉成功且不死锁
// 子进程不用gtest断言(多线程fork后gtest内部锁状态未定义), 结果经退出码回传父进程校验
TEST_F(TestUbseSdkRuntime, RuntimeFork_ChildCanReinitialize)
{
    ubse_sdk_runtime_finalize(); // FINALIZED, 契约推荐的fork前置态
    pid_t pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        uint32_t ret = ubse_sdk_runtime_initialize(); // 子进程重拉: FINALIZED→READY
        _exit(ret == UBSE_OK ? 0 : 1);
    }
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}

// atfork防御(违约路径): 锁被其他线程持有时fork, prepare持锁等待至锁释放,
// 子进程拿到干净锁, initialize拿锁不死锁
TEST_F(TestUbseSdkRuntime, RuntimeFork_PrepareWaitsForLockRelease)
{
    GateHold hold; // 模拟fork瞬间另一线程正在过闸持锁
    ASSERT_TRUE(hold.held);
    std::atomic<pid_t> childPid{-1};
    std::thread forker([&childPid]() {
        pid_t pid = fork(); // prepare阻塞在g_sdkRuntimeMtx.lock()直至主线程释放
        if (pid == 0) {
            uint32_t ret = ubse_sdk_runtime_initialize(); // 锁干净, 不死锁
            _exit(ret == UBSE_OK ? 0 : 1);
        }
        childPid = pid;
    });
    std::this_thread::sleep_for(WAIT_SLICE); // 让fork确定阻塞在prepare
    hold.Release();                          // 释放锁, fork穿越
    forker.join();
    ASSERT_GT(childPid.load(), 0);
    int status = 0;
    ASSERT_EQ(waitpid(childPid.load(), &status, 0), childPid.load());
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}

// 排空超时分支: 超时后继续拆除且不清零计数, 重拉后旧调用Exit不误扣新调用记账
TEST_F(TestUbseSdkRuntime, RuntimeFinalize_DrainTimeoutKeepsAccounting)
{
    g_sdkDrainTimeoutSec = 0; // 白盒置0: 排空立即超时, 无需等待默认上限
    GateHold stale;           // 模拟无法按期返回的在途调用
    ASSERT_TRUE(stale.held);
    ubse_sdk_runtime_finalize(); // 排空超时, 继续拆除, 残余计数保持不清零
    ASSERT_EQ(ubse_sdk_runtime_initialize(), UBSE_OK);

    GateHold fresh; // 重拉后的新在途调用
    ASSERT_TRUE(fresh.held);
    stale.Release();         // 旧调用此时才返回
    EXPECT_TRUE(fresh.held); // 新调用仍在途, 计数未被旧Exit扣穿

    // 恢复默认排空上限: 第二次finalize应正常等待fresh释放而非立即超时
    g_sdkDrainTimeoutSec = DEFAULT_TOTAL_TIMEOUT / 1000 + 60;
    std::atomic<bool> done{false};
    std::thread finalizer([&done]() {
        ubse_sdk_runtime_finalize();
        done = true;
    });
    std::this_thread::sleep_for(WAIT_SLICE);
    EXPECT_FALSE(done.load()); // fresh仍持有, 排空未完成(旧调用Exit未误扣新计数)
    fresh.Release();
    finalizer.join();
    EXPECT_TRUE(done.load());
}

// 长连接重拉路径: finalize→initialize后, 长连接入口不再被门禁拒绝,
// 走完整PerSistentConnect重建路径(建executor/连接/建epoll), 重建后监听注册恢复可用
TEST_F(TestUbseSdkRuntime, RuntimeReinitialize_LongLinkRebuild)
{
    ubse_sdk_runtime_finalize();
    EXPECT_EQ(ubse_sdk_runtime_initialize(), UBSE_OK);

    // 门禁已放行: 长连接重建走完整路径, 连接结果如实返回(成功而非BUSY)
    MOCKER_CPP(&UbseUDSClient::ConnectToServer).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUDSClient::CreateEpoll).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ubse_long_link_connect(), UBSE_OK);

    // 重建后故障注册入口可用(监听重注册)
    MOCKER_CPP(&UbseUDSClient::RegisterLongLinkNotify).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ubse_shm_fault_register(NoopFaultHandler), UBSE_OK);

    // 再次finalize拆除重建后的长连接, 不崩溃不挂起
    ubse_sdk_runtime_finalize();
    EXPECT_EQ(InvokeOnce(), UBSE_ERR_RESOURCE_BUSY);
}
} // namespace ubse::ut::ipc
