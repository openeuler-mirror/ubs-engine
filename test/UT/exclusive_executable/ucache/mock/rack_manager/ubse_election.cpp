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

#include "ubse_election.h"
#include <string>
#include <vector>

namespace ubse::election {
/**
 * @brief 查询所有物理节点 nodeIds
 * @param[out] nodeIds: 所有物理节点id
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetNodeIds(std::vector<std::string> &nodeIds)
{
    nodeIds = {"Node0", "Node1"};
    return 0;
}

/**
 * @brief 新增选举角色改变的回调函数
 * @param[in] handler 回调函数
 * @return 成功返回 0, 失败返回非 0
 */
uint32_t UbseElectionChangeAttachHandler(const UbseElectionHandler &handler)
{
    return 0;
}

/**
 * @brief 移除选举角色改变的回调函数
 * @param[in] handler 回调函数
 * @return 成功返回 0, 失败返回非 0
 */
uint32_t UbseElectionChangeDeAttachHandler(const UbseElectionHandler &handler)
{
    return 0;
}

/**
 * 获取当前角色
 * @param role [OUT] 待获取角色
 * @return RackResult, 成功返回0, 失败返回非0
 */
uint32_t RackGetRole(std::string &role)
{
    role = "Master";
    return 0;
}

/**
 * 配置当前角色
 * @param role [IN] 目标角色
 * @return RackResult, 成功返回0, 失败返回非0
 */
uint32_t RackSetRole(std::string &role)
{
    return 0;
}

/**
 * 获得Master角色节点ID
 * @param masterNodeId [out] Master角色节点ID
 * @return RackResult, 成功返回0, 失败返回非0
 */
uint32_t UbseGetMasterNodeId(std::string &masterNodeId)
{
    masterNodeId = "NODE11347";
    return 0;
}

/**
 * 获得当前节点的NodeID
 * @param currentNodeId [out] 获得当前节点的NodeID
 * @return RackResult, 成功返回0, 失败返回非0
 */
uint32_t UbseGetCurrentNodeId(std::string &currentNodeId)
{
    currentNodeId = "NODE11347";
    return 0;
}
} // namespace ubse::election