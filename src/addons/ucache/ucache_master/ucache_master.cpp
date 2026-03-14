/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ucache_master.h"

#include <atomic>
#include <ctime>
#include <thread>
#include <vector>

#include "master_task_controller.h"
#include "ucache_config.h"
#include "ucache_error.h"
#include "ucache_serialize.h"

#include "ubse_com.h"
#include "borrow_action.h"
#include "borrow_strategy.h"
#include "bottleneck_strategy.h"
#include "event_handler.h"
#include "mem_borrow.h"
#include "ucache_migrate_strategy.h"

namespace ucache::master {
using namespace ubse::log;
using namespace ubse::com;
using namespace ucache::master::migration;
using namespace ucache::mem_borrow;
using namespace ucache::borrow_action;
using namespace ucache::borrow_strategy;
using namespace ucache::master::bottleneck;
using namespace ucache::serialize;

const std::string TARGET_NODE = "Node1";
static std::atomic<bool> g_masterThreadStopFlag{false};
static std::thread g_ucacheMasterThread;

std::vector<MigrationAction> CalMemoryMigrationStrategy()
{
    std::map<std::string, NodeMemoryInfo> nodes;
    std::map<std::string, std::map<std::string, PageCacheSensitiveTag>> nodeTags;
    for (auto it = BorrowNodeStat::nodeIdToNodeStatMap.begin(); it != BorrowNodeStat::nodeIdToNodeStatMap.end(); it++) {
        struct NodeMemoryInfo node;
        std::string nodeName = it->second.GetNodeId();
        node.usedMemory = it->second.GetUsedMem();
        node.freeMemory = it->second.GetFreeMem();
        node.pageCacheMemory = it->second.GetPageCacheMem();
        node.borrowedUsageRate = it->second.GetBorrowedMemUsage();
        node.memoryShortage = it->second.GetScarcity();
        node.pageCacheAppNums = it->second.GetPagecacheAppNums();
        node.minFreeKbytes = it->second.GetFreeMemMin();
        nodes[nodeName] = node;
    }
    // 从应用瓶颈分析模块得到各节点应用标签
    nodeTags = BottleneckStrategy::GetInstance().GetContainerState();
    // 调用迁移策略获得迁移动作集
    return MemoryMigrationStrategy(MemBorrowTopo::globalMemBorrowTopo.borrowMap, nodes, nodeTags);
}

uint32_t SendMigrateCommands(MigrationAction action)
{
    struct MigrationStrategyParam param;
    param.dstNid = action.dstNumaId;
    param.highWatermarkPages = action.stopWatermark;
    param.lowWatermarkPages = action.startWatermark;
    param.dockerIds = action.dockerIds;
    auto it = MemBorrowTopo::globalMemBorrowTopo.borrowMemInfoPerNodeMap.find(action.fromNode);
    if (it == MemBorrowTopo::globalMemBorrowTopo.borrowMemInfoPerNodeMap.end()) {
        return BORROW_TOPO_ERROR;
    }
    struct BorrowMemInfoPerNode borrowMemInfo = it->second;
    for (auto it = borrowMemInfo.numaNodeSize.begin(); it != borrowMemInfo.numaNodeSize.end(); it++) {
        param.srcNids.push_back(it->first);
    }

    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
        << "Sending migration commands from node " << action.fromNode;
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Sending migration commands to nid: " << param.dstNid;
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_NAME) << "srcNids: ";
    for (auto nid : param.srcNids) {
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_NAME) << "nid: " << nid;
    }
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
        << "highWatermarkPages: " << param.highWatermarkPages << " lowWatermarkPages: " << param.lowWatermarkPages;
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "dockerIds: ";
    for (auto id : param.dockerIds) {
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "docker container: " << id;
    }
    UCacheOutStream out;
    out << param;
    TaskRequest tReq{};
    tReq.payload = out.Str();
    tReq.type = TaskType::MIGRATION_STRATEGY;
    TaskResponse tResp{};
    // 只检查RPC是否成功，不检查迁移执行是否成功
    uint32_t ret = DispatchTask(tReq, tResp, action.fromNode);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "DispatchTask failed, ret=" << ret <<".";
    }
    return ret;
}

uint32_t SetPageCacheAppNums()
{
    std::map<std::string, std::map<std::string, PageCacheSensitiveTag>> nodeTags =
        BottleneckStrategy::GetInstance().GetContainerState();
    std::vector<BorrowStrategyRawData> rawDatas;
    DataCollect::GetBorrowStrategyRawData(rawDatas);
    for (const auto &[nodeId, nodeTag] : nodeTags) {
        auto nodeRawData =
            std::find_if(rawDatas.begin(), rawDatas.end(),
                         [&nodeId](const BorrowStrategyRawData &rawData) { return rawData.nodeId == nodeId; });
        if (nodeRawData == rawDatas.end()) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node " << nodeId << " not found in rawData";
            return BORROW_DATA_ERROR;
        }
        int pagecacheAppNums = 0;
        for (const auto &[dockerId, tag] : nodeTag) {
            if (tag == PageCacheSensitiveTag::SENSITIVE) {
                pagecacheAppNums++;
            }
        }
        nodeRawData->pagecacheAppNums = pagecacheAppNums;
    }
    DataCollect::SetBorrowStrategyRawData(rawDatas);
    return UCACHE_OK;
}

