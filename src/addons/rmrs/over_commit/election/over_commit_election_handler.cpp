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

#include "over_commit_election_handler.h"
#include "ubse_election.h"
#include "ubse_logger.h"
#include "interface_guard.h"
#include "mp_configuration.h"
#include "mp_error.h"
namespace mempooling::over_commit {
using namespace ubse::log;
using namespace ubse::election;
uint32_t OverCommitSwitchoverHandler(UbseElectionEventType& type, UBSE_ID_TYPE& nodeId)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[Election] CHANGE_TO_SWITCHOVER event enter.";
    auto status = InterfaceGuard::GetStatus();
    if (!(status.isOutMemBorrowRunning || status.isOutMemMigrateRunning || status.isOutMemReturnRunning)) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[Election][OverCommit] Success, allowing for switch_over.";
        return MEM_POOLING_OK;
    } else {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[Election][OverCommit] Failed, interfaces status OutMemBorrowRunning=" << status.isOutMemBorrowRunning
            << ", OutMemMigrateRunning=" << status.isOutMemMigrateRunning
            << ", OutMemReturnRunning=" << status.isOutMemReturnRunning << ".";
        return MEM_POOLING_ERROR;
    }
}
// 这里的Priority和订阅事件还需要确认
uint32_t OverCommitSubscribeSwitchover()
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[Election] OverCommitSubscribeSwitchover enter.";
    UbseElectionHandlerBuilder builder;
    builder.SetName("OVER_COMMIT_CHANGE_TO_SWITCHOVER")
        .SetPriority(UbseElectionHandlerPriority::LOW)
        .SetType(UbseElectionEventType::CHANGE_TO_SWITCHOVER)
        .SetHandler(OverCommitSwitchoverHandler);
    auto ret = UbseElectionChangeAttachHandler(builder.Build());
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[Election] Register election handler of switch_over failed, ret code [" << ret << "].";
        return ret;
    }
    return MEM_POOLING_OK;
}
} // namespace mempooling::over_commit