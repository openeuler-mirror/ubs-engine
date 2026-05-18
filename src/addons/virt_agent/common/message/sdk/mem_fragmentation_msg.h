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

#ifndef MEM_FRAGMENTATION_MSG_H
#define MEM_FRAGMENTATION_MSG_H

#include "base_message.h"
#include "mempooling_def.h"
#include "vm_def.h"

namespace vm {
using namespace vm::mempooling;

constexpr uint32_t VIRT_MEM_MAX_NODE_ID_LENGTH = 48;
constexpr uint32_t VIRT_MAX_UUID_LENGTH = 37;
constexpr uint32_t VIRT_MAX_NAME_LENGTH = 128;
constexpr uint32_t VIRT_MAX_STATE_LENGTH = 128;
constexpr uint32_t UBS_VA_HOST_NAME_MAX = 64;
constexpr uint32_t MEM_TASK_ID_MAX = 256;

struct KeyValuePair {
    char key[VIRT_MEM_MAX_NODE_ID_LENGTH];
    char value[MAX_NODE_NUM][VIRT_MEM_MAX_NODE_ID_LENGTH];
    size_t value_count;
};

struct NodeAntiDictionary {
    struct KeyValuePair entries[MAX_NODE_NUM];
    size_t entry_count;
};

typedef struct {
    uint64_t pageSize{};
    uint64_t hugePageTotal{};
    uint64_t hugePageFree{};
} numa_page_data;

typedef struct {
    time_t timestamp;
    char node_id[VIRT_MEM_MAX_NODE_ID_LENGTH];
    char host_name[UBS_VA_HOST_NAME_MAX];
    int16_t numa_id;
    int16_t socket_id;  // Socket ID mapped to CPUs bound to this NUMA
    int16_t is_local;   // Whether this is a local NUMA (0: non-local, 1: local)
    uint64_t mem_total; // Total memory of this NUMA node (inclusive), collected from system files, in kB
    uint64_t mem_free;  // Free memory on this NUMA node, collected from system files, in kB
    numa_page_data *huge_page_data;
    uint64_t numaPageInfoCount;
} numa_info_t;

typedef struct {
    int16_t numaId;   // CPU NUMA ID
    int16_t socketId; // CPU socket ID
    bool isLocal;
    uint64_t pageSize; // VM page size, default is 2 MB huge pages (2048 KBytes)
    int64_t usedMem;
} vm_numa_info_for_c;

typedef struct {
    char nodeId[VIRT_MEM_MAX_NODE_ID_LENGTH]; // Physical node ID (from control-plane configuration file)
    char hostName[UBS_VA_HOST_NAME_MAX];      // Physical node host name (from VM XML definition)
    char uuid[VIRT_MAX_UUID_LENGTH];          // VM UUID (from VM XML definition)
    char name[VIRT_MAX_NAME_LENGTH];          // VM name (from VM XML definition)
    char state[VIRT_MAX_STATE_LENGTH];        // VM state
    int64_t vmCreateTime;
    uint64_t maxMem;
    pid_t pid;
} vm_meta_data_for_c;

typedef struct {
    time_t timestamp;
    vm_meta_data_for_c metadata;
    vm_numa_info_for_c *numaInfo;
    uint64_t numaInfoCount;
} vm_domain_info_for_c;

typedef struct {
    char borrow_ids_ptr[MAX_BORROW_ID_COUNT][MAX_BORROW_ID_LENGTH];
    uint16_t present_numa_ids_ptr[MAX_BORROW_ID_COUNT];
    uint32_t borrow_ids_size;
    uint32_t present_numa_ids_size;
    char task_id[MEM_TASK_ID_MAX];
} mem_borrow_result_c;

using async_task_status_c = enum {
    ASYNC_TASK_NOT_EXIST = 0,
    ASYNC_TASK_RUNNING = 1,
    ASYNC_TASK_SUCCESS = 2,
    ASYNC_TASK_FAILED = 3
};

typedef struct {
    char task_id[MEM_TASK_ID_MAX];
    async_task_status_c status;
    uint32_t resultCode;
    mem_borrow_result_c memBorrowResult{};
} async_task_info_c;

class MemTaskResultQueryMsg : public BaseMessage {
public:
    MemTaskResultQueryMsg() = default;

