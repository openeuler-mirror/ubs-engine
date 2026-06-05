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

#ifndef MEM_MANAGER_H
#define MEM_MANAGER_H

#include "mem_manager_base.h"

namespace mempooling {
using namespace mempooling::sync;
using namespace ubse::mem::controller;
using namespace mempooling::message;

void PrintNumaSocketMap(const std::map<std::string, std::map<int, uint16_t>>& numaSocketMap);
bool GetNumaSocket(const std::map<std::string, std::map<int, uint16_t>>& numaSocketMap, const std::string& nodeId,
                   int numaId, uint16_t& socketId);
//  内存标志符最大长度
const std::uint64_t MEM_OBMM_MAX_HCCS_MEM_CNT = 32;

struct LentNuma {
    LentNuma() = default;

    uint16_t numaId{};   //  借用numa
    uint64_t lentSize{}; //  借用内存大小，单位kB

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "numa_Id=" << numaId << ",";
        oss << "lent_Size=" << lentSize;
        oss << "}";
        return oss.str();
    }
};

struct BorrowRecord {
    BorrowRecord() = default;

    std::string name{};                  //  借用标识符
    uint64_t size{0};                    //  借用内存大小，单位kB
    std::string lentNode{};              //  借出节点
    std::vector<uint64_t> lentMemId{};   //  借出内存id, 无法自采
    uint16_t lentSocketId{0};            //  借出内存socketId
    std::vector<LentNuma> lentNuma{};    //  借出numa
    std::string borrowNode{};            //  借入节点
    int16_t borrowLocalNuma{0};          //  借入numa, app 借用时有效，否则为-1
    int16_t borrowRemoteNuma{-1};        //  借入numa, remote 借用时有效，否则为-1
    std::vector<uint64_t> borrowMemId{}; //  借入memId
    uid_t uid{0};               // 发起借用方运行用户的uid，后续资源管理权限都由此用户管理
    std::string username{};     // 发起借用方运行用户的名称，后续资源管理权限都由此用户管理
    uint16_t borrowSocketId{0}; //  借入内存socketId

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "name=" << name << ",";
        oss << "size=" << size << ",";
        oss << "uid=" << uid << ",";
        oss << "username=" << username << ",";
        oss << "lentNode=" << lentNode << ", lent_Mem_Id=";
        for (const auto& memid : lentMemId) {
            oss << memid << ",";
        }
        oss << "lent_Socket_Id=" << lentSocketId << ",";
        oss << "borrow_Socket_Id=" << borrowSocketId << ",";
        oss << "lent_Numa=";
        for (const auto& numa : lentNuma) {
            oss << numa.ToString() << ",";
        }
        oss << "borrow_Node=" << borrowNode << ",";
        oss << "borrow_Local_Numa=" << borrowLocalNuma << ",";
        oss << "borrow_Remote_Numa=" << borrowRemoteNuma << ", borrow_Mem_Id=";
        for (const auto& memid : borrowMemId) {
            oss << memid << ",";
        }
        oss << "}";
        return oss.str();
    }
};

struct NumaLevelBorrowedDecision {
    uint16_t presentNumaId;
    uint16_t oldNumaId;
    std::vector<pid_t> pids;
    uint64_t totalBorrowSize;
    std::map<std::string, std::string> borrowResultMap; // oldName->newName

    std::string ToString() const
    {
        std::ostringstream oss;
        std::string pidStr{};
        for (auto pid : pids) {
            pidStr += std::to_string(pid) + ",";
        }

        std::string borrowResultMapStr{};
        for (auto [oldName, newName] : borrowResultMap) {
            borrowResultMapStr += "[oldName=" + oldName + ", newName=" + newName + "] ";
        }

        oss << "NumaLevelBorrowedDecision{"
            << "oldNumaId=" << oldNumaId << ", presentNumaId=" << presentNumaId << ", pids=[" << pidStr << "]"
            << ", totalBorrowSize=" << totalBorrowSize << "KB"
            << ", borrowResultMap=" << borrowResultMapStr << "}";
        return oss.str();
    }
};

struct BorrowIdLevelBorrowedDecision {
    uint16_t presentNumaId;
    uint16_t oldNumaId;
    std::vector<pid_t> pids;
    uint64_t borrowSize;
    std::string oldName;
    std::string newName;

