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

#ifndef MEMPOOL_BORROW_MODULE_H
#define MEMPOOL_BORROW_MODULE_H

#include <securec.h>
#include <sys/types.h>
#include <ubse_com.h>
#include <ubse_def.h>
#include <ubse_logger.h>
#include <ubse_node.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>
#include "export_type.h"
#include "mem_manager.h"
#include "mempooling_message.h"
#include "mp_borrow_conf_util.h"
#include "mp_configuration.h"
#include "mp_error.h"
#include "mp_module.h"
#include "mp_vector_util.h"
#include "turbo_rmrs_interface.h"

namespace mempooling {
using namespace ubse::log;
using namespace ubse::nodeController;
using namespace ubse::com;
using namespace mempooling::message;

struct SrcMemoryBorrowParam {
    SrcMemoryBorrowParam() = default;

    std::string srcNid{};   // 借入方节点Id
    int16_t srcSocketId{};  // 借入方socket Id
    int16_t srcNumaId{};    // 借入方numa Id 当前限制为1
    uid_t uid{0};           // 借入方用户uid
    std::string username{}; // 借入方用户名

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "srcNid=" << srcNid << ",";
        oss << "srcSocketId=" << srcSocketId << ",";
        oss << "srcNumaId=" << srcNumaId;
        oss << "uid=" << uid << ",";
        oss << "username=" << username << ".";
        oss << "}";
        return oss.str();
    }
};

bool compareNodeMemInfo(const std::string nodeId, const std::pair<std::string, NodeMemInfo> &a,
                        const std::pair<std::string, NodeMemInfo> &b);
bool compareSocketMemInfo(const std::unordered_map<std::string, std::unordered_set<uint16_t>> &nodeIdToNumaIdSetMap,
                          const std::vector<RackMemNumaPair> &borrowedItemVec, const SrcMemoryBorrowParam &srcParam,
                          const std::pair<std::string, NodeMemInfo> &a, const std::pair<std::string, NodeMemInfo> &b);
uint32_t GeneratePerNodeNumaSocketMap(const std::vector<MemNodeData> &memNodeDataVec,
                                      std::map<std::string, std::map<int, uint16_t>> &numaSocketMap);

struct WaterMark {
    WaterMark() = default;

    uint16_t highWaterMark = 85;
    uint16_t lowWaterMark = 80;

    std::string ToString() const
    {
        std::ostringstream oss;
        return oss.str();
    }
};

struct DestMemoryBorrowParam {
    std::string destNid{};
    uint16_t destSocketId{};
    uint16_t destNumaNum{1}; // 当前限制为1
    std::vector<int> destNumaId{};
    std::vector<uint64_t> memSize{}; // 单位KByte

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"({"destNid"=")" << destNid << R"(",)";
        oss << R"("destSocketId"=)" << destSocketId << R"(,)";
        oss << R"("destNumaNum"=)" << destNumaNum << R"(,)";
        oss << R"("destNumaId"=)";
        for (auto &destNumaIdItem : destNumaId) {
            oss << destNumaIdItem << ",";
        }
        oss << R"(,)";
        oss << R"("memSize"=)";
        for (auto &memSizeItem : memSize) {
            oss << memSizeItem << ",";
        }
        oss << R"(,)";
        oss << R"(})";
        return oss.str();
    }
};

struct MemBorrowStrategyResult {
    SrcMemoryBorrowParam srcParam;
    uint64_t borrowSize;
    std::vector<DestMemoryBorrowParam> destParam;

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"({"srcParam"=)" << srcParam.ToString() << R"(,)";
        oss << R"("borrowSize"=)" << borrowSize << R"(,)";
        oss << R"("destParam"=)" << VectorUtil::VectorToStr(destParam) << R"(})";
        return oss.str();
    }
};

