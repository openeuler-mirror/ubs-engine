/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * UbseEngine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_npu_libvirt_monitor.h"
#include <dlfcn.h>
#include <atomic>
#include <thread>
#include "ubse_error.h"
#include "ubse_logger.h"

namespace ubse::npu::vm_monitor {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

static constexpr int VIR_DOMAIN_EVENT_ID_LIFECYCLE = 0;
static constexpr int VIR_DOMAIN_EVENT_ID_REBOOT = 1;
static constexpr int KEEP_ALIVE_INTERVAL = 10; // keepalive 探测间隔（秒），libvirt 每隔该时长发送心跳探测连接存活性
static constexpr unsigned int KEEP_ALIVE_COUNT = 3; // keepalive 探测失败次数阈值，连续该次数无响应后判定连接失活
static constexpr int WATCHDOG_INTERVAL_MS = 1000; // 看门狗周期，保证断链后事件循环仍能有界返回
static constexpr int SLEEP_SLICE_MS = 100; // 睡眠分片时长，保证 Stop() 触发后事件线程能及时退出
static constexpr int CONNECT_TIMEOUT_MS = 3000; // virConnectOpen 超时阈值，规避 libvirtd 长时间无响应导致 Stop() 挂死

extern "C" {
using VirConnectPtr = void*;
using VirDomainPtr = void*;
using VirConnectOpen = VirConnectPtr (*)(const char*);
using VirConnectClose = int (*)(VirConnectPtr);
using VirEventRegisterDefaultImpl = int (*)();
using VirEventRunDefaultImpl = int (*)();
using VirConnectDomainEventRegisterAny = int (*)(VirConnectPtr, VirDomainPtr, int, void*, void*, void*);
using VirConnectDomainEventDeregisterAny = int (*)(VirConnectPtr, int);
using VirDomainGetName = const char* (*)(VirDomainPtr);
using VirDomainGetXMLDesc = char* (*)(VirDomainPtr, unsigned int);
using VirConnectSetKeepAlive = int (*)(VirConnectPtr, int, unsigned int);
using VirConnectIsAlive = int (*)(VirConnectPtr);
using VirEventTimeoutCallback = void (*)(int, void*);
using VirEventFreeCallback = void (*)(void*);
using VirEventAddTimeout = int (*)(int, VirEventTimeoutCallback, void*, VirEventFreeCallback);
using VirEventRemoveTimeout = int (*)(int);
}

class LibvirtMonitorImpl {
public:
    explicit LibvirtMonitorImpl(std::string uri,
                                int reconnectIntervalMs = LibvirtMonitor::DEFAULT_RECONNECT_INTERVAL_MS)
        : uri_(std::move(uri)),
          reconnectIntervalMs_(reconnectIntervalMs)
    {
    }

    ~LibvirtMonitorImpl()
    {
        Stop();
    }

    void SetCallBack(const EventCallback& cb)
    {
        userCallback_ = cb;
    }

    bool Start()
    {
        if (running_.load()) {
            return true;
        }
        stopRequested_.store(false);
        if (!LoadLibrary()) {
            return false;
        }
        if (virEventRegisterDefaultImpl_() < 0) {
            UBSE_LOG_ERROR << "Failed to register event impl.";
            CleanupResources();
            return false;
        }
        // 看门狗定时器：保证事件循环 poll 有界返回，断链检测与 Stop() 才能执行
        watchdogTimerId_ = virEventAddTimeout_(WATCHDOG_INTERVAL_MS, WatchdogTimerCallback, nullptr, nullptr);
        if (watchdogTimerId_ < 0) {
            UBSE_LOG_ERROR << "Failed to register watchdog timer.";
            CleanupResources();
            return false;
        }
        if (!OpenAndRegister()) {
            CleanupResources();
            return false;
        }

        running_.store(true);
        eventThread_ = std::thread([this]() {
            while (running_.load()) {
                if (virEventRunDefaultImpl_() < 0) {
                    UBSE_LOG_ERROR << "Error in virEvent loop. Retrying...";
                    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_SLICE_MS));
                }
                if (!ConnectionAlive()) {
                    UBSE_LOG_WARN << "libvirt connection lost, trying to reconnect.";
                    Reconnect();
                }
            }
        });
        return true;
    }

    void Stop()
    {
        if (!running_.exchange(false)) {
            return;
        }
        stopRequested_.store(true);
        if (eventThread_.joinable()) {
            eventThread_.join();
        }
        CleanupResources();
    }

    // 统一资源清理：释放连接与看门狗定时器，供 Start() 失败路径与 Stop() 共用
    // 不 dlclose：detached 线程可能仍在执行 virConnectOpen_，卸载会导致崩溃
    void CleanupResources()
    {
        ReleaseConnection();
        if (watchdogTimerId_ >= 0) {
            virEventRemoveTimeout_(watchdogTimerId_);
            watchdogTimerId_ = -1;
        }
    }

    bool IsRunning() const
    {
        return running_.load();
    }

