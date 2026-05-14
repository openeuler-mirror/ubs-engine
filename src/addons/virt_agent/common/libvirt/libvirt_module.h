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

#ifndef VM_LIBVIRT_MODULE_H
#define VM_LIBVIRT_MODULE_H

#include <cstdint>

#include "vm_error.h"

namespace vm::libvirt {
using VirConnectPtr = void*;
enum class VirDomainAbortJobFlagsValues : uint8_t {
    VIR_DOMAIN_ABORT_JOB_POSTCOPY = 1 << 0, // Interrupt post-copy migration.
    VIR_DOMAIN_ABORT_JOB_HAM = 1 << 1,      // Interrupt ham migration.
};

using VirConnectOpenFunc = void* (*)(const char*);
using VirConnectCloseFunc = void (*)(void*);
using VirDomainFreeFunc = int (*)(void*);
using VirDomainAbortJobFlagsFunc = int (*)(void*, VirDomainAbortJobFlagsValues);
using VirDomainLookupByUUIDStringFunc = void* (*)(void*, const char*);

class LibvirtModule {
public:
    static VmResult Init();

    static void DeInit();

    static VirConnectOpenFunc VirConnectOpen();

    static VirConnectCloseFunc VirConnectClose();

    static VirDomainFreeFunc VirDomainFree();

    static VirDomainLookupByUUIDStringFunc VirDomainLookupByUuidString();

    static VirDomainAbortJobFlagsFunc VirDomainAbortJobFlags();

private:
    static void* libvirtHandle;
    static VirConnectOpenFunc virConnectOpenFunc;
    static VirConnectCloseFunc virConnectCloseFunc;
    static VirDomainFreeFunc virDomainFreeFunc;
    static VirDomainAbortJobFlagsFunc virDomainAbortJobFlagsFunc;
    static VirDomainLookupByUUIDStringFunc virDomainLookupByUUIDStringFunc;
};
} // namespace vm::libvirt

#endif // VM_LIBVIRT_MODULE_H
