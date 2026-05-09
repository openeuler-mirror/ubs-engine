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

#ifndef MEMPOOLING_MODULE_H
#define MEMPOOLING_MODULE_H

#include <map>
#include <optional>
#include <string>
#include <vector>
#include "mempooling_def.h"
#include "vm_error.h"

namespace vm::mempooling {
using UBSRMRSUpdateAntiNodeFunc = uint32_t (*)(const std::map<std::string, std::vector<std::string>> &);
using UBSRMRSMemBorrowStrategyFunc = uint32_t (*)(const SrcMemoryBorrowParam &, const uint64_t &,
                                                  MemBorrowStrategyResult &);
using UBSRMRSMemBorrowExecuteFunc = uint32_t (*)(const SrcMemoryBorrowParam &,
                                                 const std::vector<DestMemoryBorrowParam> &, MemBorrowExecuteResult &);
using UBSRMRSMigrateStrategyFunc = uint32_t (*)(const std::string &, const std::vector<VMPresetParam> &,
                                                const uint64_t &, MigrateStrategyResult &);
using UBSRMRSMigrateExecuteFunc = uint32_t (*)(const std::string &, const std::vector<VMMigrateOutParam> &,
                                               const std::uint64_t &, const std::vector<std::string> &);
using UBSRMRSMemFreeFunc = uint32_t (*)(const std::string &);
using UBSRMRSMemBorrowRollbackFunc = uint32_t (*)(const std::string &, const std::vector<std::string> &);
using UBSRMRSGetVmInfoListOnNodeFunc = uint32_t (*)(std::vector<VmDomainInfo> &);
using UBSRMRSGetNumaInfoListOnNodeFunc = uint32_t (*)(std::vector<NumaInfo> &);
using UBSRMRSMemBorrowFunc = uint32_t (*)(const SrcMemoryBorrowParam &, const std::vector<uint64_t> &,
                                          const WaterMark &, MemBorrowExecuteResult &);
using UBSRMRSMemMigrateFunc = uint32_t (*)(const SrcMemoryBorrowParam &, const std::vector<VMPresetParam> &,
                                           const MemBorrowExecuteResult &);
using UBSRMRSMemReturnFunc = uint32_t (*)(const SrcMemoryBorrowParam &, const std::vector<std::string> &,
                                          const std::vector<pid_t> &);
using UBSRMRSSetRunModeFunc = uint32_t (*)(const int &);

using UBSRMRSPidNumaInfoCollectFunc = uint32_t (*)(const SrcMemoryBorrowParam &, const std::vector<pid_t> &,
                                                   std::vector<PidInfo> &);
using UBSRMRSSetWaterMarkFunc = uint32_t (*)(const WaterMark &);

using UBSRMRSSmapAddProcessTrackingFunc = uint32_t (*)(const std::vector<pid_t> &, const std::vector<uint32_t> &, int,
                                                       const std::optional<std::vector<uint32_t>> &);

using UBSRMRSSmapRemoveProcessTrackingFunc = uint32_t (*)(const std::vector<pid_t> &, int);

using UBSRMRSSmapEnableProcessMigrateFunc = uint32_t (*)(const std::vector<pid_t> &, int, int);

class MempoolingModule {
public:
    static VmResult Init();
    static void DeInit();
    static UBSRMRSUpdateAntiNodeFunc UBSRMRSUpdateAntiNode();
    static UBSRMRSMemBorrowStrategyFunc UBSRMRSMemBorrowStrategy();
    static UBSRMRSMemBorrowExecuteFunc UBSRMRSMemBorrowExecute();
    static UBSRMRSMigrateStrategyFunc UBSRMRSMigrateStrategy();
    static UBSRMRSMigrateExecuteFunc UBSRMRSMigrateExecute();
    static UBSRMRSMemFreeFunc UBSRMRSMemFree();
    static UBSRMRSMemBorrowRollbackFunc UBSRMRSMemBorrowRollback();
    static UBSRMRSGetVmInfoListOnNodeFunc UBSRMRSGetVmInfoListOnNode();
    static UBSRMRSGetNumaInfoListOnNodeFunc UBSRMRSGetNumaInfoListOnNode();
    static UBSRMRSMemBorrowFunc UBSRMRSMemBorrow();
    static UBSRMRSMemMigrateFunc UBSRMRSMemMigrate();
    static UBSRMRSMemReturnFunc UBSRMRSMemReturn();
    static UBSRMRSSetRunModeFunc UBSRMRSSetRunMode();
    static UBSRMRSPidNumaInfoCollectFunc UBSRMRSPidNumaInfoCollect();
    static UBSRMRSSetWaterMarkFunc UBSRMRSSetWaterMark();
    static UBSRMRSSmapAddProcessTrackingFunc UBSRMRSSmapAddProcessTracking();
    static UBSRMRSSmapRemoveProcessTrackingFunc UBSRMRSSmapRemoveProcessTracking();
    static UBSRMRSSmapEnableProcessMigrateFunc UBSRMRSSmapEnableProcessMigrate();

private:
    static void *libmempoolingHandler_;
    static UBSRMRSUpdateAntiNodeFunc ubsRMRSUpdateAntiNodeFunc_;
    static UBSRMRSMemBorrowStrategyFunc ubsRMRSMemBorrowStrategyFunc_;
    static UBSRMRSMemBorrowExecuteFunc ubsRMRSMemBorrowExecuteFunc_;
    static UBSRMRSMigrateStrategyFunc ubsRMRSMigrateStrategyFunc_;
    static UBSRMRSMigrateExecuteFunc ubsRMRSMigrateExecuteFunc_;
    static UBSRMRSMemFreeFunc ubsRMRSMemFreeFunc_;
    static UBSRMRSMemBorrowRollbackFunc ubsRMRSMemBorrowRollbackFunc_;
    static UBSRMRSGetVmInfoListOnNodeFunc ubsRMRSGetVmInfoListOnNodeFunc_;
    static UBSRMRSGetNumaInfoListOnNodeFunc ubsRMRSGetNumaInfoListOnNodeFunc_;
    static UBSRMRSMemBorrowFunc ubsRMRSMemBorrowFunc_;
    static UBSRMRSMemMigrateFunc ubsRMRSMemMigrateFunc_;
    static UBSRMRSMemReturnFunc ubsRMRSMemReturnFunc_;
    static UBSRMRSSetRunModeFunc ubsRMRSSetRunModeFunc_;
    static UBSRMRSPidNumaInfoCollectFunc ubsRMRSPidNumaInfoCollectFunc_;
    static UBSRMRSSetWaterMarkFunc ubsRMRSSetWaterMarkFunc_;
    static UBSRMRSSmapAddProcessTrackingFunc ubsRMRSSmapAddProcessTrackingFunc_;
    static UBSRMRSSmapRemoveProcessTrackingFunc ubsRMRSSmapRemoveProcessTrackingFunc_;
    static UBSRMRSSmapEnableProcessMigrateFunc ubsRMRSSmapEnableProcessMigrateFunc_;
};

} // namespace vm::mempooling

#endif // MEMPOOLING_MODULE_H
