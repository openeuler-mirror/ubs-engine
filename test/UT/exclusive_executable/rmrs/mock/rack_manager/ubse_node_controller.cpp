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

#include "ubse_node_controller.h"
#include <vector>

namespace ubse::nodeController {

/**
 * 获取全量节点列表，agent节点向主节点请求
 * @return
 */
std::unordered_map<std::string, UbseNodeInfo> UbseNodeController::GetAllNodes()

{
    std::unordered_map<std::string, UbseNodeInfo> maps{};
    return maps;
}

uint32_t UbseNodeController::GetMemGroupNodeList(UbseMemGroupNodeList& groupList)
{
    return 0;
}

uint32_t UbseNodeController::GetMemProviderNodeList(UbseMemProviderNodeList& providerList)
{
    return 0;
}

UbseNodeInfo UbseNodeController::GetNodeById(const std::string& nodeId)
{
    UbseNodeInfo ubseNodeInfo;
    ubseNodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    return ubseNodeInfo;
}

} // namespace ubse::nodeController