struct MemBorrowExecuteResult {
    std::vector<std::string> borrowIds;
    std::vector<uint16_t> presentNumaId;

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"({"borrowIds"=[)";
        for (size_t i = 0; i < borrowIds.size(); ++i) {
            oss << "\"" << borrowIds[i] << "\"";
            if (i + 1 < borrowIds.size()) {
                oss << ",";
            }
        }
        oss << R"(],"presentNumaId"=[)";
        for (size_t i = 0; i < presentNumaId.size(); ++i) {
            oss << presentNumaId[i];
            if (i + 1 < presentNumaId.size()) {
                oss << ",";
            }
        }
        oss << "]}";
        return oss.str();
    }
};

struct VMPresetParam {
    pid_t pid;      // vm对应pid
    uint16_t ratio; // 迁出最大比例
};

struct MemBorrowStrategyMultiResult {
    SrcMemoryBorrowParam srcParam;
    std::vector<uint64_t> borrowSizes;
    std::vector<DestMemoryBorrowParam> destParam;
    bool byNodeFault;
};

struct MigrateExecuteParam {
    std::vector<turbo::rmrs::VMMigrateOutParam> vmInfoList;
    uint64_t waitingTime;
    std::vector<std::string> borrowIdList;
};

struct MemNodeDataNew {
    std::string nodeId{};      // 节点名
    SocketData socket{};       // socket 数据
    std::string hostname{};    // 主机名
    bool isRegisterRm = false; // 该节点是否有可连接的 RM，非 OS 固定为 false
};

struct RemoteNumaSocketInfo {
    int16_t borrowRemoteNuma{-1}; // 借入numa, remote 借用时有效，否则为-1
    std::string lentNode{};       // 借出节点
    uint16_t lentSocketId{0};     // 借出内存socketId
};

struct MigrateStrategyParam {
    std::vector<VMPresetParam> vmInfoList;                                     // 虚拟机列表及最大迁出比例
    std::uint64_t borrowSize;                                                  // 需要匀出本地内存大小
    std::vector<RemoteNumaSocketInfo> remoteNumaInfo;                          // 提炼内存账本信息
    std::unordered_map<std::string, std::vector<MemNodeDataNew>> nodeTopology; // 获取全量拓扑信息
    std::string nodeId;                                                        // 从节点Id
    std::vector<uint16_t> timeOutNumas;                                        // 归还超时的远端numa
};

struct ConvertVmParam {
    pid_t pid;
    uint16_t localNumaId; // 虚拟机本地numa
    std::string srcNid;   // 当前节点id
};

struct NumaHugePageInfo {
    uint16_t numaId{};
    uint64_t hugePageTotal{};
    uint64_t hugePageFree{};
    bool isLocal;
    bool isRemote;
};

struct VmNumaInfoBrr {
    pid_t pid;
    uint16_t localNumaId;
    uint16_t remoteNumaId;
    uint64_t localUsedMem;
    uint64_t remoteUsedMem;
};

struct NumaQueryInfo {
    std::vector<mempooling::exportV2::NumaInfo> numaInfos; // 当前节点 NUMA 信息
    std::vector<uint16_t> remoteNumaIdList;                // 远端 NUMA 节点 ID 列表
    std::map<uint16_t, NumaHugePageInfo> numaInfoMap;      // 远端 NUMA 节点 ID -> 对应大页信息
    std::vector<NumaHugePageInfo> numaHugePageInfoSumList; // 所有远端节点大页统计信息
};

struct VMQueryInfo {
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    std::vector<VmNumaInfoBrr> allVmNumaInfoInfoList;
    std::map<pid_t, VmNumaInfoBrr> vmNumaInfoMap;
    std::map<pid_t, uint64_t> vmMigratableMemMap;
};

class MempoolBorrowModule {
public:
    static MempoolBorrowModule &Instance()
    {
        static MempoolBorrowModule instance;
        return instance;
    }

    static MpResult Init();

