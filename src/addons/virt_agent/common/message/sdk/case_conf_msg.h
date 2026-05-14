/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef CASE_CONF_MSG_H
#define CASE_CONF_MSG_H

#include "base_message.h"

namespace vm {

constexpr uint32_t VIRT_MAX_CUR_CASE_LENGTH = 128;
constexpr uint32_t VIRT_MAX_OVERCOMMITMENT_RATIO_LENGTH = 128;
constexpr uint32_t VIRT_MAX_MIGRATE_WATER_LINE_LENGTH = 128;
constexpr uint32_t VIRT_MAX_MSG_LENGTH = 128;
constexpr uint32_t VIRT_MAX_NODE_ID_LENGTH = 48;

typedef struct {
    char cur_case[VIRT_MAX_CUR_CASE_LENGTH];
    char over_commitment_ratio[VIRT_MAX_OVERCOMMITMENT_RATIO_LENGTH];
    char migrate_waterLine[VIRT_MAX_MIGRATE_WATER_LINE_LENGTH];
    uint64_t index;
    char host_id[VIRT_MAX_NODE_ID_LENGTH];
} case_conf_info_t;

typedef struct {
    uint32_t ret;
} case_conf_set_info_t;

struct CaseConfInfo {
    std::string curCase{};
    std::string overCommitmentRatio{};
    std::string migrateWaterLine{};
    uint64_t index{};
    std::string host_id{};
};

struct CaseConfSetInfo {
    std::string caseType{};
    float overCommitmentRatio{};
    std::string msg{};
    uint32_t ret{};
};

class CaseConfGetMsg : public BaseMessage {
public:
    CaseConfGetMsg() = default;

    explicit CaseConfGetMsg(CaseConfInfo caseConf) : caseConf_(std::move(caseConf)){};

    explicit CaseConfGetMsg(uint8_t* rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    void SetCaseConf(CaseConfInfo& caseConf)
    {
        caseConf_ = caseConf;
    }

    case_conf_info_t GetCaseConf();

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    CaseConfInfo caseConf_{};
};

class CaseConfSetMsg : public BaseMessage {
public:
    CaseConfSetMsg() = default;

    explicit CaseConfSetMsg(CaseConfSetInfo caseConf) : caseConf_(std::move(caseConf)){};

    explicit CaseConfSetMsg(uint8_t* rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    void SetCaseConf(CaseConfSetInfo& caseConf)
    {
        caseConf_ = caseConf;
    }

    case_conf_set_info_t GetCaseConf();

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    CaseConfSetInfo caseConf_{};
};

} // namespace vm

#endif // CASE_CONF_MSG_H
