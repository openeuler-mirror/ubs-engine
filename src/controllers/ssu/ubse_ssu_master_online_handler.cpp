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
#include "ubse_ssu_utils.h"

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
            // 采集器瞬时失败不阻塞节点就绪，后续周期性采集会重试；
            // 也不跳过本地挂载恢复（重挂不依赖collector，仅账本对账部分挂到采集成功回调链补执行）
            UBSE_LOG_ERROR << "MasterOnlineHandler: StartCollecting failed, ret=" << FormatRetCode(collectRet);
        } else {
            // 立即重建账本；首次采集失败时缓存为空且采集未完成（跳过重建，保持未就绪），
 	        // 由定时器采集成功后的恢复回调（SetOnFirstCollectFailRecovery）触发重建
            instance.RebuildLedgerFromDevList();
            UBSE_LOG_INFO << "MasterOnlineHandler: collector started and ledger rebuilt on master role";
        }
    } else {
        // 非主节点（agent/standby），停止collector，处理主备切换场景
        UbseSsuServiceImp::GetInstance().StopCollecting();
        UBSE_LOG_INFO << "MasterOnlineHandler: collector stopped on non-master node, role=" << role;
    }

    // 本地挂载记录恢复（重挂 + 账本对账）：executor异步执行，不阻塞事件回调。
    // master分支：重挂立即执行，对账部分受ledgerReady_门禁（账本未就绪时挂到采集成功回调链补执行）；
    // 非master分支：agent/新任standby均重报，覆盖主备切换后attachNodes全量收敛
    auto executor = ubse::ssu::utils::GetSsuExecutor();
    if (executor == nullptr) {
        UBSE_LOG_ERROR << "MasterOnlineHandler: get ssu executor failed, skip local attachment recovery";
        return UBSE_OK;
    }
    executor->Execute([]() { UbseSsuServiceImp::GetInstance().RecoverLocalAttachments(); });

    return UBSE_OK;
}

// 节点上下线事件桥接：NODE_DOWN记录首次离线时刻（幂等），
// NODE_UP清除离线记录；离场判定由collector周期对账按持续时间做出
uint32_t UbseSsuMasterOnlineHandler::NodeStateHandler(UbseElectionEventType& type, UBSE_ID_TYPE& nodeId)
{
    if (nodeId.empty()) {
        return UBSE_OK;
    }
    auto &instance = UbseSsuServiceImp::GetInstance();
    if (type == UbseElectionEventType::NODE_DOWN) {
        instance.OnNodeDownEvent(nodeId);
    } else if (type == UbseElectionEventType::NODE_UP) {
        instance.OnNodeUpEvent(nodeId);
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

    // 监听节点上下线事件（仅master收到）：
    // NODE_DOWN记录首次离线时刻，超阈值后由collector周期对账做离场处理；NODE_UP清除记录
    UbseElectionHandlerBuilder NodeUpBuilder;
    NodeUpBuilder.SetHandler(NodeStateHandler);
    NodeUpBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    NodeUpBuilder.SetSequenceId(SSU_HA_SEQUENCE_ID);
    NodeUpBuilder.SetType(UbseElectionEventType::NODE_UP);
    NodeUpBuilder.SetName("UbseSsuNodeUp");
    UbseElectionChangeAttachHandler(NodeUpBuilder.Build());

    UbseElectionHandlerBuilder NodeDownBuilder;
    NodeDownBuilder.SetHandler(NodeStateHandler);
    NodeDownBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    NodeDownBuilder.SetSequenceId(SSU_HA_SEQUENCE_ID);
    NodeDownBuilder.SetType(UbseElectionEventType::NODE_DOWN);
    NodeDownBuilder.SetName("UbseSsuNodeDown");
    UbseElectionChangeAttachHandler(NodeDownBuilder.Build());

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

    UbseElectionHandlerBuilder NodeUpBuilder;
    NodeUpBuilder.SetHandler(NodeStateHandler);
    NodeUpBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    NodeUpBuilder.SetSequenceId(SSU_HA_SEQUENCE_ID);
    NodeUpBuilder.SetType(UbseElectionEventType::NODE_UP);
    NodeUpBuilder.SetName("UbseSsuNodeUp");
    UbseElectionChangeDeAttachHandler(NodeUpBuilder.Build());

    UbseElectionHandlerBuilder NodeDownBuilder;
    NodeDownBuilder.SetHandler(NodeStateHandler);
    NodeDownBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    NodeDownBuilder.SetSequenceId(SSU_HA_SEQUENCE_ID);
    NodeDownBuilder.SetType(UbseElectionEventType::NODE_DOWN);
    NodeDownBuilder.SetName("UbseSsuNodeDown");
    UbseElectionChangeDeAttachHandler(NodeDownBuilder.Build());

    // 解注册时停止collector
    UbseSsuServiceImp::GetInstance().StopCollecting();

    UBSE_LOG_INFO << "UbseSsuMasterOnlineHandler Uninitialized";
}

} // namespace ubse::ssu::service
