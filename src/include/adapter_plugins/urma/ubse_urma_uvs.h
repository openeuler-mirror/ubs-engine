/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_UVS_INTERFACE_H
#define UBSE_UVS_INTERFACE_H
#include <stdint.h>
#include <map>
#include <string> // for string, basic_string
#include <vector>
#include "ubse_common_def.h"
#include "ubse_node_controller.h"

namespace ubse::urma {
using ubse::common::def::UbseResult;
using ubse::nodeController::PhysicalLink;

struct UbseUrmaUvsFe {
    std::string ubpuId;
    std::string entityId;
    std::string primaryEid;
    std::map<std::string, std::string> portEid;
};

struct UbseUrmaUvsAggrDev {
    std::string urmaDevEid;
    std::vector<UbseUrmaUvsFe> feList;
};

struct UbseUrmaUvsNodeInfo {
    std::string nodeId;
    std::vector<UbseUrmaUvsAggrDev> devList;
};

/**
 * @brief 下发拓扑和聚合设备信息到urma_uvs
 * @param [in] current_slot_id：当前节点ID
 * @param [in] allLinkInfo：拓扑链路信息
 * @param [in] bondingInfo：聚合设备信息
 * @return 成功返回0, 失败返回非0
 */
UbseResult UbsePushTopoAndBondingToUvs(std::string& current_slot_id, const std::vector<PhysicalLink>& allLinkInfo,
                                       const std::vector<UbseUrmaUvsNodeInfo>& bondingInfo);

/**
 * @brief 获得urma设备子路径名称。从urma_uvs获取EID对应的子路径名称（如 /dev/urma0/subpath_abc 中的 subpath_abc）
 * @param [in] urmaEid：urma设备Eid, 格式为 0000:0000:0000:0000:0000:0000:0100:0000
 * @param [out] urmaEidName：urma设备子路径名称，表示urma_uvs给urma设备起的名称。其中聚合设备仅激活后可获得名称
 * @return 成功返回0, 失败返回非0
 */
UbseResult UbseGetUrmaSubpathByEid(const std::string& urmaEid, std::string& urmaSubpath);

/**
 * @brief 获得聚合设备状态。从urma_uvs获取聚合设备名称，判断聚合设备是否已经激活
 * @param [in] urmaEid：聚合设备Eid, 格式为 0000:0000:0000:0000:0000:0000:0100:0000
 * @param [out] isActivate：聚合设备是否激活
 * @return 成功返回0, 失败返回非0
 */
UbseResult UbseGetBondingActiveStateByEid(const std::string& urmaEid, bool& isActive);

/**
 * @brief 下发信息到urma_uvs激活聚合设备
 * @param [in] urmaEid：聚合设备Eid, 格式为 0000:0000:0000:0000:0000:0000:0100:0000
 * @param [in] aggrDevName：聚合设备名称，最大长度为AGGR_DEV_NAME_LEN-1，不包含'\0'
 * @return 成功返回0, 失败返回非0
 */
UbseResult UbseActiveBonding(const std::string& urmaEid, const std::string& aggrDevName);

/**
 * @brief 下发信息到urma_uvs取消激活聚合设备
 * @param [in] urmaEid：聚合设备Eid, 格式为 0000:0000:0000:0000:0000:0000:0100:0000
 * @return 成功返回0, 失败返回非0
 */
UbseResult UbseDeactiveBonding(const std::string& urmaEid);
} // namespace ubse::urma

#endif // UBSE_UVS_INTERFACE_H
