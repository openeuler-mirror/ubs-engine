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
    out << electionPkt.type << electionPkt.masterId << electionPkt.standbyId << electionPkt.turnId
        << electionPkt.sequenceId << electionPkt.agentCount << electionPkt.agentIds << electionPkt.masterStatus
        << electionPkt.standbyStatus << electionPkt.broadcast << electionPkt.rev1 << electionPkt.rsv2;
}
inline void ElectionPktDeserialize(ubse::serial::UbseDeSerialization &in, ubse::election::ElectionPkt &electionPkt)
{
    in >> electionPkt.type >> electionPkt.masterId >> electionPkt.standbyId >> electionPkt.turnId >>
        electionPkt.sequenceId >> electionPkt.agentCount >> electionPkt.agentIds >> electionPkt.masterStatus >>
        electionPkt.standbyStatus >> electionPkt.broadcast >> electionPkt.rev1 >> electionPkt.rsv2;
}
inline void ElectionReplyPktSerialize(ubse::serial::UbseSerialization &out,
                                      ubse::election::ElectionReplyPkt &electionReplyPkt)
{
    out << electionReplyPkt.type << electionReplyPkt.replyId << electionReplyPkt.replyResult
        << electionReplyPkt.masterId << electionReplyPkt.turnId << electionReplyPkt.standbyStatus
        << electionReplyPkt.broadcast << electionReplyPkt.rsv << electionReplyPkt.length;
}
inline void ElectionReplyPktDeserialize(ubse::serial::UbseDeSerialization &in,
                                        ubse::election::ElectionReplyPkt &electionReplyPkt)
{
    in >> electionReplyPkt.type >> electionReplyPkt.replyId >> electionReplyPkt.replyResult >>
        electionReplyPkt.masterId >> electionReplyPkt.turnId >> electionReplyPkt.standbyStatus >>
        electionReplyPkt.broadcast >> electionReplyPkt.rsv >> electionReplyPkt.length;
}
} // namespace ubse::election::data::conversion
#endif // UBSE_ELECTION_DATA_CONVERSION_H
