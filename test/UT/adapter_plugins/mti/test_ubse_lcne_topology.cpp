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

#include "test_ubse_lcne_topology.h"

#include <mockcpp/mokc.h>

#include "lcne/ubse_lcne_sub_topo_change_info.h"
#include "lcne/ubse_lcne_topology_client.h"
#include "lcne/ubse_topo_cna.h"
#include "ubse_conf.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_event_module.h"
#include "ubse_http_module.h"
#include "ubse_lcne_topology.h"
#include "adapter_plugins/mti/ubse_mti_def.h"

namespace ubse::mti {
using namespace ubse::context;
using namespace ubse::lcne;
using namespace ubse::http;
using namespace ubse::event;
using namespace ubse::adapter_plugins::mti;
void TestUbseLcneTopology::SetUp()
{
    Test::SetUp();
}

void TestUbseLcneTopology::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneTopology, UbseDevGetCna)
{
    UbseLcneTopology topology;
    std::vector<LcneNodeCnaInfo> infos;
    std::vector<LcnePortCnaInfo> ports;
    ports.push_back(LcnePortCnaInfo{"0", "0x0403", static_cast<uint32_t>(std::stoul("0x0403", nullptr, 16))});
    ports.push_back(LcnePortCnaInfo{"1", "0x0404", static_cast<uint32_t>(std::stoul("0x0404", nullptr, 16))});
    LcneNodeCnaInfo info{"1", "1", "1", "0x0401", static_cast<uint32_t>(std::stoul("0x0401", nullptr, 16)), ports};
    infos.push_back(info);
    std::vector<LcnePortCnaInfo> ports1;
    ports1.push_back(LcnePortCnaInfo{"0", "0x040c", static_cast<uint32_t>(std::stoul("0x040c", nullptr, 16))});
    ports1.push_back(LcnePortCnaInfo{"1", "0x040d", static_cast<uint32_t>(std::stoul("0x040d", nullptr, 16))});
    LcneNodeCnaInfo info1{"1", "2", "1", "0x0402", static_cast<uint32_t>(std::stoul("0x0402", nullptr, 16)), ports1};
    infos.push_back(info1);

    UbseDevName devName("1", "1");
    UbseDeviceInfo deviceInfo;
    UbseMtiCpuTopoPortInfo portInfo;
    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> portMap;
    portMap[UbseDevPortName("0")] = portInfo;
    UbseDevTopology::mapped_type entry;
    entry.first = deviceInfo;
    entry.second = portMap;
    topology.ubseTopologyInfo[devName] = entry;

    const auto func = &UbseTopoCna::QueryTopoCna;
    MOCKER_CPP(func).stubs().with(outBound(infos)).will(returnValue(UBSE_OK));

    UbseResult ret = topology.UbseDevGetCna();
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneTopology, UbseDevGetCna_Failed)
{
    const auto func = &UbseTopoCna::QueryTopoCna;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));

    UbseLcneTopology topology;
    UbseResult ret = topology.UbseDevGetCna();
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneTopology, RegHttpHandler)
{
    const auto func = &UbseHttpModule::RegHttpService;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));

    UbseLcneTopology topology;
    UbseResult ret = topology.RegHttpHandler();
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneTopology, Start)
{
    const auto func = &UbseLcneLinkInfo::SubLcneLinkInfo;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));

    const auto func1 = &UbseLcneTopology::CreateDevTopology;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_OK));

    UbseLcneTopology topology;
    UbseResult ret = topology.Start();
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneTopology, Start_Failed)
{
    const auto func = &UbseLcneLinkInfo::SubLcneLinkInfo;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));

    const auto func1 = &UbseLcneTopology::CreateDevTopology;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_ERROR));

    UbseLcneTopology topology;
    UbseResult ret = topology.Start();
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneTopology, CreateDevTopology)
{
    const auto func = &UbseLcneTopology::UbseDevGetTopology;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));

    const auto func1 = &UbseLcneTopology::UbseDevGetCna;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_OK));

    UbseLcneTopology topology;
    UbseResult ret = topology.CreateDevTopology();
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneTopology, CreateDevTopology_GetTopologyFailed)
{
    const auto func = &UbseLcneTopology::UbseDevGetTopology;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));

    UbseLcneTopology topology;
    UbseResult ret = topology.CreateDevTopology();
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneTopology, CreateDevTopology_GetCnaFailed)
{
    const auto func = &UbseLcneTopology::UbseDevGetTopology;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));

    const auto func1 = &UbseLcneTopology::UbseDevGetCna;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_ERROR));

    UbseLcneTopology topology;
    UbseResult ret = topology.CreateDevTopology();
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneTopology, PubUbseTopoChangeEvent)
{
    std::string message{};
    UbseEventModule module;
    std::shared_ptr<UbseEventModule> event = std::make_shared<UbseEventModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseEventModule>).stubs().will(returnValue(event));
    const auto func = &UbseEventModule::UbsePubEvent;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));

    UbseLcneTopology topology;
    UbseResult ret = topology.PubUbseTopoChangeEvent(message);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneTopology, PubUbseTopoChangeEvent_EventModuleNull)
{
    std::string message{};
    UbseLcneTopology topology;
    UbseResult ret = topology.PubUbseTopoChangeEvent(message);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneTopology, PubUbseTopoChangeEvent_PubEventFailed)
{
    std::string message{};
    UbseEventModule module;
    std::shared_ptr<UbseEventModule> event = std::make_shared<UbseEventModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseEventModule>).stubs().will(returnValue(event));
    const auto func = &UbseEventModule::UbsePubEvent;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));

    UbseLcneTopology topology;
    UbseResult ret = topology.PubUbseTopoChangeEvent(message);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneTopology, PortUpDownFunc)
{
    UbseHttpRequest req{};
    UbseHttpResponse resp{};
    const auto func = &UbseLcneTopology::CreateDevTopology;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
    const auto func1 = &UbseLcneTopology::PubUbseTopoChangeEvent;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_ERROR));

    UbseLcneTopology topology;
    UbseResult ret = topology.PortUpDownFunc(req, resp);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneTopology, UbseDevGetTopology_Success)
{
    UbseLcneTopology topology;
    std::vector<LcneNodeInfo> lcneNodes;
    LcneNodeInfo node;
    node.slotId = "1";
    node.chipId = "1";
    node.cardId = "1";
    node.type = "CPU";
    LcnePortInfo port0;
    port0.portId = "1";
    port0.remotePortId = "1";
    node.ports.push_back(port0);
    LcnePortInfo port1;
    port1.portId = "2";
    port1.remotePortId = "2";
    port1.remoteSlotId = "2";
    port1.remoteChipId = "1";
    port1.portStatus = "up";
    node.ports.push_back(port1);
    lcneNodes.push_back(node);

    MOCKER_CPP(&UbseLcneTopologyClient::GetTopology).stubs().with(outBound(lcneNodes)).will(returnValue(UBSE_OK));

    UbseResult ret = topology.UbseDevGetTopology();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneTopology, UbseDevGetTopology_Failed)
{
    UbseLcneTopology topology;
    MOCKER_CPP(&UbseLcneTopologyClient::GetTopology).stubs().will(returnValue(UBSE_ERROR));

    UbseResult ret = topology.UbseDevGetTopology();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneTopology, UbseDevGetTopology_PortsEmpty)
{
    UbseLcneTopology topology;
    std::vector<LcneNodeInfo> lcneNodes;
    LcneNodeInfo node;
    node.slotId = "1";
    node.chipId = "1";
    node.cardId = "1";
    node.type = "CPU";
    lcneNodes.push_back(node);

    MOCKER_CPP(&UbseLcneTopologyClient::GetTopology).stubs().with(outBound(lcneNodes)).will(returnValue(UBSE_OK));

    UbseResult ret = topology.UbseDevGetTopology();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneTopology, EraseEdge_CPU)
{
    UbseLcneTopology topology;
    UbseDevName devName("1", "1");
    UbseDeviceInfo deviceInfo;
    deviceInfo.type = UbseDevType::CPU;
    UbseMtiCpuTopoPortInfo portInfo;
    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> portMap;
    portMap[UbseDevPortName("0")] = portInfo;
    UbseDevTopology::mapped_type entry;
    entry.first = deviceInfo;
    entry.second = portMap;
    topology.ubseTopologyInfo[devName] = entry;

    topology.UbseEraseEdge(devName, UbseDevPortName("0"));
    EXPECT_EQ(topology.ubseTopologyInfo[devName].second.size(), 0);
}