    explicit MemTaskResultQueryMsg(async_task_info_c asyncTaskInfoC) : asyncTaskInfoC_(std::move(asyncTaskInfoC)) {};

    explicit MemTaskResultQueryMsg(uint8_t *rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    void SetInputInfos(const async_task_info_c asyncTaskInfoC)
    {
        asyncTaskInfoC_ = asyncTaskInfoC;
    }

    async_task_info_c GetTaskResult();

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    async_task_info_c asyncTaskInfoC_{};
};

class MemBorrowExecuteResultMsg : public BaseMessage {
public:
    MemBorrowExecuteResultMsg() = default;

    explicit MemBorrowExecuteResultMsg(mem_borrow_result_c memBorrowResultC)
        : memBorrowResultC_(std::move(memBorrowResultC)) {};

    explicit MemBorrowExecuteResultMsg(uint8_t *rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    void SetInputInfos(const mem_borrow_result_c memBorrowResultC)
    {
        memBorrowResultC_ = memBorrowResultC;
    }

    mem_borrow_result_c GetBorrowResult();

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    mem_borrow_result_c memBorrowResultC_{};
};

class MemFragmentationMsg : public BaseMessage {
public:
    MemFragmentationMsg() = default;

    explicit MemFragmentationMsg(std::vector<NumaInfo> numaInfos) : numaInfos_(std::move(numaInfos)) {};

    explicit MemFragmentationMsg(uint8_t *rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    void SetNumaInfos(const std::vector<NumaInfo> &numaInfos)
    {
        numaInfos_ = numaInfos;
    }

    VmResult GetNumaInfo(std::vector<numa_info_t> &numaInfo);

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    std::vector<NumaInfo> numaInfos_{};
};

class MemFragmentationVmInfoMsg : public BaseMessage {
public:
    MemFragmentationVmInfoMsg() = default;

    explicit MemFragmentationVmInfoMsg(std::vector<mempooling::VmDomainInfo> vmInfoList)
        : vmInfoList_(std::move(vmInfoList)) {};

    explicit MemFragmentationVmInfoMsg(uint8_t *rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    std::vector<vm_domain_info_for_c> GetVmInfo();

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    std::vector<mempooling::VmDomainInfo> vmInfoList_{};
};

typedef struct {
    char src_nid[VIRT_MEM_MAX_NODE_ID_LENGTH];
    int16_t src_socket_id;
    int16_t src_numa_id;
    uint64_t borrow_size;
} src_memory_borrow_param;

class MemFragmentationMemBorrowStrategyInputMsg : public BaseMessage {
public:
    MemFragmentationMemBorrowStrategyInputMsg() = default;

    explicit MemFragmentationMemBorrowStrategyInputMsg(src_memory_borrow_param srcMemoryBorrowParam)
        : srcMemoryBorrowParam_(srcMemoryBorrowParam)
    {
    }

    explicit MemFragmentationMemBorrowStrategyInputMsg(uint8_t *rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    src_memory_borrow_param GetInputMsg() const;

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    src_memory_borrow_param srcMemoryBorrowParam_{};
};

typedef struct {
    char host_id[VIRT_MEM_MAX_NODE_ID_LENGTH];
    uint16_t socket_id;
    uint16_t numa_nums;
    int numa_ids[MAX_DEST_NUMA_NUM];
    uint64_t mem_sizes[MAX_DEST_NUMA_NUM];
} dst_numa_info_c;

typedef struct {
    char src_host_id[VIRT_MEM_MAX_NODE_ID_LENGTH];
    int16_t src_socket_id;
    int16_t src_numa_id;
    uint64_t borrow_size;
    dst_numa_info_c dest_numa_infos[MAX_DEST_PARAM_SIZE];
    uint32_t dest_numa_infos_size;
} borrow_strategy_c;

typedef struct {
    borrow_strategy_c borrow_strategy;
    bool isAsync;
} borrow_setting_c;

class MemBorrowSettingMsg : public BaseMessage {
public:
    MemBorrowSettingMsg() = default;

    explicit MemBorrowSettingMsg(const borrow_setting_c borrowSettingC) : borrowSettingC_(borrowSettingC) {}

