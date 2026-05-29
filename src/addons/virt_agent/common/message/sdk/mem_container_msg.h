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

#ifndef MEM_CONTAINER_MSG_H
#define MEM_CONTAINER_MSG_H

#include <unordered_set>

#include "base_message.h"
#include "mempooling_def.h"
#include "vm_def.h"
#include "vm_mem.h"

namespace vm {
using namespace vm::mempooling;
typedef struct {
    char srcNid[NO_16];
    pid_t pids[NO_2048];
    size_t pid_size;
} pid_param_fo_c;

typedef struct {
    pid_t pid{};
    uint64_t localUsedMem{};
    uint16_t localNumaIds[NO_64];
    size_t localNumaCount;
    uint64_t remoteUsedMem{};
} pid_mem_info_for_c;

typedef struct {
    char containerId[NO_128][NO_128];
    size_t containerIdSize;
} container_id_list_for_c;

typedef struct {
    pid_t pids[NO_2048];
    size_t pidsCount;
    char* containerId;
} container_pid_info_for_c;

class MemContainerPidMemInfoInputMsg : public BaseMessage {
public:
    MemContainerPidMemInfoInputMsg() = default;

    explicit MemContainerPidMemInfoInputMsg(pid_param_fo_c pidParamForC) : pidParamForC_(std::move(pidParamForC)){};

    explicit MemContainerPidMemInfoInputMsg(uint8_t* rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    void SetInputInfos(const pid_param_fo_c pidParamForGo)
    {
        pidParamForC_ = pidParamForGo;
    }

    SrcMemoryBorrowParam GetBorrowParam();
    std::vector<pid_t> GetPids();

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    pid_param_fo_c pidParamForC_{};
};

class MemContainerPidMemInfoOutputMsg : public BaseMessage {
public:
    MemContainerPidMemInfoOutputMsg() = default;

    explicit MemContainerPidMemInfoOutputMsg(std::vector<PidInfo> pidInfos) : pidInfos_(std::move(pidInfos)){};

    explicit MemContainerPidMemInfoOutputMsg(uint8_t* rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    void SetPidInfos(const std::vector<PidInfo>& pidInfos)
    {
        pidInfos_ = pidInfos;
    }

    std::vector<pid_mem_info_for_c> GetPidInfos();

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    std::vector<PidInfo> pidInfos_{};
};

class UpdateWaterLineForCInputMsg : public BaseMessage {
public:
    UpdateWaterLineForCInputMsg() = default;

    explicit UpdateWaterLineForCInputMsg(WaterMark waterMark) : waterMark_(std::move(waterMark)){};

    explicit UpdateWaterLineForCInputMsg(uint8_t* rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    void SetInputInfos(const WaterMark waterMark)
    {
        waterMark_ = waterMark;
    }

    WaterMark GetWaterMark();

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    WaterMark waterMark_{};
};

class ContainerIdListForCInputMsg : public BaseMessage {
public:
    ContainerIdListForCInputMsg() = default;

    explicit ContainerIdListForCInputMsg(container_id_list_for_c containerIdListForC)
        : containerIdListForC_(std::move(containerIdListForC)){};

    explicit ContainerIdListForCInputMsg(uint8_t* rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    void SetInputInfos(const container_id_list_for_c& containerPidInfos)
    {
        containerIdListForC_ = containerPidInfos;
    }

    container_id_list_for_c GetContainerPidInfo();

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    container_id_list_for_c containerIdListForC_{};
};

class ContainerPidsForCInputMsg : public BaseMessage {
public:
    ContainerPidsForCInputMsg() = default;

    explicit ContainerPidsForCInputMsg(std::vector<container_pid_info_for_c> containerPidInfos)
        : containerPidInfos_(std::move(containerPidInfos)){};

