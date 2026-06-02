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

#ifndef VM_RESOURCE_COLLECT_H
#define VM_RESOURCE_COLLECT_H

#include <mutex>
#include <unordered_map>
#include <vector>

#include <ubse_def.h>
#include <ubse_mem_controller.h>

#include "vm_def.h"
#include "vm_error.h"
#include "vm_info.h"
#include "vm_lock.h"
#include "vm_numa_info.h"
#include "vm_struct.h"

namespace vm {
using std::string;
using std::vector;
using namespace ubse::mem::controller;

const std::string MEM_REPLACE_FLAG = "-rep";

class ResourceCollect {
public:
    static VmResult Init();
    static std::mutex mAllLock; // Lock the globalNumaInfoMap and globalNumaVMInfoMap resources.

    static std::string globalBorrowMapKeyPrefix;
    static std::string globalBorrowMapKey;
    static ReadWriteLock globalBorrowLock;
    static GlobalBorrowMap globalBorrowMap_;
    static uint64_t globalBorrowMapIndex_;

    static ResourceCollect &GetInstance()
    {
        static ResourceCollect gInstance;
        return gInstance;
    };
    std::unordered_map<std::string, VMBasicInfo> GetVMInfo(VMNodeLocInfo &nodeLocInfo);

    std::vector<pid_t> GetPidsOnNuma(const VMNodeLocInfo &nodeLocInfo, const std::string &flag = "");
    void InheritNumaInfo(GlobalNumaInfo &numaInfo, VMNodeLocInfo &tempNodeLoc);
    VmResult UpdateVMStatus(const NumaMemInfoMap &numaMemInfoMap, const std::string &uuid, const pid_t &pid,
                            const VmMigrateStatus &vmMigrateStatus);
    VmResult VmResourceCollectInfoHandle(vector<HostVmDomainInfo> &vmDomainInfoCollectList,
                                         vector<HostNumaCpuInfo> &numaInfoCollectList);

    NumaVMInfoMap GetGlobalSampleVMInfo()
    {
        std::lock_guard<std::mutex> lockGuard(mVMInfoLock);
        return globalNumaVMInfoMap;
    }

    GlobalNumaInfoMap *GetGlobalSampleNumaInfo()
    {
        std::lock_guard<std::mutex> lockGuard(mGlobalNumaLock);
        return &globalNumaInfoMap;
    }

    void PrintSetInfo();
    void KeepVMBasicInfo();
    void KeepNumaInfo();

    GlobalBorrowMap GetGlobalBorrowMap()
    {
        ReadLocker lock(&globalBorrowLock);
        return globalBorrowMap_;
    }
    void UpdateGlobalNumaInfoMapNumaMemBorrow(VMNodeLocInfo &vmNodeLocInfo, uint64_t borrowMemSize);
    VmResult UpdateGlobalNumaInfoMapAndGlobalNumaVMInfoMap(HostVmDomainInfo &hostVmDomainInfo,
                                                           HostNumaCpuInfo &hostNumaCpuInfo);
    static void UpdateGlobalBorrowMap(const std::vector<BorrowIdStatus> &borrowIdStatuses);
    static void DeleteGlobalBorrowMap(const std::vector<std::string> &borrowIds);
    static VmResult SyncGlobalBorrowMap(const std::vector<UbseNumaMemoryImportDebtInfo> &debtInfos);
    static VmResult InitGlobalBorrowMap();
    static void QueryHandler(const std::string &keyPrefix, const std::string &key, const UbseByteBuffer &buff,
                             void *ctx);
    static void DeInitGlobalBorrowMap();

    VmResult LoadVmMigrateData();

private:
    ResourceCollect() = default;
    ~ResourceCollect() = default;

    std::mutex mGlobalNumaLock{};
    std::mutex mVMInfoLock{};
    std::mutex mKeepInfoLock{};
    GlobalNumaInfoMap globalNumaInfoMap{};
    NumaVMInfoMap globalNumaVMInfoMap{}; // Delete VMs from the map table after migration.
    VMInfoToKeepMap VMInfoToKeep{};
    NumaInfoToKeepMap NumaToKeep{};

    void AddToGlobalVmMap(const vm::VmDomainInfo &ele, VMNodeLocInfo &tempNodeLoc, VMBasicInfo &tempVmBasicInfo);

    void VMDomainInfoToVmBasicInfo(VMBasicInfo &vmBasicInfoIn, const vm::VmDomainInfo &vmDomainInfo);

    void AddVmDomainInfoCollectToVmMap(const HostVmDomainInfo &vmDomainInfoCollect);

    void AddNumaInfo(const HostNumaCpuInfo &numaInfoIn);

    void ClearGlobalVMInfo();

    void ClearGlobalNumaInfo();

    void ClearVMInfoToKeep();

    void ClearNumaToKeep();
};
} // namespace vm

#endif // VM_RESOURCE_COLLECT_H
