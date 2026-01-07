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
#include "ubse_error.h"
#include "ubse_node_controller.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_def.h"

namespace ubse::urmaController {
using namespace ubse::common::def;
using namespace ubse::urma;

typedef struct {
    std::string vfe0Path;
    std::string vfe1Path;
    std::string bondingPath;
    std::string bondingEid;
} UbseUrmaDevPath;

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
    UbseResult UbseGetLocalUrmaDevInfoByType(const UrmaDevType type, std::vector<std::string> &nameInfo,
                                             std::vector<uint32_t> &status);
    UbseResult UbseAllocUrmaDev(const std::string name, UbseUrmaDevPath &devPaths);
    UbseResult UbseFreeUrmaDev(const std::string name);

    UbseResult UbseGetUrmaDevInfoByNodeIdAndType(const UrmaDevType type, const uint32_t &nodeId,
                                                 std::vector<UbseUrmaInfoForQuery> &devInfos);

    std::vector<ubse::nodeController::PhysicalLink> GetDirConnectInfo();
    static UbseResult UbseTopoLinkChangeHandler(std::string &eventId, const std::string &eventMesage);
    static UbseResult UbseNodeJoinHandler(std::string &eventId, const std::string &eventMesage);

private:
    void DoNodeJoin();
    void DoTopoLinkChange();
    bool UbseUrmaBandWidthCheck(UbseUrmaInfo urmaInfo, const std::string profileName);
    UbseResult UbseQueryUrmaInfoByRpc(const uint32_t &nodeId, const UrmaDevType type,
                                      std::vector<UbseUrmaInfoForQuery> &urmaInfo);
};

template <typename Func, typename... Args>
UbseResult CallFuncRetry(Func func, Args &&...args)
{
    static_assert(std::is_invocable_r_v<UbseResult, Func, Args...>,
                  "Func must be callable with the provided arguments and return UbseResult");
    const int retryCount = 5;
    const int sleepSecondPerTry = 1;
    UbseResult ret = UBSE_OK;
    for (int i = 0; i < retryCount; ++i) {
        ret = func(std::forward<Args>(args)...);
        if (ret == UBSE_OK) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(sleepSecondPerTry));
    }
    return ret;
}
} // namespace ubse::urmaController
#endif // UBSE_URMA_CONTROLLER_H