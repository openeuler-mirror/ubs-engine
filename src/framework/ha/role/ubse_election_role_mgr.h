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

#ifndef UBSE_ELECTION_ROLE_MGR_H
#define UBSE_ELECTION_ROLE_MGR_H

#include <iostream>
#include <memory>
#include <shared_mutex>
#include <utility>
#include "../ubse_election_comm_mgr.h"
#include "../ubse_election_def.h"
#include "../ubse_election_node_mgr.h"
#include "ubse_com_module.h"
#include "ubse_election_module.h"
#include "ubse_election_role.h"
#include "ubse_election_role_agent.h"
#include "ubse_election_role_initializer.h"
#include "ubse_election_role_master.h"
#include "ubse_election_role_standby.h"
#include "ubse_node_mgr.h"

namespace ubse::election {
#define MODULE_LOG_NAME "ubse"
class RoleMgr {
public:
    RoleMgr()
    {
        UbseElectionNodeMgr &nodeMgr = UbseElectionNodeMgr::GetInstance();
        Node myself;
        nodeMgr.GetMyselfNode(myself);
        currentRole_ = SafeMakeShared<Initializer>();
        if (!currentRole_) {
            UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared Initializer currentRole failed.";
        }
        commMgr_ = SafeMakeShared<UbseElectionCommMgr>(myself.id, "UbseMasterRpcServer");
        if (!commMgr_) {
            UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared Initializer commMgr failed.";
        }
        std::lock_guard<std::mutex> lock(mutex_);
        // 初始化管理组数量缓存
        InitManagingGroupCount();
        std::unordered_map<uint16_t, std::vector<nodeMgr::UbseNodeStaticInfo>> nodeMap =
            nodeMgr::GetAllNodesStoredByGroup();
        auto discoveryTargetMap = ComputeDiscoveryTargets(myself.id, nodeMap);
        for (const auto &kv : discoveryTargetMap) {
            UBSE_LOG_INFO << "[ELECTION] discoveryTargetMap:group Id = " << kv.first << ", node id = " << kv.second;
            if (kv.second != myself.id) {
                discoveryTargetByGroup[kv.first] = kv.second;
            }
        }
        // 失败计数初始化
        for (const auto& kv : discoveryTargetByGroup) {
            connFailedCntByGroup[kv.first] = 0;
        }
    };

    static RoleMgr &GetInstance()
    {
        static RoleMgr roleMgr;
        return roleMgr;
    }

    std::shared_ptr<ElectionRole> GetRole();

    std::shared_ptr<ElectionRole> GetGlobalRole();

    void SwitchRole(RoleType roleType, RoleContext &ctx);

    void SwitchGlobalRole(GlobalRoleType globalRoleType, RoleContext &ctx);

    std::shared_ptr<UbseElectionCommMgr> GetCommMgr()
    {
        return commMgr_;
    };

    uint32_t RoleChangeAttach(UbseElectionEventType type, UbseElectionHandler handler);

    uint32_t RoleChangeDeAttach(UbseElectionEventType type, UbseElectionHandler handler);

    uint32_t RoleChangeNotify(UbseElectionEventType type, UBSE_ID_TYPE newId);

    void RoleChangeNotifyAsync(UbseElectionEventType type, UBSE_ID_TYPE newId);
    std::string GetIpById(const UBSE_ID_TYPE &id);

    uint32_t RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt &rcvPkt, ElectionReplyPkt &reply);
    void ProcTimer();
    void GlobalProcTimer();
    void ConnectInterManagingGroup(); // 不同管理组之间建立的连接
    void QueryManagingMaster();
    std::vector<UBSE_ID_TYPE> GetManagingGroupMasterIds();
    UBSE_ID_TYPE GetGlobalMasterId();
    
    /* *
     * 获取指定组信息
     * @param groupId 组ID
     * @param state 输出参数，返回组信息
     * @return UBSE_OK 成功，UBSE_ERROR 失败
     */
    UbseResult GetGroupState(const UBSE_ID_TYPE &groupId, GroupSummaryInfo &state);
    
    /* *
     * 获取所有组信息
     * @return 组信息映射表
     */
    std::unordered_map<UBSE_ID_TYPE, GroupSummaryInfo> GetAllGroupStates();

    /* *
     * 获取挂载组的主备信息
     * @param groupId 挂载组ID
     * @param state 输出参数，返回挂载组信息
     * @return UBSE_OK 成功，UBSE_ERROR 失败
     */
    UbseResult GetMountedGroupState(const UBSE_ID_TYPE &groupId, GroupSummaryInfo &state);

    /* *
     * @brief 获取挂载组主备信息
     * @param groupId 挂载组ID
     * @param state 挂载组主备信息
     */
    std::unordered_map<UBSE_ID_TYPE, GroupSummaryInfo> GetMountedGroupStates();

    /* *
     * @brief 获取管理组节点ID列表
     * @param groupId 挂载组ID
     * @param state 挂载组主备信息
     */
    std::unordered_map<UBSE_ID_TYPE, std::vector<UBSE_ID_TYPE>> GetManagingGroupNodeIdsMap();

    /* *
     * @brief 获取挂载到管理组的挂载组节点ID列表
     * @param groupId 挂载组ID
     * @param state 挂载组主备信息
     */
    std::unordered_map<UBSE_ID_TYPE, std::vector<UBSE_ID_TYPE>> GetMountedGroupNodeIdsMap();

