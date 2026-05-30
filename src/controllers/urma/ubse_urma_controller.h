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

#ifndef UBSE_URMA_CONTROLLER_H
#define UBSE_URMA_CONTROLLER_H

#include <vector>
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_node_controller.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_controller_qos.h"
#include "ubse_urma_def.h"

namespace ubse::urmaController {
using common::def::UbseResult;
using nodeController::PhysicalLink;
using urma::UbseUrmaDevBrief;
using urma::UbseUrmaDevPath;
using urma::UbseUrmaInfo;
using urma::UbseUrmaUvsNodeInfo;

class UbseUrmaController {
public:
    static UbseUrmaController& GetInstance()
    {
        static UbseUrmaController instance;
        return instance;
    }

    UbseResult UbseUrmaGetDevs(std::vector<std::string>& nameInfo, std::vector<uint32_t>& status,
                               std::vector<uint64_t>& hwResIds);
    UbseResult UbseAllocUrmaDev(const std::string& name, UbseUrmaDevPath& devPaths);
    UbseResult UbseFreeUrmaDev(const std::string name);
    UbseResult UbseGetUrmaDevsByNodeId(const uint32_t& nodeId, std::vector<UbseUrmaDevBrief>& devInfos);
    static UbseResult UbseTopoLinkChangeHandler(std::string& eventId, const std::string& eventMessage);
    static UbseResult UbseNodeJoinHandler(std::string& eventId, const std::string& eventMessage);

    void FillUrmaDevsByUvsInfo(const std::string& nodeId, std::vector<UbseUrmaUvsNodeInfo>& uvsInfos);
    UbseResult ActivateSpecifyUrmaDev(const std::string& urmaName);
    void GetLocalUrmaDevs(std::vector<UbseUrmaDevBrief>& devInfos);
    bool IsUrmaDevCreated(const UbseUrmaInfo& urmaInfo);

private:
    UbseResult DoNodeJoin(const std::string& joinNodeId);
    UbseResult HandleNodeJoinWithRetry(const std::string& joinNodeId);
    UbseResult HandleTopoLinkChangeWithRetry();
    UbseResult DoTopoLinkChange();
    UbseResult UbseGetUrmaDevsByRpc(const uint32_t& nodeId, std::vector<UbseUrmaDevBrief>& urmaInfo);
};

std::vector<ubse::nodeController::PhysicalLink> GetDirConnectInfo();
UbseResult UbseUrmaControllerSetUvsInfo(const std::string& current_slot_id,
                                        const std::vector<PhysicalLink>& allLinkInfo,
                                        const std::vector<UbseUrmaUvsNodeInfo>& bondingInfo);
/**
 * @brief 查询指定urma的状态，如果为空则查询所有urma，查询后设置urmaInfo状态
 * @param nodeId: 节点ID
 * @param urmaName: urma name，为空时查询所有urma
 * @return 成功返回0, 失败返回非0
 */
void RefreshAllUrmaDevsState(const std::string& nodeId);
void RefreshUrmaDevStateByName(const std::string& nodeId, const std::string& urmaName);
bool IsUdmaDevHealthy(const std::string& feEid);
UbseResult QueryAllPortsDown(bool& isAllPortDown);
} // namespace ubse::urmaController
#endif // UBSE_URMA_CONTROLLER_H