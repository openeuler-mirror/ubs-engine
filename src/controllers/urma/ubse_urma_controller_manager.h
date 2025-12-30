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

#ifndef UBSE_URMA_CONTROLLER_MANAGER_H
#define UBSE_URMA_CONTROLLER_MANAGER_H
#include <map>
#include "lock/ubse_lock.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_urma_def.h"

namespace ubse::urmaController {
using namespace ubse::common::def;
using namespace ubse::urma;
class UbseUrmaControllerManager {
public:
    static UbseUrmaControllerManager &GetInstance()
    {
        static UbseUrmaControllerManager instance;
        return instance;
    }
    UbseResult InsertNewBoundingInfo(
        const std::string &nodeId,
        const std::vector<UbseFeInfo> &feInfos); // 更新对应节点的fe信息，计算出bounding设备信息，并下发
    UbseResult GetFeInfoByNodeId(const std::string &nodeId, std::vector<UbseFeInfo> &feInfos);
    bool IsBoundingInfoExists(const std::string &nodeId);

    UbseResult GetUrmaNameByType(const UrmaDevType type, std::vector<std::string> &boundingName);

    UbseResult GetVfeByBoundingName(const std::string &boundingName, std::vector<UbseFeInfo> &feInfos);

    UbseResult AllocByBoundingName(const std::string &boundingName, UbseUrmaInfo &boundingInfos);
    UbseResult GetLocalUrmaDevInfo(const std::string urmaName, UbseUrmaInfo &urmaInfo);

private:
    std::vector<UbseUrmaInfo> urmaList{}; // nodeid和计算的bounding信息
    utils::ReadWriteLock rwLock;
    std::vector<UbseFeInfo> FeLists{};
};
} // namespace ubse::urmaController

#endif // UBSE_URMA_CONTROLLER_MANAGER_H
