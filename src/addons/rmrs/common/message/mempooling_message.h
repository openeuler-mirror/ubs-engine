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

#ifndef MEMPOOLING_MESSAGE_H
#define MEMPOOLING_MESSAGE_H

#include <cstdint>
#include <string>

#include "ubse_com.h"
#include "mp_module.h"
#include "turbo_def.h"
#include "turbo_rmrs_interface.h"

namespace mempooling::message {

const char* const libubturbo_client_PATH = "/usr/lib64/libubturbo_client.so";

const uint32_t OPCODE_COMMON = 13;
const uint32_t OPCODE_MIGRATE_STRATEGY = 14;
const uint32_t OPCODE_MEMBORROW_ROLLBACK = 17;
const uint32_t OPCODE_FAULT_NODE_NUMA_REPLACE_RETURN = 19;
const uint32_t OPCODE_OVER_COMMIT_MEM_MIGRATE = 20;
const uint32_t OPCODE_SET_SMAP_REMOTE_NUMA_INFO = 21;
const uint32_t OPCODE_SMAP_REMOVE = 22;
const uint32_t OPCODE_FM_NOT = 23;
const uint32_t OPCODE_FM_SAME = 24;
const uint32_t OPCODE_FM_VM = 25;
const uint32_t OPCODE_SMAP_PROCESS_QUERY = 26;
const uint32_t OPCODE_ADD_FAULT_NODE = 27;
const uint32_t OPCODE_DEL_FAULT_NODE = 28;
const uint32_t OPCODE_OVER_COMMIT_MEMID_FAULT_GET_VM_INFO = 29;
const uint32_t OPCODE_OVER_COMMIT_MEM_ID_FAULT_EXECUTE = 30;
const uint32_t OPCODE_OVER_COMMIT_MIGRATE_GET_LOCAL_VM_INFO = 31;
const uint32_t OPCODE_PID_NUMA_INFO_COLLECT = 32;
const uint32_t OPCODE_NUMA_MEM_INFO_COLLECT = 33;
const uint32_t OPCODE_OVER_COMMIT_UCACHE_SEND_MIGRTIAON_STRATEGY = 34;
const uint32_t OPCODE_OVER_COMMIT_UCACHE_STOP_MIGRTIAON = 35;
const uint32_t OPCODE_GETVMINFO_IMMEDIATELY = 36;
const uint32_t OPCODE_SMAP_ADD_CTRL = 37;
const uint32_t OPCODE_SMAP_VMFREQ_CTRL = 38;
const uint32_t OPCODE_SMAP_REMOVE_CTRL = 39;
const uint32_t OPCODE_SMAP_ENABLE_CTRL = 40;
const uint32_t OPCODE_SMAP_BACK = 41;
const uint32_t OPCODE_SMAP_ENABLE_NUMA = 42;
const uint32_t OPCODE_OVER_COMMIT_UPDATE_UCACHE_RATIO = 43;
const uint32_t OPCODE_GET_NODEINFO = 44;
const uint32_t OPCODE_GET_ALL_NODEINFO = 45;
const uint32_t OPCODE_SYNC_DATA_TO_NODE = 46;
const uint32_t OPCODE_SYNC_ANTI_DATA_TO_NODE = 47;
const uint32_t OPCODE_SYNC_BIND_TYPE_DATA_TO_NODE = 48;
const uint32_t OPCODE_OVER_COMMIT_MEM_ID_FAULT_RETURN_EXECUTE = 49;
const uint32_t OPCODE_OVER_COMMIT_FAULT_NUMA_PROCESS = 50;
const uint32_t OPCODE_OVER_COMMIT_MEM_ID_FAULT_DIRECTLY_RETURN_EXECUTE = 51;
const uint32_t OPCODE_SMAP_PROCESS_MIGRATE_DISABLE = 52;
const uint32_t OPCODE_CHECK_UBTURBO_IS_ALIVE = 53;
const uint32_t OPCODE_OVER_COMMIT_FAULT_HANDLE_MEM_BORROW = 54;
const uint32_t OPCODE_OVER_COMMIT_FAULT_NUMA_PROCESS_SIMPLIFIED = 55;
const uint32_t OPCODE_NUMA_LEVEL_EXECUTE = 56;
const uint32_t OPCODE_BORROW_ID_LEVEL_EXECUTE = 57;
const uint32_t OPCODE_GET_BORROWED_DECISION = 58;
const uint32_t OPCODE_SMAP_PROCESS_MIGRATE_ENABLE = 59;
const uint32_t OPCODE_GET_NUMA_BIND_TYPE_FROM_NODE = 60;

using OSTurboFunctionCaller = uint32_t (*)(const std::string& function, const TurboByteBuffer& params,
                                           TurboByteBuffer& result);
using OSTurboSetIpcTimeLimit = uint32_t (*)(uint32_t timeLimit);
using UBTurboRMRSAgentMigrateStrategy = uint32_t (*)(const turbo::rmrs::MigrateStrategyParamRMRS& migrateStrategyParam,
                                                     turbo::rmrs::MigrateStrategyResult& migrateStrategyResult);
using UBTurboRMRSAgentMigrateExecute = uint32_t (*)(const turbo::rmrs::MigrateStrategyResult& migrateStrategyResult);
using UBTurboRMRSAgentMigrateBack = uint32_t (*)(turbo::rmrs::MigrateBackResult& migrateBackResult);
using UBTurboRMRSAgentBorrowRollBack =
    uint32_t (*)(std::map<std::string, std::set<turbo::rmrs::BorrowIdInfo>>& borrowIdsPidsMap);
using UBTurboRMRSAgentPidNumaInfoCollect =
    uint32_t (*)(const turbo::rmrs::PidNumaInfoCollectParam& pidNumaInfoCollectParam,
                 turbo::rmrs::PidNumaInfoCollectResult& pidNumaInfoCollectResult);
using UBTurboRMRSAgentNumaMemInfoCollect =
    uint32_t (*)(const turbo::rmrs::NumaMemInfoCollectParam& numaMemInfoCollectParam,
                 turbo::rmrs::ResponseInfoSimpo& responseInfoSimpo);
using UBTurboRMRSAgentUCacheMigrateStrategy = uint32_t (*)(
    const turbo::rmrs::UCacheMigrationStrategyParam& uCacheMigrationStrategyParam, turbo::rmrs::ResCode& resCode);
using UBTurboRMRSAgentUCacheMigrateStop = uint32_t (*)(turbo::rmrs::ResCode& resCode);
using UBTurboRMRSAgentUpdateUCacheRatio = uint32_t (*)(const turbo::rmrs::MigrationInfoParam& migrationInfoParam,
                                                       turbo::rmrs::UCacheRatioRes& uCacheRatioRes);

class MempoolingMessage {
public:
    static uint32_t Init();
    static uint32_t DeInit();
    static uint32_t InitOSTurboIpcClient();

