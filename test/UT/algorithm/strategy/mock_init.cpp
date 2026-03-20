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

#include "mock_init.h"
#include "mem_pool_strategy.h"

using namespace tc::rs::mem;

StrategyParam GetRs1630DefaultParam()
{
    StrategyParam param;
    param.numHosts = 4;
    param.numAvailNumas = 16;
    int8_t numSocketPerHost = 2;
    int8_t numNumaPerSocket = 2;
    int idx = 0;
    for (int16_t hostId = 0; static_cast<int32_t>(hostId) < param.numHosts; hostId++) {
        for (int8_t socketId = 0; socketId < numSocketPerHost; socketId++) {
            for (int8_t numaId = 0; numaId < numNumaPerSocket; numaId++) {
                param.availNumas[idx].hostId = hostId;
                param.availNumas[idx].socketId = socketId;
                param.availNumas[idx].numaId = static_cast<int8_t>(numaId + socketId * numSocketPerHost);
                idx++;
            }
        }
    }
    int32_t latencies[16][16] = {{100, 120, 220, 200, 200, 220, 200, 220, 200, 220, 240, 220, 200, 220, 240, 220},
                                 {120, 100, 200, 180, 220, 240, 220, 240, 220, 240, 220, 200, 220, 240, 220, 200},
                                 {220, 200, 100, 120, 200, 220, 200, 220, 240, 220, 200, 220, 240, 220, 200, 220},
                                 {200, 180, 120, 100, 220, 240, 220, 240, 220, 200, 220, 240, 220, 200, 220, 240},
                                 {200, 220, 200, 220, 100, 120, 220, 200, 200, 220, 240, 220, 200, 220, 240, 220},
                                 {220, 240, 220, 240, 120, 100, 200, 180, 220, 240, 220, 200, 220, 240, 220, 200},
                                 {200, 220, 200, 220, 220, 200, 100, 120, 220, 240, 220, 200, 220, 240, 220, 200},
                                 {220, 240, 220, 240, 200, 180, 120, 100, 220, 200, 220, 240, 220, 200, 220, 240},
                                 {200, 220, 240, 220, 200, 220, 220, 220, 100, 120, 220, 200, 200, 220, 200, 220},
                                 {220, 240, 220, 200, 220, 240, 240, 200, 120, 100, 200, 180, 220, 240, 220, 240},
                                 {240, 220, 200, 220, 240, 220, 220, 220, 220, 200, 100, 120, 200, 220, 200, 220},
                                 {220, 200, 220, 240, 220, 200, 200, 240, 200, 180, 120, 100, 220, 240, 220, 240},
                                 {200, 220, 240, 220, 200, 220, 220, 220, 200, 220, 200, 220, 100, 120, 220, 200},
                                 {220, 240, 220, 200, 220, 240, 240, 200, 220, 240, 220, 240, 120, 100, 200, 180},
                                 {240, 220, 200, 220, 240, 220, 220, 220, 200, 220, 200, 220, 220, 200, 100, 120},
                                 {220, 200, 220, 240, 220, 200, 200, 240, 220, 240, 220, 240, 200, 180, 120, 100}};

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
        param.maxBorrowHosts[i] = NUM_HOSTS - 1;
        param.hostMeshLocs[i].x = 0;
        param.hostMeshLocs[i].y = static_cast<int8_t>(i);
    }
    param.maxMemSizePerBorrow = 4 * 1024;
    param.watermarkGrain = WatermarkGrain::HOST_WATERMARK;
    param.unitMemSize = 128;

    return param;
}

StrategyParam GetRs1630DefaultParamMissingNuma()
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

StrategyParam GetRs1650DefaultParam(int numHosts)
{
    StrategyParam param;
    param.numHosts = numHosts;
    int numTotalNumas = numHosts * 4;
    param.numAvailNumas = numTotalNumas;
    int8_t numSocketPerHost = 2;
    int8_t numNumaPerSocket = 2;
    int idx = 0;
    for (int16_t hostId = 0; static_cast<int32_t>(hostId) < param.numHosts; hostId++) {
        for (int8_t socketId = 0; socketId < numSocketPerHost; socketId++) {
            for (int8_t numaId = 0; numaId < numNumaPerSocket; numaId++) {
                param.availNumas[idx].hostId = hostId;
                param.availNumas[idx].socketId = socketId;
                param.availNumas[idx].numaId = static_cast<int8_t>(numaId + socketId * numSocketPerHost);
                idx++;
            }
        }
    }
    // 1650代际，无需设置numa之前的访问时延（numaLatencies）
    param.enableCustomLatencies = false;
    for (int i = 0; i < numTotalNumas; i++) {
        param.numaMemCapacities[i] = 256 * 1024;
        param.memOutHardLimit[i] = 256 * 1024;
    }
    for (int i = 0; i < numHosts; i++) {
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
        param.maxBorrowHosts[i] = numHosts - 1;
        MeshLoc meshLoc;
        meshLoc.x = i % 4;
        meshLoc.y = i / 4;
        param.hostMeshLocs[i] = meshLoc;
    }
    param.maxMemSizePerBorrow = 4 * 1024;
    param.watermarkGrain = WatermarkGrain::HOST_WATERMARK;
    param.unitMemSize = 128;

    return param;
}

StrategyParam GetRs1D8DefaultParam(int numHosts)
{
    StrategyParam param;
    param.numHosts = numHosts;
    int numTotalNumas = numHosts * 4;
    param.numAvailNumas = numTotalNumas;
    int8_t numSocketPerHost = 2;
    int8_t numNumaPerSocket = 2;
    int idx = 0;
    for (int16_t hostId = 0; static_cast<int32_t>(hostId) < param.numHosts; hostId++) {
        for (int8_t socketId = 0; socketId < numSocketPerHost; socketId++) {
            for (int8_t numaId = 0; numaId < numNumaPerSocket; numaId++) {
                param.availNumas[idx].hostId = hostId;
                param.availNumas[idx].socketId = socketId;
                param.availNumas[idx].numaId = static_cast<int8_t>(numaId + socketId * numSocketPerHost);
                idx++;
            }
        }
    }
    // 1650代际，无需设置numa之前的访问时延（numaLatencies）
    param.enableCustomLatencies = false;
    for (int i = 0; i < numTotalNumas; i++) {
        param.numaMemCapacities[i] = 256 * 1024;
        param.memOutHardLimit[i] = 256 * 1024;
    }
    for (int i = 0; i < numHosts; i++) {
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
        param.maxBorrowHosts[i] = numHosts - 1;
        MeshLoc meshLoc;
        meshLoc.x = i;
        meshLoc.y = 0;
        param.hostMeshLocs[i] = meshLoc;
    }
    param.maxMemSizePerBorrow = 4 * 1024;
    param.watermarkGrain = WatermarkGrain::HOST_WATERMARK;
    param.unitMemSize = 128;

    return param;
}