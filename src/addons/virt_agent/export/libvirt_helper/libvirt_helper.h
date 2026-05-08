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

#ifndef VM_LIBVIRT_HELPER_H
#define VM_LIBVIRT_HELPER_H
#include <string>

#include "libvirt_module.h"
#include "vm_error.h"

namespace vm {
using std::string;

class LibvirtHelper {
public:
    LibvirtHelper() = default;
    LibvirtHelper(const LibvirtHelper &) = delete;
    LibvirtHelper &operator=(const LibvirtHelper &) = delete;

    static inline LibvirtHelper &GetInstance()
    {
        static LibvirtHelper instance;
        return instance;
    }
    static void FreeDomain(void *domain);

    VmResult Init();
    void DeInit();
    VmResult Connect();
    VmResult CloseConn();
    VmResult DomainAbortJobFlags(const string &uuid, libvirt::VirDomainAbortJobFlagsValues flags, int tryTimes = 1);

private:
    libvirt::VirConnectPtr virConnect{};
};
} // namespace vm

#endif // VM_LIBVIRT_HELPER_H
