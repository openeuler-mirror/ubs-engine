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

#include "test_ubse_lcne_module.h"

#include <dlfcn.h>
#include <mockcpp/mokc.h>

#include "lcne/ubse_lcne_busInstance.h"
#include "lcne/ubse_lcne_host_info.h"
#include "lcne/ubse_lcne_node_info.h"
#include "lcne/ubse_lcne_urma_eid.h"
#include "ubse_conf_module.h"
#include "ubse_conf.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_lcne_module.h"
#include "ubse_str_util.h"
#include "adapter_plugins/mti/ubse_mti_def.h"
#include "adapter_plugins/mti/ubse_mti_interface.h"
namespace ubse::mti {
using namespace ubse::context;
using namespace ubse::lcne;
using namespace ubse::config;
using namespace ubse::utils;
using namespace ubse::adapter_plugins::mti;

using UvsSetTopoInfo = uint32_t (*)(void* topo, uint32_t topNum);

void TestUbseLcneModule::SetUp()
{
    Test::SetUp();
}

void TestUbseLcneModule::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneModule, TestUbseGetLocalNodeInfo)
{
    UbseLcneModule module;
    UbseMtiNodeInfo ubseNodeInfo;

    UbseResult ret = module.UbseGetLocalNodeInfo(ubseNodeInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneModule, UbseGetAllNodeInfos)
{
    UbseLcneModule module;
    std::vector<UbseMtiNodeInfo> ubseNodeInfos;

    UbseResult ret = module.UbseGetAllNodeInfos(ubseNodeInfos);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneModule, UbseGetDevTopology)
{
    UbseLcneModule module;
    UbseDevTopology devTopology;

    UbseResult ret = module.UbseGetDevTopology(devTopology);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneModule, GetBondingEidByNodeId)
{
    UbseLcneModule module;
    std::string bondingEid;
    std::string nodeId = "1";
    UbseResult ret = module.GetBondingEidByNodeId(bondingEid, nodeId);
    EXPECT_EQ(ret, UBSE_ERROR);

    UbseMtiNodeInfo ubseNodeInfo_{"1", "1234:5678:8765:4321:1234:5678:8765:4321"};
    module.ubseNodeInfos_.push_back(ubseNodeInfo_);
    ret = module.GetBondingEidByNodeId(bondingEid, nodeId);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(bondingEid, "1234:5678:8765:4321:1234:5678:8765:4321");
}

TEST_F(TestUbseLcneModule, GenerateBondingEid)
{
    UbseLcneModule module;
    unsigned char bondingEid[40];
    std::string nodeId = "1";
    UbseResult ret = module.GenerateBondingEid(nodeId, bondingEid);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneModule, GenerateBondingEid_Failed)
{
    UbseLcneModule module;
    unsigned char bondingEid[40];
    std::string nodeId = "abc";
    UbseResult ret = module.GenerateBondingEid(nodeId, bondingEid);
    EXPECT_EQ(ret, UBSE_ERROR_PARSE_ARGS_FAILED);
}

TEST_F(TestUbseLcneModule, IsPrimaryEidExist)
{
    UbseLcneModule module;
    std::string nodeId = "1";

    EXPECT_FALSE(module.IsPrimaryEidExist(nodeId));

    UbseMtiNodeInfo ubseNodeInfo_{"1", "1234:5678:8765:4321:1234:5678:8765:4321"};
    module.ubseNodeInfos_.push_back(ubseNodeInfo_);
    EXPECT_TRUE(module.IsPrimaryEidExist(nodeId));
}

TEST_F(TestUbseLcneModule, GetMtiComEid)
{
    UbseLcneModule module;
    UbseDevName devName("1", "1");
    adapter_plugins::mti::UbseMtiEidGroup info;
    info.primaryEid = "1234:5678:8765:4321:1234:5678:8765:4321";
    std::string urmaEid = "1234:5678:8765:4321:1234:5678:8765:4325";
    info.portEids.emplace("2", urmaEid);
    module.allSocketComEid.emplace(devName, info);

    auto ret = module.GetMtiComEid();
    EXPECT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[devName].primaryEid, "1234:5678:8765:4321:1234:5678:8765:4321");
    EXPECT_EQ(ret[devName].portEids["2"], "1234:5678:8765:4321:1234:5678:8765:4325");
}

TEST_F(TestUbseLcneModule, GetLocalBoardIOInfo)
{
    UbseLcneModule module;
    UbseDevName devName("1", "1");
    UbseLcneIODieInfo info;
    info.guid = "01-0101-0-1-0101-0101-010101-0101010101";
    info.primaryCna = "0x0085a7";
    info.chipType = DevType::CPU;
    module.localBoardIOInfo.emplace(devName, info);

    auto ret = module.GetLocalBoardIOInfo();
    EXPECT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[devName].guid, "01-0101-0-1-0101-0101-010101-0101010101");
    EXPECT_EQ(ret[devName].primaryCna, "0x0085a7");
    EXPECT_EQ(ret[devName].chipType, DevType::CPU);
}

