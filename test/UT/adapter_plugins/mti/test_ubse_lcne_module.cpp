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
    MtiNodeInfo ubseNodeInfo;

    UbseResult ret = module.UbseGetLocalNodeInfo(ubseNodeInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneModule, UbseGetAllNodeInfos)
{
    UbseLcneModule module;
    std::vector<MtiNodeInfo> ubseNodeInfos;

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

    MtiNodeInfo ubseNodeInfo_{"1", "1234:5678:8765:4321:1234:5678:8765:4321"};
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

    MtiNodeInfo ubseNodeInfo_{"1", "1234:5678:8765:4321:1234:5678:8765:4321"};
    module.ubseNodeInfos_.push_back(ubseNodeInfo_);
    EXPECT_TRUE(module.IsPrimaryEidExist(nodeId));
}

TEST_F(TestUbseLcneModule, GetAllSocketComEid)
{
    UbseLcneModule module;
    UbseDevName devName("1", "1");
    adapter_plugins::mti::UbseUrmaEidInfo info;
    info.primaryEid = "1234:5678:8765:4321:1234:5678:8765:4321";
    std::string urmaEid = "1234:5678:8765:4321:1234:5678:8765:4325";
    info.portEidList.emplace("2", urmaEid);
    module.allSocketComEid.emplace(devName, info);

    auto ret = module.GetAllSocketComEid();
    EXPECT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[devName].primaryEid, "1234:5678:8765:4321:1234:5678:8765:4321");
    EXPECT_EQ(ret[devName].portEidList["2"], "1234:5678:8765:4321:1234:5678:8765:4325");
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