    /* *
     * @brief 获取管理组的挂载组Master映射
     * @param groupId 挂载组ID
     * @param state 挂载组主备信息
     */
    std::unordered_map<UBSE_ID_TYPE, UBSE_ID_TYPE> GetManagingGroupMountedMasterMap();

    /* *
     * @brief 更新挂载组主备信息
     * @param groupId 挂载组ID
     * @param state 挂载组主备信息
     */
    void UpdateMountedGroupState(const UBSE_ID_TYPE &groupId, const GroupSummaryInfo &state);

    /* *
     * @brief 更新管理组节点ID列表
     * @param groupId 管理组ID
     * @param nodeIds 该管理组的节点ID列表
     */
    void UpdateManagingGroupNodeIds(const UBSE_ID_TYPE &groupId, const std::vector<UBSE_ID_TYPE> &nodeIds);

    /* *
     * @brief 更新挂载到管理组的挂载组节点ID列表
     * @param groupId 管理组ID
     * @param nodeIds 挂载到该管理组的挂载组节点ID列表
     */
    void UpdateMountedGroupNodeIds(const UBSE_ID_TYPE &groupId, const std::vector<UBSE_ID_TYPE> &nodeIds);

    /* *
     * @brief 更新管理组的挂载组Master映射
     * @param groupId 管理组ID
     * @param mountedMasterId 挂载到该管理组的挂载组Master节点ID
     */
    void UpdateManagingGroupMountedMaster(const UBSE_ID_TYPE &groupId, const UBSE_ID_TYPE &mountedMasterId);

    /* *
     * 更新发现连接目标节点（当挂载组建链到非Master节点后，通过QUERY获取真正的Master）
     * @param groupId 组ID
     * @param targetNodeId 新的目标节点ID（真正的管理组Master）
     */
    void UpdateDiscoveryTarget(const UBSE_ID_TYPE &groupId, const UBSE_ID_TYPE &targetNodeId);

    /* *
     * 判断指定组是否为管理组
     * @param groupId 组ID
     * @return true 表示该组为管理组，false 表示该组为挂载组或输入无效
     */
    bool IsManagingGroup(const UBSE_ID_TYPE &groupId);

private:
    void InitManagingGroupCount();

    std::unordered_map<std::string, std::string> ComputeDiscoveryTargets(const std::string &myNodeId,
        const std::unordered_map<uint16_t, std::vector<nodeMgr::UbseNodeStaticInfo>> &nodeMap);

    UbseResult SendQueryInformation(UBSE_ID_TYPE destId, const ElectionPkt &pkt);
    std::mutex queryMutex_{};
    std::mutex discoveryMtx_{};
    std::shared_mutex topologyMtx_{};
    std::unordered_map<std::string, UBSE_ID_TYPE> discoveryTargetByGroup{}; // groupId:长连节点
    std::unordered_map<std::string, uint32_t> connFailedCntByGroup{}; // groupId:丢失次数
    std::unordered_map<std::string, GroupSummaryInfo> groupStates{}; // groupId:主备信息(管理组)
    std::unordered_map<UBSE_ID_TYPE, GroupSummaryInfo> mountedGroupStates{}; // groupId:主备信息(挂载组)
    // 管理组GroupId:管理组节点idList
    std::unordered_map<UBSE_ID_TYPE, std::vector<UBSE_ID_TYPE>> managingGroupNodeIdsMap{};
    // 管理组GroupId:挂载到该组的挂载组节点idList
    std::unordered_map<UBSE_ID_TYPE, std::vector<UBSE_ID_TYPE>> mountedGroupNodeIdsMap{};
    // 管理组GroupId:上一次上报的挂载组MasterId
    std::unordered_map<UBSE_ID_TYPE, UBSE_ID_TYPE> managingGroupMountedMasterMap{};
    UBSE_ID_TYPE globalMasterId_{}; // 通过query报文查询得到
    uint16_t managingGroupCount_ = 0; // 管理组数量缓存
    static std::shared_ptr<RoleMgr> instance_;
    std::shared_ptr<ElectionRole> currentRole_;
    std::shared_ptr<ElectionRole> globalCurrentRole_ = nullptr;
    std::shared_ptr<UbseElectionCommMgr> commMgr_;
    // 使用 shared_ptr 存储 Handler
    using HandlerPtr = std::shared_ptr<UbseElectionHandler>;
    using HandlerWeakPtr = std::weak_ptr<UbseElectionHandler>;

    // 安全句柄结构
    struct SafeHandler {
        HandlerWeakPtr weak_handler;
        std::string name;
        UbseElectionHandlerPriority priority;
        int sequenceId;
        std::mutex mutex;

        SafeHandler(HandlerWeakPtr weak_handler, std::string name, UbseElectionHandlerPriority priority, int sequenceId)
            : weak_handler(std::move(weak_handler)),
              name(std::move(name)),
              priority(priority),
              sequenceId(sequenceId)
        {
        }

        bool operator<(const SafeHandler &other) const
        {
            if (priority != other.priority) {
                return priority < other.priority;
            }
            return sequenceId < other.sequenceId;
        }
    };
    std::vector<HandlerPtr> active_handlers_;
    std::unordered_map<UbseElectionEventType, std::vector<std::shared_ptr<SafeHandler>>> handlers_;
    std::recursive_mutex mProcessorLock_{};
    std::mutex mutex_;
};
#undef MODULE_LOG_NAME
} // namespace ubse::election
#endif // UBSE_ELECTION_ROLE_MGR_H
