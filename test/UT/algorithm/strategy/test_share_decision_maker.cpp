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

#include "test_share_decision_maker.h"
#include <mockcpp/mockcpp.hpp>
#include "share_decision_maker.h"

namespace ubse::ut::algorithm {
using namespace tc::rs::mem;

void TestShareDecisionMaker::SetUp()
{
    Test::SetUp();
}
void TestShareDecisionMaker::TearDown()
{
    Test::TearDown();
}

StrategyParam GetDefaultParamMissingNuma()
{
    StrategyParam param;
    param.numHosts = 2;
    param.numAvailNumas = 7;
    int idx = 0;
    for (int16_t host = 0; host < 2; host++) {
        for (int16_t socket = 0; socket < 2; socket++) {
            if (host == 1 && socket == 1) {
                param.availNumas[idx].hostId = host;
                param.availNumas[idx].socketId = int8_t(socket);
                param.availNumas[idx].numaId = 2;
                idx++;
            } else {
                param.availNumas[idx].hostId = host;
                param.availNumas[idx].socketId = int8_t(socket);
                param.availNumas[idx].numaId = int8_t(socket * 2);
                idx++;
                param.availNumas[idx].hostId = host;
                param.availNumas[idx].socketId = int8_t(socket);
                param.availNumas[idx].numaId = int8_t(socket * 2 + 1);
                idx++;
            }
        }
    }
    int32_t latencies[7][7] = {
        {100, 120, 200, 200, 220, 200, 220}, {120, 100, 180, 220, 240, 220, 240}, {200, 180, 100, 220, 240, 220, 240},
        {200, 220, 220, 100, 120, 220, 200}, {220, 240, 240, 120, 100, 200, 180}, {200, 220, 220, 220, 200, 100, 120},
        {220, 240, 240, 200, 180, 120, 100},
    };

    param.enableCustomLatencies = true;
    for (int i = 0; i < param.numAvailNumas; i++) {
        param.numaMemCapacities[i] = 256 * 1024;
        param.memOutHardLimit[i] = 256 * 1024;
        for (int j = 0; j < param.numAvailNumas; j++) {
            param.numaLatencies[i][j] = latencies[i][j];
        }
    }
    for (int i = 0; i < param.numHosts; i++) {
        param.memHighLineL0[i] = 90;
        param.memHighLineL1[i] = 95;
        if (param.memHighLineL1[i] <= param.memHighLineL0[i]) {
            std::cerr << "Init param error! memHighLineL1 must be larger than memHighLineL0." << std::endl;
        }
        param.memLowLine[i] = 70;
        param.maxMemBorrowed[i] = 256 * 1024 * 0.25;
        param.maxMemLent[i] = 256 * 1024 * 0.25;
        param.maxMemShared[i] = 256 * 1024 * 0.25;
        param.maxMemOut[i] = 256 * 1024 * 0.25;
        param.maxBorrowHosts[i] = param.numHosts - 1;
        param.hostMeshLocs[i].x = 0;
        param.hostMeshLocs[i].y = static_cast<int8_t>(i);
    }
    param.maxMemSizePerBorrow = 4 * 1024;
    param.watermarkGrain = WatermarkGrain::HOST_WATERMARK;
    param.unitMemSize = 128;

    return param;
}

TEST_F(TestShareDecisionMaker, TestShareDecisionMaker)
{
    // 初始化
    MemPoolStrategy &strategy = MemPoolStrategy::GetInstance();
    // 初始化
    auto param = GetDefaultParamMissingNuma();
    param.algoMode = AlgoMode::SELF_DEVELOPED;
    strategy.Init(param);

    ShareRequest shareRequest;

    // 构造借用接口的入参
    // 构造src
    shareRequest.srcLoc.hostId = 0;
    shareRequest.srcLoc.socketId = -1;
    shareRequest.srcLoc.numaId = -1;

    // 构造 Request Size
    shareRequest.requestSize = 7L * 1024;

    // 构造 Region
    shareRequest.region.type = ShmRegionType::ALL2ALL_SHARE;

    shareRequest.region.num = 4;
    shareRequest.region.nodeId[0] = 0;
    shareRequest.region.nodeId[1] = 1;
    shareRequest.region.nodeId[2] = 2;
    shareRequest.region.nodeId[3] = 3;

    UbseStatus ubseStatus;
    for (int i = 0; i < NUM_TOTAL_NUMA; i++) {
        ubseStatus.numaStatus[i].numa = param.availNumas[i];
        ubseStatus.numaLedgerStatus[i].numa = param.availNumas[i];
        ubseStatus.numaStatus[i].memTotal = static_cast<uint64_t>((1L + i) * 1024 * 1024 * 1024);
        ubseStatus.numaStatus[i].memUsed = 0L;
        ubseStatus.numaStatus[i].memFree = static_cast<uint64_t>((1L + i) * 1024 * 1024 * 1024);
        ubseStatus.numaLedgerStatus[i].memBorrowed = 0L;
        ubseStatus.numaLedgerStatus[i].memLent = 0L;
        ubseStatus.numaLedgerStatus[i].memShared = 0L;
    }
    ShareResult shareResult;

    EXPECT_EQ(UBSE_OK, strategy.MemoryShare(shareRequest, ubseStatus, shareResult));
}
} // namespace ubse::ut::algorithm