    std::string ToString() const
    {
        std::ostringstream oss;
        std::string pidStr{};
        for (auto pid : pids) {
            pidStr += std::to_string(pid) + ",";
        }
        oss << "BorrowIdLevelBorrowedDecision{"
            << "oldName=" << oldName << ", newName=" << newName << ", oldNumaId=" << oldNumaId
            << ", presentNumaId=" << presentNumaId << ", pids=[" << pidStr << "]"
            << ", borrowSize=" << borrowSize << "KB"
            << "}";
        return oss.str();
    }
};

// 已经执行了借用，但是迁移失败的决策
struct BorrowedDecision {
    std::string borrowNodeId;                         // 该故障numa对应的nodeId
    uint16_t remoteNumaId;                            // 该故障numa对应的远端numaId
    bool isNumaLevel{false};                          // true为NUMA级别决策, false则为borrowId级别决策
    NumaLevelBorrowedDecision numaBorrowedDecision{}; // NUMA级别决策结果
    std::vector<BorrowIdLevelBorrowedDecision> borrowIdBorrowedDecisions{};

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "BorrowedDecision{"
            << "borrowNodeId=" << borrowNodeId << ", remoteNumaId=" << remoteNumaId << ", isNumaLevel=" << isNumaLevel;
        if (isNumaLevel) {
            oss << ", " << numaBorrowedDecision.ToString();
        } else {
            oss << ", borrowIdDecisions=[";
            for (size_t i = 0; i < borrowIdBorrowedDecisions.size(); ++i) {
                if (i != 0)
                    oss << ", ";
                oss << borrowIdBorrowedDecisions[i].ToString();
            }
            oss << "]";
        }
        oss << "}";
        return oss.str();
    }
};

struct NumaMemInfo {
    uint16_t numaId{0};
    uint16_t socketId{0};
    uint64_t reservedMem{0};
    uint64_t lentMem{0};
    uint64_t borrowableMem{0};
    uint64_t borrowedMem{0};
    uint64_t memFree{0};   // 空闲内存大小，单位kB
    uint64_t vmMemFree{0}; // freeHugepages，单位kB

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "NumaId: " << numaId << ", socketId: " << socketId << ", ReservedMem: " << reservedMem
            << ", LentMem: " << lentMem << ", BorrowableMem: " << borrowableMem << ", BorrowedMem: " << borrowedMem
            << ", MemFree: " << memFree << ", VmMemFree: " << vmMemFree;
        return oss.str();
    }
};

struct NodeMemInfo {
    time_t timestamp{0};
    uint64_t totalReservedMem{0};
    uint64_t totalBorrowableMem{0};
    uint64_t totalLentMem{0};
    uint64_t totalBorrowedMem{0};
    uint32_t blockSize{128}; // 芯片表项内存拆分粒度大小，单位M
    std::vector<NumaMemInfo> localnumaMemInfo;

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "Total Reserved Mem: " << totalReservedMem << ", Total Borrowable Mem: " << totalBorrowableMem
            << ", Total Lent Mem: " << totalLentMem << ", Total Borrowed Mem: " << totalBorrowedMem
            << "\nLocal Numa Mem Info:\n";

        for (const auto& numaInfo : localnumaMemInfo) {
            oss << "  " << numaInfo.ToString() << "\n";
        }

        return oss.str();
    }
};

struct BorrowItem {
    BorrowItem() = default;
    std::string borrowId;
    std::string srcNid{};
    uint16_t srcRemoteNumaId{};
    std::string dstNid{};
    uint16_t dstSocketId{};
    uint64_t scanCount{}; // 扫描次数
};

struct ReservedInfo {
    uint64_t reservedRatio; // 比例
    uint64_t memLent;       // 借出内存（字节）
    uint64_t memShared;     // 共享内存（字节）
};

// 借用标识符和虚机的映射关系
class Name2VmInfo {
public:
    static Name2VmInfo& Instance()
    {
        static Name2VmInfo instance;
        return instance;
    }
    MpResult Update(const std::string& nodeId, std::map<std::string, std::set<turbo::rmrs::BorrowIdInfo>>& value);
    MpResult Query(const std::string& nodeId, std::map<std::string, std::set<turbo::rmrs::BorrowIdInfo>>& value);
    MpResult GetRawData(UbseByteBuffer& data);
    MpResult PutRawData(UbseByteBuffer& data);

private:
    std::map<std::string, std::map<std::string, std::set<turbo::rmrs::BorrowIdInfo>>> vmInfoData;
    std::mutex mtx;
};

