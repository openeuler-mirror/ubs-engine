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

#ifndef UBSE_ELECTION_DATA_CONVERSION_H
#define UBSE_ELECTION_DATA_CONVERSION_H
#include "ubse_election_def.h"
#include "ubse_serial_util.h"
namespace ubse::election::data::conversion {
using namespace ubse::serial;
inline void ElectionPktSerialize(ubse::serial::UbseSerialization &out, ubse::election::ElectionPkt &electionPkt)
{
    out << electionPkt.type << electionPkt.masterId << electionPkt.masterIp << electionPkt.standbyId << electionPkt.turnId
        << electionPkt.sequenceId << electionPkt.agentCount << electionPkt.agentIds << electionPkt.masterStatus
        << electionPkt.standbyStatus << electionPkt.broadcast << electionPkt.queryGroupNodeIds
        << electionPkt.globalMasterId << electionPkt.globalStandbyId;
}
inline void ElectionPktDeserialize(ubse::serial::UbseDeSerialization &in, ubse::election::ElectionPkt &electionPkt)
{
    in >> electionPkt.type >> electionPkt.masterId >> electionPkt.masterIp >> electionPkt.standbyId >> electionPkt.turnId >>
        electionPkt.sequenceId >> electionPkt.agentCount >> electionPkt.agentIds >> electionPkt.masterStatus >>
        electionPkt.standbyStatus >> electionPkt.broadcast >> electionPkt.queryGroupNodeIds
        >> electionPkt.globalMasterId >> electionPkt.globalStandbyId;
}
inline void ElectionReplyPktSerialize(ubse::serial::UbseSerialization &out,
                                      ubse::election::ElectionReplyPkt &electionReplyPkt)
{
    out << electionReplyPkt.type << electionReplyPkt.replyId << electionReplyPkt.groupId << electionReplyPkt.replyResult
        << electionReplyPkt.masterId << electionReplyPkt.standbyId
        << electionReplyPkt.turnId << electionReplyPkt.standbyStatus
        << electionReplyPkt.broadcast << electionReplyPkt.managingGroupNodeIds << electionReplyPkt.cascadeGroupNodeIds
        << electionReplyPkt.cascadeGroupId << electionReplyPkt.cascadeMasterId << electionReplyPkt.cascadeStandbyId;
}
inline void ElectionReplyPktDeserialize(ubse::serial::UbseDeSerialization &in,
                                        ubse::election::ElectionReplyPkt &electionReplyPkt)
{
    in >> electionReplyPkt.type >> electionReplyPkt.replyId >> electionReplyPkt.groupId >>
        electionReplyPkt.replyResult >> electionReplyPkt.masterId >> electionReplyPkt.standbyId >>
        electionReplyPkt.turnId >> electionReplyPkt.standbyStatus >>
        electionReplyPkt.broadcast >> electionReplyPkt.managingGroupNodeIds >> electionReplyPkt.cascadeGroupNodeIds
        >> electionReplyPkt.cascadeGroupId >> electionReplyPkt.cascadeMasterId >> electionReplyPkt.cascadeStandbyId;
}

inline void InterGroupInfoSerialize(ubse::serial::UbseSerialization &out,
                                        ubse::election::InterGroupInfo &interGroupInfo)
{
    out << interGroupInfo.type << interGroupInfo.nodeId << interGroupInfo.groupId << interGroupInfo.groupMasterId
        << interGroupInfo.groupStandbyId << interGroupInfo.groupNodeIds << interGroupInfo.globalMasterId
        << interGroupInfo.globalStandbyId;
}

inline void InterGroupInfoDeserialize(ubse::serial::UbseDeSerialization &in,
                                        ubse::election::InterGroupInfo &interGroupInfo)
{
    in >> interGroupInfo.type >> interGroupInfo.nodeId >> interGroupInfo.groupId >> interGroupInfo.groupMasterId >>
        interGroupInfo.groupStandbyId >> interGroupInfo.groupNodeIds >> interGroupInfo.globalMasterId >>
        interGroupInfo.globalStandbyId;
}
} // namespace ubse::election::data::conversion
#endif // UBSE_ELECTION_DATA_CONVERSION_H
