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

#ifndef UBSE_HEART_BEAT_PKT_H
#define UBSE_HEART_BEAT_PKT_H

#include "ubse_base_message.h"
#include "ubse_election_def.h"
namespace ubse::election::message {
using namespace ubse::message;
class UbseElectionPktSimpo : public UbseBaseMessage {
public:
    UbseElectionPktSimpo() = default;

    explicit UbseElectionPktSimpo(const ElectionPkt &);

    explicit UbseElectionPktSimpo(uint8_t *rawDev, uint32_t size)
    {
        SetInputRawData(rawDev, size);
    }

    inline ElectionPkt GetElectionPkt()
    {
        return electionPkt_;
    }

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

    std::string ToString() const override;

private:
    ElectionPkt electionPkt_{};
};
using UbseElectionPktSimpoPtr = Ref<UbseElectionPktSimpo>;
}

#endif // UBSE_HEART_BEAT_PKT_H
