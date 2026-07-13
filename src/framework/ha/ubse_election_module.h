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

#ifndef UBSE_ELECTION_MODULE_H
#define UBSE_ELECTION_MODULE_H

#include <atomic>
#include <functional>
#include <mutex>
#include "ubse_base_message.h"
#include "ubse_election.h"
#include "ubse_election_def.h"
#include "ubse_election_pkt_handler.h"
#include "ubse_module.h"

namespace ubse::election {
using namespace ubse::module;
using namespace ubse::message;

class UbseElectionModule : public UbseModule {
public:
    static constexpr const char* kModuleName = "UbseElectionModule";
    std::string Name() const override { return kModuleName; }

    // 构造函数
    UbseElectionModule() = default;

    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

    /* *
     * 获得所有节点的Id
     * @param allNodes [out] 所有节点Id, ip, port
     */
    UbseResult UbseGetAllNodes(Node &master, Node &standby, std::vector<Node> &agent);

    /* *
     * 查询全局唯一主节点信息
     * @param[out] 返回集群唯一主节点信息
     * @return UBSE_OK 成功，UBSE_ERROR 失败
     */
    UbseResult UbseGetMasterNode(Node &masterNode);

    /* *
   * 查询全局唯一备节点信息
   * @param[out] 返回集群唯一备节点信息
   * @return UBSE_OK 成功，UBSE_ERROR 失败
   */
    UbseResult UbseGetStandbyNode(Node &standbyNode);

    /* *
     * 查询组内主节点信息
     * @param[out] 返回组内主节点信息
     * @return UBSE_OK 成功，UBSE_ERROR 失败
     */
    UbseResult GetLocalMasterNode(Node &localMasterNode);

    /* *
    * 查询组内备节点信息
    * @param[out] 返回组内备节点信息
    * @return UBSE_OK 成功，UBSE_ERROR 失败
    */
    UbseResult GetLocalStandbyNode(Node &localStandbyNode);

    /* *
     * @brief 获取当前节点视角的HA拓扑信息
     *
     * - GLOBAL_MASTER：返回所有global组及级联组节点信息
     * - GLOBAL_STANDBY/GLOBAL_AGENT：返回本组及其级联组节点信息
     * - GLOBAL_CASCADE/STANDBY/AGENT：返回本组节点信息及其管理组节点信息
     * - Standby/Agent：返回本组节点信息
     * @param haTopology [out] 输出拓扑信息结构体
     * @return UBSE_OK 成功，UBSE_ERROR 失败
     */
    UbseResult GetCurNodeGlobalTopoInfo(HaTopologyInfo &haTopology);

    /* *
     * 查询当前节点ID
     * @param currentNode 当前节点信息
     */
    UbseResult GetCurrentNode(Node &currentNode);

    /* *
     * @brief 查询主节点状态
     * @param[in/out] status 主节点状态
     * @return 成功返回0, 失败返回非0
     */
    UbseResult GetMasterStatus(uint8_t &status);

    /* *
     * @brief 查询备节点状态
     * @param[in/out] status 备节点状态
     * @return 成功返回0, 失败返回非0
     */
    UbseResult GetStandbyStatus(uint8_t &status);

    UbseResult GetNodeIpInfoById(const std::string &id, std::string &ip);

    /* *
     * @brief 查询lcne里的所有节点
     * @param[out]  所有物理节点
     * @return 成功返回0, 失败返回非0
     */
    static UbseResult GetAllNodes(std::vector<Node> &allNodes);

    bool IsLeader();

    void TimerTaskElection();
    void TimerTaskCom();
    /* *
     * @brief 主节点OS Panic之后，立即触发备节点升主流程的调用接口
     */
    void SwitchMasterFromStandby();

    /* *
     * @brief 主节点故障之后，立即触发降为agent
     */
    void SwitchAgentFromMaster();

private:
    std::vector<std::thread> threads_;
    std::mutex timerTaskElectionMtx_;
    std::mutex timerTaskComMtx_;
};
} // namespace ubse::election
#endif // UBSE_ELECTION_MODULE_H