    MpResult MemBorrowStrategy(const SrcMemoryBorrowParam &srcParam, const uint64_t borrowSize,
                               MemBorrowStrategyResult &borrowStrategyResult);
    MpResult MemBorrowStrategyMultiple(const SrcMemoryBorrowParam &srcParam, const std::vector<uint64_t> &borrowSizes,
                                       std::string &destPreNid, MemBorrowStrategyMultiResult &borrowStrategyResult);
    MpResult MemBorrowStrategyMultipleUB(const SrcMemoryBorrowParam &srcParam, const std::vector<uint64_t> &borrowSizes,
                                         std::string &destPreNid, uint16_t socketId,
                                         MemBorrowStrategyMultiResult &borrowStrategyResult);
    MpResult ValidateBorrowExecuteParam(const DestMemoryBorrowParam &destParam, MemMallocAttr &memAttr,
                                        const MemBorrowExecuteResult &borrowExecuteResult, uint64_t &totalSize);
    MpResult ValidateBorrowParamSamePlane(const SrcMemoryBorrowParam &srcParam,
                                          const std::vector<DestMemoryBorrowParam> &destParams);
    MpResult ValidateDestNids(const SrcMemoryBorrowParam &srcParam,
                              const std::vector<DestMemoryBorrowParam> &destParams);
    MpResult MemBorrowFailedRollback(const MemBorrowExecuteResult &borrowExecuteResult);
    MpResult MemBorrowExecute(const SrcMemoryBorrowParam &srcParam, const std::vector<DestMemoryBorrowParam> &destParam,
                              MemBorrowExecuteResult &borrowExecuteResult);
    MpResult ExecuteSingleBorrow(const DestMemoryBorrowParam &destParam, const SrcMemoryBorrowParam &srcParam,
                                 MemBorrowExecuteResult &borrowExecuteResult);
    static MpResult MemBorrowExecuteInOverCommit(const SrcMemoryBorrowParam &srcParam,
                                                 const std::vector<uint64_t> &borrowSizes, const WaterMark &waterMark,
                                                 MemBorrowExecuteResult &borrowExecuteResult,
                                                 const bool isFault = false);
    static MpResult ProcessSingleBorrowInOverCommit(const SrcMemoryBorrowParam &srcParam,
                                                    const UbseMemNumaCandidateOpt &opt, const bool &isFault,
                                                    UbseMemNumaDesc &desc);
    MpResult MemFree(std::string nodeId);

    MpResult SafeUint64To32(uint32_t &targetNum, uint64_t tmp);

private:
    MpResult ValidateBorrowSize(const uint64_t borrowSize);
    MpResult ValidateSrcparam(const SrcMemoryBorrowParam &srcParam);
    MpResult GetMemoryInfo(std::unordered_map<std::string, NodeMemInfo> &nodeMemMap,
                           const SrcMemoryBorrowParam &srcParam, std::vector<std::string> &antiNodeMemVec);
    void FilterAndSortNodes(std::unordered_map<std::string, NodeMemInfo> &nodeMemMap, std::string srcNid,
                            const std::vector<std::string> &antiNodeMemVec,
                            std::vector<std::pair<std::string, NodeMemInfo>> &nodeVec);
    void FilterNodesBySocketProximity(
        std::unordered_map<std::string, NodeMemInfo> &nodeMemMap, const std::vector<MemNodeData> &foundNodeData,
        std::unordered_map<std::string, std::unordered_set<uint16_t>> &nodeIdToNumaIdSetMap);
    MpResult FilterBorrowableNodes(const SrcMemoryBorrowParam &srcParam, const std::vector<std::string> &antiNodeMemVec,
                                   std::unordered_map<std::string, NodeMemInfo> &nodeMemMap);
    MpResult FilterAndSortSockets(std::unordered_map<std::string, NodeMemInfo> &nodeMemMap,
                                  const SrcMemoryBorrowParam &srcParam, const std::vector<std::string> &antiNodeMemVec,
                                  std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology,
                                  std::vector<std::pair<std::string, NodeMemInfo>> &nodeVec);
    MpResult ProcessNodeMemBorrow(const std::pair<std::string, NodeMemInfo> &node, uint32_t &needBorrowNum,
                                  MemBorrowStrategyResult &borrowStrategyResult);
    MpResult GetSocket2CurMemSizeMap(const std::string &nodeId, std::map<int, uint64_t> &socket2CurLeftMemSizeMap);
    MpResult GetSocketInfo(DestMemoryBorrowParam &tempParam, uint32_t numaId);
    void AddMemoryParamsToResult(uint32_t haveFourGbNum, uint32_t moreBorrowNum, DestMemoryBorrowParam &tempParam,
                                 MemBorrowStrategyResult &borrowStrategyResult);
    void UpdateNodeMemInfoWithNuma(std::unordered_map<std::string, NodeMemInfo> &nodeMemMap);
    MpResult GetNodeInfoByNodeId(const std::string &nodeId, UbseNodeInfo &nodeInfo);
    MpResult GetAndSetBlockSize(const std::string &nodeId);

