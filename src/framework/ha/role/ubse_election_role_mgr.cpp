/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_election_role_mgr.h"
#include <memory>
#include "ubse_context.h"
namespace ubse::election {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::context;
std::shared_ptr<RoleMgr> RoleMgr::instance_ = nullptr;

std::shared_ptr<ElectionRole> RoleMgr::GetRole()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return currentRole_;
}

void RoleMgr::ProcTimer()
{
    std::shared_ptr<ElectionRole> role = GetRole();
    role->ProcTimer();
}

uint32_t RoleMgr::RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt& rcvPkt, ElectionReplyPkt& reply)
{
    std::shared_ptr<ElectionRole> role = GetRole();
    return role->RecvPkt(srcID, rcvPkt, reply);
}

void RoleMgr::SwitchRole(RoleType roleType, RoleContext& ctx)
{
    // 锁外构造新角色：避免持全局锁期间执行构造/旧角色析构
    // （旧 Master 析构会自旋等待在途回调，持锁会放大为全局选举阻塞）
    std::shared_ptr<ElectionRole> newRole;
    switch (roleType) {
        case RoleType::MASTER:
            newRole = SafeMakeShared<Master>(ctx);
            break;
        case RoleType::STANDBY:
            newRole = SafeMakeShared<Standby>(ctx);
            break;
        case RoleType::AGENT:
            newRole = SafeMakeShared<Agent>(ctx);
            break;
        case RoleType::INITIALIZER:
            newRole = SafeMakeShared<Initializer>();
            break;
    }
    if (!newRole) {
        UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared currentRole failed.";
        return;
    }
    // 读取旧角色类型与交换指针在同一临界区内，保证决策与替换原子一致（避免 TOCTOU）
    RoleType oldRoleType = RoleType::INITIALIZER;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (currentRole_) {
            oldRoleType = currentRole_->GetRoleType();
        }
        currentRole_ = std::move(newRole);
    }
    switch (roleType) {
        case RoleType::MASTER:
            if (oldRoleType != RoleType::STANDBY) {
                RoleChangeNotifyAsync(UbseElectionEventType::CHANGE_TO_MASTER, ctx.masterId);
            }
            RoleChangeNotifyAsync(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION, ctx.masterId);
            UBSE_LOG_INFO << "[ELECTION] SwitchRole Master, node_id = " << ctx.masterId << ".";
            break;
        case RoleType::STANDBY:
            RoleChangeNotifyAsync(UbseElectionEventType::CHANGE_TO_STANDBY, ctx.standbyId);
            UBSE_LOG_INFO << "[ELECTION] SwitchRole Standby: " << ctx.standbyId << ".";
            break;
        case RoleType::AGENT:
            RoleChangeNotifyAsync(UbseElectionEventType::CHANGE_TO_AGENT, ctx.standbyId);
            UBSE_LOG_INFO << "[ELECTION] SwitchRole Agent.";
            RoleChangeNotifyAsync(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION, ctx.masterId);
            UBSE_LOG_INFO << "[ELECTION] The Master is online: " << ctx.masterId << ", turnId is: " << ctx.turnId;
            break;
        case RoleType::INITIALIZER:
            UBSE_LOG_INFO << "[ELECTION] SwitchRole Initializer.";
            break;
    }
}

uint32_t RoleMgr::RoleChangeAttach(UbseElectionEventType type, UbseElectionHandler handler)
{
    std::unique_lock<std::recursive_mutex> uniqueLock(mProcessorLock_);
    // 名称查重
    auto& handlers = handlers_[type];
    if (std::any_of(handlers.begin(), handlers.end(), [&](const auto& h) { return h->name == handler.name; })) {
        UBSE_LOG_ERROR << "[ELECTION] RoleChangeAttach handler name dup - " << handler.name;
        return UbseElectionDupNameError;
    }

    // 创建共享对象
    HandlerPtr handler_ptr = SafeMakeShared<UbseElectionHandler>(handler);
    if (!handler_ptr) {
        UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared currentRole failed.";
        return UbseElectionHandlerError;
    }
    active_handlers_.push_back(handler_ptr);

    auto safe_handler_ptr =
        SafeMakeShared<SafeHandler>(handler_ptr, handler.name, handler.priority, handler.sequenceId);
    if (!safe_handler_ptr) {
        UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared failed.";
        return UbseElectionHandlerError;
    }
    handlers.emplace_back(safe_handler_ptr);

    // 排序
    std::sort(handlers.begin(), handlers.end(),
              [](const std::shared_ptr<SafeHandler>& a, const std::shared_ptr<SafeHandler>& b) { return *a < *b; });
    return UbseElectionOk;
}