    static void* osturboClientHandle;
    static OSTurboFunctionCaller osturboFunctionCaller;
    static OSTurboSetIpcTimeLimit osturboSetIpcTimeLimit;
    static UBTurboRMRSAgentMigrateStrategy rmrsMigrateStrategy;
    static UBTurboRMRSAgentMigrateExecute rmrsMigrateExecute;
    static UBTurboRMRSAgentMigrateBack rmrsMigrateBack;
    static UBTurboRMRSAgentBorrowRollBack rmrsBorrowRollBack;
    static UBTurboRMRSAgentPidNumaInfoCollect rmrsPidNumaInfoCollect;
    static UBTurboRMRSAgentNumaMemInfoCollect rmrsNumaMemInfoCollect;
    static UBTurboRMRSAgentUCacheMigrateStrategy rmrsUCacheMigrateStrategy;
    static UBTurboRMRSAgentUCacheMigrateStop rmrsUCacheMigrateStop;
    static UBTurboRMRSAgentUpdateUCacheRatio rmrsUpdateUCacheRatio;

private:
    std::string hostName;
    std::string nodeId;

    static uint32_t DlsymMemFragInterface();
    static uint32_t DlsymOverCommitInterface();
    static uint32_t DlsymUcacheInterface();
};

struct DemoDataSimpo {
    uint32_t code;
    std::string message;
    std::vector<std::string> msgArr;
};

class MpMessageSubModule : public mempooling::MpSubModule {
public:
    MpResult Init() override
    {
        auto ret = MempoolingMessage::Init();
        if (ret != MEM_POOLING_OK) {
            return MEM_POOLING_ERROR;
        }
        return MEM_POOLING_OK;
    }
    void DeInit() override
    {
        MempoolingMessage::DeInit();
    }
    const std::string Name() override
    {
        return "Message";
    }
};

} // namespace mempooling::message

#endif
