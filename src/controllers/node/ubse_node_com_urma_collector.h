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

#ifndef UBSE_NODE_COM_URMA_COLLECTOR_H
#define UBSE_NODE_COM_URMA_COLLECTOR_H
#include "ubse_common_def.h"
#include "ubse_node_controller.h"
#include "adapter_plugins/mti/ubse_mti_def.h"
#include "adapter_plugins/urma/ubse_urma_uvs.h"

namespace ubse::nodeController {
using ubse::adapter_plugins::mti::UbseMtiEidGroup;
using ubse::adapter_plugins::mti::UbseMtiIouInfo;
using ubse::common::def::UbseResult;
using ubse::urma::UbseUrmaUvsAggrDev;
using ubse::urma::UbseUrmaUvsFe;
using ubse::urma::UbseUrmaUvsNodeInfo;

class UbseNodeComUrmaCollector {
public:
    static UbseNodeComUrmaCollector& GetInstance()
    {
        static UbseNodeComUrmaCollector instance;
        return instance;
    };

    UbseResult GetAllComUrma(std::vector<UbseUrmaUvsNodeInfo>& hostUrmaInfos);

    UbseResult GetComUrmaByNodeId(const std::string& nodeId, std::vector<UbseUrmaUvsNodeInfo>& hostUrmaInfos);

    UbseResult FillComUrmaInfo();

    UbseResult FillComUrmaInfoClos();

    UbseResult SetComUrma(std::vector<PhysicalLink>& allLinkInfo, bool isBeforeElection);

    // 获取当前节点MTI感知的设备拓扑信息(仅包含UP的端口)
    UbseResult GetCurNodeTopo(std::vector<PhysicalLink>& allLinkInfo);

    // 获取当前节点MTI感知的端口信息
    UbseResult GetCurNodePorts(std::vector<PhysicalLink>& allLinkInfo);

    UbseResult GetCurNodeIouList(std::vector<UbseMtiIouInfo>& iouList);

private:
    UbseResult ProcessClusterNode(const std::string& curNodeId, uint32_t serverIdx);

    UbseResult ProcessFeDevice(uint32_t serverIdx, const UbseUrmaUvsFe& srcFe, UbseUrmaUvsFe& destFe);

    void FillComUrmaFeInfo(const std::string& nodeId, const std::pair<UbseMtiIouInfo, UbseMtiEidGroup>& socketComEid);

    std::map<std::string, UbseUrmaUvsAggrDev> comUrmaInfos;
};

} // namespace ubse::nodeController
#endif // UBSE_NODE_COM_URMA_COLLECTOR_H
