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

#ifndef UBS_ENGINE_UBSE_NODE_CONTROLLER_AGENT_H
#define UBS_ENGINE_UBSE_NODE_CONTROLLER_AGENT_H

#include "ubse_common_def.h"
#include "ubse_node_controller.h"
#include "ubse_thread_pool.h"
#include "ubse_com_module.h"

namespace ubse::nodeController {
using namespace ubse::common::def;
using namespace ubse::task_executor;
using namespace ubse::com;

class UbseNodeControllerAgent {
public:
    static UbseNodeControllerAgent &GetInstance()
    {
        static UbseNodeControllerAgent instance;
        return instance;
    }

    UbseResult Initialize();

    void UnInitialize();

    UbseResult Start();

    void Stop();

private:
    /**
     * 周期采集上报节点内存&拓扑回调
     */
    UbseResult UbseNodeInfoReportTimerHandler();

    /**
     * 监听LCNE拓扑变更，采集上报节点内存&拓扑
     * @param eventId
     * @param eventMessage
     * @return
     */
    static UbseResult UbseNodeInfoLcneNotifyHandler(std::string &eventId, std::string &eventMessage);

    void StartExec();

    UbseTaskExecutorPtr taskExecutor_{};
};

/**
 * Agent向Master查询全量节点列表
 * @param nodeId 目标Master节点ID
 * @param infos 输出参数，全量节点信息列表
 * @return UbseResult 操作结果
 */
UbseResult GetAllNodeInfoFromRemote(const std::string &nodeId, std::vector<UbseNodeInfo> &infos);

/**
 * Agent向Master查询全量链路信息
 * @param nodeId 目标Master节点ID
 * @param devDirConnectInfoRemote 输出参数，全量链路信息映射表
 * @return UbseResult 操作结果
 */
UbseResult UbseGetDirConnectInfoFromRemote(const std::string &nodeId,
                                           std::map<std::string, PhysicalLink> &devDirConnectInfoRemote);

/**
 * 注册Agent端消息处理器
 */
UbseResult RegAgentMsgHandler();

/**
 * Agent处理Master的采集请求
 * @param req 请求数据，包含序列化的采集参数
 * @param resp 响应数据，返回当前节点的序列化信息
 * @return UbseResult 处理结果
 */
UbseResult CollectNodeInfoHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

/**
 * Agent向Master周期上报节点信息
 * @param nodeId 目标Master节点ID
 * @param info 要上报的节点信息
 * @return UbseResult 发送结果
 */
UbseResult UbseNodeReportNodeInfo(const std::string &nodeId, const UbseNodeInfo &info);

/**
 * Agent向Master上报LCNE拓扑变化
 * @param nodeId 目标Master节点ID
 * @param info 包含拓扑变化的节点信息
 * @return UbseResult 发送结果
 */
UbseResult LcneChangeReportNodeInfo(const std::string &nodeId, const UbseNodeInfo &info);

/**
 * Master从Agent采集节点信息
 * @param nodeId 目标Agent节点ID
 * @param info 输出参数，采集到的节点信息
 * @return UbseResult 采集结果
 */
UbseResult CollectRemoteNodeInfo(const std::string &nodeId, UbseNodeInfo &info);

/**
 * Agent下发本节点urma topo
 * @param isBeforeElection 是否在选举完成前下发，默认为false
 * @return UbseResult 下发结果
 */
UbseResult SetUrmaUvs(bool isBeforeElection);

/**
 * Agent发布 urma变化事件给 urmactl
 * @param nodeId 目标Agent节点ID
 * @param action 变化的动作
 * @return UbseResult 发布结果
 */
UbseResult PubNodeUrmaChange(std::string &nodeId, std::string action);

/**
 * Agent处理主节点发送的节点变更通知
 * @param req 请求数据，包含序列化的拓扑变更信息
 * @param resp 响应数据，返回处理结果
 * @return UbseResult 处理结果
 */
UbseResult nodeChangeHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

} // namespace ubse::nodeController

#endif // UBS_ENGINE_UBSE_NODE_CONTROLLER_AGENT_H
