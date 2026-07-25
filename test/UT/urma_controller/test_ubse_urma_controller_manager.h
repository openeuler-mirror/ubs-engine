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

#ifndef TEST_UBSE_URMA_CONTROLLER_MANAGER_H
#define TEST_UBSE_URMA_CONTROLLER_MANAGER_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mockcpp/mokc.h>
#include "ubse_common_def.h"
#include "ubse_election.h"
#include "ubse_mti_interface.h"
#include "ubse_urma_def.h"
#include "adapter_plugins/urma/ubse_urma_uvs.h"

namespace ubse::urmaController {
using common::def::UbseResult;
UbseResult GetHostUrmaDev(const std::string& nodeId, ubse::urma::UbseUrmaUvsNodeInfo& uvsInfo);
bool CompareStringsNumerically(const std::string& left, const std::string& right);
ubse::urma::FeType ConvertLcneFeTypeToUrmaFeType(const ubse::adapter_plugins::mti::UbseMtiFeType& lcneFeType);
uint64_t GenerateHwResId(const ubse::adapter_plugins::mti::UbseMtiFeInfo& lcneFe);
bool ValidateLcneFeInfo(const std::vector<std::vector<ubse::adapter_plugins::mti::UbseMtiFeInfo>>& feInfos);
void CalculateFeTopoType(const std::vector<std::vector<ubse::adapter_plugins::mti::UbseMtiFeInfo>>& feInfos);
ubse::urma::EidGroup MakeEidGroup(ubse::adapter_plugins::mti::UbseMtiEidGroup& src,
                                  const std::shared_ptr<ubse::urma::UbseFeInfo>& feInfo);
void SortContainerFeInfos(std::vector<std::vector<ubse::adapter_plugins::mti::UbseMtiFeInfo>>& feInfos);
UbseResult ParseFeIdsFromEid(const std::string& devEid, std::pair<uint16_t, uint16_t>& feIds);
std::shared_ptr<ubse::urma::UbseFeInfo> FindMatchingFeInfo(
    const std::map<std::string, ubse::urma::UbseUrmaInfo, ubse::urma::UrmaNameCompare>& urmaList,
    const std::string& ubpuId, const std::string& entityId, uint64_t& hwResId);
UbseResult FetchCurNodeComDev(const std::string& nodeId, ubse::urma::UbseUrmaUvsAggrDev& comDev);
UbseResult AppendUrmaListToUvsDevList(
    const std::map<std::string, ubse::urma::UbseUrmaInfo, ubse::urma::UrmaNameCompare>& urmaList,
    ubse::urma::UbseUrmaUvsNodeInfo& uvsInfo);
UbseResult FillUrmaUvsNodeInfo(bool isBuildHostOnly, ubse::urma::UbseUrmaNodeInfo& nodeInfo,
                               ubse::urma::UbseUrmaUvsNodeInfo& tmpUvsInfo);
bool IsSameFeWithHostUrmaDev(const std::string& nodeId, const ubse::urma::UbseUrmaUvsNodeInfo& hostUrmaInfo,
                             const ubse::adapter_plugins::mti::UbseMtiFeInfo& lcneFe);
void SwapCommEidGroupToHostPosition(std::vector<ubse::adapter_plugins::mti::UbseMtiEidGroup>& eidGroups,
                                    const std::string& commPrimaryEid, size_t hostIdx);
void FilterCommEidGroupFromContainer(
    std::vector<std::vector<ubse::adapter_plugins::mti::UbseMtiFeInfo>>& containerFeInfos,
    const ubse::urma::UbseUrmaUvsNodeInfo& hostUrmaInfo);
UbseResult SplitFeInfos(const std::string& nodeId,
                        const std::vector<std::vector<ubse::adapter_plugins::mti::UbseMtiFeInfo>>& feInfos,
                        std::vector<std::vector<ubse::adapter_plugins::mti::UbseMtiFeInfo>>& containerFeInfos,
                        std::vector<std::vector<ubse::adapter_plugins::mti::UbseMtiFeInfo>>& hostFeInfos);
UbseResult FilterFeInfosForNonClos(
    const std::string& nodeId, const std::vector<std::vector<ubse::adapter_plugins::mti::UbseMtiFeInfo>>& feInfos,
    std::vector<std::vector<ubse::adapter_plugins::mti::UbseMtiFeInfo>>& containerFeInfos,
    std::vector<std::vector<ubse::adapter_plugins::mti::UbseMtiFeInfo>>& hostFeInfos);
UbseResult CheckAndStoreUrmaBonding(
    const std::string& nodeId, std::vector<std::vector<ubse::adapter_plugins::mti::UbseMtiFeInfo>>& feInfos,
    std::vector<std::vector<ubse::adapter_plugins::mti::UbseMtiFeInfo>>& containerFeInfos,
    std::vector<std::vector<ubse::adapter_plugins::mti::UbseMtiFeInfo>>& hostFeInfos);
} // namespace ubse::urmaController
namespace ubse::urmaControllerManager::ut {
class TestUbseUrmaControllerManager : public testing::Test {
    void SetUp() override
    {
        Test::SetUp();
    }

    void TearDown() override
    {
        Test::TearDown();
        GlobalMockObject::verify();
    }
};
} // namespace ubse::urmaControllerManager::ut

#endif // TEST_UBSE_URMA_CONTROLLER_MANAGER_H
