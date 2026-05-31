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

#ifndef UBSE_LIBVIRTMONITOR_H
#define UBSE_LIBVIRTMONITOR_H

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include "ubse_common_def.h"

namespace ubse::npu::vm_monitor {
using namespace ubse::common::def;
enum class VirDomainEventType {
    VIR_DOMAIN_EVENT_DEFINED = 0,
    VIR_DOMAIN_EVENT_UNDEFINED = 1,
    VIR_DOMAIN_EVENT_STARTED = 2,
    VIR_DOMAIN_EVENT_SUSPENDED = 3,
    VIR_DOMAIN_EVENT_RESUMED = 4,
    VIR_DOMAIN_EVENT_STOPPED = 5,
    VIR_DOMAIN_EVENT_SHUTDOWN = 6,
    VIR_DOMAIN_EVENT_PMSUSPENDED = 7,
    VIR_DOMAIN_EVENT_CRASHED = 8,
    VIR_DOMAIN_EVENT_LAST = 9,
    VIR_DOMAIN_EVENT_REBOOT = 99
};
using EventCallback = std::function<void(std::string_view, VirDomainEventType, std::shared_ptr<char>)>;

class LibvirtMonitorImpl;

class LibvirtMonitor {
public:
    explicit LibvirtMonitor(const std::string& uri = "qemu:///system");

    ~LibvirtMonitor();

    LibvirtMonitor(const LibvirtMonitor&) = delete;

    LibvirtMonitor& operator=(const LibvirtMonitor&) = delete;

    LibvirtMonitor(LibvirtMonitor&&) noexcept;

    LibvirtMonitor& operator=(LibvirtMonitor&&) noexcept;

    void SetCallBack(const EventCallback& cb);

    bool Start();

    void Stop();

    bool IsRunning() const;

private:
    std::unique_ptr<LibvirtMonitorImpl> pImpl_;
};
} // namespace ubse::npu::vm_monitor
#endif // UBSE_LIBVIRTMONITOR_H