    explicit ContainerPidsForCInputMsg(uint8_t* rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    void SetInputInfos(const std::vector<container_pid_info_for_c>& containerPidInfos)
    {
        containerPidInfos_ = containerPidInfos;
    }

    std::vector<container_pid_info_for_c> GetContainerPidInfos();

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    std::vector<container_pid_info_for_c> containerPidInfos_{};
};

typedef struct {
    uint16_t highWaterMark;
    uint16_t lowWaterMark;
} WaterMarkC;

typedef struct {
    int socketId;
    int numaId;
} SrcLocationC;

typedef struct {
    char srcNid[NO_16];
    SrcLocationC srcLocations[NO_16];
    size_t srcLocationsSize;
} BorrowParamC;

typedef struct {
    BorrowParamC borrowParam;
    uint64_t borrowSizes[NO_64];
    size_t borrowSizesSize;
    WaterMarkC waterMark;
} MemBorrowRequestC;

class MemContainerWaterLineMemBorrowInputMsg : public BaseMessage {
public:
    MemContainerWaterLineMemBorrowInputMsg() = default;

    explicit MemContainerWaterLineMemBorrowInputMsg(MemBorrowRequestC memBorrowRequest)
        : memBorrowRequest_(std::move(memBorrowRequest))
    {
    }

    explicit MemContainerWaterLineMemBorrowInputMsg(uint8_t* rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    VmResult Serialize() override;

    VmResult Deserialize() override;

    VmResult GetParams(NodeLocInfo& nodeLocInfo, std::vector<uint64_t>& borrowSizes, WaterMark& waterMark);

private:
    MemBorrowRequestC memBorrowRequest_{};
};

class MemContainerWaterLineMemBorrowOutputMsg : public BaseMessage {
public:
    MemContainerWaterLineMemBorrowOutputMsg() = default;

    explicit MemContainerWaterLineMemBorrowOutputMsg(std::vector<std::string> borrowIds)
        : borrowIds_(std::move(borrowIds))
    {
    }

    explicit MemContainerWaterLineMemBorrowOutputMsg(uint8_t* rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    VmResult Serialize() override;

    VmResult Deserialize() override;

    std::vector<std::string> GetBorrowIds() const
    {
        return borrowIds_;
    }

private:
    std::vector<std::string> borrowIds_{};
};

typedef struct {
    pid_t pid;
    int ratio;
} ContainerParamC;

typedef struct {
    BorrowParamC borrowParam;
    char borrowIds[NO_64][NO_128];
    size_t borrowIdsSize;
    ContainerParamC containerParam[NO_64];
    size_t containerParamSize;
} MemMigrateRequestC;

class MemContainerWaterLineMemMigrateInputMsg : public BaseMessage {
public:
    MemContainerWaterLineMemMigrateInputMsg() = default;

    explicit MemContainerWaterLineMemMigrateInputMsg(MemMigrateRequestC memMigrateRequest)
        : memMigrateRequest_(std::move(memMigrateRequest))
    {
    }

    explicit MemContainerWaterLineMemMigrateInputMsg(uint8_t* rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    VmResult Serialize() override;

    VmResult Deserialize() override;

    VmResult GetParams(NodeLocInfo& nodeLocInfo, std::unordered_set<std::string>& borrowIdSet,
                       std::vector<VMPresetParam>& vmPresetParamList);

private:
    MemMigrateRequestC memMigrateRequest_{};
};

typedef struct {
    BorrowParamC borrowParam;
    char borrowIds[NO_64][NO_128];
    size_t borrowIdsSize;
    pid_t pids[NO_64];
    size_t pidsSize;
} MemReturnRequestC;

class MemContainerWaterLineMemReturnInputMsg : public BaseMessage {
public:
    MemContainerWaterLineMemReturnInputMsg() = default;

    explicit MemContainerWaterLineMemReturnInputMsg(MemReturnRequestC memReturnRequest)
        : memReturnRequest_(std::move(memReturnRequest))
    {
    }

    explicit MemContainerWaterLineMemReturnInputMsg(uint8_t* rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    VmResult Serialize() override;

    VmResult Deserialize() override;

    VmResult GetParams(NodeLocInfo& nodeLocInfo, std::vector<std::string>& borrowIds, std::vector<pid_t>& pids);

private:
    MemReturnRequestC memReturnRequest_{};
};
} // namespace vm

#endif // MEM_CONTAINER_MSG_H
