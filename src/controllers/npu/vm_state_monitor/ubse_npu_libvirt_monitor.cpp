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
#include "ubse_error.h"
#include "ubse_logger.h"

namespace ubse::npu::vm_monitor {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

static constexpr int VIR_DOMAIN_EVENT_ID_LIFECYCLE = 0;

enum class VirDomainEventType {
    VIR_DOMAIN_EVENT_DEFINED = 0,
    VIR_DOMAIN_EVENT_UNDEFINED = 1,
    VIR_DOMAIN_EVENT_STARTED = 2,
    VIR_DOMAIN_EVENT_SUSPENDED = 3,
    VIR_DOMAIN_EVENT_RESUMED = 4,
    VIR_DOMAIN_EVENT_STOPPED = 5,
    VIR_DOMAIN_EVENT_SHUTDOWN = 6,
    VIR_DOMAIN_EVENT_PMSUSPENOED = 7,
    VIR_DOMAIN_EVENT_CRASHED = 8,
    VIR_DOMAIN_EVENT_LAST = 9,
};

extern "C" {
    using VirConnectPtr = void *;
    using VirDomainPtr = void *;
    using VirConnectOpen = VirConnectPtr (*)(const char *);
    using VirConnectClose = int (*)(VirConnectPtr);
    using VirEventRegisterDefaultImpl = int (*)();
    using VirEventRunDefaultImpl = int (*)();
    using VirConnectDomainEventRegisterAny = int (*)(VirConnectPtr, VirDomainPtr, int, void *, void *, void *);
    using VirConnectDomainEventDeregisterAny = int (*)(VirConnectPtr, int);
    using VirDomainGetName = const char* (*)(VirDomainPtr);
    using VirDomainGetXMLDesc = char* (*)(VirDomainPtr, unsigned int);
    using VirFree = void (*)(void *);
}

class LibvirtMonitorImpl {
public:
    explicit LibvirtMonitorImpl(std::string uri)
        : uri_(std::move(uri)) {}

    ~LibvirtMonitorImpl()
    {
        Stop();
    }

    void SetCallBack(const EventCallback &cb)
    {
        userCallback_ = cb;
    }

