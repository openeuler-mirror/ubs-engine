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

#include "ubse_urma_controller_manager.h"
#include "ubse_election.h"

namespace ubse::urmaController {
using namespace ubse::election;

UbseResult UbseUrmaControllerManager::GetLocalUrmaDevInfo(const std::string urmaName, UbseUrmaInfo &urmaInfo)
{
    /* 获取本节点信息 */
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    for (auto i : urmaList) {
        if ((urmaName == i.name) && (currentNodeInfo.nodeId == std::to_string(i.nodeId))) {
            urmaInfo = i;
            return UBSE_OK;
        }
    }

    return UBSE_ERROR;
}

} // namespace ubse::urmaController