    uint32_t gBlockSize{128}; // 芯片表项内存拆分粒度大小，单位M

private:
    MempoolBorrowModule() = default;
    ~MempoolBorrowModule() = default;
    MempoolBorrowModule(const MempoolBorrowModule &) = delete;
    MempoolBorrowModule &operator=(const MempoolBorrowModule &) = delete;

    MpResult MemBackExecute(std::string nodeId, uint16_t numaId);
};

uint32_t MigrateStrategyRecvHandler(const turbo::rmrs::MigrateStrategyParam &migrateStrategyParam,
                                    turbo::rmrs::MigrateStrategyResult &migrateStrategyResult);
void DistributeNumaMemInfo(std::vector<mempooling::exportV2::NumaInfo> &numaInfos,
                           std::map<uint16_t, NumaHugePageInfo> &numaInfoMap,
                           std::vector<NumaHugePageInfo> &numaHugePageInfoSumList);
void GetRemoteNumaList(std::vector<NumaHugePageInfo> &numaHugePageInfoSumList, std::vector<uint16_t> &remoteNumaIdList);
void GetLocalVmInfo(std::vector<VmNumaInfoBrr> &allVmNumaInfoInfoList, std::map<pid_t, VmNumaInfoBrr> &VmNumaInfoMap,
                    std::vector<mempooling::exportV2::VmDomainInfo> &vmDomainInfos);
uint32_t GetNumaData(std::vector<mempooling::exportV2::NumaInfo> &numaInfos, std::vector<uint16_t> &remoteNumaIdList,
                     std::map<uint16_t, NumaHugePageInfo> &numaInfoMap,
                     std::vector<NumaHugePageInfo> &numaHugePageInfoSumList);
uint32_t GetVMData(std::vector<mempooling::exportV2::VmDomainInfo> &vmDomainInfos,
                   std::vector<VmNumaInfoBrr> &allVmNumaInfoInfoList, std::map<pid_t, VmNumaInfoBrr> &vmNumaInfoMap);
bool IsSamePlaneBorrow(ConvertVmParam vmParam, uint16_t remoteNumaId,
                       const std::vector<mempooling::exportV2::NumaInfo> &numaInfos,
                       const std::vector<turbo::rmrs::RemoteNumaSocketInfo> &remoteNumaSocketInfo,
                       const std::unordered_map<std::string, std::vector<turbo::rmrs::MemNodeDataNew>> &nodeTopology);
uint32_t ConvertMigrateStrategyParam(const turbo::rmrs::MigrateStrategyParam &migrateStrategyParam,
                                     turbo::rmrs::MigrateStrategyParamRMRS &migrateStrategyParamRMRS);
} // namespace mempooling

#endif // MEMPOOL_BORROW_MODULE_H
