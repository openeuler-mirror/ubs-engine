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

#include "ubse_topology_interface.h"

namespace ubse::mti {
/**
 * @brief 获取LCNE提供的本节点信息
 * @param[out] rackNodeInfo: 当前节点信息
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetLocalNodeInfo(MtiNodeInfo &rackNodeInfo)
{
    return 0;
}

/**
 * @brief 获取LCNE感知的集群信息
 * @param[out] rackNodeInfos: 整个集群节点信息
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetAllNodeInfos(std::vector<UbseMtiNodeInfo> &rackNodeInfos)
{
    return 0;
}
} // namespace ubse::mti