TEST_F(TestUbseLcneTopology, EraseEdge_EdgeNotExist)
{
    UbseLcneTopology topology;
    UbseDevName devName("1", "1");
    UbseDeviceInfo deviceInfo;
    UbseMtiCpuTopoPortInfo portInfo;
    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> portMap;
    portMap[UbseDevPortName("0")] = portInfo;
    UbseDevTopology::mapped_type entry;
    entry.first = deviceInfo;
    entry.second = portMap;
    topology.ubseTopologyInfo[devName] = entry;

    topology.UbseEraseEdge(UbseDevName{"2", "1"}, UbseDevPortName("1"));
    EXPECT_EQ(topology.ubseTopologyInfo[devName].second.size(), 1);
}

TEST_F(TestUbseLcneTopology, EraseEdge_NotCpu)
{
    UbseLcneTopology topology;
    UbseDevName devName("1", "1");
    UbseDeviceInfo deviceInfo;
    deviceInfo.type = UbseDevType::DPU;
    UbseMtiCpuTopoPortInfo portInfo;
    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> portMap;
    portMap[UbseDevPortName("0")] = portInfo;
    UbseDevTopology::mapped_type entry;
    entry.first = deviceInfo;
    entry.second = portMap;
    topology.ubseTopologyInfo[devName] = entry;

    topology.UbseEraseEdge(devName, UbseDevPortName("0"));
    EXPECT_EQ(topology.ubseTopologyInfo[devName].second.size(), 1);
}
}  // namespace ubse::mti