struct BorrowIdRedirection {
public:
    static BorrowIdRedirection& Instance()
    {
        static BorrowIdRedirection instance;
        return instance;
    }
    MpResult Update(const std::string key, const std::string value);
    MpResult Remove(const std::string key);
    MpResult Query(const std::string key, std::string& value);

    MpResult GetRawData(UbseByteBuffer& buffer);
    MpResult PutRawData(UbseByteBuffer& buffer);

private:
    std::unordered_map<std::string, std::string> borrowIdRedirectionMap;
    std::mutex mtxBorrowIdRedirect;
};

class BorrowIdsCompleted {
public:
    static BorrowIdsCompleted& Instance()
    {
        static BorrowIdsCompleted instance;
        return instance;
    }
    MpResult Update(const std::string borrowId);
    MpResult Remove(const std::string borrowId);
    MpResult Query(std::vector<std::string>& borrowIdsCompletedList);
    MpResult GetRawData(UbseByteBuffer& data, bool needLock);
    MpResult PutRawData(UbseByteBuffer& data);

private:
    std::unordered_set<std::string> borrowIdsCompleted;
    std::mutex mtxBorrowIdsCompleted;
};

class FaultNuma {
public:
    static FaultNuma& Instance()
    {
        static FaultNuma instance;
        return instance;
    }

    bool GetFaultNumaList(const std::string& nodeId, std::vector<uint32_t>& numaIdList)
    {
        std::lock_guard<std::mutex> lk(mutex_);

        auto it = faultNumaMap_.find(nodeId);
        if (it == faultNumaMap_.end()) {
            return false;
        }

        numaIdList = it->second;

        return true;
    }

    void AddFaultNuma(const std::string& nodeId, uint32_t numaId)
    {
        std::lock_guard<std::mutex> lk(mutex_);

        auto& numaList = faultNumaMap_[nodeId];

        auto it = std::find(numaList.begin(), numaList.end(), numaId);
        if (it == numaList.end()) {
            numaList.push_back(numaId);
        }
    }

    void RemoveFaultNuma(const std::string& nodeId, uint32_t numaId)
    {
        std::lock_guard<std::mutex> lk(mutex_);

        auto it = faultNumaMap_.find(nodeId);
        if (it == faultNumaMap_.end()) {
            return;
        }

        auto& numaList = it->second;

        numaList.erase(std::remove(numaList.begin(), numaList.end(), numaId), numaList.end());

        if (numaList.empty()) {
            faultNumaMap_.erase(it);
        }
    }

    bool IsFaultNuma(const std::string& nodeId, uint32_t numaId)
    {
        std::lock_guard<std::mutex> lk(mutex_);

        auto it = faultNumaMap_.find(nodeId);
        if (it == faultNumaMap_.end()) {
            return false;
        }

        auto& numaList = it->second;

        return std::find(numaList.begin(), numaList.end(), numaId) != numaList.end();
    }

    void PrintFaultNuma()
    {
        std::lock_guard<std::mutex> lk(mutex_);

        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultNuma] Dump begin, FaultNumaMap.size=" << faultNumaMap_.size() << ".";

        for (const auto& kv : faultNumaMap_) {
            const std::string& nodeId = kv.first;
            const auto& numaList = kv.second;

            std::ostringstream oss;
            oss << "[FaultNuma] nodeId=" << nodeId << ", numaList=[";

            for (size_t i = 0; i < numaList.size(); ++i) {
                oss << numaList[i];
                if (i + 1 != numaList.size()) {
                    oss << ",";
                }
            }
            oss << "].";

            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << oss.str();
        }
    }

private:
    FaultNuma() = default;

    ~FaultNuma() = default;

    FaultNuma(const FaultNuma&) = delete;

    FaultNuma& operator=(const FaultNuma&) = delete;

private:
    std::unordered_map<std::string, std::vector<uint32_t>> faultNumaMap_;
    std::mutex mutex_;
};