    explicit MemBorrowSettingMsg(uint8_t *rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    borrow_setting_c GetMemBorrowSettingMsg();

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    borrow_setting_c borrowSettingC_{};
};

class MemFragmentationMemBorrowStrategyOutputMsg : public BaseMessage {
public:
    MemFragmentationMemBorrowStrategyOutputMsg() = default;

    explicit MemFragmentationMemBorrowStrategyOutputMsg(const borrow_strategy_c outputMsg) : outputMsg_(outputMsg) {}

    explicit MemFragmentationMemBorrowStrategyOutputMsg(uint8_t *rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    VmResult SetOutputMsg(const MemBorrowStrategyResult &borrowStrategyResult);

    borrow_strategy_c GetMemBorrowStrategyOutputMsg();

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    borrow_strategy_c outputMsg_{};
};

class MemFragmentationMemBorrowExecuteOutputMsg : public BaseMessage {
public:
    MemFragmentationMemBorrowExecuteOutputMsg() = default;

    explicit MemFragmentationMemBorrowExecuteOutputMsg(MemBorrowExecuteResult outputMsg) : outputMsg_(outputMsg) {}

    explicit MemFragmentationMemBorrowExecuteOutputMsg(uint8_t *rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    MemBorrowExecuteResult GetMemBorrowExecuteOutputMsg();

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    MemBorrowExecuteResult outputMsg_{};
};

typedef struct {
    pid_t pid;
    uint16_t ratio;
} MemMigrateStrategyVmInfo;

typedef struct {
    uint64_t borrowSize;
    char borrowInNode[VIRT_MEM_MAX_NODE_ID_LENGTH];
    MemMigrateStrategyVmInfo vmInfoList[MAX_VM_NUM];
    uint32_t vmInfoListSize;
} MemMigrateStrategySrcParam;

class MemFragmentationMemMigrateStrategyInputMsg : public BaseMessage {
public:
    MemFragmentationMemMigrateStrategyInputMsg() = default;
    ~MemFragmentationMemMigrateStrategyInputMsg() = default;

    explicit MemFragmentationMemMigrateStrategyInputMsg(uint8_t *rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    VmResult SetInputMsg(MemMigrateStrategySrcParam memMigrateSrcParam);

    MemMigrateStrategySrcParam GetInputMsg() const;

    VmResult Serialize() override;
    VmResult Deserialize() override;

private:
    MemMigrateStrategySrcParam inputMsg{};
};

typedef struct {
    char node_id[VIRT_MEM_MAX_NODE_ID_LENGTH];
    char borrow_id_list[MAX_BORROW_ID_COUNT][MAX_BORROW_ID_LENGTH];
    uint32_t borrow_id_size;
} RollbackSrcParam;

struct RollbackParams {
    std::string node_id;
    std::vector<std::string> borrow_id_list;
    uint32_t borrow_id_size;

    RollbackParams() = default;

    explicit RollbackParams(const RollbackSrcParam *srcParam)
    {
        if (srcParam == nullptr) {
            return;
        }
        this->node_id = srcParam->node_id;
        this->borrow_id_size = srcParam->borrow_id_size;
        for (uint32_t i = 0; i < srcParam->borrow_id_size; ++i) {
            this->borrow_id_list.emplace_back(srcParam->borrow_id_list[i]);
        }
    }
};

class MemRollbackMsg : public BaseMessage {
public:
    MemRollbackMsg() = default;

    explicit MemRollbackMsg(RollbackParams &rollbackParams) : rollbackParams_(std::move(rollbackParams)) {}

