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

#ifndef UBSE_ELECTION_COMM_MGR_H
#define UBSE_ELECTION_COMM_MGR_H

#include <memory>
#include <shared_mutex>
#include "ubse_com_base.h"
#include "ubse_election.h"
#include "ubse_election_def.h"
#include "ubse_event_module.h"

namespace ubse::election {
using namespace ubse::com;
using namespace ubse::event;

struct NodeLinkInfo {
    std::string nodeId;
    uint32_t ubseLinkState;
    uint32_t timeStamp;
    std::string changeChType;
};

class UbseElectionCommMgr : public UbseComBase {
public:
    /* *
     * @brief 连接到指定的目标节点
     * 内部存一下哪些节点成功connect
     * @param[in] dstId 目标节点的ID
     * @return 成功返回0，不成功返回非0
     */
    UbseElectionCommMgr() = default;
    explicit UbseElectionCommMgr(const std::string &nodeId, const std::string &name) : UbseComBase(nodeId, name) {}
    uint32_t Connect(const UBSE_ID_TYPE &dstIp);
    // 和管理组的master节点建链
    uint32_t ConnectMasterNode(const UBSE_ID_TYPE &dstId, const UBSE_ID_TYPE &dstIp);
    // 和管理组保持长连接，目的查询该管理组的groupMaster
    uint32_t ConnectForGroupMaster(const UBSE_ID_TYPE &dstId, const UBSE_ID_TYPE &dstIp);

    UbseResult DisConnect(const UBSE_ID_TYPE &dstId);

    UbseResult SendElectionPkt(UBSE_ID_TYPE destID, const ElectionPkt &pkt, ElectionReplyPkt &reply);

    std::vector<UBSE_ID_TYPE> GetConnectedNodes() const;
    // 获取建链上的管理组的MasterId
    std::vector<UBSE_ID_TYPE> GetConnectedMasterNodes() const;
    // 获取其他组的长连接nodeId
    std::unordered_map<std::string, UBSE_ID_TYPE> GetInterManagementGroupLinkMap() const;

    UbseResult ElectionResponseHandler(std::string &eventId, std::string &eventMessage);

    UbseResult ElectionFaultHandler(std::string &eventId, std::string &eventMessage);

    UbseResult ElectionTopoChangeHandler(std::string &eventId, std::string &eventMessage);

    UbseResult RegNewChannelCb([[maybe_unused]] UbseComCallBackForHA func) override
    {
        return UBSE_OK;
    };

    UbseResult RegBrokenChannelCb([[maybe_unused]] UbseComCallBackForHA func) override
    {
        return UBSE_OK;
    };

    UbseResult NewChannelCB(const std::string &remoteIp, const std::string &remoteNodeId);

    UbseResult ElectionSubEvent();

    UbseResult Start() override;

    void Stop() override {}

private:
    void ElectionNodeDownNotify(const std::string &nodeId);
    std::vector<UBSE_ID_TYPE> connectedIntraGroupNodes_; // 已建立连接的本组内NodeId列表
    std::vector<UBSE_ID_TYPE> connectedInterMgmtMasters_; // 已建立连接的其他管理组的MasterId列表
    mutable std::shared_mutex mtx_{};
    mutable std::shared_mutex interMgmtMastersMtx_{};
    mutable std::shared_mutex interMgmtGroupLinkMtx_{};
    std::unordered_map<std::string, UBSE_ID_TYPE> interMgmtGrpLinkMap_; // 和其他管理组的长连接节点map
};
} // namespace ubse::election
#endif // UBSE_ELECTION_COMM_MGR_H