TEST_F(TestUbseLcneModule, GetLcneData)
{
    UbseLcneModule module;
    MOCKER_CPP(&UbseLcneTopology::RegHttpHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneTopology::Start).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneUrmaEid::GetUrmaEid).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneBusInstance::QueryBusinstance).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneNodeInfo::QueryAllLcneIODieInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneHostInfo::QueryLcneHostInfo).stubs().will(returnValue(UBSE_OK)).then(returnValue(UBSE_ERROR));

    UbseResult ret = module.GetLcneData();
    EXPECT_EQ(ret, UBSE_OK);
    ret = module.GetLcneData();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneModule, GetUbseLcneOSInfo)
{
    UbseLcneModule module;
    UbseLcneOSInfo info{"1", "2", LogicEntityType::host, "3", LogicEntityStatus::online};
    module.localBoardHostInfo = info;

    auto ret = module.GetUbseLcneOSInfo();
    EXPECT_EQ(ret.busInstanceEid, "1");
    EXPECT_EQ(ret.guid, "2");
    EXPECT_EQ(ret.logicEntityStatus, LogicEntityStatus::online);
}

TEST_F(TestUbseLcneModule, GetLcneConf)
{
    UbseLcneModule module;
    std::shared_ptr<UbseConfModule> Conf = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(Conf));
    MOCKER_CPP(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(module.GetLcneConf(), UBSE_OK);
}

TEST_F(TestUbseLcneModule, GetLcneConf_ConfigFailed)
{
    UbseLcneModule module;
    EXPECT_EQ(module.GetLcneConf(), UBSE_ERROR_MODULE_LOAD_FAILED);

    std::shared_ptr<UbseConfModule> Conf = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(Conf));
    MOCKER_CPP(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(module.GetLcneConf(), UBSE_OK);
}

TEST_F(TestUbseLcneModule, ConvertPortConfStrToInt_Failed)
{
    UbseLcneModule module;
    std::string s = "abc";
    int port;
    EXPECT_EQ(module.ConvertPortConfStrToInt(s, port), UBSE_ERROR);
}

TEST_F(TestUbseLcneModule, ConvertPortConfStrToInt_OutRange)
{
    UbseLcneModule module;
    std::string s = "123";
    int port;
    EXPECT_EQ(module.ConvertPortConfStrToInt(s, port), UBSE_ERROR);
}

TEST_F(TestUbseLcneModule, GetLcneConf_ConvertFailed)
{
    UbseLcneModule module;
    std::shared_ptr<UbseConfModule> Conf = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(Conf));
    MOCKER_CPP(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneModule::ConvertPortConfStrToInt).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(module.GetLcneConf(), UBSE_OK);
}

TEST_F(TestUbseLcneModule, UnInitialize)
{
    UbseLcneModule module;
    module.UnInitialize();
}

TEST_F(TestUbseLcneModule, Start)
{
    UbseLcneModule module;
    EXPECT_EQ(module.Start(), UBSE_OK);
}

TEST_F(TestUbseLcneModule, Stop)
{
    UbseLcneModule module;
    module.Stop();
}

TEST_F(TestUbseLcneModule, Initialize_Success)
{
    UbseLcneModule module;
    MOCKER_CPP(&UbseLcneModule::GetLcneConf).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneModule::GetLcneData).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneModule::FillNodeComInfo).stubs().will(returnValue(UBSE_OK));
    UbseResult ret = module.Initialize();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneModule, Initialize_FillNodeComInfoFailed)
{
    UbseLcneModule module;
    MOCKER_CPP(&UbseLcneModule::GetLcneConf).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneModule::GetLcneData).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneModule::FillNodeComInfo).stubs().will(returnValue(UBSE_ERROR));
    UbseResult ret = module.Initialize();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneModule, Initialize_GetLcneConfFailed)
{
    UbseLcneModule module;
    MOCKER_CPP(&UbseLcneModule::GetLcneConf).stubs().will(returnValue(UBSE_ERROR));
    UbseResult ret = module.Initialize();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneModule, FillNodeComInfo_Success)
{
    UbseLcneModule module;
    std::string localNodeId = "1";
    module.ubseLcneBusInstanceInfo.localNodeId = localNodeId;
    module.allSocketComEid.emplace(UbseDevName("1", "2"), adapter_plugins::mti::UbseMtiEidGroup{});

    MOCKER_CPP(&UbseLcneModule::IsPrimaryEidExist).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseLcneModule::GenerateBondingEid).stubs().will(returnValue(UBSE_OK));

    std::string expectedBondingEidString = "4245:4944::";
    MOCKER_CPP(&UbseLcneModule::BytesToIPv6String).stubs().will(returnValue(expectedBondingEidString));

    UbseResult ret = module.FillNodeComInfo();
    EXPECT_EQ(ret, UBSE_OK);

    EXPECT_EQ(module.ubseNodeInfos_.size(), 1);
    EXPECT_EQ(module.ubseNodeInfos_.front().nodeId, localNodeId);
    EXPECT_EQ(module.ubseNodeInfos_.front().eid, expectedBondingEidString);
}
}  // namespace ubse::mti