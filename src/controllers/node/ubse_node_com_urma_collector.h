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
#include "adapter_plugins/urma/ubse_urma_uvs.h"
#include "adapter_plugins/mti/ubse_mti_def.h"

namespace ubse::nodeController {
using namespace ubse::common::def;
using namespace ubse::urma;
using namespace ubse::adapter_plugins::mti;

class UbseNodeComUrmaCollector {
public:
    static UbseNodeComUrmaCollector &GetInstance()
    {
        static UbseNodeComUrmaCollector instance;
        return instance;
    };

    UbseResult GetAllComUrma(std::vector<UbseUrmaUvsNodeInfo> &hostUrmaInfos);

    UbseResult FillComUrmaInfo();

    UbseResult FillComUrmaInfoClos();

    UbseResult SetComUrma(std::vector<PhysicalLink> &allLinkInfo, bool isBeforeElection);

    UbseResult GetCurNodeTopo(std::vector<PhysicalLink> &allLinkInfo);

    UbseResult GetCurNodeIouList(std::vector<UbseMtiIouInfo> &iouList);

private:
    std::map<std::string, UbseUrmaUvsAggrDev> comUrmaInfos;
};

UbseResult OverwriteEid(const uint32_t podId, const uint32_t serverId, const std::string &baseEid, std::string &result);

UbseResult GenerateBondingEid(const uint32_t id0, const uint32_t id1, const uint32_t id2, const uint32_t id3,
                              std::string &bondingEid);
} // namespace ubse::nodeController
#endif // UBSE_NODE_COM_URMA_COLLECTOR_H