private:
    std::string uri_;
    void* dlHandle_ = nullptr;
    VirConnectPtr connection_ = nullptr;
    int lifecycleCallbackId_ = -1;
    int rebootCallbackId_ = -1;
    int watchdogTimerId_ = -1;
    EventCallback userCallback_;

    VirConnectOpen virConnectOpen_ = nullptr;
    VirConnectClose virConnectClose_ = nullptr;
    VirEventRegisterDefaultImpl virEventRegisterDefaultImpl_ = nullptr;
    VirEventRunDefaultImpl virEventRunDefaultImpl_ = nullptr;
    VirConnectDomainEventRegisterAny virConnectDomainEventRegisterAny_ = nullptr;
    VirConnectDomainEventDeregisterAny virConnectDomainEventDeregisterAny_ = nullptr;
    VirDomainGetName virDomainGetName_ = nullptr;
    VirDomainGetXMLDesc virDomainGetXMLDesc_ = nullptr;
    VirConnectSetKeepAlive virConnectSetKeepAlive_ = nullptr;
    VirConnectIsAlive virConnectIsAlive_ = nullptr;
    VirEventAddTimeout virEventAddTimeout_ = nullptr;
    VirEventRemoveTimeout virEventRemoveTimeout_ = nullptr;

    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    int reconnectIntervalMs_ =
        LibvirtMonitor::DEFAULT_RECONNECT_INTERVAL_MS; // 重连失败后的重试间隔，可注入（测试加速）
    std::thread eventThread_;

    bool LoadLibrary()
    {
        if (dlHandle_) {
            return true; // 已加载，避免重复 dlopen 导致句柄泄漏
        }
        dlHandle_ = dlopen("libvirt.so", RTLD_LAZY);
        if (!dlHandle_) {
            UBSE_LOG_ERROR << "libvirt.so dlopen failed:" << dlerror();
            return false;
        }

        auto loadSymbol = [this](const char* symbolName, void*& symbolPtr) {
            symbolPtr = dlsym(dlHandle_, symbolName);
            if (!symbolPtr) {
                UBSE_LOG_ERROR << "Missing symbol:" << symbolName;
                return false;
            }
            return true;
        };

        struct SymbolInfo {
            const char* symbolName;
            void*& symbolPtr;
        };

        std::vector<SymbolInfo> symbols = {
            {"virConnectOpen", reinterpret_cast<void*&>(virConnectOpen_)},
            {"virConnectClose", reinterpret_cast<void*&>(virConnectClose_)},
            {"virEventRegisterDefaultImpl", reinterpret_cast<void*&>(virEventRegisterDefaultImpl_)},
            {"virEventRunDefaultImpl", reinterpret_cast<void*&>(virEventRunDefaultImpl_)},
            {"virConnectDomainEventRegisterAny", reinterpret_cast<void*&>(virConnectDomainEventRegisterAny_)},
            {"virConnectDomainEventDeregisterAny", reinterpret_cast<void*&>(virConnectDomainEventDeregisterAny_)},
            {"virDomainGetName", reinterpret_cast<void*&>(virDomainGetName_)},
            {"virDomainGetXMLDesc", reinterpret_cast<void*&>(virDomainGetXMLDesc_)},
            {"virConnectSetKeepAlive", reinterpret_cast<void*&>(virConnectSetKeepAlive_)},
            {"virConnectIsAlive", reinterpret_cast<void*&>(virConnectIsAlive_)},
            {"virEventAddTimeout", reinterpret_cast<void*&>(virEventAddTimeout_)},
            {"virEventRemoveTimeout", reinterpret_cast<void*&>(virEventRemoveTimeout_)}};

        for (auto& symbol : symbols) {
            if (!loadSymbol(symbol.symbolName, symbol.symbolPtr)) {
                dlclose(dlHandle_);
                dlHandle_ = nullptr;
                return false;
            }
        }
        return true;
    }

    // 看门狗空回调：保证事件循环 poll 有有限超时
    static void WatchdogTimerCallback(int timer, void* opaque)
    {
        (void)timer;
        (void)opaque;
    }

    // virConnectOpen 超时封装：detached 线程执行连接，主线程轮询结果，拷贝参数规避 UAF
    VirConnectPtr ConnectWithTimeout()
    {
        // 原子交接：线程 CONNECTING→FINISHED，主线程 CONNECTING→ABANDONED
        // exchange 见对方终态者兜底关闭，保证恰好处置一次
        enum class ConnState
        {
            CONNECTING,
            FINISHED,
            ABANDONED
        };
        struct ConnectResult {
            std::atomic<ConnState> state{ConnState::CONNECTING};
            VirConnectPtr conn = nullptr;
        };
        auto cr = std::make_shared<ConnectResult>();
        std::string uriCopy = uri_;
        VirConnectOpen openFn = virConnectOpen_;
        VirConnectClose closeFn = virConnectClose_;

        std::thread([uriCopy, openFn, closeFn, cr]() {
            if (openFn != nullptr) {
                cr->conn = openFn(uriCopy.c_str());
            }
            if (cr->state.exchange(ConnState::FINISHED) == ConnState::ABANDONED && cr->conn != nullptr &&
                closeFn != nullptr) {
                closeFn(cr->conn); // 主线程已放弃，兜底关闭
            }
        }).detach();

        // 见 FINISHED 取走连接；stopRequested_ 触发提前放弃
        for (int waited = 0; waited < CONNECT_TIMEOUT_MS; waited += SLEEP_SLICE_MS) {
            if (stopRequested_.load()) {
                break;
            }
            if (cr->state.load() == ConnState::FINISHED) {
                return cr->conn;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_SLICE_MS));
        }
        // 声明放弃；若线程恰好已完成则兜底关闭
        if (cr->state.exchange(ConnState::ABANDONED) == ConnState::FINISHED && cr->conn != nullptr) {
            virConnectClose_(cr->conn);
        }
        if (stopRequested_.load()) {
            UBSE_LOG_INFO << "virConnectOpen to " << uri_ << " aborted by Stop()";
        } else {
            UBSE_LOG_ERROR << "virConnectOpen to " << uri_ << " timeout after " << CONNECT_TIMEOUT_MS << "ms";
        }
        return nullptr;
    }

    // 建立连接并注册 keepalive 与事件回调，供 Start() 与重连共用
    bool OpenAndRegister()
    {
        connection_ = ConnectWithTimeout();
        if (!connection_) {
            UBSE_LOG_ERROR << "Failed to connect to " << uri_;
            return false;
        }
        if (virConnectSetKeepAlive_(connection_, KEEP_ALIVE_INTERVAL, KEEP_ALIVE_COUNT) < 0) {
            UBSE_LOG_WARN << "Set keepalive failed, Stop may block until next libvirt event.";
        }
        lifecycleCallbackId_ = virConnectDomainEventRegisterAny_(connection_, nullptr, VIR_DOMAIN_EVENT_ID_LIFECYCLE,
                                                                 reinterpret_cast<void*>(EventCallbackThunk), this,
                                                                 nullptr);
        if (lifecycleCallbackId_ < 0) {
            UBSE_LOG_ERROR << "Failed to register domain lifecycle event callback.";
            ReleaseConnection();
            return false;
        }
        rebootCallbackId_ = virConnectDomainEventRegisterAny_(connection_, nullptr, VIR_DOMAIN_EVENT_ID_REBOOT,
                                                              reinterpret_cast<void*>(GenericEventCallback), this,
                                                              nullptr);
        if (rebootCallbackId_ < 0) {
            UBSE_LOG_ERROR << "Failed to register domain reboot event callback.";
            ReleaseConnection();
            return false;
        }
        return true;
    }

    // 注销回调并关闭连接，保留看门狗定时器与动态库句柄供重连复用
    void ReleaseConnection()
    {
        if (connection_ && rebootCallbackId_ >= 0) {
            virConnectDomainEventDeregisterAny_(connection_, rebootCallbackId_);
            rebootCallbackId_ = -1;
        }
        if (connection_ && lifecycleCallbackId_ >= 0) {
            virConnectDomainEventDeregisterAny_(connection_, lifecycleCallbackId_);
            lifecycleCallbackId_ = -1;
        }
        if (connection_) {
            virConnectClose_(connection_);
            connection_ = nullptr;
        }
    }

    // 自动重连：周期性重试直至成功或 Stop()，成功后重新注册 keepalive 与事件回调，恢复事件订阅
    // 关键节点（开始重连、每次尝试、成功/失败）打印时间戳、重连次数、耗时与失败原因，便于现网排查连接抖动
    void Reconnect()
    {
        ReleaseConnection();
        int attempt = 0;
        auto reconnectStart = std::chrono::steady_clock::now();
        UBSE_LOG_WARN << "Reconnect start to " << uri_;
        while (running_.load()) {
            attempt++;
            auto attemptStart = std::chrono::steady_clock::now();
            if (OpenAndRegister()) {
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - attemptStart)
                                     .count();
                auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                                     reconnectStart)
                                   .count();
                UBSE_LOG_INFO << "Reconnected to " << uri_ << " successfully, attempt=" << attempt
                              << ", this attempt cost=" << elapsedMs << "ms, total cost=" << totalMs << "ms";
                return;
            }
            auto elapsedMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - attemptStart)
                    .count();
            UBSE_LOG_WARN << "Reconnect to " << uri_ << " failed, attempt=" << attempt
                          << ", this attempt cost=" << elapsedMs << "ms"
                          << ", reason=virConnectOpen failed or keepalive/register failed"
                          << ", retrying in " << reconnectIntervalMs_ << "ms";
            InterruptibleSleep(reconnectIntervalMs_);
        }
        UBSE_LOG_WARN << "Reconnect to " << uri_ << " aborted, attempt=" << attempt << ", reason=Stop() called";
    }

    bool ConnectionAlive() const
    {
        return connection_ != nullptr && virConnectIsAlive_(connection_) == 1;
    }

    void InterruptibleSleep(int totalMs)
    {
        for (int sleptMs = 0; sleptMs < totalMs && running_.load(); sleptMs += SLEEP_SLICE_MS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_SLICE_MS));
        }
    }

    static void EventCallbackThunk(VirConnectPtr conn, VirDomainPtr dom, int event, int detail, void* opaque)
    {
        auto* self = static_cast<LibvirtMonitorImpl*>(opaque);
        self->HandleEvent(conn, dom, event, detail);
    }

    UbseResult HandleEvent(VirConnectPtr conn, VirDomainPtr dom, int event, int detail)
    {
        UBSE_LOG_INFO << "Received event:" << event << " detail:" << detail;
        if (dom == nullptr) {
            return UBSE_OK;
        }
        const char* domName = virDomainGetName_(dom);
        if (!domName) {
            return UBSE_OK;
        }
        auto eventType = static_cast<VirDomainEventType>(event);
        char* xmlCStr = nullptr;
        xmlCStr = virDomainGetXMLDesc_(dom, 0);
        if (!xmlCStr) {
            return UBSE_OK;
        }
        auto deleter = [](char* desc) {
            if (desc) {
                free(static_cast<void*>(desc));
            }
        };
        std::shared_ptr<char> sp(xmlCStr, deleter);
        if (userCallback_) {
            try {
                userCallback_(domName, eventType, sp);
            } catch (...) {
                UBSE_LOG_ERROR << "[ERROR] Exception in user callback for domain " << domName;
            }
        }
        return UBSE_OK;
    }

    static void GenericEventCallback(VirConnectPtr conn, VirDomainPtr dom, void* opaque)
    {
        auto* self = static_cast<LibvirtMonitorImpl*>(opaque);
        self->HandleEvent(conn, dom, static_cast<int>(VirDomainEventType::VIR_DOMAIN_EVENT_REBOOT), 0);
    }
};

LibvirtMonitor::LibvirtMonitor(const std::string& uri, int reconnectIntervalMs)
    : pImpl_(std::make_unique<LibvirtMonitorImpl>(uri, reconnectIntervalMs))
{
}

LibvirtMonitor::~LibvirtMonitor() = default;

LibvirtMonitor::LibvirtMonitor(LibvirtMonitor&&) noexcept = default;

LibvirtMonitor& LibvirtMonitor::operator=(LibvirtMonitor&&) noexcept = default;

void LibvirtMonitor::SetCallBack(const EventCallback& cb)
{
    pImpl_->SetCallBack(cb);
}

bool LibvirtMonitor::Start()
{
    return pImpl_->Start();
}

void LibvirtMonitor::Stop()
{
    pImpl_->Stop();
}

bool LibvirtMonitor::IsRunning() const
{
    return pImpl_->IsRunning();
}
} // namespace ubse::npu::vm_monitor