    explicit MemRollbackMsg(uint8_t *rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }
    RollbackParams GetRollbackParams() const
    {
        return rollbackParams_;
    }

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    RollbackParams rollbackParams_{};
};

typedef struct {
    uint16_t destNumaId;
    uint64_t memSize;
    pid_t pid;
} VmMigrateStrategy;

typedef struct {
    uint32_t vmInfoListSize;
    VmMigrateStrategy *vmInfoList;
    uint64_t waitingTime;
} MemMigrateStrategy;

class MemFragmentationMemMigrateStrategyOutputMsg : public BaseMessage {
public:
    MemFragmentationMemMigrateStrategyOutputMsg() = default;
    ~MemFragmentationMemMigrateStrategyOutputMsg() override;
    explicit MemFragmentationMemMigrateStrategyOutputMsg(MemMigrateStrategy memMigrateStrategy)
        : outputMsg(memMigrateStrategy)
    {
    }
    explicit MemFragmentationMemMigrateStrategyOutputMsg(uint8_t *rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    VmResult SetOutputMsg(MigrateStrategyResult migrateStrategyResult);

    MemMigrateStrategy GetOutputMsg() const;

    VmResult Serialize() override;
    VmResult Deserialize() override;

private:
    MemMigrateStrategy outputMsg{};
};

typedef struct {
    char borrowInNode[VIRT_MEM_MAX_NODE_ID_LENGTH];
    char borrowIds[MAX_BORROW_ID_COUNT][MAX_BORROW_ID_LENGTH];
    uint32_t borrowIdsCount;
    VmMigrateStrategy vmInfoList[MAX_VM_NUM];
    uint32_t vmInfoListSize;
    uint64_t waitingTime;
} MemMigrateExecuteSrcParam;

class MemFragmentationMemMigrateExecuteInputMsg : public BaseMessage {
public:
    MemFragmentationMemMigrateExecuteInputMsg() = default;
    ~MemFragmentationMemMigrateExecuteInputMsg() = default;
    explicit MemFragmentationMemMigrateExecuteInputMsg(uint8_t *rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    VmResult SetInputMsg(MemMigrateExecuteSrcParam srcParam);
    MemMigrateExecuteSrcParam GetInputMsg() const;

    VmResult Serialize() override;
    VmResult Deserialize() override;

private:
    MemMigrateExecuteSrcParam inputMsg{};
};

namespace mem_fragmentation {

/** ==============big memory virtual machine============== */
typedef struct {
    char node_id[VIRT_MEM_MAX_NODE_ID_LENGTH];
    numa_info_t *numa_infos;
    uint32_t numa_len;
    bool is_current;
} node_info_s;

typedef struct {
    node_info_s *node_infos;
    uint32_t node_len;
} node_info_list_s;

struct NodeInfo {
    NodeInfo() = default;

    std::string nodeId{};
    std::vector<NumaInfo> numaInfos{};
    bool isCurrent{};
};

class MemFragmentationNodeInfoListMsg : public BaseMessage {
public:
    MemFragmentationNodeInfoListMsg() = default;
    ~MemFragmentationNodeInfoListMsg() override = default;

    explicit MemFragmentationNodeInfoListMsg(std::vector<NodeInfo> nodeInfoList) : nodeInfoList(std::move(nodeInfoList))
    {
    }

    explicit MemFragmentationNodeInfoListMsg(uint8_t *rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    VmResult Serialize() override;
    VmResult Deserialize() override;

    [[nodiscard]] std::vector<NodeInfo> GetNodeInfoList() const
    {
        return nodeInfoList;
    }

private:
    std::vector<NodeInfo> nodeInfoList{};
};

typedef struct {
    int16_t socket_id;
    int16_t numa_id;
} numa_meta_info_s;

typedef struct {
    char src_nid[VIRT_MEM_MAX_NODE_ID_LENGTH];
    numa_meta_info_s *numa_meta_infos;
    uint32_t numa_len;
    uint64_t borrow_size;
} mem_borrow_param_s;

struct NumaMetaInfo {
    NumaMetaInfo() = default;

    int16_t socketId = -1;
    int16_t numaId = -1;

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream oss;
        oss << numaId << ":" << socketId;
        return oss.str();
    }
};

struct BorrowParam {
    BorrowParam() = default;

    std::string nodeId{};
    std::vector<NumaMetaInfo> numaMetaInfos{};
    uint64_t borrowSize{}; // MB

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream oss;
        oss << "nodeId=" << nodeId << ", numaMetaInfos=";
        for (const auto numaMetaInfo : numaMetaInfos) {
            oss << numaMetaInfo.ToString() << ",";
        }
        oss << " borrowSize=" << borrowSize;
        return oss.str();
    }
};

class MemFragmentationMemBorrowParamMsg : public BaseMessage {
public:
    MemFragmentationMemBorrowParamMsg() = default;
    ~MemFragmentationMemBorrowParamMsg() override = default;

