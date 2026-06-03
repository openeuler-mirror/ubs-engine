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
using ubse::adapter_plugins::mmi::UbseMemLenderInfo;
using ubse::adapter_plugins::mmi::UbseMemLenderLinkInfo;
using ubse::common::def::UbseResult;
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
constexpr uint64_t FILTER_BY_MEMORY_RADIUS = 1 << 14;

// 指定借出节点
constexpr uint64_t CHECK_BORROW_SIZE_MEET_LIMIT = 1 << 15;
constexpr uint64_t CHECK_BORROW_NODE_HAS_LENT = 1 << 16;
constexpr uint64_t CHECK_LEND_NODE_HAS_BORROWED = 1 << 17;
constexpr uint64_t CHECK_LEND_NODE_IS_LENDER = 1 << 18;
constexpr uint64_t CHECK_LEND_NUMA_IS_ENOUGH = 1 << 19;
constexpr uint64_t CHECK_LEND_NODE_IS_IN_GROUP = 1 << 20;
constexpr uint64_t CHECK_LEND_NODE_IS_IN_CANDIDATELIST = 1 << 21;
constexpr uint64_t CHECK_NODE_IS_DOWN = 1 << 22;
constexpr uint64_t CHECK_BY_MEMORY_RADIUS = 1 << 23;
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
    // 采用算法决策时，过滤已借入的提供者节点（已借入的节点不能再借出）
    UbseResult FilterLendNodeHasBorrowed();
    // 采用算法决策时，过滤非借出节点，只保留标记为 lender 的节点
    UbseResult FilterNodeIsLender();
    // 采用算法决策时，根据同平面拓扑信息过滤 NUMA，只保留与请求端在同一平面内的 NUMA
    UbseResult FilterNumaBySamePlane();
    // 采用算法决策时，根据候选节点列表过滤 NUMA，只保留候选列表中的节点
    UbseResult FilterCandidateNodeList();
    // 采用算法决策时，根据分组配置过滤，只保留与请求节点在同一组的提供者节点
    UbseResult FilterNodeByGroup();
    // 采用算法决策时，过滤状态异常的节点（宕机节点不可借出）
    UbseResult FilterNodeIsDown();
    // 采用算法决策时，根据共享节点列表过滤，只保留指定提供者列表中的节点
    UbseResult FilterShareNodeList();
    // 采用算法决策时，根据借出端 socket 信息过滤 NUMA
    UbseResult FilterNumaByLendSocket();
    // 采用算法决策时，过滤借出次数超限的 socket
    UbseResult FilterInvalidSocketLendTimes();
    // 采用算法决策时，根据指定的借出节点/端口信息过滤，仅保留匹配的 NUMA
    UbseResult FilterByLenderInfo();
    // 采用算法决策时，基于同平面拓扑过滤共享节点，保留请求端所在平面内的节点
    UbseResult FilterShareBySamePlane();
    // 采用算法决策时，过滤链路端口不通的节点
    UbseResult FilterByLinkPortDown();
    // 采用算法决策时，综合借入半径和借出半径过滤
    UbseResult FilterByMemoryRadius();
    // 采用算法决策时，根据借入半径限制过滤，已达上限时仅保留已有债务的提供者
    UbseResult FilterByBorrowRadius();
    // 采用算法决策时，根据借出半径限制过滤，排除已达上限且未借给当前请求节点的 lender
    UbseResult FilterByLenderRadius();

    // 检查内存配置（分配器、block size）是否合法
    UbseResult CheckMemoryConfigIsValid();
    // 检查请求借入大小是否超过节点最大借入限制
    UbseResult CheckBorrowSizeMeetLimit();
    // 指定借出方时，检查借入节点是否已借出（已借出的节点不能再借入）
    UbseResult CheckBorrowNodeHasLent();
    // 指定借出方时，检查借出节点是否已借入（已借入的节点不能再借出）
    UbseResult CheckLendNodeHasBorrowed();
    // 指定借出方时，检查借出节点是否配置为 lender
    UbseResult CheckLendNodeIsLender();
    // 指定借出方时，检查借出端 NUMA 预留内存是否充足
    UbseResult CheckLendNumaIsEnough();
    // 指定借出方时，检查借出节点是否在请求节点的分组中
    UbseResult CheckLendNodeIsInGroup();
    // 指定借出方时，检查借出节点是否在候选列表中
    UbseResult CheckLendNodeIsInCandidatelist();
    // 指定借出方时，检查借出节点状态是否正常
    UbseResult CheckLendNodeIsDown();
    // 指定借出方时，检查借出信息（nodeId/socketId/numaId/portId）是否合法
    UbseResult CheckLenderInfoIsValid() const;
    // 指定借出方时，检查借入和借出是否超出各自半径限制
    UbseResult CheckByMemoryRadius();

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
        {FILTER_LINK_PORT_DOWN, &UbseMemValidator::FilterByLinkPortDown},
        {FILTER_BY_MEMORY_RADIUS, &UbseMemValidator::FilterByMemoryRadius},
        {CHECK_BY_MEMORY_RADIUS, &UbseMemValidator::CheckByMemoryRadius}};
};
} // namespace ubse::mem::strategy
#endif
