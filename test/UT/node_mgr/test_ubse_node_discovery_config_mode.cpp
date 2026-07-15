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

#include "test_ubse_node_discovery_config_mode.h"

#include "conf_mode/ubse_node_discovery_config_mode.h"
#include "ubse_net_util.h"
#include "ubse_error.h"
#include "ubse_node_static_info_mgr.h"

namespace ubse::nodeMgr {
using namespace ubse::utils;

void TestUbseNodeDiscoveryConfigMode::SetUp()
{
    Test::SetUp();
}

void TestUbseNodeDiscoveryConfigMode::TearDown()
{
    UbseNodeStaticInfoMgr::GetInstance().nodes_.clear();
    UbseNodeDiscoveryConfigMode::GetInstance().isClos_ = false;
    UbseNodeDiscoveryConfigMode::GetInstance().podCapability_ = DEFAULT_POD_CAPABILITY;
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeDiscoveryConfigMode, TestInit)
{
    std::vector<std::string> ipList{"192.168.100.100", "192.168.100.107"};
    MOCKER(&UbseNodeStaticInfoMgr::IsClos).stubs().will(returnValue(true));
    MOCKER(&UbseNodeStaticInfoMgr::GetPodCapability).stubs().will(returnValue(DEFAULT_POD_CAPABILITY));
    MOCKER(&UbseNodeStaticInfoMgr::GetClusterIpList).stubs().will(returnValue(ipList));

    MOCKER(UbseNetUtil::FindLocalIpInIpList)
        .stubs()
        .will(returnValue(UBSE_ERROR_EMPTY))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_ERROR_EMPTY, UbseNodeDiscoveryConfigMode::GetInstance().Init());

    MOCKER(&UbseNodeDiscoveryConfigMode::GenerateClusterTopo).stubs().will(ignoreReturnValue());
    EXPECT_EQ(UBSE_OK, UbseNodeDiscoveryConfigMode::GetInstance().Init());
}

TEST_F(TestUbseNodeDiscoveryConfigMode, TestInitWithEmptyIpList)
{
    std::vector<std::string> emptyIpList{};
    MOCKER(&UbseNodeStaticInfoMgr::IsClos).stubs().will(returnValue(false));
    MOCKER(&UbseNodeStaticInfoMgr::GetClusterIpList).stubs().will(returnValue(emptyIpList));
    MOCKER(&UbseNodeStaticInfoMgr::InitCurNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseNetUtil::FindLocalIpInIpList).stubs().will(returnValue(UBSE_ERROR_EMPTY));
    EXPECT_EQ(UBSE_ERROR_EMPTY, UbseNodeDiscoveryConfigMode::GetInstance().Init());
}

TEST_F(TestUbseNodeDiscoveryConfigMode, TestParseIpList)
{
    std::vector<std::string> emptyList{};
    UbseNetUtil::ParseIpList("192.168.100.107-192.168.100.104", emptyList);
    EXPECT_EQ(emptyList.size(), 0);

    std::vector<std::string> nodeList{};
    UbseNetUtil::ParseIpList(
        "192.168.100.100-192.168.100.104,192.168.100.106,192.168.100.108-192.168.100.110", nodeList);
    EXPECT_EQ(nodeList.size(), 9);
    EXPECT_EQ(nodeList[0], "192.168.100.100");
    EXPECT_EQ(nodeList[1], "192.168.100.101");
    EXPECT_EQ(nodeList[2], "192.168.100.102");
    EXPECT_EQ(nodeList[3], "192.168.100.103");
    EXPECT_EQ(nodeList[4], "192.168.100.104");
    EXPECT_EQ(nodeList[5], "192.168.100.106");
    EXPECT_EQ(nodeList[6], "192.168.100.108");
    EXPECT_EQ(nodeList[7], "192.168.100.109");
    EXPECT_EQ(nodeList[8], "192.168.100.110");
}

TEST_F(TestUbseNodeDiscoveryConfigMode, GenerateClusterTopo)
{
    std::vector<std::string> nodeList{};
    UbseNetUtil::ParseIpList(
        "192.168.100.100-192.168.100.104,192.168.100.106,192.168.100.108-192.168.100.110", nodeList);
    UbseNodeStaticInfo info{};
    info.groupId = 1;
    info.addr = "192.168.100.101";
    UbseNodeStaticInfoMgr::GetInstance().SetCurrentNode(info);
    UbseNodeDiscoveryConfigMode::GetInstance().podCapability_ = 2;
    UbseNodeDiscoveryConfigMode::GetInstance().isClos_ = true;
    UbseNodeDiscoveryConfigMode::GetInstance().GenerateClusterTopo(nodeList);
    EXPECT_EQ(5, UbseNodeStaticInfoMgr::GetInstance().GetAllNodes().size());
}
} // namespace ubse::nodeMgr