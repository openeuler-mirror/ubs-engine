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

#include "test_ubse_node_controller_util.h"

#include "src/framework/config/ubse_conf_module.h"
#include "src/framework/context/ubse_context.h"
#include "src/res_plugins/mti/ubse_lcne_module.h"
#include "ubse_node_controller_collector.h"

namespace ubse::node_controller::ut {
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::mti;
void TestUbseNodeControllerUtil::SetUp()
{
    Test::SetUp();
}

void TestUbseNodeControllerUtil::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeControllerUtil, Lock)
{
    UbseNodeControllerLockMgr::TryWriteLock("util1");
    UbseNodeControllerLockMgr::WriteUnLock("util1");

    UbseNodeControllerLockMgr::ReadLock("util2");
    UbseNodeControllerLockMgr::ReadUnLock("util2");

    UbseNodeControllerLockMgr::TryReadLock("util3");
    UbseNodeControllerLockMgr::ReadUnLock("util3");
}

TEST_F(TestUbseNodeControllerUtil, IsUBEnable)
{
    std::shared_ptr<UbseConfModule> nullModule = nullptr;
    std::shared_ptr<UbseConfModule> module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));

    EXPECT_EQ(IsUBEnable(), true);
    MOCKER(&UbseConfModule::GetConf<bool>).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(IsUBEnable(), true);
}

static UbseResult MockGetConfString(UbseConfModule *This, std::string &section, std::string &configKey,
                                    std::string &configVal)
{
    configVal = "192.168.100.100-192.168.100.102,192.168.100.104";
    return UBSE_OK;
}

TEST_F(TestUbseNodeControllerUtil, GetClusterIpListFromConf)
{
    std::shared_ptr<UbseConfModule> nullModule = nullptr;
    std::shared_ptr<UbseConfModule> module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));

    std::unordered_map<std::string, std::string> ipMap = GetClusterIpListFromConf();
    EXPECT_EQ(ipMap.size(), 0);
    MOCKER(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_ERROR)).then(invoke(MockGetConfString));
    ipMap = GetClusterIpListFromConf();
    EXPECT_EQ(ipMap.size(), 0);

    ipMap = GetClusterIpListFromConf();
    EXPECT_EQ(ipMap.size(), 4);
    EXPECT_EQ(ipMap["192.168.100.100"], "1");
    EXPECT_EQ(ipMap["192.168.100.101"], "2");
    EXPECT_EQ(ipMap["192.168.100.102"], "3");
    EXPECT_EQ(ipMap["192.168.100.104"], "4");
}

UbseResult MockUbseInvalidGetLocalNodeInfo(UbseLcneModule *, MtiNodeInfo &ubseNodeInfo)
{
    ubseNodeInfo.nodeId = "node";
    ubseNodeInfo.eid = "1";
    return UBSE_OK;
}

UbseResult MockUbseGetLocalNodeInfo(UbseLcneModule *, MtiNodeInfo &ubseNodeInfo)
{
    ubseNodeInfo.nodeId = "1";
    ubseNodeInfo.eid = "1";
    return UBSE_OK;
}

UbseResult MockCollectIpList(UbseNodeInfo &ubseNodeInfo)
{
    UbseIpAddr ipv4{};
    ipv4.type = UbseIpType::UBSE_IP_V4;
    ipv4.ipv4.addr[0] = 127;
    ipv4.ipv4.addr[1] = 0;
    ipv4.ipv4.addr[2] = 0;
    ipv4.ipv4.addr[3] = 1;

    UbseIpAddr ipv42{};
    ipv42.type = UbseIpType::UBSE_IP_V4;
    ipv42.ipv4.addr[0] = 192;
    ipv42.ipv4.addr[1] = 168;
    ipv42.ipv4.addr[2] = 100;
    ipv42.ipv4.addr[3] = 104;
    ubseNodeInfo.ipList = {ipv4, ipv42};
    return UBSE_OK;
}

std::unordered_map<std::string, std::string> MockGetClusterIpListFromConf()
{
    std::unordered_map<std::string, std::string> map = {{"192.168.100.104", "1"}};
    return map;
}

TEST_F(TestUbseNodeControllerUtil, GetCurNodeInfo)
{
    std::shared_ptr<UbseLcneModule> nullModule = nullptr;
    std::shared_ptr<UbseLcneModule> module = std::make_shared<UbseLcneModule>();
    MOCKER(&UbseContext::GetModule<UbseLcneModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));

    UbseNodeInfo info{};
    EXPECT_NO_THROW(GetCurNodeInfo(info));

    MOCKER(&UbseLcneModule::UbseGetLocalNodeInfo)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(invoke(MockUbseInvalidGetLocalNodeInfo))
        .then(invoke(MockUbseGetLocalNodeInfo));
    // err
    EXPECT_NO_THROW(GetCurNodeInfo(info));
    // invalid nodeId
    EXPECT_NO_THROW(GetCurNodeInfo(info));

    MOCKER(IsUBEnable).stubs().will(returnValue(false));
    MOCKER(CollectIpList).stubs().will(returnValue(UBSE_ERROR)).then(invoke(MockCollectIpList));
    MOCKER(GetClusterIpListFromConf).stubs().will(invoke(MockGetClusterIpListFromConf));
    EXPECT_NO_THROW(GetCurNodeInfo(info));
    EXPECT_NO_THROW(GetCurNodeInfo(info));
    EXPECT_EQ(info.nodeId, "1");
    EXPECT_EQ(info.comIp, "192.168.100.104");
}

std::unordered_map<std::string, std::string> MockGetEmptyClusterIpListFromConf()
{
    std::unordered_map<std::string, std::string> map{};
    map.clear();
    return map;
}

TEST_F(TestUbseNodeControllerUtil, GetStaticNodeInfoFromConf)
{
    MOCKER(IsUBEnable).stubs().will(returnValue(false));
    std::unordered_map<std::string, std::string> map{};
    map.clear();
    MOCKER(GetClusterIpListFromConf).stubs().will(returnValue(map)).then(invoke(MockGetClusterIpListFromConf));

    std::vector<UbseNodeInfo> infos = GetStaticNodeInfoFromConf();
    EXPECT_EQ(infos.size(), 0);

    infos = GetStaticNodeInfoFromConf();
    EXPECT_EQ(infos.size(), 1);
    EXPECT_EQ(infos[0].nodeId, "1");
    EXPECT_EQ(infos[0].comIp, "192.168.100.104");
}
} // namespace ubse::node_controller::ut