class SmapEnableCompleted {
public:
    static SmapEnableCompleted& Instance()
    {
        static SmapEnableCompleted instance;
        return instance;
    }
    MpResult Update(const int16_t numaId);
    MpResult Remove(const int16_t numaId);
    MpResult Query(std::vector<int16_t>& smapEnableCompletedList);
    MpResult GetRawData(UbseByteBuffer& data, bool needLock);
    MpResult PutRawData(UbseByteBuffer& data);

private:
    // smapEnableCompleted中存放的为disable以后尚未enable的remoteNumaId
    std::unordered_set<int16_t> smapEnableCompleted;
    std::mutex mtxSmapEnableCompleted;
};

class FaultHandleBorrowedDecision {
public:
    static FaultHandleBorrowedDecision& Instance()
    {
        static FaultHandleBorrowedDecision instance;
        return instance;
    }
    MpResult Update(const uint16_t numaId, const BorrowedDecision& decision);
    MpResult Remove(const uint16_t numaId);
    MpResult Query(BorrowedDecision& decision, const uint16_t numaId);
    MpResult QueryAll(std::vector<BorrowedDecision>& decisionList);
    MpResult GetRawData(UbseByteBuffer& data, bool needLock);
    MpResult PutRawData(UbseByteBuffer& data);
    void ToString();

private:
    // borrowedDecisionMap中存放的为故障numa对应的borrowedDecision
    std::unordered_map<uint16_t, BorrowedDecision> borrowedDecisionMap;
    std::mutex mtxBorrowedDecision;
};

class BorrowIdInFaultProcess {
public:
    static BorrowIdInFaultProcess& Instance()
    {
        static BorrowIdInFaultProcess instance;
        return instance;
    }
    MpResult Update(const std::string borrowId);
    MpResult Remove(const std::string borrowId);
    MpResult Query(std::vector<std::string>& BorrowIdInFaultProcessList);
    MpResult GetRawData(UbseByteBuffer& data, bool needLock);
    MpResult PutRawData(UbseByteBuffer& data);
    MpResult Clear();

private:
    // BorrowIdInFaultProcess中存放故障处理新借来的并且还在故障处理过程中的内存块
    std::unordered_set<std::string> borrowIdInFaultProcess;
    std::mutex mtxBorrowIdInFaultProcess;
};

class RemovePidCompleted {
public:
    static RemovePidCompleted& Instance()
    {
        static RemovePidCompleted instance;
        return instance;
    }
    MpResult Update(const uint16_t numaId, const std::vector<pid_t> pids);
    MpResult Remove(const uint16_t numaId, const std::vector<pid_t> pids);
    MpResult Query(std::unordered_map<uint16_t, std::unordered_set<pid_t>>& removePidCompletedList);
    MpResult GetRawData(UbseByteBuffer& data, bool needLock);
    MpResult PutRawData(UbseByteBuffer& data);

private:
    // removePidCompleted中存放的为尚未移除纳管的pids
    std::unordered_map<uint16_t, std::unordered_set<pid_t>> removePidCompleted;
    std::mutex mtxRemovePidCompleted;
};

class VmInfosCompleted {
public:
    static VmInfosCompleted& Instance()
    {
        static VmInfosCompleted instance;
        return instance;
    }
    MpResult Update(const pid_t pid, std::string remoteNumaId, std::string borrowInNode);
    MpResult Remove(const pid_t pid);
    MpResult Query(std::unordered_map<pid_t, std::string>& vmInfosCompletedMap);
    MpResult PutRawData(UbseByteBuffer& data);
    MpResult GetRawData(UbseByteBuffer& data, bool needLock);

private:
    std::unordered_map<pid_t, std::string> vmMap;
    std::mutex mtxVmInfosCompleted;
};

class AntiNode {
public:
    static AntiNode& Instance()
    {
        static AntiNode instance;
        return instance;
    }
    MpResult Update(const std::map<std::string, std::vector<std::string>>& nodeAntiMap);
    MpResult Query(const std::string& srcNid, std::vector<std::string>& antiNodeMemVec);
    MpResult PutRawData(UbseByteBuffer& buffer);
    MpResult GetRawData(UbseByteBuffer& buffer, bool needLock);
    MpResult BuildSyncAntiNode(const UbseByteBuffer& buffer, UbseByteBuffer& syncData);
    void SetAntiNodeParam(const MpUpdateAntiNodeParam& param)
    {
        antiNodeParam = param;
    }

private:
    MpUpdateAntiNodeParam antiNodeParam;
    std::mutex mtxAnti;
};

