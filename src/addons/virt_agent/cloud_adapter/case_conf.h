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

#ifndef VM_CASE_CONF_H
#define VM_CASE_CONF_H

#include <cmath>

#include <ubse_def.h>

#include "vm_error.h"

namespace vm {

constexpr int WAIT_TIME = 5; // Unit: s
constexpr int WAIT_TIME_FOR_SET_MODE = 1;
constexpr int OVER_COMMITMENT_MODE = 0;
constexpr int MEM_FRAGMENTATION_MODE = 1;
constexpr float_t MEM_FRAGMENTATION_RATIO = 1.0;   // The overcommitment ratio in fragmentation scenarios is 1.0.
constexpr float_t MAX_OVER_COMMITMENT_RATIO = 1.8; // Maximum overcommitment ratio, which ranges from 1.0 to 1.8.
const std::string OVER_COMMITMENT_CASE = "overCommitment";
const std::string MEM_FRAGMENTATION_CASE = "memFragmentation";
const std::string CASE_CONF_KEY_PREFIX = "ubse_";
const std::string CASE_CONF_KEY = "vm_caseConf";

struct CaseAndOvercommitmentRatio {
    std::string curCase{};
    float_t overCommitmentRatio{};
    uint64_t index{};
};

// Parameters related to the scenario and overcommitment ratio
struct CaseConfParam {
    bool FromJson(const std::string &jsonString);

    std::string caseType{};
    float_t overCommitmentRatio{};
};

struct CaseConfResultParam {
    std::string ToJson();

    uint32_t ret{};
    std::string msg{};
    std::string data{};
};

VmResult SetCaseConfToDB(CaseConfParam &caseParam);

class CaseConf {
public:
    static CaseConf &GetInstance();
    VmResult Init();
    static VmResult QueryCaseAndOverCommitmentRatio(CaseAndOvercommitmentRatio &caseConf);
    static void CaseRegisterRun();
    static void RunQueryCaseConf();

private:
    CaseConf() = default;
    ~CaseConf() = default;
    static bool SetMemPoolingRunMode(const std::string &curCase);
    static bool SetMemPoolingWaterMark();
    static void SetMemPoolingParams(const std::string &curCase);
    static void UbseStorageDealData(const std::string &keyPrefix, const std::string &key, const UbseByteBuffer &buff,
                                    void *ctx);
    static VmResult CaseAndOvercommitmentRatioDeserial(const std::string &data,
                                                       CaseAndOvercommitmentRatio &caseAndOvercommitmentRatio);
    static UbseByteBuffer caseConfBuffer;
    static uint64_t index;
    static bool ConvertOverCommitmentRatioToFloat(const std::string &ratioStr, float_t &ratio);
};
} // namespace vm

#endif // VM_CASE_CONF_H
