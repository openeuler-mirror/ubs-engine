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
using ubse::com::UbseComBase;
using ubse::com::UbseComCallBackForHA;
using ubse::com::UbseResult;

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
    explicit UbseElectionCommMgr(const std::string& nodeId, const std::string& name) : UbseComBase(nodeId, name) {}
    uint32_t Connect(const UBSE_ID_TYPE& dstIp);

    UbseResult DisConnect(const UBSE_ID_TYPE& dstId);

    UbseResult SendElectionPkt(UBSE_ID_TYPE destID, const ElectionPkt& pkt, ElectionReplyPkt& reply);

    std::vector<UBSE_ID_TYPE> GetConnectedNodes() const;

    UbseResult ElectionResponseHandler(std::string& eventId, std::string& eventMessage);

    UbseResult ElectionFaultHandler(std::string& eventId, std::string& eventMessage);

    UbseResult ElectionTopoChangeHandler(std::string& eventId, std::string& eventMessage);

    UbseResult RegNewChannelCb([[maybe_unused]] UbseComCallBackForHA func) override
    {
        return UBSE_OK;
    };

    UbseResult RegBrokenChannelCb([[maybe_unused]] UbseComCallBackForHA func) override
    {
        return UBSE_OK;
    };

    UbseResult NewChannelCB(const std::string& remoteIp, const std::string& remoteNodeId);

    UbseResult ElectionSubEvent();

    UbseResult Start() override;

    void Stop() override {}

private:
    void ElectionNodeDownNotify(const std::string& nodeId);
    std::vector<UBSE_ID_TYPE> connectSuccessNodes_;
    mutable std::shared_mutex mtx_{};
};
} // namespace ubse::election
#endif // UBSE_ELECTION_COMM_MGR_H
