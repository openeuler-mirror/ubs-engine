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
#include "ubse_topology_interface.h"
#include "ubse_urma.h"
#include "ubse_urma_uvs.h"

namespace ubse::nodeController {
using namespace ubse::common::def;
using namespace ubse::urma;
using namespace ubse::mti;

class UbseNodeComUrmaCollector {
public:
    static UbseNodeComUrmaCollector &GetInstance()
    {
        static UbseNodeComUrmaCollector instance;
        return instance;
    };

    UbseResult GetAllComUrma(std::vector<UbseUrmaInfo> &hostUrmaInfos);

    UbseResult FillComUrmaInfo();

    UbseResult SetComUrma(std::vector<PhysicalLink> &allLinkInfo);

    UbseResult GetCurNodeTopo(std::vector<PhysicalLink> &allLinkInfo);

    UbseResult GetCurNodeIouList(std::vector<UbseLcneIouInfo> &iouList);

private:
    std::map<std::string, UbseUrmaInfo> comUrmaInfos; // key=nodeid
};

} // namespace ubse::nodeController
#endif // UBSE_NODE_COM_URMA_COLLECTOR_H