    MemFragmentationMemBorrowParamMsg(BorrowParam param, bool isAsync)
        : borrowPram(std::move(param)),
          isAsync(std::move(isAsync))
    {
    }

    explicit MemFragmentationMemBorrowParamMsg(uint8_t *rawData, const uint32_t &size)
    {
        SetInputRawData(rawData, size);
    }

    VmResult Serialize() override;
    VmResult Deserialize() override;

    BorrowParam GetBorrowParam() const
    {
        return borrowPram;
    }

    bool GetIsAsync() const
    {
        return isAsync;
    }

private:
    BorrowParam borrowPram{};
    bool isAsync{};
};

typedef struct {
    mem_borrow_result_c *mem_borrow_result_list;
    uint16_t mem_borrow_result_list_len;
} mem_borrow_result_s;

class MemFragmentationMemBorrowResultMsg : public BaseMessage {
public:
    MemFragmentationMemBorrowResultMsg() = default;
    ~MemFragmentationMemBorrowResultMsg() override = default;

    explicit MemFragmentationMemBorrowResultMsg(std::vector<mem_borrow_result_c> memBorrowRstCs)
        : memBorrowRstCs(std::move(memBorrowRstCs))
    {
    }

    explicit MemFragmentationMemBorrowResultMsg(uint8_t *rawData, const uint32_t &size)
    {
        SetInputRawData(rawData, size);
    }

    VmResult Serialize() override;
    VmResult Deserialize() override;

    [[nodiscard]] std::vector<mem_borrow_result_c> GetMemBorrowResultList() const
    {
        return memBorrowRstCs;
    }

private:
    std::vector<mem_borrow_result_c> memBorrowRstCs;
};

typedef struct {
    uint32_t numa_id;
    uint32_t quota;
} numa_quota_s;

typedef struct {
    numa_quota_s *local_numas;
    uint8_t local_numa_len;
    numa_quota_s *remote_numas;
    uint8_t remote_numa_len;
} page_swap_pair_s;

typedef struct {
    page_swap_pair_s *page_swap_pairs;
    uint8_t page_swap_pairs_len;
} page_swap_enable_s;

struct NumaQuota {
    NumaQuota() = default;

    uint32_t numaId;
    uint32_t quota;

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream oss;
        oss << "numaId=" << numaId << ", quota=" << quota;
        return oss.str();
    }
};

struct PageSwapPair {
    PageSwapPair() = default;

    std::vector<NumaQuota> localNumaQuotas;
    std::vector<NumaQuota> remoteNumaQuotas;

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream oss;
        oss << "localNumaQuotas=";
        for (const auto numaQuota : localNumaQuotas) {
            oss << numaQuota.ToString() << "; ";
        }
        oss << "remoteNumaQuotas=";
        for (const auto numaQuota : remoteNumaQuotas) {
            oss << numaQuota.ToString() << "; ";
        }
        return oss.str();
    }
};

class MemFragmentationPageSwapEnableMsg : public BaseMessage {
public:
    MemFragmentationPageSwapEnableMsg() = default;
    ~MemFragmentationPageSwapEnableMsg() override = default;

    explicit MemFragmentationPageSwapEnableMsg(pid_t pid, std::vector<PageSwapPair> pageSwapPairs)
        : pid(std::move(pid)),
          pageSwapPairs(std::move(pageSwapPairs))
    {
    }

    explicit MemFragmentationPageSwapEnableMsg(uint8_t *rawData, const uint32_t &size)
    {
        SetInputRawData(rawData, size);
    }

    VmResult Serialize() override;
    VmResult Deserialize() override;

    [[nodiscard]] pid_t GetPid() const
    {
        return pid;
    }

    [[nodiscard]] std::vector<PageSwapPair> GetPageSwapPair() const
    {
        return pageSwapPairs;
    };

private:
    pid_t pid{};
    std::vector<PageSwapPair> pageSwapPairs{};
};
} // namespace mem_fragmentation
} // namespace vm

#endif // MEM_FRAGMENTATION_MSG_H
