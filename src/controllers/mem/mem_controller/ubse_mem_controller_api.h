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

#ifndef UBSE_MEM_CONTROLLER_API_H
#define UBSE_MEM_CONTROLLER_API_H

#include <cstdint>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

#include "ubse_error.h"
#include "ubse_mmi_interface.h"
#include "ubse_node_controller.h"
#include "api/ubse_mem_controller_addr_api.h"
#include "api/ubse_mem_controller_fd_api.h"
#include "api/ubse_mem_controller_numa_api.h"
#include "api/ubse_mem_controller_share_api.h"

namespace ubse::mem::controller {
using ubse::adapter_plugins::mmi::UbseMemBorrowImportBaseObj;
using ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo;
using ubse::adapter_plugins::mmi::UbseMemNumaBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemState;
using ubse::nodeController::UbseNodeClusterState;
using ubse::nodeController::UbseNodeController;

enum class BorrowedType
{
    FD,
    NUMA,
    ADDR,
    SHARED
};

std::shared_mutex& GetDecoderImportMutex();

/* *
 * 初始化
 * @return 0: 成功; 非0: 失败
 */
void RegisterNodeCtlNotify();

uint32_t CreateTaskExecutor();

uint32_t Init();

void UnInit();

void Stop();

uint32_t GetCnaInfoWhenImport(const std::string& exportNodeId, const std::string& importNodeId,
                              UbseMemBorrowImportBaseObj& importObj, const bool isFdOrAddr = false,
                              uint32_t specifiedPortId = UINT32_MAX);

uint32_t GetCnaInfoForNumaBorrow(const std::string& exportNodeId, const std::string& importNodeId,
                                 UbseMemNumaBorrowImportObj& importObj);

// 填充importNumaInfos的portId(chipId)（fd/addr借用场景，算法后调用）
void FillImportNumaPortAndChipId(const std::string& exportNodeId, int exportSocketId, const std::string& importNodeId,
                                 std::vector<UbseMemDebtNumaInfo>& importNumaInfos,
                                 uint32_t specifiedPortId = UINT32_MAX);

/* *
 * 启动时加载所有初始对象
 * @return 0: 成功; 非0: 失败
 */
uint32_t LoadLocalAllObjs(const ubse::nodeController::UbseNodeInfo& node);

template <class ObjMap>
void GetNoStateObjMap(ObjMap& objMap, const UbseMemState& state)
{
    for (auto itreator = objMap.begin(); itreator != objMap.end();) {
        if (itreator->second.status.state == state) {
            itreator = objMap.erase(itreator);
            continue;
        }
        ++itreator;
    }
}

template <class importType, class exportType>
UbseResult GetStateByObjExist(const bool& importObjExist, const bool& exportObjExist, const importType& importObj,
                              const exportType& exportObj)
{
    if (!importObjExist && exportObjExist) {
        if (exportObj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            return UBSE_ERR_NOT_EXIST;
        }
        auto nodeId = exportObj.importNodeId;
        auto allNodeInfo = UbseNodeController::GetInstance().GetAllNodes();
        if (allNodeInfo[nodeId].clusterState == UbseNodeClusterState::UBSE_NODE_WORKING) {
            return UBSE_ERR_UNIMPORT_SUCCESS;
        }

        uint8_t retryTimes = 5;
        while (allNodeInfo[nodeId].clusterState == UbseNodeClusterState::UBSE_NODE_SMOOTHING && retryTimes--) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200)); // sleep 200ms
            allNodeInfo = UbseNodeController::GetInstance().GetAllNodes();
        }
    }

    if (importObjExist && !exportObjExist) {
        return GetErrorCodeByObjState(importObj);
    }

    return UBSE_ERR_NOT_EXIST;
}
void ClearNodeMap();
} // namespace ubse::mem::controller

#endif // UBSE_MEM_CONTROLLER_API_H