TEST_F(TestUbseLcneModule, GetTopologyInfo)
{
    UbseLcneModule module;
    std::map<std::string, std::vector<IODieInfo>> allNodeIOdieInfo;

    UbseDevName devName("1", "1");
    adapter_plugins::mti::UbseUrmaEidInfo info;
    info.primaryEid = "1234:5678:8765:4321:1234:5678:8765:4321";
    std::string urmaEid = "1234:5678:8765:4321:1234:5678:8765:4325";
    info.portEidList.emplace("2", urmaEid);
    module.allSocketComEid.emplace(devName, info);

    UbseResult ret = module.GetTopologyInfo(allNodeIOdieInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneModule, GetTopologyInfo_ParseFailed)
{
    UbseLcneModule module;
    std::map<std::string, std::vector<IODieInfo>> allNodeIOdieInfo;

    UbseDevName devName("1", "1");
    adapter_plugins::mti::UbseUrmaEidInfo info;
    info.primaryEid = "1234:5678:8765:4321:1234:5678:8765:4321";
    std::string urmaEid = "1234:5678:8765:4321:1234:5678:8765:4325";
    info.portEidList.emplace("2", urmaEid);
    module.allSocketComEid.emplace(devName, info);

    MOCKER_CPP(&ubse::utils::ConvertStrToInt).stubs().will(returnValue(UBSE_ERROR_PARSE_ARGS_FAILED));
    UbseResult ret = module.GetTopologyInfo(allNodeIOdieInfo);
    EXPECT_EQ(ret, UBSE_ERROR_PARSE_ARGS_FAILED);
}

TEST_F(TestUbseLcneModule, GetTopologyInfo_GetIoDiePortEidFailed)
{
    UbseLcneModule module;
    std::map<std::string, std::vector<IODieInfo>> allNodeIOdieInfo;

    UbseDevName devName("1", "1");
    adapter_plugins::mti::UbseUrmaEidInfo info;
    info.primaryEid = "1234:5678:8765:4321:1234:5678:8765:4321";
    std::string urmaEid = "1234:5678:8765:4321:1234:5678:8765:4325";
    info.portEidList.emplace("2", urmaEid);
    module.allSocketComEid.emplace(devName, info);

    MOCKER_CPP(&UbseLcneModule::GetIoDiePortEid).stubs().will(returnValue(UBSE_ERROR_INVAL));
    UbseResult ret = module.GetTopologyInfo(allNodeIOdieInfo);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseLcneModule, GetTopologyInfo_PortEidListOutRange)
{
    UbseLcneModule module;
    std::map<std::string, std::vector<IODieInfo>> allNodeIOdieInfo;

    UbseDevName devName("1", "1");
    adapter_plugins::mti::UbseUrmaEidInfo info;
    info.primaryEid = "";
    std::string urmaEid = "1234:5678:8765:4321:1234:5678:8765:4325";
    info.portEidList.emplace("2", urmaEid);
    module.allSocketComEid.emplace(devName, info);
    UbseDevName devName1("1", "2");
    adapter_plugins::mti::UbseUrmaEidInfo info1;
    info1.primaryEid = "1234:5678:8765:4321:1234:5678:8765:4321";
    for (int i = 1; i <= 12; ++i) {
        urmaEid = "1234:5678:8765:4321:1234:5678:8765:" + std::to_string(4324 + i);
        info1.portEidList.emplace(std::to_string(i), urmaEid);
    }
    module.allSocketComEid.emplace(devName1, info1);

    UbseResult ret = module.GetTopologyInfo(allNodeIOdieInfo);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseLcneModule, GetIoDiePortEid_Success)
{
    UbseLcneModule module;
    std::map<std::string, std::string> portEidList;
    IODieInfo ioDieInfo;

    UbseDevName devName("1", "1");
    std::string portInfo;
    portEidList.emplace("236", portInfo);

    UbseDeviceInfo deviceInfo;
    UbseMtiCpuTopoPortInfo portInfo1;
    portInfo1.remoteSlotId = "1";
    portInfo1.remoteChipId = "2";
    portInfo1.remotePortId = "236";
    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> portMap;
    portMap[UbseDevPortName("236")] = portInfo1;
    UbseDevTopology::mapped_type entry;
    entry.first = deviceInfo;
    entry.second = portMap;
    UbseLcneTopology topology;
    topology.ubseTopologyInfo[devName] = entry;
    MOCKER_CPP(&UbseLcneTopology::UbseGetDevTopology)
        .stubs()
        .with(outBound(topology.ubseTopologyInfo))
        .will(returnValue(UBSE_OK));

    adapter_plugins::mti::UbseUrmaEidInfo socketInfo;
    std::string remotePortInfo;
    socketInfo.portEidList.emplace("236", remotePortInfo);
    module.allSocketComEid.emplace(UbseDevName("1", "2"), socketInfo);
    MOCKER_CPP(&UbseLcneModule::ParseColonHexString).stubs().will(returnValue(UBSE_OK));

    UbseResult ret = module.GetIoDiePortEid(devName, ioDieInfo, portEidList);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneModule, GetIoDiePortEid_UrmaEidParseFailed)
{
    UbseLcneModule module;
    std::map<std::string, std::string> portEidList;
    IODieInfo ioDieInfo;

    UbseDevName devName("1", "1");
    std::string urmaEid = "abcd";
    portEidList.emplace("236", urmaEid);

    UbseResult ret = module.GetIoDiePortEid(devName, ioDieInfo, portEidList);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseLcneModule, GetIoDiePortEid_PortNameNotFound)
{
    UbseLcneModule module;
    std::map<std::string, std::string> portEidList;
    IODieInfo ioDieInfo;

    UbseDevName devName("1", "1");
    std::string urmaEid = "1234:5678:8765:4321:1234:5678:8765:4325";
    portEidList.emplace("236", urmaEid);

    UbseDeviceInfo deviceInfo;
    UbseMtiCpuTopoPortInfo portInfo1;
    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> portMap;
    portMap[UbseDevPortName("0")] = portInfo1;
    UbseDevTopology::mapped_type entry;
    entry.first = deviceInfo;
    entry.second = portMap;
    UbseLcneTopology topology;
    topology.ubseTopologyInfo[devName] = entry;
    MOCKER_CPP(&UbseLcneTopology::UbseGetDevTopology)
        .stubs()
        .with(outBound(topology.ubseTopologyInfo))
        .will(returnValue(UBSE_OK));

    UbseResult ret = module.GetIoDiePortEid(devName, ioDieInfo, portEidList);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseLcneModule, GetIoDiePortEid_DevNameNotFound)
{
    UbseLcneModule module;
    std::map<std::string, std::string> portEidList;
    IODieInfo ioDieInfo;

    UbseDevName devName("1", "1");
    std::string urmaEid = "1234:5678:8765:4321:1234:5678:8765:4325";
    portEidList.emplace("235", urmaEid);

    UbseDeviceInfo deviceInfo;
    UbseMtiCpuTopoPortInfo portInfo1;
    portInfo1.remoteSlotId = "1";
    portInfo1.remoteChipId = "2";
    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> portMap;
    portMap[UbseDevPortName("236")] = portInfo1;
    UbseDevTopology::mapped_type entry;
    entry.first = deviceInfo;
    entry.second = portMap;
    UbseLcneTopology topology;
    topology.ubseTopologyInfo[devName] = entry;
    MOCKER_CPP(&UbseLcneTopology::UbseGetDevTopology)
        .stubs()
        .with(outBound(topology.ubseTopologyInfo))
        .will(returnValue(UBSE_OK));

    adapter_plugins::mti::UbseUrmaEidInfo socketInfo;
    std::string remotePortInfo;
    socketInfo.portEidList.emplace("236", remotePortInfo);
    module.allSocketComEid.emplace(UbseDevName("1", "3"), socketInfo);

    UbseResult ret = module.GetIoDiePortEid(devName, ioDieInfo, portEidList);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseLcneModule, GetIoDiePortEid_PortIdNotFound)
{
    UbseLcneModule module;
    std::map<std::string, std::string> portEidList;
    IODieInfo ioDieInfo;

    UbseDevName devName("1", "1");
    std::string urmaEid = "1234:5678:8765:4321:1234:5678:8765:4325";
    portEidList.emplace("236", urmaEid);

    UbseDeviceInfo deviceInfo;
    UbseMtiCpuTopoPortInfo portInfo1;
    portInfo1.remoteSlotId = "1";
    portInfo1.remoteChipId = "2";
    portInfo1.remotePortId = "1";
    portInfo1.portStatus = UbseMtiCpuTopoPortStatus::UP;
    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> portMap;
    portMap[UbseDevPortName("236")] = portInfo1;
    UbseDevTopology::mapped_type entry;
    entry.first = deviceInfo;
    entry.second = portMap;
    UbseLcneTopology topology;
    topology.ubseTopologyInfo[devName] = entry;
    MOCKER_CPP(&UbseLcneTopology::UbseGetDevTopology)
        .stubs()
        .with(outBound(topology.ubseTopologyInfo))
        .will(returnValue(UBSE_OK));

    adapter_plugins::mti::UbseUrmaEidInfo socketInfo{};
    std::string remotePortInfo{};
    socketInfo.portEidList.emplace("236", remotePortInfo);
    module.allSocketComEid.clear();
    module.allSocketComEid.emplace(UbseDevName("1", "2"), socketInfo);

    UbseResult ret = module.GetIoDiePortEid(devName, ioDieInfo, portEidList);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseLcneModule, GetIoDiePortEid_PeerPortEidParseFailed)
{
    UbseLcneModule module;
    std::map<std::string, std::string> portEidList;
    IODieInfo ioDieInfo;

    UbseDevName devName("1", "1");
    std::string urmaEid = "1234:5678:8765:4321:1234:5678:8765:4325";
    portEidList.emplace("236", urmaEid);

    UbseDeviceInfo deviceInfo;
    UbseMtiCpuTopoPortInfo portInfo1;
    portInfo1.remoteSlotId = "1";
    portInfo1.remoteChipId = "2";
    portInfo1.remotePortId = "236";
    portInfo1.portStatus = UbseMtiCpuTopoPortStatus::UP;
    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> portMap;
    portMap[UbseDevPortName("236")] = portInfo1;
    UbseDevTopology::mapped_type entry;
    entry.first = deviceInfo;
    entry.second = portMap;
    UbseLcneTopology topology;
    topology.ubseTopologyInfo[devName] = entry;
    MOCKER_CPP(&UbseLcneTopology::UbseGetDevTopology)
        .stubs()
        .with(outBound(topology.ubseTopologyInfo))
        .will(returnValue(UBSE_OK));

    adapter_plugins::mti::UbseUrmaEidInfo socketInfo{};
    std::string remoteUrmaEid = "ABCD";
    socketInfo.portEidList.emplace("236", remoteUrmaEid);
    module.allSocketComEid.clear();
    module.allSocketComEid.emplace(UbseDevName("1", "2"), socketInfo);

    UbseResult ret = module.GetIoDiePortEid(devName, ioDieInfo, portEidList);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
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
    MOCKER_CPP(&UbseLcneModule::SetUvsComInfo).stubs().will(returnValue(UBSE_OK));
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

TEST_F(TestUbseLcneModule, SetUvsComInfo_GetTopologyInfoFailed)
{
    UbseLcneModule module;
    MOCKER_CPP(&UbseLcneModule::GetTopologyInfo).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(module.SetUvsComInfo(), UBSE_ERROR);
}

TEST_F(TestUbseLcneModule, SetUvsComInfo_FillTopoArrayFailed)
{
    UbseLcneModule module;
    std::map<std::string, std::vector<IODieInfo>> map;
    UbseDevName devName1("1", "1");
    std::vector<IODieInfo> infos;
    IODieInfo info{};
    infos.emplace_back(info);
    map.emplace("1-1", infos);

    MOCKER_CPP(&UbseLcneModule::GetTopologyInfo).stubs().with(outBound(map)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneModule::FillTopoArray).stubs().will(returnValue(UBSE_ERROR));

    UbseResult ret = module.SetUvsComInfo();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneModule, SetUvsComInfo_dlopenFailed)
{
    UbseLcneModule module;
    std::map<std::string, std::vector<IODieInfo>> map;
    UbseDevName devName1("1", "1");
    std::vector<IODieInfo> infos;
    IODieInfo info{};
    infos.emplace_back(info);
    map.emplace("1-1", infos);

    MOCKER_CPP(&UbseLcneModule::GetTopologyInfo).stubs().with(outBound(map)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneModule::FillTopoArray).stubs().will(returnValue(UBSE_OK));

    UbseResult ret = module.SetUvsComInfo();
    EXPECT_EQ(ret, UBSE_ERROR_FILE_NOT_EXIST);
}

TEST_F(TestUbseLcneModule, SetUvsComInfo_dlsymFailed)
{
    UbseLcneModule module;
    std::map<std::string, std::vector<IODieInfo>> map;
    UbseDevName devName1("1", "1");
    std::vector<IODieInfo> infos;
    IODieInfo info{};
    infos.emplace_back(info);
    map.emplace("1-1", infos);

    MOCKER_CPP(&UbseLcneModule::GetTopologyInfo).stubs().with(outBound(map)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneModule::FillTopoArray).stubs().will(returnValue(UBSE_OK));

    void* mockHandle = reinterpret_cast<void*>(0x1234);
    MOCKER_CPP(&dlopen).stubs().will(returnValue(mockHandle));
    MOCKER_CPP(&dlsym).stubs().will(returnValue(static_cast<void*>(nullptr)));
    MOCKER_CPP(&dlclose).stubs().with(mockHandle).will(returnValue(0));

    UbseResult ret = module.SetUvsComInfo();
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseLcneModule, SetUvsComInfo_Success)
{
    UbseLcneModule module;
    std::map<std::string, std::vector<IODieInfo>> map;
    UbseDevName devName1("1", "1");
    std::vector<IODieInfo> infos;
    IODieInfo info{};
    infos.emplace_back(info);
    map.emplace("1-1", infos);

    MOCKER_CPP(&UbseLcneModule::GetTopologyInfo).stubs().with(outBound(map)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneModule::FillTopoArray).stubs().will(returnValue(UBSE_OK));
    void* mockHandle = reinterpret_cast<void*>(0x1234);
    MOCKER_CPP(&dlopen).stubs().will(returnValue(mockHandle));

    using UvsSetTopoInfoFunc = UbseResult (*)(TopoInfo*, uint32_t);
    auto mockUvsSetTopoInfo = [](TopoInfo* topoArray, uint32_t size) -> UbseResult {
        static_cast<void>(topoArray);
        static_cast<void>(size);
        return UBSE_OK;
    };
    UvsSetTopoInfoFunc uvsSetTopoInfoFunc = mockUvsSetTopoInfo;
    MOCKER_CPP(&dlsym).stubs().will(returnValue(reinterpret_cast<void*>(uvsSetTopoInfoFunc)));
    MOCKER_CPP(&dlclose).stubs().with(mockHandle).will(returnValue(0));

    UbseResult ret = module.SetUvsComInfo();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneModule, SetUvsComInfo_uvsSetTopoInfoFailed)
{
    UbseLcneModule module;
    std::map<std::string, std::vector<IODieInfo>> map;
    UbseDevName devName1("1", "1");
    std::vector<IODieInfo> infos;
    IODieInfo info{};
    infos.emplace_back(info);
    map.emplace("1-1", infos);

    MOCKER_CPP(&UbseLcneModule::GetTopologyInfo).stubs().with(outBound(map)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneModule::FillTopoArray).stubs().will(returnValue(UBSE_OK));
    void* mockHandle = reinterpret_cast<void*>(0x1234);
    MOCKER_CPP(&dlopen).stubs().will(returnValue(mockHandle));

    using UvsSetTopoInfoFunc = UbseResult (*)(TopoInfo*, uint32_t);
    auto mockUvsSetTopoInfo = [](TopoInfo* topoArray, uint32_t size) -> UbseResult {
        static_cast<void>(topoArray);
        static_cast<void>(size);
        return UBSE_ERROR;
    };
    UvsSetTopoInfoFunc uvsSetTopoInfoFunc = mockUvsSetTopoInfo;
    MOCKER_CPP(&dlsym).stubs().will(returnValue(reinterpret_cast<void*>(uvsSetTopoInfoFunc)));
    MOCKER_CPP(&dlclose).stubs().with(mockHandle).will(returnValue(0));

    UbseResult ret = module.SetUvsComInfo();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneModule, FillTopoArray_Success)
{
    UbseLcneModule module;
    std::map<std::string, std::vector<IODieInfo>> map;
    UbseDevName devName1("1", "1");
    std::vector<IODieInfo> infos;
    IODieInfo info{};
    infos.emplace_back(info);
    map.emplace(devName1.devName, infos);

    std::string expectedBondingEid = "00:11:22:33:44:55";
    MOCKER_CPP(&UbseLcneModule::GetBondingEidByNodeId)
        .stubs()
        .with(outBound(expectedBondingEid), any())
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseLcneModule::ParseColonHexString).stubs().will(returnValue(UBSE_OK));

    std::unique_ptr<TopoInfo[]> topoArray = std::make_unique<TopoInfo[]>(1);
    UbseResult ret = module.FillTopoArray(map, topoArray);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneModule, FillTopoArray_IoDieInfoParseFailed)
{
    UbseLcneModule module;
    std::map<std::string, std::vector<IODieInfo>> map;
    UbseDevName devName1("1", "1");
    std::vector<IODieInfo> infos;
    IODieInfo info{};
    infos.emplace_back(info);
    map.emplace(devName1.devName, infos);

    std::string expectedBondingEid = "00:11:22:33:44:55";
    MOCKER_CPP(&UbseLcneModule::GetBondingEidByNodeId)
        .stubs()
        .with(outBound(expectedBondingEid), any())
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseLcneModule::ParseColonHexString).stubs().will(returnValue(UBSE_ERROR_INVAL));

    std::unique_ptr<TopoInfo[]> topoArray = std::make_unique<TopoInfo[]>(1);
    UbseResult ret = module.FillTopoArray(map, topoArray);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseLcneModule, FillTopoArray_GetBondingEidByNodeIdFailed)
{
    UbseLcneModule module;
    std::map<std::string, std::vector<IODieInfo>> map;
    UbseDevName devName1("1", "1");
    std::vector<IODieInfo> infos;
    IODieInfo info{};
    infos.emplace_back(info);
    map.emplace(devName1.devName, infos);

    MOCKER_CPP(&UbseLcneModule::GetBondingEidByNodeId).stubs().will(returnValue(UBSE_ERROR));

    std::unique_ptr<TopoInfo[]> topoArray = std::make_unique<TopoInfo[]>(1);
    UbseResult ret = module.FillTopoArray(map, topoArray);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseLcneModule, FillNodeComInfo_Success)
{
    UbseLcneModule module;
    std::string localNodeId = "1";
    module.ubseLcneBusInstanceInfo.localNodeId = localNodeId;
    module.allSocketComEid.emplace(UbseDevName("1", "2"), adapter_plugins::mti::UbseUrmaEidInfo{});

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