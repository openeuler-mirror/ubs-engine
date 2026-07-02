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

#ifndef VM_SMAP_HELPER_H
#define VM_SMAP_HELPER_H

#include <chrono>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <csignal>
#include "ubse_file_util.h"
#include "mp_configuration.h"
#include "mp_error.h"
#include "mp_module.h"
#include "mp_smap_module.h"
#include "over_commit_def.h"

namespace mempooling::smap {
using std::mutex;

class MpSmapHelper {
public:
    static MpSmapHelper& GetInstance();
    static const size_t setSmapRemoteNumaInfoMaxRetryCount;
    static const uint32_t setSmapRemoteNumaInfoMaxRetryInterval; // 单位s

    static MpResult Init();

    static void VmSmapClose();

    MpResult GetHugePageCanonicalPath(const std::string& remoteNumaId, std::string& filePath);

    static MpResult GetOriginalHugePages(const std::string& filePath, uint64_t& originalHugePages);

    static MpResult RewriteHugePages(const std::string& realPath, uint64_t targetHugePages);

    MpResult AllocateHugePages(std::vector<uint64_t>& remoteNumaIds, std::vector<uint64_t>& borrowSizes);

    MpResult ReleaseHugePages(std::vector<uint64_t>& remoteNumaIds, std::vector<uint64_t>& borrowSizes);

    void RollBackHugePagesIfNeeded(bool hugePageAllocated, std::vector<uint64_t>& remoteNumaIds,
                                   std::vector<uint64_t>& borrowSizes);

    MpResult ReleaseHugePagesWithRetry(uint64_t numaId, uint64_t borrowSize);

    MpResult TryAllocateHugePagesOnce(const std::string& filePath, uint64_t targetHugePages);

    MpResult AllocateHugePagesWithRetry(uint64_t numaId, uint64_t borrowSize);

    static int QueryVMFreqArray(int pidIn, uint16_t* dataIn, uint32_t lengthIn, uint32_t& lengthOut, int dataSource);

    static MpResult SmapMode(int runMode);

    static MpResult SmapMigrateRemoteNuma(MigrateNumaMsg msg);

    static MpResult SmapMigratePidRemoteNumaHelper(pid_t* pidArr, int len, int srcNid, int destNid);

    static MpResult SmapMigratePidMultiRemoteNumaHelper(MigrateEscapeMsg& msg);

    static MpResult SmapMigratePidMultiRemoteNumaHelperWithRetry(MigrateEscapeMsg& msg);

    static int SmapEnableProcessMigrateHelper(pid_t* pidArr, int len, int enable, int flags);

    static MpResult SetSmapRemoteNumaInfo(const int16_t& srcNumaId,
                                          const std::vector<over_commit::MemBorrowInfoWithSrc>& memBorrowInfoWithSrcs);

    static MigrateOutMsg GetMigrateOutMsgInOverCommit(
        const std::vector<over_commit::MemMigrateResult>& memMigrateResults, const uint16_t ratio);

    static MigrateOutMsg GetMigrateOutMsgInOverCommitMultiNuma(
        const std::vector<over_commit::MemMigrateResult>& memMigrateResults, const uint16_t ratio);

    static MpResult MigrateOutInOverCommit(const std::vector<over_commit::MemMigrateResult>& memMigrateResults,
                                           uint16_t ratio = SMAP_RATIO_MP);

    static MpResult ReadAndSetRunMode();

    static MpResult SetRunModeAndWrite(int runMode);

    static int SmapAddProcessTrackingHelper(const std::vector<pid_t>& pidVec, const std::vector<uint32_t>& scanTimeVec,
                                            int scanType, const std::vector<uint32_t>& durationVec);

    static int SmapRemoveProcessTrackingHelper(const std::vector<pid_t>& pidVec, int flags);

    static MpResult SmapMigrateBack(MigrateBackMsg& migrateBackMsg);

    static MpResult SmapMigrateBackSync(MigrateBackMsg& migrateBackMsg);

    static MpResult SmapEnableNuma(EnableNodeMsg& enableMsg);

    static MpResult GetLocalSmapBackResult(uint64_t taskId);

    static MpResult SmapGetBackResult(uint64_t taskId, uint16_t& ret);

    static MpResult SmapQueryProcessConfigHelper(int nid, std::vector<ProcessPayload>& processPayloadList);
    static MpResult GetVmRatioOnFaultNumaBySmap(const int16_t faultNumaId,
                                                std::unordered_map<pid_t, smap::ProcessPayload>& processPayloadMap);

    static void FilterValidPidsByLocalNode(std::vector<pid_t>& pidList);

    static void FilterValidPidsRpc(const std::string srcNid, std::vector<pid_t>& pidList);

    static void RollBackSmapEnablePids(std::vector<pid_t>& pids);

    static MpResult SmapQueryProcessAndFilter(int nid, std::vector<pid_t>& pidList);

    static MpResult SmapRemovePidsHelper(const std::vector<pid_t>& pids, int16_t remoteNumaId);

    static const int smapParamErrorCode;    // SMAP参数错误码 Invalid argument -22
    static const int smapDealErrorCode;     // SMAP处理异常错误码 -9
    static const int smapApplyMemErrorCode; // SMAP处理异常错误码 -12
    static const int smapTimeDoutErrorCode; // SMAP处理异常错误码 Connection timed out -110
    static const int smapPermErrorCode;     // SMAP处理异常错误码 Operation not permitted -1
    static const int smapIOErrorCode;       // SMAP处理异常错误码 I/O error -5
    static const int smapRangeErrorCode;    // SMAP处理异常错误码 Math result not representable -34
    static const int smapBadFNErrorCode;    // SMAP处理异常错误码 Bad file number -9

    static const int smapMigrateBackDefaultDestNid; // smapMigrateBack destNid 默认参数设置为 -1
    static const int enableModeDisableNumaMig;      // 关闭远端nume冷热流动标识符 0
    static const int enableModeEnableNumaMig;       // 开启远端numa冷热流动标识符 1
    static const int paStartEndRangeSize;           // smapMigrateBack paStartEnd 包含数组大小 2
    static const int SMAP_QUERY_PID_NUM;
    static const int SMAP_PARTIAL_SUCCESS; // smapMigrateOut 部分迁出成功
private:
    const std::string HUGEPAGES_PATH_HEAD = "/sys/devices/system/node/node";
    const std::string HUGEPAGES_PATH_TAIL = "/hugepages/hugepages-2048kB/nr_hugepages";
    const std::string FD_SMAP_VM_DEVICE = "/dev/smap_vm_device";
    std::unordered_map<uint64_t, std::unique_ptr<std::mutex>> numaAllocMutexMap;
    std::mutex mapMutex;
};

} // namespace mempooling::smap

#endif // VM_SMAP_HELPER_H
