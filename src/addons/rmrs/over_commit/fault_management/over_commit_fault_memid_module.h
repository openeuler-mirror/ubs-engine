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

#ifndef MEMPOOLING_OVER_COMMIT_FAULT_MEMID_MODULE_H
#define MEMPOOLING_OVER_COMMIT_FAULT_MEMID_MODULE_H

#include <string>
#include <unordered_map>
#include "fault_memid_module.h"
#include "mempooling_interface.h"
#include "over_commit_def.h"
#include "over_commit_storage.h"
namespace mempooling {
using namespace mempooling::over_commit;
struct VmNumaInfoWithSocket {
    pid_t pid;
    uint16_t localNumaId;
    uint16_t remoteNumaId;
    uint64_t localUsedMem;
    uint64_t remoteUsedMem;
    uint64_t localFreeMem;
    // 这里会根据传来的值是否为 -1 进行有效判断
    int16_t socketId;
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "pid: " + std::to_string(pid) + ", localNumaId: " + std::to_string(localNumaId) +
                   ", remoteNumaId: " + std::to_string(remoteNumaId) +
                   ", localUsedMem: " + std::to_string(localUsedMem) +
                   ", remoteUsedMem: " + std::to_string(remoteUsedMem) + ", socketId: " + std::to_string(socketId);
        oss << "}";
        return oss.str();
    }
};

// 获取VmInfo，传入参数，序列化和反序列化
struct OverCommitFaultVmNumaInfoParam {
    uint16_t remoteNumaId;
};

// 获取VmInfo，结果，序列化和反序列化
struct OverCommitFaultVmNumaInfoResult {
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
};
struct OverCommitFaultMemIdExecuteParam {
    std::vector<pid_t> pids;
    int16_t localNumaId;
    uint16_t remoteNumaId;
    uint16_t preRemoteNumaId;
    uint64_t borrowSize;
    uint64_t remoteNumaTotalSize;
    uint64_t preRemoteNumaTotalSize;
    bool isDiffRemoteNuma;

    std::string ToString() const
    {
        std::ostringstream oss;
        std::string pidsStr;
        for (auto& pid : pids) {
            pidsStr += std::to_string(pid) + ", ";
        }
        oss << "{";
        oss << "pid: [" + pidsStr + "], localNumaId: " + std::to_string(localNumaId) +
                   ", remoteNumaId: " + std::to_string(remoteNumaId) +
                   ", preRemoteNumaId: " + std::to_string(preRemoteNumaId) +
                   ", borrowSize: " + std::to_string(borrowSize) +
                   ", remoteNumaTotalSize: " + std::to_string(remoteNumaTotalSize) +
                   ", preRemoteNumaTotalSize: " + std::to_string(preRemoteNumaTotalSize) +
                   ", isDiffRemoteNuma: " + std::to_string(isDiffRemoteNuma);
        oss << "}";
        return oss.str();
    }
};

struct GetNumaSizePara {
    std::string borrowInNid;
    int16_t srcNumaId;
    uint16_t remoteNumaId;
    uint16_t preRemoteNumaId;
};

MpResult GetLocalBorrowNumaIdOfMemId(const std::string& localNodeId, int16_t& localNumaId, uint16_t memId);
uint64_t GetLocalNumaOnRemoteNumaBorrowSize(const std::string& localNodeId, uint16_t localNumaId, uint16_t remoteNumaId,
                                            NumaBindType bindType);
MpResult GetRemoteNumaSize(uint64_t& remoteNumaTotalSize, GetNumaSizePara param, NumaBindType bindType);
MpResult GetPreRemoteNumaSize(uint64_t& preRemoteTotalSize, GetNumaSizePara param, NumaBindType bindType);
void ShowPids(const FMVmInfoResult& fMVmInfoResult, const uint64_t faultMemSize);
void GetBothVmNumaInfoMultiScene(std::vector<VmNumaInfoWithSocket>& allVmNumaInfoOnRemoteNuma,
                                 std::vector<VmNumaInfo>& numaInfoOnBoth);
MpResult GetContainerNumaInfoList(outinterface::SrcMemoryBorrowParam oParam,
                                  std::vector<VmNumaInfoWithSocket>& vmNumaInfoWithSocketList, uint16_t remoteNumaId,
                                  NumaBindType bindType);
MpResult MemBorrowExecute(SrcMemoryBorrowParam srcParam, uint64_t borrowSize, WaterMark water,
                          MemBorrowExecuteResult& borrowExecuteResult);