    bool Start()
    {
        if (running_.load()) {
            return true;
        }
        if (!LoadLibrary()) {
            return false;
        }
        if (virEventRegisterDefaultImpl_() < 0) {
            UBSE_LOG_ERROR << "Failed to register event impl.";
            return false;
        }
        connection_ = virConnectOpen_(uri_.c_str());
        if (!connection_) {
            UBSE_LOG_ERROR << "Failed to connect to " << uri_;
            return false;
        }
        callbackId_ = virConnectDomainEventRegisterAny_(connection_, nullptr, VIR_DOMAIN_EVENT_ID_LIFECYCLE,
                                                        reinterpret_cast<void *>(EventCallbackThunk), this, nullptr);
        if (callbackId_ < 0) {
            UBSE_LOG_ERROR << "Failed to register domain event callback.";
            virConnectClose_(connection_);
            connection_ = nullptr;
            return false;
        }
        running_.store(true);
        eventThread_ = std::thread([this]() {
            while (running_.load()) {
                if (virEventRunDefaultImpl_() < 0) {
                    UBSE_LOG_ERROR << "Error in virEvent loop. Retrying...";
                    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 100: ms
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
        if (eventThread_.joinable()) {
            eventThread_.join();
        }
        if (connection_ && callbackId_ >= 0) {
            virConnectDomainEventDeregisterAny_(connection_, callbackId_);
            callbackId_ = -1;
        }
        if (connection_) {
            virConnectClose_(connection_);
            connection_ = nullptr;
        }
        if (dlHandle_) {
            dlclose(dlHandle_);
            dlHandle_ = nullptr;
        }
    }

    bool IsRunning() const
    {
        return running_.load();
    }

private:
    std::string uri_;
    void *dlHandle_ = nullptr;
    VirConnectPtr connection_ = nullptr;
    int callbackId_ = -1;
    EventCallback userCallback_;

    VirConnectOpen virConnectOpen_ = nullptr;
    VirConnectClose virConnectClose_ = nullptr;
    VirEventRegisterDefaultImpl virEventRegisterDefaultImpl_ = nullptr;
    VirEventRunDefaultImpl virEventRunDefaultImpl_ = nullptr;
    VirConnectDomainEventRegisterAny virConnectDomainEventRegisterAny_ = nullptr;
    VirConnectDomainEventDeregisterAny virConnectDomainEventDeregisterAny_ = nullptr;
    VirDomainGetName virDomainGetName_ = nullptr;
    VirDomainGetXMLDesc virDomainGetXMLDesc_ = nullptr;
    VirFree virFree_ = nullptr;

    std::atomic<bool> running_{false};
    std::thread eventThread_;

    bool LoadLibrary()
    {
        dlHandle_ = dlopen("libvirt.so", RTLD_LAZY);
        if (!dlHandle_) {
            UBSE_LOG_ERROR << "libvirt.so dlopen failed:" << dlerror();
            return false;
        }

        auto loadSymbol = [this](const char *symbolName, void *&symbolPtr) {
            symbolPtr = dlsym(dlHandle_, symbolName);
            if (!symbolPtr) {
                UBSE_LOG_ERROR << "Missing symbol:" << symbolName;
                return false;
            }
            return true;
        };

        struct SymbolInfo {
            const char *symbolName;
            void *&symbolPtr;
        };

        std::vector<SymbolInfo> symbols = {
            {"virConnectOpen", reinterpret_cast<void *&>(virConnectOpen_)},
            {"virConnectClose", reinterpret_cast<void *&>(virConnectClose_)},
            {"virEventRegisterDefaultImpl", reinterpret_cast<void *&>(virEventRegisterDefaultImpl_)},
            {"virEventRunDefaultImpl", reinterpret_cast<void *&>(virEventRunDefaultImpl_)},
            {"virConnectDomainEventRegisterAny", reinterpret_cast<void *&>(virConnectDomainEventRegisterAny_)},
            {"virConnectDomainEventDeregisterAny", reinterpret_cast<void *&>(virConnectDomainEventDeregisterAny_)},
            {"virDomainGetName", reinterpret_cast<void *&>(virDomainGetName_)},
            {"virDomainGetXMLDesc", reinterpret_cast<void *&>(virDomainGetXMLDesc_)},
            {"virFree", reinterpret_cast<void *&>(virFree_)}
        };

        for (auto &symbol : symbols) {
            if (!loadSymbol(symbol.symbolName, symbol.symbolPtr)) {
                return false;
            }
        }
        return true;
    }

    static UbseResult EventCallbackThunk(VirConnectPtr conn, VirDomainPtr dom, int event, int detail,
                                                  void *opaque)
    {
        auto *self = static_cast<LibvirtMonitorImpl *>(opaque);
        return self->HandleEvent(conn, dom, event, detail);
    }

    UbseResult HandleEvent(VirConnectPtr conn, VirDomainPtr dom, int event, int detail)
    {
        if (dom == nullptr) {
            return UBSE_OK;
        }
        const char *domName = virDomainGetName_(dom);
        if (!domName) {
            return UBSE_OK;
        }
        VmEventType eventType;
        switch (static_cast<VirDomainEventType>(event)) {
            case VirDomainEventType::VIR_DOMAIN_EVENT_DEFINED:
                eventType = VmEventType::CREATE;
                break;
            case VirDomainEventType::VIR_DOMAIN_EVENT_STARTED:
                eventType = VmEventType::STARTED;
                break;
            case VirDomainEventType::VIR_DOMAIN_EVENT_STOPPED:
            case VirDomainEventType::VIR_DOMAIN_EVENT_SHUTDOWN:
                eventType = VmEventType::STOPPED;
                break;
            case VirDomainEventType::VIR_DOMAIN_EVENT_UNDEFINED:
                eventType = VmEventType::DESTROY;
                break;
            default:
                return UBSE_OK;
        }
        char *xmlCStr = nullptr;
        xmlCStr = virDomainGetXMLDesc_(dom, 0);
        if (!xmlCStr) {
            return UBSE_OK;
        }
        auto deleter = [this](char *desc) {
            if (desc) {
                virFree_(static_cast<void *>(desc));
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
};

LibvirtMonitor::LibvirtMonitor(const std::string &uri)
    : pImpl_(std::make_unique<LibvirtMonitorImpl>(uri)) {}

LibvirtMonitor::~LibvirtMonitor() = default;

LibvirtMonitor::LibvirtMonitor(LibvirtMonitor &&) noexcept = default;

LibvirtMonitor &LibvirtMonitor::operator=(LibvirtMonitor &&) noexcept = default;

void LibvirtMonitor::SetCallBack(const EventCallback &cb)
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