class BorrowRecordHelper {
public:
    static BorrowRecordHelper& Instance()
    {
        static BorrowRecordHelper instance;
        return instance;
    }
    MpResult Init();
    // 获取内存账本
    MpResult CollectBorrowRecords(const std::string nodeId, std::vector<BorrowRecord>& borrowRecords);
    MpResult CollectBorrowRecordsWithFault(const std::string nodeId, std::vector<BorrowRecord>& borrowRecords);
    MpResult CollectBorrowRecordsOnlyBorrowIn(const std::string nodeId, const int& numaId,
                                              std::vector<BorrowRecord>& borrowRecords);
    // isFilter标志位，表示是否为filter函数调用账本采集，默认值为false
    MpResult CollectBorrowRecordsAll(std::vector<BorrowRecord>& borrowRecords, bool isFault = false,
                                     bool isFilter = false);
    // 更新内存账本 - 通过内存子系统接口查询全量信息到全局变量
    // isFilter标志位，表示是否为filter函数调用账本采集，默认值为false
    MpResult UpdateBorrowRecords(bool isFilter = false);
    MpResult UpdateBorrowRecordsAllWithFault();
    MpResult UpdateBorrowRecordsWithFault(const std::string nodeId, std::vector<UbseNumaMemoryDebtInfo>& debtInfos);
    MpResult UpdateBorrowRecordsWithFragmentFault(std::string nodeId);
    bool ConvertDebtToRecord(const UbseNumaMemoryDebtInfo& debtInfo, BorrowRecord& outRecord);
    MpResult CollectBorrowableInfo(const std::string& nodeId,
                                   NodeMemoryInfoWithReservedMem& nodeMemoryInfoWithReservedMem);
    MpResult CollectBorrowableInfoList(const std::vector<std::string>& nodeId,
                                       std::vector<NodeMemoryInfoWithReservedMem>& nodeMemoryInfoList);
    // 根据借入呈现numaId，获取全量borrowId/name
    MpResult GetBorrowIdByNumaId(std::vector<std::string>& borrowIds, const uint16_t numaId, const std::string nodeId);
    MpResult GetDebtInfosWithRetry(std::vector<UbseNumaMemoryDebtInfo>& debtInfos);
    MpResult GetValidDebtInfosWithRetry(std::vector<UbseNumaMemoryDebtInfo>& debtInfos);
    MpResult GetFragmentFaultBorrowRecords(std::string nodeId, std::vector<BorrowRecord>& fragMentFaultBorrowRecords);

private:
    MpResult GenBorrowRecords(const rapidjson::Value& doc, std::vector<BorrowRecord>& borrowRecords);
    std::vector<BorrowRecord> gBorrowRecords;
    std::map<std::string, std::vector<BorrowRecord>> gBorrowRecordsFragmentFault; // key: nodeId value: debts of nodeId
};

class MemReturnManager {
public:
    static MemReturnManager& Instance()
    {
        static MemReturnManager instance;
        return instance;
    }
    MpResult Update(const std::string& borrowId, BorrowItem& value);
    MpResult Remove(const std::string& borrowId);
    MpResult Query(const std::string& borrowId, BorrowItem& value);
    MpResult QueryAll(std::unordered_map<std::string, BorrowItem>& borrowCacheAll);
    MpResult GetRawData(UbseByteBuffer& buffer);
    MpResult PutRawData(UbseByteBuffer& buffer);
    bool IsAllReturned(const std::string& srcNid, const uint16_t& srcRemoteNumaId);

private:
    std::unordered_map<std::string, BorrowItem> borrowCache; // key为borrowId，value为对应的借用记录
    std::shared_mutex mtxMemReturnManager;
};

class MemRequestHelper {
public:
    static MpResult ParseBorrowRecordFields(const rapidjson::Value& doc, BorrowRecord& record);
    static MpResult ParseMemIdArray(const rapidjson::Value& doc, BorrowRecord& record);
    static MpResult ParseLentNumaArray(const rapidjson::Value& doc, std::vector<LentNuma>& lentNumaVec);
};

