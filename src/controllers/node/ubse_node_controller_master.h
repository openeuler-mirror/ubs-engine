/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBS_ENGINE_UBSE_NODE_CONTROLLER_MASTER_H
#define UBS_ENGINE_UBSE_NODE_CONTROLLER_MASTER_H

#include <mutex>
#include <shared_mutex>

#include "ubse_com_module.h"
#include "ubse_common_def.h"
#include "ubse_node_controller.h"
#include "ubse_thread_pool.h"

namespace ubse::nodeController {
using namespace ubse::common::def;
using namespace ubse::task_executor;
using namespace ubse::com;

class UbseNodeControllerMaster {
public:
    static UbseNodeControllerMaster &GetInstance()
    {
        static UbseNodeControllerMaster instance;
        return instance;
    }

    UbseResult Initialize();

    void UnInitialize();

    UbseResult Start();

    void Stop();

    /**
     * 从节点周期采集上报处理回调
     * @param nodeInfo
     */
    UbseResult UbseNodeReportHandler(const UbseNodeInfo& nodeInfo);

    /**
     * 从节点lcne拓扑变化采集上报回调
     * @param nodeInfo
     */
    UbseResult UbseLcneTopologyChangeHandler(const UbseNodeInfo& nodeInfo);

private:
    UbseResult UbseMasterOnlineHandler(const std::string &nodeId);

    UbseResult UbseNodeDownHandler(const std::string &nodeId);

    UbseResult UbseNodeUpHandler(const std::string &nodeId);

    void UbseNodeLedgerTimerHandler();

    void UbseNodeCycleLedger(const std::string &nodeId);

    void UbseNodeUpLedger(const std::string &nodeId);

    void UbseNodeLedger(const std::string &nodeId);

    void UbseNodeCleanAfterSwitchStandby();

    /**
     * 节点上报汇聚，每隔1min，打印一次节点上报记录
     */
    void ReportAggregation();

    UbseTaskExecutorPtr taskExecutor_{};

    // map<nodeId, reportCount>
    std::unordered_map<std::string, uint64_t> reportCounters_{};
    std::shared_mutex rwReportMutex_{};
    std::mutex cvMutex_{};
    std::condition_variable cv_{};
    std::atomic<bool> isLogAggregationRunning_ = false;
};

/**
 * 注册Master端4个RPC处理器
 */
UbseResult RegMasterMsgHandler();

/**
 * 处理Agent上报的LCNE拓扑变化
 * @param req 请求数据，包含序列化的节点信息
 * @param resp 响应数据，返回处理结果
 * @return UbseResult 处理结果
 */
UbseResult LcneChangeNodeInfoHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

/**
 * 处理Agent周期上报的节点信息
 * @param req 请求数据，包含序列化的节点信息
 * @param resp 响应数据，返回处理结果
 * @return UbseResult 处理结果
 */
UbseResult UbseNodeReportNodeInfoHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

/**
 * 处理Agent查询全量节点列表
 * @param req 请求数据
 * @param resp 响应数据，包含全量节点列表
 * @return UbseResult 处理结果
 */
UbseResult GetAllNodeInfoFromRemoteHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

/**
 * 处理Agent查询全量链路信息
 * @param req 请求数据
 * @param resp 响应数据，包含全量链路信息
 * @return UbseResult 处理结果
 */
UbseResult UbseGetDirConnectInfoFromRemoteHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

/**
 * Master从Agent采集节点信息
 * @param nodeId 目标Agent节点ID
 * @param info 输出参数，采集到的节点信息
 * @return UbseResult 采集结果
 */
UbseResult CollectRemoteNodeInfo(const std::string &nodeId, UbseNodeInfo &info);
} // namespace ubse::nodeController

#endif // UBS_ENGINE_UBSE_NODE_CONTROLLER_MASTER_H
