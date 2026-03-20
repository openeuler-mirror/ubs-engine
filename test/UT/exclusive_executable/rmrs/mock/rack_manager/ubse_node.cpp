/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_node.h"
namespace ubse::nodeController {

/**
* 获取集群的拓扑信息
* @param topologies 节点的拓扑信息
* @return RackResult, 成功返回0, 失败返回非0
*/
uint32_t RackGetNodeTopology(std::vector<UbseNodeTopology> &topologies)
{
    return 0;
}

/**
 * @brief 获取全量拓扑信息接口
 * @param[out] nodeTopology: 所有节点和他的一跳链接信息
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseMemGetTopologyInfo(std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology)
{
    return 0;
}

}