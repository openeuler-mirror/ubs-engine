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

#include "mem_pool_config.h"
#include <cstring>
#include <iomanip>
#include <set>
#include "mem_pool_strategy_impl.h"
#include "ubse_logger.h"

namespace tc::rs::mem {
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");
MemPoolConfig::MemPoolConfig(const StrategyParam &param)
{
    CheckNodeParameters(param);
    // 初始化静态配置参数, 算法权重参数归一化
    memStaticParam = param;
    NormalizeStrategy(memStaticParam);
    const BorrowAlgoParam &borrowParam = memStaticParam.borrowParam;

    // 开启自定义时延, 表示填写了numaLatencies, 未填写hostMeshLoc.
    if (memStaticParam.enableCustomLatencies) {
        // 填写了mStaticParam->numaLatencies, 将时延信息复制到mConfig对象中
        for (int i = 0; i < NUM_TOTAL_NUMA_FULLY_CONNECTED; i++) {
            for (int j = 0; j < NUM_TOTAL_NUMA_FULLY_CONNECTED; j++) {
                memLatencyInfo.numaToNumaLatency[i][j] = memStaticParam.numaLatencies[i][j];
            }
        }
        // 基于链路时延信息, 确保所有host全连接, 保证算法不会基于hostMeshLoc开展计算.
        if (!CheckFullConnectivity()) {
            UBSE_LOG_ERROR << "Invalid enableCustomLatencies.";
            throw std::invalid_argument("Error! enableCustomLatencies is true while hosts are not fully connected.");
        }
    } else { // 没有开启自定义时延, 表示填写了hostMeshLoc, 自动计算时延信息. 检查hostMeshLoc的正确性.
        // 基于hostMeshLoc生成链路时延信息
        RefreshNumaDelays();
    }
    // 默认系统全连接
    memIsFullyConnected = true;
    // 记录节点所在行列与hostId的对应关系
    MapTopologyToIndices();
    // 构建numa, socket全局index映射矩阵
    BuildIndexMatrix();
    // 统计可用socket数量、可用socket列表
    GetSockets();
    // 统计系统链路时延信息
    SysLatencyProcess();
    // 统计所有节点借出内存上限的最大值
    GetBorrowedMaxMem();
}

bool MemPoolConfig::IsHostMeshLocValid() const
{
    // 获得系统所有host的id列表
    std::vector<int> hostList(memStaticParam.numAvailNumas);
    for (int i = 0; i < memStaticParam.numAvailNumas; i++) {
        hostList[i] = memStaticParam.availNumas[i].hostId;
    }
    std::set<int> hostSet(hostList.begin(), hostList.begin() + memStaticParam.numAvailNumas);
    // 检查hostMeshLoc的有效性
    for (int i = 0; i < NUM_HOSTS; i++) {
        if ((hostSet.find(i) == hostSet.end() && IsNonNegative(memStaticParam.hostMeshLocs[i])) ||
            (hostSet.find(i) != hostSet.end() && IsNonPositive(memStaticParam.hostMeshLocs[i]))) {
            UBSE_LOG_ERROR << "Error! hostMeshLocs[" << i << "] is invalid.";
            return false;
        }
    }

    return true;
}

void MemPoolConfig::RefreshNumaDelays()
{
    const int32_t latBase = 100;  /* 本地numa访问内地内存的基础时延 */
    const int32_t latNuma = 20;   /* 同一个socket内跨numa所增加的时延 */
    const int32_t latSocket = 80; /* 跨P（跨socket，走HCCS）所增加的时延 */
    const int32_t latNbr = 200;   /* 直连邻居所增加的时延 */
    // host内任意两个numa之间的访问时延
    int32_t numaLatenciesInHost[NUM_NUMA_PER_HOST][NUM_NUMA_PER_HOST] = {
        {latBase, latBase + latNuma, latBase + latSocket, latBase + latSocket},
        {latBase + latNuma, latBase, latBase + latSocket, latBase + latSocket},
        {latBase + latSocket, latBase + latSocket, latBase, latBase + latNuma},
        {latBase + latSocket, latBase + latSocket, latBase + latNuma, latBase}};
    // numaLatenciesAcrossHost[xAxisConnect][iNumaAtXAxis][jNumaAtXAxis]
    int32_t numaLatenciesAcrossHost[2][2][2]{};
    numaLatenciesAcrossHost[0][0][0] = latBase + latNbr;
    numaLatenciesAcrossHost[0][0][1] = latBase + latNbr + latNuma;
    numaLatenciesAcrossHost[0][1][0] = latBase + latNbr + latNuma;
    numaLatenciesAcrossHost[0][1][1] = latBase + latNbr + latNuma + latNuma;
    numaLatenciesAcrossHost[1][0][0] = latBase + latNbr + latNuma + latNuma;
    numaLatenciesAcrossHost[1][0][1] = latBase + latNbr + latNuma;
    numaLatenciesAcrossHost[1][1][0] = latBase + latNbr + latNuma;
    numaLatenciesAcrossHost[1][1][1] = latBase + latNbr;
    /** numaLatencies[i][j] 表示的是 numa-i 访问 numa-j 上的内存的时延；也就是说，numa-i是借入方(访问方)，numa-j是借出方；如果是非直连邻居，则无法访问，时延为-1； */
    for (int i = 0; i < memStaticParam.numAvailNumas; i++) {
        for (int j = 0; j < memStaticParam.numAvailNumas; j++) {
            auto iNumaIdxInHost = memStaticParam.availNumas[i].numaId;
            auto jNumaIdxInHost = memStaticParam.availNumas[j].numaId;
            auto iHostId = memStaticParam.availNumas[i].hostId;
            auto jHostId = memStaticParam.availNumas[j].hostId;
            auto iSocketId = memStaticParam.availNumas[i].socketId;
            auto jSocketId = memStaticParam.availNumas[j].socketId;
            const MeshLoc &iHostMeshLoc = memStaticParam.hostMeshLocs[iHostId];
            const MeshLoc &jHostMeshLoc = memStaticParam.hostMeshLocs[jHostId];
            const auto iNeighborNodes = memStaticParam.neighborNodes[iHostId];
            const auto jNeighborNodes = memStaticParam.neighborNodes[jHostId];
            if (iHostId == jHostId) {
                memLatencyInfo.numaToNumaLatency[i][j] = numaLatenciesInHost[iNumaIdxInHost][jNumaIdxInHost];
            } else if (iNeighborNodes.find(jHostId) == iNeighborNodes.end()
                       || jNeighborNodes.find(iHostId) == jNeighborNodes.end()) {
                // 非直连邻居，无法访问对端的内存，时延设置为负数
                memLatencyInfo.numaToNumaLatency[i][j] = -1;
            } else if (iSocketId == jSocketId) {
                // 直连邻居, 同一个P
                int8_t xAxisConnect = (iHostMeshLoc.x != jHostMeshLoc.x) ? 1 : 0; /* numa-i和numa-j是否通过x轴连接 */
                int8_t iNumaAtXAxis = (iNumaIdxInHost % 2); /* numa-i是否为x轴连接上的numa */
                int8_t jNumaAtXAxis = (jNumaIdxInHost % 2); /* numa-j是否为x轴连接上的numa */
                memLatencyInfo.numaToNumaLatency[i][j] =
                    numaLatenciesAcrossHost[xAxisConnect][iNumaAtXAxis][jNumaAtXAxis];
            } else {
                // 直连邻居, 不是同一个P
                int8_t xAxisConnect = (iHostMeshLoc.x != jHostMeshLoc.x) ? 1 : 0; /* numa-i和numa-j是否通过x轴连接 */
                int8_t iNumaAtXAxis = (iNumaIdxInHost % 2); /* numa-i是否为x轴连接上的numa */
                // 访问方numa，位于numa-i和numa-j之间的连接轴上  |  访问方numa，不在numa-i和numa-j之间的连接轴上
                memLatencyInfo.numaToNumaLatency[i][j] = (xAxisConnect == iNumaAtXAxis) ?
                                                             latBase + latNbr + latSocket :
                                                             latBase + latNbr + latSocket + latNuma;
            }
        }
    }
}

bool MemPoolConfig::CheckFullConnectivity() const
{
    for (int i = 0; i < memStaticParam.numAvailNumas; i++) {
        for (int j = 0; j < memStaticParam.numAvailNumas; j++) {
            if (memLatencyInfo.numaToNumaLatency[i][j] == -1) {
                return false;
            }
        }
    }

    return true;
}

BResult MemPoolConfig::BuildIndexMatrix()
{
    for (int i = 0; i < NUM_HOSTS; i++) {
        for (int j = 0; j < NUM_NUMA_PER_HOST; j++) {
            memNumaLoc2Idx[i][j] = -1;
        }
        for (int j = 0; j < NUM_SOCKET_PER_HOST; j++) {
            memSocketLoc2Idx[i][j] = -1;
            for (int k = 0; k < NUM_NUMA_PER_SOCKET; k++) {
                memNumaLocToIdx[i][j][k] = -1;
            }
        }
    }
    int socketIndex = 0;
    for (int i = 0; i < memStaticParam.numAvailNumas; i++) {
        const MemLoc &numa = memStaticParam.availNumas[i];
        int numaId = numa.numaId - numa.numaId / NUM_NUMA_PER_SOCKET * NUM_NUMA_PER_SOCKET;
        memNumaLoc2Idx[numa.hostId][numa.numaId] = i;
        memNumaLocToIdx[numa.hostId][numa.socketId][numaId] = i;
        if (memSocketLoc2Idx[numa.hostId][numa.socketId] == -1) {
            memSocketLoc2Idx[numa.hostId][numa.socketId] = socketIndex;
            socketIndex++;
        }
    }
    return UBSE_OK;
}

BResult MemPoolConfig::GetSockets()
{
    for (int i = 0; i < memStaticParam.numAvailNumas; i++) {
        MemLoc numa = memStaticParam.availNumas[i];
        numa.numaId = -1;
        memAvailSockets[GetSocketIndex(numa)] = numa;
    }
    for (auto socket : memAvailSockets) {
        if (!(socket.hostId == -1 && socket.socketId == -1 && socket.numaId == -1)) {
            memAvailSocketsCnt++;
        }
    }

    return UBSE_OK;
}

BResult MemPoolConfig::SysLatencyProcess()
{
    std::vector<std::vector<int>> socketNumaLatencyNum(memAvailSocketsCnt,
                                                       std::vector<int>(memStaticParam.numAvailNumas, 0));
    std::vector<std::vector<int>> socketSocketLatencyNum(memAvailSocketsCnt, std::vector<int>(memAvailSocketsCnt, 0));
    std::vector<std::vector<int>> socketHostLatencyNum(memAvailSocketsCnt,
                                                       std::vector<int>(memStaticParam.numHosts, 0));

    memLatencyInfo.maxSysLatency = -1;
    memLatencyInfo.minSysLatency = INT32_MAX;
    for (int i = 0; i < memStaticParam.numAvailNumas; i++) {
        for (int j = 0; j < memStaticParam.numAvailNumas; j++) {
            const MemLoc &numa1 = memStaticParam.availNumas[i];
            const MemLoc &numa2 = memStaticParam.availNumas[j];
            int32_t latency = memLatencyInfo.numaToNumaLatency[i][j];
            UpdateMinLatency(latency);
            UpdateMaxLatency(latency);
            memLatencyInfo.socketToNumaLatency[GetSocketIndex(numa1)][j] += latency;
            memLatencyInfo.socketToSocketLatency[GetSocketIndex(numa1)][GetSocketIndex(numa2)] += latency;
            memLatencyInfo.socketToHostLatency[GetSocketIndex(numa1)][numa2.hostId] += latency;

            socketNumaLatencyNum[GetSocketIndex(numa1)][j] += 1;
            socketSocketLatencyNum[GetSocketIndex(numa1)][GetSocketIndex(numa2)] += 1;
            socketHostLatencyNum[GetSocketIndex(numa1)][numa2.hostId] += 1;
        }
    }
    for (int i = 0; i < memAvailSocketsCnt; i++) {
        for (int j = 0; j < memStaticParam.numAvailNumas; j++) {
            CalculateLatency(memLatencyInfo.socketToNumaLatency[i][j], socketNumaLatencyNum[i][j]);
        }
        for (int j = 0; j < memAvailSocketsCnt; j++) {
            CalculateLatency(memLatencyInfo.socketToSocketLatency[i][j], socketSocketLatencyNum[i][j]);
        }
        for (int j = 0; j < memStaticParam.numHosts; j++) {
            CalculateLatency(memLatencyInfo.socketToHostLatency[i][j], socketHostLatencyNum[i][j]);
        }
    }

    return UBSE_OK;
}

void MemPoolConfig::CalculateLatency(int32_t &latency, int num)
{
    if (num == 0) {
        UBSE_LOG_ERROR << "SysLatencyProcess division by zero.";
        throw std::invalid_argument("Error! CalculateLatency is false while num is zero.");
    }
    latency = latency / num;
}

void MemPoolConfig::UpdateMinLatency(int32_t latency)
{
    if (latency < memLatencyInfo.minSysLatency && latency != -1) {
        memLatencyInfo.minSysLatency = latency;
    }
}

void MemPoolConfig::UpdateMaxLatency(int32_t latency)
{
    memLatencyInfo.maxSysLatency = std::max(memLatencyInfo.maxSysLatency, latency);
}

BResult MemPoolConfig::GetBorrowedMaxMem()
{
    memMaxBorrowed = 0;
    for (int i = 0; i < memStaticParam.numHosts; i++) {
        memMaxBorrowed = std::max(memMaxBorrowed, memStaticParam.maxMemBorrowed[i]);
    }
    return UBSE_OK;
}

int MemPoolConfig::GetSocketIndex(MemLoc loc)
{
    if (loc.hostId < 0 || loc.hostId >= NUM_HOSTS || loc.socketId < 0 || loc.socketId >= NUM_SOCKET_PER_HOST) {
        UBSE_LOG_ERROR << "Get socket index error! Array out of bounds!";
        throw std::out_of_range("Array out of bounds in MemPoolConfig::GetSocketIdx! Please check whether requestLoc "
                                "or StrategyParam.availNuma is valid!");
    }

    if (memSocketLoc2Idx[loc.hostId][loc.socketId] == -1 ||
        memSocketLoc2Idx[loc.hostId][loc.socketId] >= NUM_TOTAL_SOCKET) {
        UBSE_LOG_ERROR << "Socket is not available! hostId = " << loc.hostId
                       << ", socketId = " << static_cast<int>(loc.socketId) << "!";
        throw std::out_of_range("Socket does not exist in MemPoolConfig::GetSocketIdx! Please check whether "
                                "requestLoc or StrategyParam.availNuma is valid!");
    }
    return memSocketLoc2Idx[loc.hostId][loc.socketId];
}

int MemPoolConfig::GetNumaIndex(MemLoc loc)
{
    if (loc.hostId < 0 || loc.hostId >= NUM_HOSTS || loc.numaId < 0 || loc.numaId >= NUM_TOTAL_NUMA / NUM_HOSTS) {
        UBSE_LOG_ERROR << "Get numa index error! Array out of bounds!";
        throw std::out_of_range("Array out of bounds in MemPoolConfig::GetNumaIdx! "
                                "Please check whether requestLoc or StrategyParam.availNuma is valid!");
    }
    if (memNumaLoc2Idx[loc.hostId][loc.numaId] == -1 || memNumaLoc2Idx[loc.hostId][loc.numaId] >= NUM_TOTAL_NUMA) {
        UBSE_LOG_ERROR << "Numa is not available! hostId = " << loc.hostId
                       << ", numaId = " << static_cast<int>(loc.numaId) << "!";
        throw std::out_of_range("Numa does not exist in MemPoolConfig::GetNumaIdx! Please check whether requestLoc or "
                                "StrategyParam.availNuma is valid!");
    }
    return memNumaLoc2Idx[loc.hostId][loc.numaId];
}

int *MemPoolConfig::GetNumaListInSocket(int32_t hostId, int32_t socketId)
{
    if (hostId < 0 || hostId >= NUM_HOSTS || socketId < 0 || socketId >= NUM_SOCKET_PER_HOST) {
        UBSE_LOG_ERROR << "Get numa list in socket error! Array out of bounds!";
        throw std::out_of_range("Array out of bounds in MemPoolConfig::GetNumaListInSocket! Please check whether "
                                "requestLoc or StrategyParam.availNuma is valid!");
    }
    return memNumaLocToIdx[hostId][socketId];
}

int *MemPoolConfig::GetNumaListInHost(int32_t hostId)
{
    if (hostId < 0 || hostId >= NUM_HOSTS) {
        UBSE_LOG_ERROR << "Get numa list in host error! Array out of bounds!";
        throw std::out_of_range("Array out of bounds in MemPoolConfig::GetNumaListInHost! Please check whether "
                                "requestLoc or StrategyParam.availNuma is valid!");
    }
    return memNumaLoc2Idx[hostId];
}

bool MemPoolConfig::IsNonNegative(MeshLoc cord) const
{
    return (cord.x >= 0 || cord.y >= 0);
}

bool MemPoolConfig::IsNonPositive(MeshLoc cord) const
{
    return (cord.x < 0 || cord.y < 0);
}

void MemPoolConfig::CheckNodeParameters(const StrategyParam &param)
{
    if (param.numHosts > NUM_HOSTS) {
        UBSE_LOG_ERROR << "Error! Number of hosts exceeds the maximum limit. Current value=" << param.numHosts
                       << ", Maximum allowed=" << NUM_HOSTS;
        throw std::invalid_argument("Error! The number of hosts is out of bound. Current value=" +
                                    std::to_string(param.numHosts) + ", Maximum allowed=" + std::to_string(NUM_HOSTS));
    }
    if (param.numAvailNumas > NUM_TOTAL_NUMA) {
        UBSE_LOG_ERROR << "Error! Number of available NUMAs exceeds the maximum limit. Current value="
                       << param.numAvailNumas << ", Maximum allowed=" << NUM_TOTAL_NUMA;
        throw std::invalid_argument("Error! The number of available NUMAs is out of bound. Current value=" +
                                    std::to_string(param.numAvailNumas) +
                                    ", Maximum allowed=" + std::to_string(NUM_TOTAL_NUMA));
    }
}

} // namespace tc::rs::mem