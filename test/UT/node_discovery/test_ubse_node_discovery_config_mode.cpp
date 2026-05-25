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

#include "node_mode/ubse_node_discovery_config_mode.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_net_util.h"
#include "ubse_node_discovery.h"
#include "ubse_smbios.h"

namespace ubse::nodeDiscovery {
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::utils;

void TestUbseNodeDiscoveryConfigMode::SetUp()
{
    Test::SetUp();
}

void TestUbseNodeDiscoveryConfigMode::TearDown()
{
    UbseNodeDiscovery::GetInstance().nodes_.clear();
    Test::TearDown();
    GlobalMockObject::verify();
}

uint32_t MockFindSameNetMaskNotInList(const std::string &ipStr, std::string &localIp)
{
    localIp = "192.168.100.108";
    return UBSE_OK;
}

uint32_t MockFindSameNetMaskInList(const std::string &ipStr, std::string &localIp)
{
    localIp = "192.168.100.107";
    return UBSE_OK;
}

TEST_F(TestUbseNodeDiscoveryConfigMode, TestInit)
{
    MOCKER(&adapter_plugins::smbios::UbseSmbios::IsClosType).stubs().will(returnValue(true));
    std::shared_ptr<UbseConfModule> nullModule = nullptr;
    std::shared_ptr<UbseConfModule> module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    MOCKER(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(UBSE_ERROR_NULLPTR, UbseNodeDiscoveryConfigMode::GetInstance().Init());
    EXPECT_EQ(UBSE_ERROR, UbseNodeDiscoveryConfigMode::GetInstance().Init());

    std::vector<std::string> emptyList{};
    std::vector<std::string> ipList{"192.168.100.100", "192.168.100.107"};
    MOCKER(UbseNetUtil::ParseIpList).stubs().will(returnValue(emptyList)).then(returnValue(ipList));
    EXPECT_EQ(UBSE_ERROR_NULLPTR, UbseNodeDiscoveryConfigMode::GetInstance().Init());

    MOCKER(&UbseNodeDiscoveryConfigMode::InitCurNodeInfo)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_ERROR, UbseNodeDiscoveryConfigMode::GetInstance().Init());
    MOCKER(UbseNetUtil::FindLocalIpByRemote)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(invoke(MockFindSameNetMaskNotInList))
        .then(invoke(MockFindSameNetMaskInList));
    EXPECT_EQ(UBSE_ERROR, UbseNodeDiscoveryConfigMode::GetInstance().Init());
    EXPECT_EQ(UBSE_ERROR_NULLPTR, UbseNodeDiscoveryConfigMode::GetInstance().Init());
    MOCKER(&UbseNodeDiscoveryConfigMode::GenerateClusterTopo).stubs().will(ignoreReturnValue());
    EXPECT_EQ(UBSE_OK, UbseNodeDiscoveryConfigMode::GetInstance().Init());
}

TEST_F(TestUbseNodeDiscoveryConfigMode, TestParseIpList)
{
    auto emptyList = UbseNetUtil::ParseIpList("192.168.100.107-192.168.100.104");
    EXPECT_EQ(emptyList.size(), 0);
    auto nodeList =
        UbseNetUtil::ParseIpList("192.168.100.100-192.168.100.104,192.168.100.106,192.168.100.108-192.168.100.110");
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
    auto nodeList =
        UbseNetUtil::ParseIpList("192.168.100.100-192.168.100.104,192.168.100.106,192.168.100.108-192.168.100.110");
    UbseNodeStaticInfo info{};
    info.podId = 1;
    info.addr = "192.168.100.101";
    UbseNodeDiscovery::GetInstance().SetCurrentNode(info);
    UbseNodeDiscoveryConfigMode::GetInstance().podCapability_ = 2;
    UbseNodeDiscoveryConfigMode::GetInstance().GenerateClusterTopo(nodeList);
    EXPECT_EQ(5, UbseNodeDiscovery::GetInstance().GetAllNodes().size());
}
} // namespace ubse::nodeDiscovery