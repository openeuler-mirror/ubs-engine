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
#ifndef UBSE_MEM_VALIDATOR_H
#define UBSE_MEM_VALIDATOR_H
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_mmi_interface.h"
#include "mem_pool_strategy.h"

namespace ubse::mem::strategy {
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::common::def;
constexpr uint64_t CHECK_MEMORY_CONFIG_VALID = 1 << 0;
constexpr uint64_t FILTER_LEND_NODE_HAS_BORROWED = 1 << 1;
constexpr uint64_t FILTER_NODE_IS_LENDER = 1 << 2;
constexpr uint64_t FILTER_NUMA_BY_SAME_PLANE = 1 << 3;
constexpr uint64_t FILTER_NODE_BY_GROUP = 1 << 4;
constexpr uint64_t FILTER_NODE_IS_DOWN = 1 << 5;
constexpr uint64_t FILTER_CANDIDATE_NODE_LIST = 1 << 6;
constexpr uint64_t FILTER_NUMA_BY_LEND_SOCKET = 1 << 7;
constexpr uint64_t FILTER_SHARE_BY_SAME_PLANE = 1 << 8;
constexpr uint64_t FILTER_LEND_TIME_OUT = 1 << 9;
constexpr uint64_t FILTER_SHARE_NODE_LIST = 1 << 10;
constexpr uint64_t FILTER_SOCKET_BORROW_SIZE_LIMIT = 1 << 11;
constexpr uint64_t FILTER_SHARE_BY_LENDER = 1 << 12;
constexpr uint64_t FILTER_LINK_PORT_DOWN = 1 << 13;

// 指定借出节点
constexpr uint64_t CHECK_BORROW_SIZE_MEET_LIMIT = 1 << 15;
constexpr uint64_t CHECK_BORROW_NODE_HAS_LENT = 1 << 16;
constexpr uint64_t CHECK_LEND_NODE_HAS_BORROWED = 1 << 17;
constexpr uint64_t CHECK_LEND_NODE_IS_LENDER = 1 << 18;
constexpr uint64_t CHECK_LEND_NUMA_IS_ENOUGH = 1 << 19;
constexpr uint64_t CHECK_LEND_NODE_IS_IN_GROUP = 1 << 20;
constexpr uint64_t CHECK_LEND_NODE_IS_IN_CANDIDATELIST = 1 << 21;
constexpr uint64_t CHECK_NODE_IS_DOWN = 1 << 22;
class UbseMemValidator {
public:
    UbseMemValidator()
    {
        InitStatus();
    };

    inline tc::rs::mem::UbseStatus GetUbseStatus()
    {
        return ubseStatus_;
    }

    UbseResult CheckAndFilterParam(uint64_t checkMaskCode);

public:
    std::string importNodeId_;
    size_t requestSize_;
    std::vector<std::string> candidateNodeList_;
    tc::rs::mem::UbseStatus ubseStatus_;
    int srcSocket_{-1}; // 内存申请需求方节点socket信息 -1不限制
    int srcNuma_{-1};   // 内存申请需求方节点NUMA信息 -1不限制
    std::string exportNodeId_;
    std::vector<ubse::adapter_plugins::mmi::UbseNumaLocation> lenderLocs_;
    UbseMemLenderLinkInfo linkInfo_;
    UbseMemLenderInfo lenderInfo_;
    std::vector<uint64_t> lenderSizes_;
    std::vector<std::string> providerList_;

private:
    void InitStatus();

    UbseResult DisPatchHandler(uint64_t checkCode);
    UbseResult FilterLendNodeHasBorrowed();
    UbseResult FilterNodeIsLender();
    UbseResult FilterNumaBySamePlane();
    UbseResult FilterCandidateNodeList();
    UbseResult FilterNodeByGroup();
    UbseResult FilterNodeIsDown();
    UbseResult FilterShareNodeList();
    UbseResult FilterNumaByLendSocket();
    UbseResult FilterInvalidSocketLendTimes();
    UbseResult FilterByLenderInfo();
    UbseResult FilterShareBySamePlane();
    UbseResult FilterByLinkPortDown();

    UbseResult CheckMemoryConfigIsValid();
    UbseResult CheckBorrowSizeMeetLimit();
    UbseResult CheckBorrowNodeHasLent();
    UbseResult CheckLendNodeHasBorrowed();
    UbseResult CheckLendNodeIsLender();
    UbseResult CheckLendNumaIsEnough();
    UbseResult CheckLendNodeIsInGroup();
    UbseResult CheckLendNodeIsInCandidatelist();
    UbseResult CheckLendNodeIsDown();
    UbseResult CheckLenderInfoIsValid() const;

    // 定义函数指针类型
    using CheckHandlerFunc = UbseResult (UbseMemValidator::*)();

    // 创建处理函数映射表
    const std::map<uint64_t, CheckHandlerFunc> g_checkHandlers = {
        {CHECK_MEMORY_CONFIG_VALID, &UbseMemValidator::CheckMemoryConfigIsValid},
        {CHECK_BORROW_SIZE_MEET_LIMIT, &UbseMemValidator::CheckBorrowSizeMeetLimit},
        {FILTER_LEND_NODE_HAS_BORROWED, &UbseMemValidator::FilterLendNodeHasBorrowed},
        {CHECK_BORROW_NODE_HAS_LENT, &UbseMemValidator::CheckBorrowNodeHasLent},
        {FILTER_NODE_IS_LENDER, &UbseMemValidator::FilterNodeIsLender},
        {FILTER_NUMA_BY_SAME_PLANE, &UbseMemValidator::FilterNumaBySamePlane},
        {FILTER_SHARE_BY_SAME_PLANE, &UbseMemValidator::FilterShareBySamePlane},
        {CHECK_LEND_NODE_HAS_BORROWED, &UbseMemValidator::CheckLendNodeHasBorrowed},
        {FILTER_CANDIDATE_NODE_LIST, &UbseMemValidator::FilterCandidateNodeList},
        {FILTER_NODE_BY_GROUP, &UbseMemValidator::FilterNodeByGroup},
        {FILTER_NODE_IS_DOWN, &UbseMemValidator::FilterNodeIsDown},
        {CHECK_LEND_NODE_IS_LENDER, &UbseMemValidator::CheckLendNodeIsLender},
        {CHECK_LEND_NUMA_IS_ENOUGH, &UbseMemValidator::CheckLendNumaIsEnough},
        {CHECK_LEND_NODE_IS_IN_GROUP, &UbseMemValidator::CheckLendNodeIsInGroup},
        {FILTER_SHARE_NODE_LIST, &UbseMemValidator::FilterShareNodeList},
        {CHECK_LEND_NODE_IS_IN_CANDIDATELIST, &UbseMemValidator::CheckLendNodeIsInCandidatelist},
        {CHECK_NODE_IS_DOWN, &UbseMemValidator::CheckLendNodeIsDown},
        {FILTER_NUMA_BY_LEND_SOCKET, &UbseMemValidator::FilterNumaByLendSocket},
        {FILTER_LEND_TIME_OUT, &UbseMemValidator::FilterInvalidSocketLendTimes},
        {FILTER_SHARE_BY_LENDER, &UbseMemValidator::FilterByLenderInfo},
        {FILTER_LINK_PORT_DOWN, &UbseMemValidator::FilterByLinkPortDown}};
};
} // namespace ubse::mem::strategy
#endif
