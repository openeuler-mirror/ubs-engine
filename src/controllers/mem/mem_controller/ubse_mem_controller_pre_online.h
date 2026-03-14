/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#ifndef UBS_ENGINE_PRO_UBSE_MEM_CONTROLLER_PRE_ONLINE_H
#define UBS_ENGINE_PRO_UBSE_MEM_CONTROLLER_PRE_ONLINE_H

#include <shared_mutex>
#include <thread>
#include <unordered_set>
#include "ubse_common_def.h"
#include "ubse_node_controller.h"
#include "ubse_mmi_interface.h"

namespace ubse::mem::controller {
using namespace ubse::nodeController;
using namespace ubse::common::def;
using namespace ubse::adapter_plugins::mmi;

// 记录节点状态
enum class PreOnLineState {
    OFFLINE,
    ONLINE
};

// 主节点向执行节点发布预上线请求；
struct PreOnLineReq {
    std::vector<SocketCnaInfo> cnas; // 预上线参数
    std::string taskName;            // 预上线任务名称，标识主节点处理任务
};

// 执行节点预上线结束后，向主节点同步预上线结果
struct PreOnLineResp {
    std::string taskName;    // 预上线任务名称
    std::string operateNode; // 执行节点id
    UbseResult ret;          // 预上线执行结果
};

// 主节点记录，当前集群中存在的预上线任务
struct PreOnLineTask {
    std::string taskName;                    // 预上线任务名称-全局唯一
    std::string nodeId;                      // 预上线节点id
    uint64_t timestamp;                      // 预上线任务创建时间，超时后需要将状态置为offline
    std::vector<std::string> preOnLineNodes; // 与预上线节点存在LCNE直连关系的节点，需要对直连节点依次下发预上线请求
    // 预期状态，初始为 online
    // 只要有一个节点预上线失败，将预期状态置为offline，并刷新节点状态map，若最后一个节点上线完毕，且预期状态仍是online，刷新节点状态map
    PreOnLineState expectState = PreOnLineState::ONLINE;
};

UbseResult PreOnlineInit();

/**
 * 算法决策当前集群是否可以进行numa借用（不保证借用成功）；判定条件：集群存在>=2个已上线节点
 * @return
 */
bool IsClusterPreOnLineReady();

/**
 * 判断某个节点是否预上线成功
 */
bool IsNodeOnLine(const std::string &nodeId);

/**
* 校验当前是否需要进行预上线
* 1.配置是否启动预上线能力
* 2.当前节点是否已经处于ONLINE状态
* @param nodeId
* @return
*/
bool ValidPreOnLine(const std::string &nodeId);

/**
* 注册给Node controller回调，在触发时需要根据当前集群节点数进行操作
* 1. 集群只有1个节点，不进行预上线
* 2. 集群有2个节点，若存在LCNE直连关系，执行预上线动作，并根据执行结果设置上线状态；若不存在直连关系，不执行预上线动作，但是将状态设置为ONLINE
* 3. 集群>2个节点，根据当前节点是否跟集群中其余节点存在直连关系决策是否需要预上线
* @param node
* @return
*/
UbseResult PreOnlineHandler(const ubse::nodeController::UbseNodeInfo &node);

/**
* 注册给事件模块，监听LCNE拓扑变化回调，对拓扑变化节点执行预上线操作；上线逻辑同上平滑回调
* @param eventId
* @param eventMessage
* @return
*/
UbseResult LcneTopologyChangeHandler(std::string &eventId, std::string &eventMessage);

/**
* 过滤出当前集群内与当前节点LCNE直连的非故障节点；故障状态：Fault, Unknown
* @param currentNode
* @return
*/
std::unordered_set<std::string> FilterLcneRemote(ubse::nodeController::UbseNodeInfo currentNode);

/**
* 构造当前节点的预上线参数
* @param nodeId
* @return
*/
std::vector<SocketCnaInfo> GeneratePreOnLineCna(const std::string &nodeId);

/**
* 主节点-下发预上线请求
* @param currentNode
* @param isInitCluster
* @return
*/
UbseResult PreOnLineExec(ubse::nodeController::UbseNodeInfo currentNode, bool isInitCluster);

/**
* 执行节点-执行预上线操作
* @param cnas
* @return
*/
void OperatePreOnLine(PreOnLineReq req);

/**
 * 主节点-收到执行节点处理结果后处理预上线结果
 * @param reply
 */
void handlePreOnLineTask(PreOnLineResp reply);

/**
 * 执行节点-进程退出后，执行节点预下线
 */
void PreOnLineUnInit();

uint32_t SerializePreOnLine(PreOnLineReq req, uint8_t *&buffer, size_t &size);

uint32_t DeSerializePreOnLine(PreOnLineReq &req, uint8_t *buffer, size_t size);

uint32_t SerializePreOnlineResp(PreOnLineResp resp, uint8_t *&buffer, size_t &size);

uint32_t DeSerializePreOnLineResp(PreOnLineResp &resp, uint8_t *buffer, size_t size);

/**
* 主备倒换后清空预上线map
*/
void ClearOnLineMap();

/**
* 判定是否启动预上线能力
* @return
*/
bool IsPreOnLineEnable();

/**
* 查询预上线内存大小，单位MB，范围[128~262144]，默认值 4098
* @return
*/
uint64_t PreOnLineSize();

} // namespace ubse::mem::controller

#endif // UBS_ENGINE_PRO_UBSE_MEM_CONTROLLER_PRE_ONLINE_H