MpResult SetSmapRemoteNumaInfoExec(int16_t localNumaId, uint16_t remoteNumaId, uint64_t borrowSize);

class OverCommitFaultMemIdModule {
public:
    static OverCommitFaultMemIdModule& Instance()
    {
        static OverCommitFaultMemIdModule instance;
        return instance;
    }

    MpResult MemIdFaultManage(std::string borrowInNid, uint64_t memId);
    MpResult GetVmNumaInfoMapRpc(std::string importNodeId, std::vector<VmNumaInfoWithSocket>& vmNumaInfoWithSocketList,
                                 uint16_t remoteNumaId);
    void GetBothVmNumaInfo(std::vector<VmNumaInfoWithSocket>& allVmNumaInfoOnRemoteNuma, uint16_t localNumaId,
                           std::vector<VmNumaInfo>& numaInfoOnBoth, int16_t& srcSocketId);
    MpResult GetRemoteNumaVms(uint16_t remoteNumaId, std::vector<VmNumaInfoWithSocket>& vmNumaInfoWithSocketList);

    MpResult MemFreeExecuteRpc(std::string borrowId, std::string importNodeId);
    MpResult DisableSmapProcessMigrateRpc(std::vector<pid_t> pids, std::string importNodeId);
    MpResult MemFreeDirectlyExecuteRpc(std::string borrowId, std::string importNodeId);
    MpResult MemIdExecuteRpc(OverCommitFaultMemIdExecuteParam param, std::string importNodeId);
    MpResult MemIdExecute(OverCommitFaultMemIdExecuteParam param);
    MpResult VmsMigrateOtherRemoteNuma(std::vector<pid_t>& pids, uint16_t preRemoteNumaId, uint16_t remoteNumaId,
                                       int16_t localNumaId, uint64_t remoteNumaTotalSize);
    MpResult SetAndDeleteResource(std::string borrowId, outinterface::SrcMemoryBorrowParam srcParam,
                                  std::vector<MemBorrowInfo> memBorrowInfos, uint64_t faultMemSize);
    MpResult ReturnFaultMem(outinterface::SrcMemoryBorrowParam outSrcParam, std::string borrowId,
                            uint16_t preRemoteNumaId, uint64_t faultMemSize, uint64_t remoteNumaSize);
    MpResult PrepareParamForBorrowMem(outinterface::SrcMemoryBorrowParam& param, uint16_t memId,
                                      uint16_t preRemoteNumaId, std::vector<VmNumaInfo>& allVmNumaInfoOnBoth,
                                      mempooling::WaterMark& waterMark);
    MpResult GetSelectPids(FMVmInfoResult& fMVmInfoResult, uint64_t faultMemSize,
                           std::vector<VmNumaInfo>& allVmNumaInfoOnBoth);
    MpResult ExecEmpty(outinterface::SrcMemoryBorrowParam outSrcParam, std::string borrowId, uint16_t preRemoteNumaId,
                       uint64_t faultMemSize);
    MpResult GetOverCommitScene(const std::string& nodeId);
    MpResult GetPidNumaInfo(outinterface::SrcMemoryBorrowParam oParam,
                            std::vector<VmNumaInfoWithSocket>& vmNumaInfoWithSocketList, uint16_t remoteNumaId);

    void ClearFalutBidBorrowedMap()
    {
        falutBidBorrowedMap.clear();
    }

    MpResult GetWaterMark(struct WaterMark& waterMark);
    /**
     * @brief 判断memID是否在本Node上、输出borrowID对应的远端numaID、输出memId对应的borrowID的内存借用大小
     * @param borrowInNodeData 借用内存块信息
     * @param faultMemSize 故障memID所在borrowId的借用大小
     * @param preRemoteNumaId 故障memID所在borrowId的远端NUMA
     * @return MpResult
     */
    MpResult IsBorrowIdOfCurNidOverCommit(BorrowInNodeData& borrowInNodeData, uint64_t& faultMemSize,
                                          uint16_t& preRemoteNumaId, uid_t& uid, std::string& username);

private:
    std::unordered_map<std::string, MemBorrowExecuteResult> falutBidBorrowedMap;
    FaultMemIdModule& baseInstance = FaultMemIdModule::Instance();
    MpSceneType mSceneType{MpSceneType::VIRTUAL_SCENE};
    NumaBindType mBindType{NumaBindType::BIND_INVALID};
};
} // namespace mempooling
#endif // MEMPOOLING_OVER_COMMIT_FAULT_MEMID_MODULE_H