uint32_t RoleMgr::RoleChangeDeAttach(UbseElectionEventType type, UbseElectionHandler handler)
{
    std::unique_lock<std::recursive_mutex> uniqueLock(mProcessorLock_);
    auto it = handlers_.find(type);
    if (it == handlers_.end()) {
        return UbseElectionTypeError;
    }
    auto& handlers = it->second;
    auto orig_size = handlers.size();
    std::string name = handler.name;
    auto it_safe = std::find_if(handlers.begin(), handlers.end(),
                                [&name](const std::shared_ptr<SafeHandler>& ptr) { return ptr->name == name; });
    std::shared_ptr<SafeHandler> saftHandler;
    if (it_safe != handlers.end()) {
        saftHandler = *it_safe;
    } else {
        return UbseElectionHandlerError;
    }
    {
        std::unique_lock<std::mutex> lock(saftHandler->mutex);
        active_handlers_.erase(std::remove_if(active_handlers_.begin(), active_handlers_.end(),
                                              [&](const HandlerPtr& ptr) { return ptr->name == handler.name; }),
                               active_handlers_.end());
        handlers.erase(
            std::remove_if(handlers.begin(), handlers.end(), [&](const auto& h) { return h->name == handler.name; }),
            handlers.end());
    }
    return handlers.size() != orig_size ? UbseElectionOk : UbseElectionHandlerError;
}

UbseResult HandleRoleChangeNotifyError(uint32_t ret, UbseElectionEventType& type, const std::string& handlerName)
{
    if (ret == UbseElectionError) {
        UBSE_LOG_WARN << "[ELECTION] RoleChangeNotify handler error - " << handlerName;
        if (type == UbseElectionEventType::CHANGE_TO_SWITCHOVER) {
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

uint32_t RoleMgr::RoleChangeNotify(UbseElectionEventType type, UBSE_ID_TYPE newId)
{
    std::vector<std::shared_ptr<SafeHandler>> current_handlers;
    {
        std::unique_lock<std::recursive_mutex> uniqueLock(mProcessorLock_);
        auto it = handlers_.find(type);
        if (it != handlers_.end()) {
            current_handlers = it->second;
        }
    }
    uint32_t ret = UbseElectionOk;
    for (const auto& safe_h : current_handlers) {
        UBSE_LOG_INFO << "[ELECTION] RoleChangeNotify handler - " << safe_h->name << ", type=" << int(type);
        std::unique_lock<std::mutex> lock(safe_h->mutex);
        if (auto h = safe_h->weak_handler.lock()) {
            UBSE_ID_TYPE tmpId = newId;
            ret = h->handler(type, tmpId);
            if (HandleRoleChangeNotifyError(ret, type, safe_h->name) == UBSE_ERROR) {
                break;
            }
        }
    }
    if (ret == UbseElectionOk) {
        UbseContext::GetInstance().SetWorkReadiness(IS_READY);
    }
    UBSE_LOG_INFO << "[ELECTION] WorkReadiness is " << UbseContext::GetInstance().GetWorkReadiness()
                  << ",ElectionEventType is " << int(type) << ".";
    return ret;
}

void RoleMgr::RoleChangeNotifyAsync(UbseElectionEventType type, UBSE_ID_TYPE newId)
{
    // 异步执行 RoleChangeNotify
    try {
        std::thread([this, type, newId]() { RoleChangeNotify(type, newId); }).detach();
    } catch (const std::system_error&) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to create thread.";
    }
    return;
}
} // namespace ubse::election