void CheckNodeFaultAndWait()
{
    const int sleepMs = 1000;
    while (ucache::fault_handler::EventHandler::gNodeFaultFlag.load()) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node fault occurred, waiting for recovery...";
        ucache::fault_handler::EventHandler::gMasterStopFlag.store(true); // 设置停止标志，通知故障处理
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
}

bool HandleDataCollection()
{
    if (ucache::fault_handler::EventHandler::gNodeFaultFlag.load()) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node fault occurred, data collection canceled.";
        return false;
    }

    uint32_t ret = DataCollect::CollectData();
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Collect data failed, ret: " << ret;
        return false;
    }

    std::map<std::string, std::map<std::string, CgroupInfo>> cgInfos;
    DataCollect::GetCgroupInfo(cgInfos);
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << DataCollect::PrintCgroupInfo(cgInfos);
    ret = BottleneckStrategy::GetInstance().JudgeSensitive(cgInfos, UcacheConfig::GetInstance().GetMasterInterval());
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "JudgeSensitive failed, ret: " << ret;
        return false;
    }
    return true;
}

bool HandlePageCacheAppNums()
{
    uint32_t ret = SetPageCacheAppNums();
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "SetPageCacheAppNums failed, ret: " << ret;
        return false;
    }
    return true;
}

bool InitMemBorrowTopoAndBorrowNodeStat()
{
    MemBorrowTopo::InitGlobalMemBorrowTopo();
    uint32_t ret = BorrowNodeStat::InitBorrowNodeStat();
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "InitBorrowNodeStat failed, ret: " << ret;
        return false;
    }
    return true;
}

bool HandleBorrowStrategy()
{
    BorrowStrategy borrowStrategy = {};
    uint32_t ret = borrowStrategy.Init();
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "InitBorrowStrategy failed, ret: " << ret;
        return false;
    }

    borrowStrategy.Start();
    std::vector<BorrowAction> actionSet = borrowStrategy.GetActionSet();

    // 执行借用动作
    if (ucache::fault_handler::EventHandler::gNodeFaultFlag.load()) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node fault occurred, Borrow canceled.";
        return false;
    }
    ret = ExecuteBorrowActions(actionSet);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "ExecuteBorrowActions failed, ret: " << ret;
        return false;
    }
    return true;
}

bool ExecuteMigrationStrategy()
{
    // 计算迁移动作集
    std::vector<MigrationAction> migrationActions = CalMemoryMigrationStrategy();
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << PrintMigrationAction(migrationActions);

    if (ucache::fault_handler::EventHandler::gNodeFaultFlag.load()) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node fault occurred, Migration canceled.";
    }

    // 发送迁移动作
    for (const auto &action : migrationActions) {
        uint32_t ret = SendMigrateCommands(action);
        if (ret != UCACHE_OK) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "SendMigrateCommands failed, ret: " << ret;
            return false;
        }
    }
    return true;
}

void UcacheMasterMain()
{
    try {
        UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Ucache Master Main run";
        g_masterThreadStopFlag.store(false);

        while (!g_masterThreadStopFlag) {
            UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "gNodeFaultFlag is " << ucache::fault_handler::EventHandler::gNodeFaultFlag.load();
            CheckNodeFaultAndWait(); // 检查节点故障并等待恢复

            std::this_thread::sleep_for(
                std::chrono::seconds(UcacheConfig::GetInstance().GetMasterInterval())); // 等待主循环间隔时间

            if (!HandleDataCollection()) {
                continue; // 数据采集和瓶颈分析
            }

            if (!HandlePageCacheAppNums()) {
                continue; // 设置应用数量
            }

            if (!InitMemBorrowTopoAndBorrowNodeStat()) {
                continue; // 初始化拓扑和借用节点状态
            }

            if (!HandleBorrowStrategy()) {
                continue; // 处理借用策略
            }

            if (!ExecuteMigrationStrategy()) {
                continue; // 处理迁移策略
            }
        }
    } catch (const std::exception &e) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Ucache Master Main exception" << e.what();
    }
}

uint32_t Init()
{
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "InitMaster";
    g_ucacheMasterThread = std::thread(UcacheMasterMain);
    return UCACHE_OK;
}

void Exit()
{
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "ExitMaster";
    g_masterThreadStopFlag.store(true);
    if (g_ucacheMasterThread.joinable()) {
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Start join master thread.";
        g_ucacheMasterThread.join();
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "End join master thread.";
    } else {
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Master thread is not joinable.";
    }
}

} // namespace ucache::master