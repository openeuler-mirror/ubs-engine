/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_ssu_master_online_handler.h"

#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_ssu_service_imp.h"

namespace ubse::ssu::service {

using namespace ubse::log;
using namespace ubse::election;

// 需要在节点建链后触发，节点建链优先级100，mem controller优先级101
constexpr uint32_t SSU_HA_SEQUENCE_ID = 102;

UBSE_DEFINE_THIS_MODULE("ubse");

uint32_t UbseSsuMasterOnlineHandler::MasterOnlineHandler(UbseElectionEventType& type, UBSE_ID_TYPE& nodeId)
{
    (void)type;
    (void)nodeId;

    // 判断当前节点角色，仅master节点启动collector和重建账本
    std::string role;
    auto ret = UbseGetRole(role);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "MasterOnlineHandler: failed to get node role, ret=" << FormatRetCode(ret);
        // 不返回错误：瞬时失败不应阻塞 SetWorkReadiness(IS_READY)
        return UBSE_OK;
    }

    if (role == ELECTION_ROLE_MASTER) {
        auto& instance = UbseSsuServiceImp::GetInstance();
        auto collectRet = instance.StartCollecting();
        if (collectRet != UBSE_OK) {
            UBSE_LOG_ERROR << "MasterOnlineHandler: StartCollecting failed, ret=" << FormatRetCode(collectRet);
            // 不返回错误：采集器瞬时失败不应阻塞节点就绪，后续周期性采集会重试
            return UBSE_OK;
        }
        // 立即重建账本；首次采集失败时缓存为空，Rebuild为空操作（增量Put，不会覆盖已有账本），
        // 由定时器采集成功后的回调（SetOnCollectFirstSuccess）触发重建
        instance.RebuildLedgerFromDevList();
        UBSE_LOG_INFO << "MasterOnlineHandler: collector started and ledger rebuilt on master role";
    } else {
        // 非主节点（agent/standby），停止collector，处理主备切换场景
        UbseSsuServiceImp::GetInstance().StopCollecting();
        UBSE_LOG_INFO << "MasterOnlineHandler: collector stopped on non-master node, role=" << role;
    }

    return UBSE_OK;
}

void UbseSsuMasterOnlineHandler::Initial()
{
    // 监听主上线事件，在主节点时启动collector和重建账本，非主节点时停止collector
    UbseElectionHandlerBuilder Builder;
    Builder.SetHandler(MasterOnlineHandler);
    Builder.SetPriority(UbseElectionHandlerPriority::HIGH);
    Builder.SetSequenceId(SSU_HA_SEQUENCE_ID);
    Builder.SetType(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION);
    Builder.SetName("UbseSsuMasterOnLine");
    UbseElectionChangeAttachHandler(Builder.Build());

    UBSE_LOG_INFO << "UbseSsuMasterOnlineHandler Initialized";
}

void UbseSsuMasterOnlineHandler::Uninitial()
{
    UbseElectionHandlerBuilder Builder;
    Builder.SetHandler(MasterOnlineHandler);
    Builder.SetPriority(UbseElectionHandlerPriority::HIGH);
    Builder.SetSequenceId(SSU_HA_SEQUENCE_ID);
    Builder.SetType(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION);
    Builder.SetName("UbseSsuMasterOnLine");
    UbseElectionChangeDeAttachHandler(Builder.Build());

    // 解注册时停止collector
    UbseSsuServiceImp::GetInstance().StopCollecting();

    UBSE_LOG_INFO << "UbseSsuMasterOnlineHandler Uninitialized";
}

} // namespace ubse::ssu::service
