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

#ifndef VM_RES_QUERY_H
#define VM_RES_QUERY_H

#include <mutex>

#include "vm_def.h"
#include "vm_error.h"
#include "vm_info.h"
#include "vm_numa_info.h"

namespace vm {
class ResourceQuery {
public:
    static HostVmDomainInfo gHostVmDomainInfo;
    /**
     * Get information about all VMs and NUMA nodes on the local node.
     * @param hostVmDomainInfo Stores VM domain information.
     * @param hostNumaCpuInfo Stores NUMA nodes information.
     * @return VmResult, 0 indicates success, and any non-zero value indicates fail or other exception
     */
    static VmResult GetLocalHostVmCollectData(HostVmDomainInfo &hostVmDomainInfo, HostNumaCpuInfo &hostNumaCpuInfo);

    /**
     * Get information about all VMs on the local node.
     * @param hostVmDomainInfo Stores VM domain information.
     * @return VmResult, 0 indicates success, and any non-zero value indicates fail or other exception
     */
    static VmResult GetLocalHostVmDomainInfo(HostVmDomainInfo &hostVmDomainInfo);

    /**
     * Update global information about all VMs on the local node.
     * @param hostVmDomainInfo Stores VM domain information.
     * @return VmResult, 0 indicates success, and any non-zero value indicates fail or other exception
     */
    static VmResult UpdateVmDomainInfo(HostVmDomainInfo &hostVmDomainInfo);

    /**
     * Get information about all VMs on the local node from global.
     * @param hostVmDomainInfo Stores VM domain information.
     * @return VmResult, 0 indicates success, and any non-zero value indicates fail or other exception
     */
    static VmResult GetVmDomainInfosFromGlobal(HostVmDomainInfo &hostVmDomainInfo);

    /**
     * Get information about all NUMA nodes on the local node.
     * @param hostNumaCpuInfo Stores NUMA nodes information.
     * @return VmResult, 0 indicates success, and any non-zero value indicates fail or other exception
     */
    static VmResult GetLocalHostNumaInfo(HostNumaCpuInfo &hostNumaCpuInfo);

private:
    static std::mutex vmDomainMutex;
};
} // namespace vm

#endif // VM_RES_QUERY_H
