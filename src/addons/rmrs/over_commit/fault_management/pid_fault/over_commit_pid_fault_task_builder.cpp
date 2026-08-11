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

#include "over_commit_pid_fault_task_builder.h"
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include "ubse_logger.h"
#include "mp_configuration.h"
#include "over_commit_pid_fault_state_store.h"

namespace mempooling {

static const std::string TAG = "[OverCommit][PidFault][TaskBuilder] ";
#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << TAG

// 调试用: 把数值列表拼成"[a b c]"形式，方便日志里一行看清集合内容
template <typename T>
static std::string JoinToString(const std::vector<T>& values)
{
    std::ostringstream oss;
    oss << "[";
    for (const auto& v : values) {
        oss << " " << v;
    }
    oss << " ]";
    return oss.str();
}

// 调试用: 打印单个迁移任务的完整画像，真实运行时可据此核对任务构建是否符合预期
static void PrintTaskDetail(const MigrationTask& task)
{
    std::string faultUsageStr;
    for (const auto& usage : task.faultNumaUsages) {
        faultUsageStr += " (numa=" + std::to_string(usage.numaId) + ",pageKB=" + std::to_string(usage.pageSizeKB) +
                         ",usedKB=" + std::to_string(usage.usedMemKB) + ")";
    }
    LOG_DEBUG << "Task built: taskId=" << task.taskId << ", containerId=" << task.containerId
              << ", pids=" << JoinToString(task.pids) << ", migrationSizeKB=" << task.migrationSizeKB
              << ", socketId=" << task.socketId << ", sameSocketConstraint=" << task.hasSameSocketConstraint
              << ", bindType=" << static_cast<uint32_t>(task.bindType)
              << ", localNumaIds=" << JoinToString(task.localNumaIds)
              << ", relatedBorrowIds=" << JoinToString(task.relatedBorrowIds) << ", faultNumaUsages=[" << faultUsageStr
              << " ].";
}

MpResult PidFaultTaskBuilder::BuildTasks(const OverCommitFaultContext& context, std::vector<BorrowInNodePlan>& plans)
{
    LOG_INFO << "BuildTasks start, affectedNodes=" << context.affectedBorrowInNodeIds.size() << ".";

    for (const auto& borrowInNodeId : context.affectedBorrowInNodeIds) {
        BorrowInNodePlan plan;
        MpResult ret = BuildTasksForNode(context, borrowInNodeId, plan);
        if (ret != MEM_POOLING_OK) {
            LOG_WARN << "BuildTasksForNode failed for " << borrowInNodeId << ", skip.";
            continue;
        }
        // 即使没有task也可能有directReturn
        if (!plan.tasks.empty() || !plan.directReturns.empty()) {
            LOG_DEBUG << "Node " << borrowInNodeId << " plan accepted: tasks=" << plan.tasks.size()
                      << ", directReturns=" << plan.directReturns.size() << ".";
            plans.push_back(std::move(plan));
        } else {
            // 既无迁移任务也无直接归还：该节点本轮无事可做，不生成计划
            LOG_DEBUG << "Node " << borrowInNodeId << " plan empty (no task/directReturn), skip.";
        }
    }

    LOG_INFO << "BuildTasks end, plans=" << plans.size() << ".";
    return MEM_POOLING_OK;
}

MpResult PidFaultTaskBuilder::BuildTasksForNode(const OverCommitFaultContext& context,
                                                const std::string& borrowInNodeId, BorrowInNodePlan& plan)
{
    plan.borrowInNodeId = borrowInNodeId;

    // 获取可达性（采集阶段RPC探活结果）；查不到时保持默认不可达，决策阶段会转DEFER
    auto reachIt = context.nodeReachability.find(borrowInNodeId);
    if (reachIt != context.nodeReachability.end()) {
        plan.reachability = reachIt->second;
    } else {
        LOG_DEBUG << "Node " << borrowInNodeId << " reachability not collected, treat as unreachable.";
    }

    // 获取借用方用户信息（master代借时构造借用参数使用）
    auto userIt = context.nodeToBorrowUser.find(borrowInNodeId);
    if (userIt != context.nodeToBorrowUser.end()) {
        plan.borrowUser = userIt->second;
        LOG_DEBUG << "Node " << borrowInNodeId << " borrowUser: uid=" << plan.borrowUser.uid
                  << ", username=" << plan.borrowUser.username << ", socketId=" << plan.borrowUser.socketId << ".";
    } else {
        LOG_DEBUG << "Node " << borrowInNodeId << " has no borrowUser info in ledger.";
    }

    // 获取per-node故障numa列表：为空说明该节点无有效故障远端numa（半边账本已跳过），无需处理
    auto faultNumaIt = context.nodeToFaultNumaIds.find(borrowInNodeId);
    if (faultNumaIt == context.nodeToFaultNumaIds.end() || faultNumaIt->second.empty()) {
        LOG_WARN << "Node " << borrowInNodeId << " has no valid fault numa, skip.";
        return MEM_POOLING_OK;
    }
    const std::vector<uint16_t>& faultNumaIds = faultNumaIt->second;
    LOG_DEBUG << "Node " << borrowInNodeId << " faultNumaIds=" << JoinToString(faultNumaIds)
              << ", ubseReachable=" << plan.reachability.ubseReachable << ".";

    // 剔除待恢复故障numa（借入节点smap纳管查询失败且采集占用非0）: 本轮对其不建迁移task也不归还，
    // 等下轮重新采集重试，避免数据不确定时误归还或借少
    std::unordered_set<uint16_t> pendingNumaSet;
    auto pendingIt = context.nodeToPendingFaultNumaIds.find(borrowInNodeId);
    if (pendingIt != context.nodeToPendingFaultNumaIds.end()) {
        pendingNumaSet = pendingIt->second;
    }
    std::vector<uint16_t> effectiveFaultNumaIds;
    for (uint16_t numaId : faultNumaIds) {
        if (pendingNumaSet.count(numaId) > 0) {
            LOG_INFO << "Node " << borrowInNodeId << " faultNuma=" << numaId
                     << " pending recovery, skip this round.";
            continue;
        }
        effectiveFaultNumaIds.push_back(numaId);
    }
    if (effectiveFaultNumaIds.empty()) {
        LOG_INFO << "Node " << borrowInNodeId << " all fault numas pending recovery, nothing to do this round.";
        return MEM_POOLING_OK;
    }

    // per-numa borrowIds（直接归还按numa精确归还，避免跨numa误归还）
    std::unordered_map<uint16_t, std::vector<std::string>> numaBorrowIds;
    auto numaBorrowIt = context.nodeToNumaBorrowIds.find(borrowInNodeId);
    if (numaBorrowIt != context.nodeToNumaBorrowIds.end()) {
        numaBorrowIds = numaBorrowIt->second;
    }

    // 获取该节点的PID内存分布
    auto pidIt = context.nodeToPidMemInfos.find(borrowInNodeId);
    if (pidIt == context.nodeToPidMemInfos.end() || pidIt->second.empty()) {
        // 无PID信息：所有故障numa都无使用 → 逐numa直接归还该numa上的borrowId
        for (uint16_t faultNumaId : effectiveFaultNumaIds) {
            auto idIt = numaBorrowIds.find(faultNumaId);
            if (idIt == numaBorrowIds.end() || idIt->second.empty()) {
                LOG_DEBUG << "Node " << borrowInNodeId << " faultNuma=" << faultNumaId
                          << " has no borrowIds, nothing to return.";
                continue;
            }
            DirectReturnTask drTask;
            drTask.faultNumaId = faultNumaId;
            drTask.relatedBorrowIds = idIt->second;
            LOG_DEBUG << "Node " << borrowInNodeId << " faultNuma=" << faultNumaId
                      << " direct return borrowIds=" << JoinToString(drTask.relatedBorrowIds) << ".";
            plan.directReturns.push_back(std::move(drTask));
        }
        LOG_INFO << "Node " << borrowInNodeId << " has no PID usage, mark DirectReturn.";
        return MEM_POOLING_OK;
    }

    const auto& pidInfos = pidIt->second;
    MpSceneType sceneType = MpConfiguration::GetInstance().GetMpSceneType();

    // 获取该节点的borrowIds
    std::vector<std::string> borrowIds;
    auto borrowIdIt = context.nodeToBorrowIds.find(borrowInNodeId);
    if (borrowIdIt != context.nodeToBorrowIds.end()) {
        borrowIds = borrowIdIt->second;
    }

    // 过滤出在故障numa上有远端内存的PID（faultNumaUsages在采集端已按故障numa过滤）
    std::vector<PidMemInfo> affectedPidInfos;
    for (const auto& info : pidInfos) {
        if (!info.faultNumaUsages.empty()) {
            affectedPidInfos.push_back(info);
        }
    }
    LOG_DEBUG << "Node " << borrowInNodeId << " pidInfos=" << pidInfos.size()
              << ", affectedPids=" << affectedPidInfos.size() << ", sceneType=" << static_cast<uint32_t>(sceneType)
              << ".";

    if (affectedPidInfos.empty()) {
        // 所有故障numa无PID使用 → 逐numa直接归还
        for (uint16_t faultNumaId : effectiveFaultNumaIds) {
            auto idIt = numaBorrowIds.find(faultNumaId);
            if (idIt == numaBorrowIds.end() || idIt->second.empty()) {
                continue;
            }
            DirectReturnTask drTask;
            drTask.faultNumaId = faultNumaId;
            drTask.relatedBorrowIds = idIt->second;
            plan.directReturns.push_back(std::move(drTask));
        }
        LOG_INFO << "Node " << borrowInNodeId << " fault numa has no PID usage, mark DirectReturn.";
        return MEM_POOLING_OK;
    }

    // 有PID使用的故障numa走迁移-归还；完全无PID使用的故障numa仍可直接归还
    std::unordered_set<uint16_t> usedFaultNumas;
    for (const auto& info : affectedPidInfos) {
        for (const auto& usage : info.faultNumaUsages) {
            usedFaultNumas.insert(usage.numaId);
        }
    }
    for (uint16_t faultNumaId : effectiveFaultNumaIds) {
        if (usedFaultNumas.count(faultNumaId) > 0) {
            // 该numa上有PID占用 → 不能直接归还，走迁移-归还链路
            LOG_DEBUG << "Node " << borrowInNodeId << " faultNuma=" << faultNumaId
                      << " has PID usage, go migrate-return path.";
            continue;
        }
        auto idIt = numaBorrowIds.find(faultNumaId);
        if (idIt == numaBorrowIds.end() || idIt->second.empty()) {
            LOG_DEBUG << "Node " << borrowInNodeId << " faultNuma=" << faultNumaId
                      << " no usage and no borrowIds, skip.";
            continue;
        }
        DirectReturnTask drTask;
        drTask.faultNumaId = faultNumaId;
        drTask.relatedBorrowIds = idIt->second;
        LOG_DEBUG << "Node " << borrowInNodeId << " faultNuma=" << faultNumaId
                  << " no PID usage, direct return borrowIds=" << JoinToString(drTask.relatedBorrowIds) << ".";
        plan.directReturns.push_back(std::move(drTask));
    }

    // 根据场景类型构建task
    if (sceneType == MpSceneType::CONTAINER_SCENE) {
        AggregateContainerTasks(affectedPidInfos, borrowIds, plan.tasks);
    } else {
        BuildVmTasks(affectedPidInfos, borrowIds, plan.tasks);
    }

    // 同socket约束保留在task级（约束粒度=单次借用），决策阶段按socket分组分笔借用，
    // 避免同节点跨socket的task（如pid0∈socket0、pid1∈socket1）被合并成单笔借用破坏亲和性
    LOG_INFO << "Node " << borrowInNodeId << ": tasks=" << plan.tasks.size()
             << ", directReturns=" << plan.directReturns.size() << ".";
    return MEM_POOLING_OK;
}

void PidFaultTaskBuilder::AggregateContainerTasks(const std::vector<PidMemInfo>& pidInfos,
                                                  const std::vector<std::string>& borrowIds,
                                                  std::vector<MigrationTask>& tasks)
{
    // 按instanceId(容器场景=containerId)聚合，解析不到时按pid独立成组
    std::unordered_map<std::string, std::vector<const PidMemInfo*>> containerMap;
    for (const auto& info : pidInfos) {
        std::string key = info.instanceId.empty() ? ("pid_" + std::to_string(info.pid)) : info.instanceId;
        containerMap[key].push_back(&info);
    }
    LOG_DEBUG << "AggregateContainerTasks: affectedPids=" << pidInfos.size()
              << ", containerGroups=" << containerMap.size() << ".";

    for (const auto& [instanceId, infos] : containerMap) {
        MigrationTask task;
        // HDWTODO:这里需要防御一下，因为万一containerId包含pid_前缀，会导致后续逻辑错误
        task.containerId = (instanceId.find("pid_") == 0) ? "" : instanceId;
        task.migrationSizeKB = 0;
        task.relatedBorrowIds = borrowIds;

        for (const auto* info : infos) {
            task.pids.push_back(info->pid);
            task.bindType = info->bindType;
            // 同socket约束由bindType推导: BIND_MULTIPLE=无约束，其余=有约束
            task.hasSameSocketConstraint = (info->bindType != NumaBindType::BIND_MULTIPLE);
            // 故障远端明细直接累加
            for (const auto& usage : info->faultNumaUsages) {
                task.faultNumaUsages.push_back(usage);
                task.migrationSizeKB += usage.usedMemKB;
            }
            // 本地numa去重合并
            for (uint16_t localNumaId : info->localNumaIds) {
                if (std::find(task.localNumaIds.begin(), task.localNumaIds.end(), localNumaId) ==
                    task.localNumaIds.end()) {
                    task.localNumaIds.push_back(localNumaId);
                }
            }
            if (task.socketId < 0 && info->socketId >= 0) {
                task.socketId = info->socketId;
            }
        }

        // 生成taskId
        std::string idKey = task.containerId.empty() ? std::to_string(task.pids[0]) : task.containerId;
        task.taskId = "container_" + idKey;
        task.phase = TaskPhase::NONE;

        PrintTaskDetail(task);
        tasks.push_back(std::move(task));
    }
}

void PidFaultTaskBuilder::BuildVmTasks(const std::vector<PidMemInfo>& pidInfos,
                                       const std::vector<std::string>& borrowIds, std::vector<MigrationTask>& tasks)
{
    // 虚机场景: 一个VM对应一个qemu进程，按PID一对一生成迁移任务（区别于容器场景按instanceId聚合多pid）
    for (const auto& info : pidInfos) {
        MigrationTask task;
        // 迁移对象: 单PID（虚机进程），执行阶段逐pid下发smap迁移
        task.pids = {info.pid};
        task.bindType = info.bindType;
        // 同socket约束由bindType推导: BIND_MULTIPLE=无约束，其余=有约束；
        // 有约束时决策阶段BFD只能选同socket的借出numa，保证迁移后不破坏业务亲和性
        task.hasSameSocketConstraint = (info.bindType != NumaBindType::BIND_MULTIPLE);
        // 关联故障numa上的全部旧借用id，供迁移完成后条件归还（共享id需全部引用task迁移完才可还）
        task.relatedBorrowIds = borrowIds;

        // 故障远端明细/本地numa/socket直接取自采集结果（单pid无需像容器场景那样聚合去重）
        task.faultNumaUsages = info.faultNumaUsages;
        // 总迁移量 = 各故障numa实际占用之和，决策阶段据此计算新借用需求（当前只会有一个故障numa，这里是带扩展编程）
        task.migrationSizeKB = 0;
        for (const auto& usage : task.faultNumaUsages) {
            task.migrationSizeKB += usage.usedMemKB;
        }
        task.localNumaIds = info.localNumaIds;
        task.socketId = info.socketId;

        // taskId全局唯一且可重建（vm_+pid），持久化/RESUME时据此匹配历史任务状态
        task.taskId = "vm_" + std::to_string(info.pid);
        // 初始相位NONE（NEW任务），若存在同id持久化状态，后续流程会合并为RESUME相位
        task.phase = TaskPhase::NONE;

        PrintTaskDetail(task);
        tasks.push_back(std::move(task));
    }
}

} // namespace mempooling
