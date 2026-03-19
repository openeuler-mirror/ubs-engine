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

#ifndef MEMPOOL_MIGRATE_MODULE_H
#define MEMPOOL_MIGRATE_MODULE_H

#include <sys/types.h>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "mempool_borrow_module.h"
#include "mp_error.h"
#include "turbo_rmrs_interface.h"
#include "exporter.h"

namespace mempooling {
namespace migrate {

using namespace turbo::rmrs;

// smap内存迁移完成大页目标态观测浮动值
const std::uint64_t SMAP_MIGRATE_OUT_FINISH_FLOAT_RANGE = 50;
// 观测内存迁出目标态间隔 单位毫秒
const std::uint64_t OBSERVE_MIGRATE_OUT_SLEEP_TIME = 1000 * 1000;
// 迁回内存超时时间 单位毫秒  smap迁移接口限制必须大于10秒 此处为满足调用的最小传参需求值
const std::uint64_t MIGRATEOUT_TIMEOUT = 50 * 1000 + 1;

struct ReturnNumaInfo {
    uint16_t numaId{};
    uint64_t memFree{};
    uint64_t memUse{};
    uint64_t hugePageFree{};
    uint64_t hugePageUse{};
    bool isLocal;
};

struct NumaHugePageInfo {
    uint16_t numaId{};
    uint64_t hugePageTotal{};
    uint64_t hugePageFree{};
    bool isLocal;
    bool isRemote;
};

struct ReturnVmInfo {
    pid_t pid;
    uint16_t remoteNumaId;
};

struct VmNumaInfo {
    pid_t pid;
    uint16_t localNumaId;
    uint16_t remoteNumaId;
    uint64_t localUsedMem;
    uint64_t remoteUsedMem;
};

struct RollBackBorrowIdPid {
    std::vector<std::string> borrowIdList;
    std::vector<BorrowIdInfo> pidList;
};

struct RollBackForOutEntry {
    std::set<std::string> validBorrowIdSet;
    std::map<std::string, std::set<BorrowIdInfo>> curBorrowIdsPidsMap;
    MpResult errorCode = 0;  // 默认 0 表示无错误
};

struct MemBorrowRollbackParam {
    std::string nodeId;
    std::vector<std::string> borrowIdList;
    RollBackBorrowIdPid entry;
};

void ConvertNodeTopology(const std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology,
                         std::unordered_map<std::string, std::vector<turbo::rmrs::MemNodeDataNew>> &nodeTopologyNew);
uint32_t GetVmInfoImmediatelyRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);
void GetVmInfoImmediatelyResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode);

class MempoolMigrateModule {
public:
    static MpResult Init();
    static bool FillDestNumaFreeHugePageMap(std::map<uint16_t, uint64_t> &destNumaFreeHugePageMap,
        std::map<uint16_t, std::vector<turbo::rmrs::VMMigrateOutParam>> vmMigrateOutParamGroupByNumaIdMap);
    static bool GetVmInfoMap(std::map<pid_t, mempooling::exportV2::VmDomainInfo> &vmInfoMap,
                             std::vector<pid_t> pidList);
    static bool ValidateRemoteFreeSpace(const std::vector<VMMigrateOutParam> &vmMigrateOutParam);

    static bool FreeMemAndPersistent(std::set<std::string> &validBorrowIdSet,
        std::map<std::string, std::set<BorrowIdInfo>> &curBorrowIdsPidsMap,
        RollBackBorrowIdPid &outEntry);

    static MpResult ValidateSamePlane(const VMMigrateOutParam &perVmParam,
                                      const std::vector<mempooling::exportV2::VmDomainInfo> &vmDomainInfos,
                                      const std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology,
                                      const std::string &curNodeId);
    static MpResult ValidateAllPidSamePlane(const std::vector<VMMigrateOutParam> &vmMigrateOutParam);
    static MpResult MigrateStrategy(const std::string &borrowInNode,
                                    const std::vector<turbo::rmrs::VMPresetParam> &vmInfoList, std::uint64_t borrowSize,
                                    MigrateStrategyResult &migrateStrategyResult);
    static MpResult GetReturningNuma(const std::string &borrowInNode, std::vector<uint16_t> &returningNumas);

    static MpResult GetExpectRemoteNumaMem(int nid, int &ExpectMigratedPages);

    static std::vector<uint64_t> numaBorrowSize;    // 单位MB
    static const uint64_t memoryPageSize;       // 2048
    static const uint64_t memoryThreshold; // 10240kb;
    static const uint64_t memoryPreset; //  100
    static const uint64_t smapMigrateOutWaitingTimeMin;         // 内存迁出最小等待时间
    static const uint64_t smapMigrateOutWaitingTimeMax;         // 内存迁出最大等待时间
    static const double ratioDiff;                              // 迁移比例允许取整差值 0.01
    static const int hugepagetype;                              // 2M大页类型
};

class MempoolMigrateExecute {
public:
    static MpResult MigrateExecuteImpl(const std::vector<VMMigrateOutParam>& vmMigrateOutParam,
        const uint64_t waitingTime, const std::vector<std::string> &borrowIdList);
    static MpResult MigrateExecute(const std::string &borrowInNode, const std::vector<VMMigrateOutParam> &vmInfoList,
        uint64_t waitingTime, const std::vector<std::string> &borrowIdListIn);
    static MpResult MigrateExecuteRpc(const std::string &borrowInNode, const std::vector<VMMigrateOutParam> &vmInfoList,
        uint64_t waitingTime, const std::vector<std::string> &borrowIdList);
    static MpResult RpcGetVmInfoImmediately(const std::string &nodeId,
                                            std::vector<mempooling::exportV2::VmDomainInfo> &vmDomainInfos);
    static MpResult MemBorrowRollbackImpl(std::vector<std::string> &borrowIds,
        const RollBackBorrowIdPid &entryList, RollBackForOutEntry &rollBackForOutEntry);
    static MpResult GetMigrateExecuteInfo(const std::string &borrowInNode,
        const std::vector<VMMigrateOutParam> &vmInfoList, std::set<BorrowIdInfo> &pidList,
        std::unordered_set<uint16_t> &uniqueDesNumaIds);
    static MpResult ValidMigrateExecuteParam(const std::string &borrowInNode,
        const std::unordered_set<uint16_t> &uniqueDesNumaIdList);
};

} // namespace migrate
} // namespace mempooling

#endif // MEMPOOL_MIGRATE_MODULE_H