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
#include "ubse_npu_monitor_def.h"
#include "ubse_common_def.h"

namespace ubse::npu::vm_monitor {
using namespace ubse::common::def;
using EventCallback = std::function<void(std::string_view, VmEventType, std::shared_ptr<char>)>;

class LibvirtMonitorImpl;

class LibvirtMonitor {
public:
    explicit LibvirtMonitor(const std::string &uri = "qumu:///system");

    ~LibvirtMonitor();

    LibvirtMonitor(const LibvirtMonitor &) = delete;

    LibvirtMonitor &operator=(const LibvirtMonitor &) = delete;

    LibvirtMonitor(LibvirtMonitor &&) noexcept;

    LibvirtMonitor &operator=(LibvirtMonitor &&) noexcept;

    void SetCallBack(const EventCallback &cb);

    bool Start();

    void Stop();

    bool IsRunning() const;

private:
    std::unique_ptr<LibvirtMonitorImpl> pImpl_;
};
} // namespace ubse::npu::vm_monitor
#endif // UBSE_LIBVIRTMONITOR_H