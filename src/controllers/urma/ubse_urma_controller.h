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

#include <chrono>
#include <thread>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_node_controller.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_def.h"

namespace ubse::urmaController {
using namespace ubse::common::def;
using namespace ubse::urma;

class UrmaController {
public:
    static UrmaController &GetInstance()
    {
        static UrmaController instance;
        return instance;
    }

    UbseResult UbseUrmaBandWidthSet(const std::string urmaName, uint32_t minBandWidth, uint32_t maxBandWidth);
    UbseResult UbseUrmaBandWidthGet(const std::string urmaName, uint32_t &minBandWidth, uint32_t &maxBandWidth);
    UbseResult UbseUrmaBandWidthReset(const std::string urmaName);
    void UbseUrmaBandWidthUpdate(const std::string urmaName);
    // For SDK
    UbseResult UbseGetLocalUrmaDevInfo(std::vector<std::string> &nameInfo, std::vector<uint32_t> &status,
                                       std::vector<uint64_t> &hwResIds);
    UbseResult UbseAllocUrmaDev(const std::string name, UbseUrmaDevPath &devPaths);
    UbseResult UbseFreeUrmaDev(const std::string name);

UbseResult UbseGetUrmaDevInfoByNodeId(const uint32_t &nodeId, std::vector<UbseUrmaInfoForQuery> &devInfos);
    UbseResult UbseUrmaCliDevActivate(const std::string &nodeId);

    static UbseResult UbseTopoLinkChangeHandler(std::string &eventId, const std::string &eventMesage);
    static UbseResult UbseNodeJoinHandler(std::string &eventId, const std::string &eventMesage);

private:
    UbseResult DoNodeJoin(const std::string &joinNodeId);
    UbseResult HandleNodeJoinWithRetry(const std::string &joinNodeId);
    UbseResult HandleTopoLinkChangeWithRetry();
    UbseResult DoTopoLinkChange();
    bool UbseUrmaBandWidthCheck(UbseUrmaInfo urmaInfo, const std::string profileName);
UbseResult UbseQueryUrmaInfoByRpc(const uint32_t &nodeId, std::vector<UbseUrmaInfoForQuery> &urmaInfo);
};

std::vector<ubse::nodeController::PhysicalLink> GetDirConnectInfo();
UbseResult UrmaControllerSetUvsInfo(const std::string &current_slot_id, const std::vector<PhysicalLink> &allLinkInfo,
                                    const std::vector<UbseUrmaUvsNodeInfo> &bondingInfo);
UbseResult UrmaCtlActivateUrmaDevice(const std::string &nodeId);
UbseResult QueryUrmaInfoStateFromUrma(const std::string &nodeId);
UbseResult QueryAllPortsStatus(bool &isAllPortDown);
} // namespace ubse::urmaController
#endif // UBSE_URMA_CONTROLLER_H