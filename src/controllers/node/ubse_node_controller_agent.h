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

#include <shared_mutex>

#include "ubse_com_module.h"
#include "ubse_common_def.h"
#include "ubse_node_controller.h"
#include "ubse_thread_pool.h"

namespace ubse::nodeController {
using ubse::common::def::UbseResult;
using ubse::task_executor::UbseTaskExecutorPtr;

class UbseNodeControllerAgent {
public:
    static UbseNodeControllerAgent& GetInstance()
    {
        static UbseNodeControllerAgent instance;
        return instance;
    }

    UbseResult Initialize();

    void UnInitialize();

    UbseResult Start();

    void Stop();

    /**
     * 处理主节点NODE_INFO_SYNC单点推送：seq校验+双过滤后upsert镜像
     * @param req 消息体（seq + faultUpdateTimeSysMs + 节点）
     * @param resp 响应（空）
     */
    UbseResult HandleNodeInfoSync(const UbseByteBuffer& req, UbseByteBuffer& resp);

    /**
     * 处理主节点NODE_INFO_SYNC_FULL全量快照：seq校验+双过滤后覆盖镜像
     * @param req 消息体（seq + 节点列表 + FAULT保护map）
     * @param resp 响应（空）
     */
    UbseResult HandleNodeInfoSyncFull(const UbseByteBuffer& req, UbseByteBuffer& resp);

    /**
     * 清空镜像并重置lastSyncSeq（备降从、升主消费后调用）
     */
    void ClearMirror();

    /**
     * 升主消费：读取镜像快照副本（含FAULT保护窗口登记时刻），供回填nodeInfos
     * @param mirror 输出参数，节点信息镜像副本
     * @param faultProtect 输出参数，FAULT节点保护窗口登记时刻(system_clock epoch ms)
     */
    void GetMirrorSnapshot(std::unordered_map<std::string, UbseNodeInfo>& mirror,
                           std::unordered_map<std::string, uint64_t>& faultProtect);

    /**
     * 成为备角色时向主节点主动拉取全量快照，覆盖重建镜像
     */
    void PullNodeInfoFromMaster();

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
    static UbseResult UbseNodeInfoLcneNotifyHandler(std::string& eventId, std::string& eventMessage);

    void StartExec();

    // 解析并应用NODE_INFO_SYNC_FULL负载（RPC收包与主动拉取响应共用）
    UbseResult ProcessNodeInfoSyncFull(const uint8_t* data, uint32_t len);

    // 当前节点是否为主节点委派的备节点（非备角色忽略同步消息）
    bool IsStandbyNode() const;

    // 备节点镜像：主节点内存中除"主/备自己"外的存量节点快照
    std::unordered_map<std::string, UbseNodeInfo> nodeInfoMirror_;
    // FAULT节点保护窗口登记时刻(system_clock epoch ms)
    std::unordered_map<std::string, uint64_t> faultProtectMirror_;
    // 已应用的最大同步序号（乱序过滤）
    uint64_t lastSyncSeq_{0};
    std::shared_mutex mirrorMutex_;

    UbseTaskExecutorPtr taskExecutor_{};
};

/**
 * Agent向Master查询全量节点列表
 * @param nodeId 目标Master节点ID
 * @param infos 输出参数，全量节点信息列表
 * @return UbseResult 操作结果
 */
UbseResult GetAllNodeInfoFromRemote(const std::string& nodeId, std::vector<UbseNodeInfo>& infos);

/**
 * Agent向Master查询全量链路信息
 * @param nodeId 目标Master节点ID
 * @param devDirConnectInfoRemote 输出参数，全量链路信息映射表
 * @return UbseResult 操作结果
 */
UbseResult UbseGetDirConnectInfoFromRemote(const std::string& nodeId,
                                           std::map<std::string, PhysicalLink>& devDirConnectInfoRemote);

// 注册Agent端消息处理器
UbseResult RegAgentMsgHandler();

/**
 * Agent处理Master的采集请求
 * @param req 请求数据，包含序列化的采集参数
 * @param resp 响应数据，返回当前节点的序列化信息
 * @return UbseResult 处理结果
 */
UbseResult CollectNodeInfoHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);

// 主→备 单点增量端点处理器：校验发送方为当前主节点后调用HandleNodeInfoSync（经ctx获取发送方Id，防任意节点注入）
class UbseNodeInfoSyncMsgHandler : public com::UbseComBaseMessageHandler {
public:
    UbseResult Handle(const ubse::message::UbseBaseMessagePtr& req, const ubse::message::UbseBaseMessagePtr& rsp,
                      com::UbseComBaseMessageHandlerCtxPtr ctx) override;
    uint16_t GetOpCode() override;
    uint16_t GetModuleCode() override;
};

// 主→备 全量快照端点处理器：校验发送方为当前主节点后调用HandleNodeInfoSyncFull
class UbseNodeInfoSyncFullMsgHandler : public com::UbseComBaseMessageHandler {
public:
    UbseResult Handle(const ubse::message::UbseBaseMessagePtr& req, const ubse::message::UbseBaseMessagePtr& rsp,
                      com::UbseComBaseMessageHandlerCtxPtr ctx) override;
    uint16_t GetOpCode() override;
    uint16_t GetModuleCode() override;
};

/**
 * Agent向Master周期上报节点信息
 * @param nodeId 目标Master节点ID
 * @param info 要上报的节点信息
 * @return UbseResult 发送结果
 */
UbseResult UbseNodeReportNodeInfo(const std::string& nodeId, const UbseNodeInfo& info);

/**
 * Agent向Master上报LCNE拓扑变化
 * @param nodeId 目标Master节点ID
 * @param info 包含拓扑变化的节点信息
 * @return UbseResult 发送结果
 */
UbseResult LcneChangeReportNodeInfo(const std::string& nodeId, const UbseNodeInfo& info);

/**
 * Master从Agent采集节点信息
 * @param nodeId 目标Agent节点ID
 * @param info 输出参数，采集到的节点信息
 * @return UbseResult 采集结果
 */
UbseResult CollectRemoteNodeInfo(const std::string& nodeId, UbseNodeInfo& info);

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
UbseResult PubNodeUrmaChange(std::string& nodeId, std::string action);

/**
 * Agent处理主节点发送的节点变更通知
 * @param req 请求数据，包含序列化的拓扑变更信息
 * @param resp 响应数据，返回处理结果
 * @return UbseResult 处理结果
 */
UbseResult nodeChangeHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);

} // namespace ubse::nodeController

#endif // UBS_ENGINE_UBSE_NODE_CONTROLLER_AGENT_H