class MemManager {
public:
    static MemManager& Instance()
    {
        static MemManager instance;
        return instance;
    }
    MpResult GetNodeMemMap(std::unordered_map<std::string, NodeMemInfo>& outMap) const;
    MpResult GetNodeMemInfo(const std::string& nodeId, NodeMemInfo& outInfo) const;
    MpResult InitBorrowableInfo();
    // 根据borrowMemId，获取内存描述符号
    static MpResult ResolveUbBorrowableInfoList(NodeMemoryInfoWithReservedMem nodeMemoryInfoWithReservedMem,
                                                std::vector<NodeMemoryInfoWithReservedMem>& nodeMemoryInfoList);
    static void ResolveHccsBorrowableInfoList(NodeMemoryInfoWithReservedMem& nodeMemoryInfoWithReservedMem);
    static MpResult GetCanBorrowMemFromUb(RackNumaMemInfo numaMemInfo, uint64_t& canBorrowMem);
    static MpResult GenerateNumaSocketMap(std::map<std::string, std::map<int, uint16_t>>& numaSocketMap);
    static MpResult GetSocketId(const std::string& nodeId, const int& numaId, uint16_t& socketId);
    void UpdateNodeMemMap(const std::unordered_map<std::string, NodeMemoryInfoWithReservedMem>& srcMap);

    bool JudgeSampPlane(const std::string& srcNid, const uint16_t& srcSocketId, const std::string& dstNid,
                        const uint16_t& dstSocketId);

private:
    MemManager() = default;
    ~MemManager() = default;
    MemManager(const MemManager&) = delete;
    MemManager& operator=(const MemManager&) = delete;

    mutable std::mutex mtx;
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
};

void LoadDataBase(const std::string& keyPrefix, const std::string& key, const UbseByteBuffer& buff, void* ctx);
void GetBorrowIdCompletedValue(const std::string& keyPrefix, const std::string& key, const UbseByteBuffer& buff,
                               void* ctx);
void GetVmInfosCompletedValue(const std::string& keyPrefix, const std::string& key, const UbseByteBuffer& buff,
                              void* ctx);
uint32_t AntiDataReload();
uint32_t DataReloadInit();
void ResetAndDeleteBuffer(UbseByteBuffer& buffer);

class MpManagerSubModule : public MpSubModule {
public:
    MpResult Init() override
    {
        auto ret = DataReloadInit();
        if (ret != MEM_POOLING_OK) {
            return MEM_POOLING_ERROR;
        }

        // 注册通过RPC获取其他节点NUMA信息的方法
        ubse::com::UbseComEndpoint endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_GET_NODEINFO};
        ret = UbseRegRpcService(endpoint, GetNodeInfoImmediatelyRecvHandler);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemMigrate] GetNodeInfoImmediatelyRecvHandler reg failed res: " << ret << ".";
            return ret;
        }
        // 注册通过RPC获取所有节点NUMA信息的方法
        ubse::com::UbseComEndpoint endpoint_get = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_GET_ALL_NODEINFO};
        ret = UbseRegRpcService(endpoint_get, GetAllNodeInfoImmediatelyRecvHandler);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemMigrate] GetAllNodeInfoImmediatelyRecvHandler reg failed res: " << ret << ".";
            return ret;
        }
        ubse::com::UbseComEndpoint endpoint_sync = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_SYNC_DATA_TO_NODE};
        ret = UbseRegRpcService(endpoint_sync, SyncAntiDataRecvHandler);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemMigrate] SyncAntiDataRecvHandler reg failed res: " << ret << ".";
        }
        ubse::com::UbseComEndpoint endpoint_sync_anti = {.moduleId = MP_MODULE_CODE,
                                                         .serviceId = OPCODE_SYNC_ANTI_DATA_TO_NODE};
        ret = UbseRegRpcService(endpoint_sync_anti, SyncAntiDataStandByRecvHandler);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemMigrate] SyncAntiDataStandByRecvHandler reg failed res: " << ret << ".";
        }
        return MEM_POOLING_OK;
    }
    void DeInit() override
    {
        return;
    }
    const std::string Name() override
    {
        return "Manager";
    }
};

} // namespace mempooling

#endif // MEM_MANAGER_H
