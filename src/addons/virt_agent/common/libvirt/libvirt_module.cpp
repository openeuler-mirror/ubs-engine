/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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

#include "libvirt_module.h"

#include <dlfcn.h>
#include <cerrno>
#include <ubse_logger.h>

namespace vm::libvirt {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;

void *LibvirtModule::libvirtHandle = nullptr;
VirConnectOpenFunc LibvirtModule::virConnectOpenFunc = nullptr;
VirConnectCloseFunc LibvirtModule::virConnectCloseFunc = nullptr;
VirDomainFreeFunc LibvirtModule::virDomainFreeFunc = nullptr;
VirDomainAbortJobFlagsFunc LibvirtModule::virDomainAbortJobFlagsFunc = nullptr;
VirDomainLookupByUUIDStringFunc LibvirtModule::virDomainLookupByUUIDStringFunc = nullptr;

VmResult LibvirtModule::Init()
{
    libvirtHandle = dlopen("/usr/lib64/libvirt.so.0", RTLD_LAZY);
    if (libvirtHandle == nullptr) {
        UBSE_LOG_ERROR << "load libvirt.so.0 failed, " << FormatRetCode(errno);
        return VM_ERROR;
    }
    return VM_OK;
}

void LibvirtModule::DeInit()
{
    if (libvirtHandle != nullptr && dlclose(libvirtHandle) != 0) {
        UBSE_LOG_ERROR << "Failed to close libvirtHandle, error: " << dlerror();
    }
    virConnectOpenFunc = nullptr;
    virConnectCloseFunc = nullptr;
    virDomainFreeFunc = nullptr;
    virDomainAbortJobFlagsFunc = nullptr;
    virDomainLookupByUUIDStringFunc = nullptr;
    libvirtHandle = nullptr;
}

VirConnectOpenFunc LibvirtModule::VirConnectOpen()
{
    if (virConnectOpenFunc != nullptr) {
        return virConnectOpenFunc;
    }
    virConnectOpenFunc = reinterpret_cast<VirConnectOpenFunc>(dlsym(libvirtHandle, "virConnectOpen"));
    if (virConnectOpenFunc == nullptr) {
        UBSE_LOG_ERROR << "Get VirConnectOpen ptr failed, " << dlerror();
        return nullptr;
    }
    return virConnectOpenFunc;
}

VirConnectCloseFunc LibvirtModule::VirConnectClose()
{
    if (virConnectCloseFunc != nullptr) {
        return virConnectCloseFunc;
    }
    virConnectCloseFunc = reinterpret_cast<VirConnectCloseFunc>(dlsym(libvirtHandle, "virConnectClose"));
    if (virConnectCloseFunc == nullptr) {
        UBSE_LOG_ERROR << "Get virConnectClose ptr failed, " << dlerror();
        return nullptr;
    }
    return virConnectCloseFunc;
}

VirDomainFreeFunc LibvirtModule::VirDomainFree()
{
    if (virDomainFreeFunc != nullptr) {
        return virDomainFreeFunc;
    }
    virDomainFreeFunc = reinterpret_cast<VirDomainFreeFunc>(dlsym(libvirtHandle, "virDomainFree"));
    if (virDomainFreeFunc == nullptr) {
        UBSE_LOG_ERROR << "Get VirDomainFree ptr failed, " << dlerror();
        return nullptr;
    }
    return virDomainFreeFunc;
}

VirDomainLookupByUUIDStringFunc LibvirtModule::VirDomainLookupByUuidString()
{
    if (virDomainLookupByUUIDStringFunc != nullptr) {
        return virDomainLookupByUUIDStringFunc;
    }
    virDomainLookupByUUIDStringFunc =
        reinterpret_cast<VirDomainLookupByUUIDStringFunc>(dlsym(libvirtHandle, "virDomainLookupByUUIDString"));
    if (virDomainLookupByUUIDStringFunc == nullptr) {
        UBSE_LOG_ERROR << "Get virDomainLookupByUUIDStringFunc ptr failed, " << dlerror();
        return nullptr;
    }
    return virDomainLookupByUUIDStringFunc;
}

VirDomainAbortJobFlagsFunc LibvirtModule::VirDomainAbortJobFlags()
{
    if (virDomainAbortJobFlagsFunc != nullptr) {
        return virDomainAbortJobFlagsFunc;
    }
    virDomainAbortJobFlagsFunc =
        reinterpret_cast<VirDomainAbortJobFlagsFunc>(dlsym(libvirtHandle, "virDomainAbortJobFlags"));
    if (virDomainAbortJobFlagsFunc == nullptr) {
        UBSE_LOG_ERROR << "Get virDomainAbortJobFlags ptr failed, " << dlerror();
        return nullptr;
    }
    return virDomainAbortJobFlagsFunc;
}
} // namespace vm::libvirt