/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MP_LIBVIRTHELPER_H
#define MP_LIBVIRTHELPER_H

#include <thread>

#include "export_type.h"
#include "rmrs_libvirt_module.h"
#include "mp_error.h"
#include "mp_configuration.h"

namespace mempooling::exportV2 {
using mempooling::libvirt::VirConnectPtr;
using mempooling::libvirt::VirDomainPtr;
using mempooling::libvirt::VirDomainInfo;

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
    MpResult Init();
    MpResult Connect();
    MpResult Reconnect();
    MpResult CloseConn();
    MpResult CheckConnectAndReconnect();
    MpResult ConnectSetKeepAlive();
    MpResult RegisterEventDefaultImpl();
    void Shutdown();

    bool IsConnectAlive();
    void KeepAlive();
    void FreeDomain(VirDomainPtr domain);

    MpResult GetDomainByName(const std::string &name, VirDomainPtr &domain);
    MpResult GetVmUuidByDomain(VirDomainPtr domain, std::string &uuid);
    MpResult GetVmStateAndMaxMemByDomain(VirDomainPtr domain, VmDomainInfo &info);

private:
    static inline constexpr size_t VM_UUID_LEN = 37; // 36(UUID位数) + 1(\0)
    VirConnectPtr virConnect{};
    std::thread *virKeepAliveThread{nullptr};
};

} // namespace mempooling::exportV2

#endif // MP_LIBVIRT_HELPER_H