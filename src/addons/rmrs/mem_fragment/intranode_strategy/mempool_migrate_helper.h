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

#ifndef MEMPOOL_MIGRATE_HELPER_H
#define MEMPOOL_MIGRATE_HELPER_H

#include <mutex>
#include <string>
#include <unistd.h>
#include "mempool_borrow_module.h"
#include "mempool_migrate_module.h"
#include "mp_configuration.h"
#include "ubse_logger.h"
#include "ubse_com.h"
#include "ubse_common_def.h"
#include "mp_parse_util.h"
#include "mp_module.h"

namespace mempooling {
using namespace ubse::com;
using namespace ubse::log;
using namespace mempooling::migrate;
using namespace mempooling::message;

constexpr uint32_t MAX_RETRY_TIMES = 6u; // 最大重试次数：6
constexpr uint32_t RETRY_SLEEP_TIME_SEC = 20u;

MpResult RpcMemBorrowRollback(std::string nodeId, const std::vector<std::string> &borrowIdsLis);
MpResult FilterBorrowIds(const std::vector<std::string> &borrowIdsList,
                         std::map<std::string, std::set<BorrowIdInfo>> &borrowIdsPidsMap,
                         std::map<std::string, std::set<BorrowIdInfo>> &validBorrowIdsPidsMap);
MpResult GetRollBackBorrowIdPid(const std::string &nodeId, RollBackBorrowIdPid &entry,
    std::vector<std::string> borrowIdsList);
MpResult ValidBorrowIdPidMap(std::map<std::string, std::set<BorrowIdInfo>> borrowIdsPidsMap);
MpResult RollBackMigratedVmsInStandbyToMasterEvent();
MpResult MigrateExecuteInStandbyToMasterEvent(const pid_t pid, const std::string borrowInNode,
                                              const std::string remoteNumaId);
MpResult RemoveVmInfosCompletedWithRetry(const pid_t pid);

class MpMigrateSubModule : public MpSubModule {
public:
    MpResult Init() override
    {
        // 注册通过RPC获取其他节点虚机信息的方法
        UbseComEndpoint endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_GETVMINFO_IMMEDIATELY};
        auto ret = UbseRegRpcService(endpoint, GetVmInfoImmediatelyRecvHandler);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemMigrate] GetVmInfoImmediatelyRecvHandler reg failed res: " << ret << ".";
            return ret;
        }
        ret = RollBackMigratedVmsInStandbyToMasterEvent();
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate] Roll back migrated vms failed.";
        }
        return MEM_POOLING_OK;
    }
    void DeInit() override
    {
        return ;
    }
    const std::string Name() override
    {
        return "Migrate";
    }
};

} // namespace mempooling

#endif // MEMPOOL_MIGRATE_HELPER_H