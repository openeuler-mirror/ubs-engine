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

#include "test_ubse_node_discovery_static_mode.h"

#include "src/adapter_plugins/mti/ubse_lcne_module.h"
#include "src/framework/node_mgr/static_mode/ubse_node_discovery_static_mode.cpp"
#include "ubse_node_mgr.h"

namespace ubse::nodeMgr {
using namespace ubse::mti;

void TestUbseNodeDiscoveryStaticMode::SetUp()
{
    Test::SetUp();
}

void TestUbseNodeDiscoveryStaticMode::TearDown()
{
    nodeMgr::UbseNodeStaticInfoMgr::GetInstance().nodes_.clear();
    Test::TearDown();
    GlobalMockObject::verify();
}

UbseResult MockUbseGetDevTopology(UbseLcneModule *this_, UbseDevTopology &devTopology)
{
    UbseDevName downDev("1", "1");
    UbseDeviceInfo emptyDeviceInfo{};
    UbseDevPortName downDevPortName("1", "1", "1", "1");
    UbseMtiCpuTopoPortInfo downCpuTopoPortInfo{};
    downCpuTopoPortInfo.portStatus = UbseMtiCpuTopoPortStatus::DOWN;
    devTopology[downDev] = {emptyDeviceInfo, {{downDevPortName, downCpuTopoPortInfo}}};

    UbseDevName invaildDev("str", "1");
    UbseDevPortName invaildName("2", "2", "2", "2");
    UbseMtiCpuTopoPortInfo invaildCpuTopoPortInfo{};
    invaildCpuTopoPortInfo.portStatus = UbseMtiCpuTopoPortStatus::UP;
    devTopology[invaildDev] = {emptyDeviceInfo, {{invaildName, invaildCpuTopoPortInfo}}};

    UbseDevName invaildRemoteDev("3", "3");
    UbseDevPortName invaildRemoteName("3", "3", "3", "3");
    UbseMtiCpuTopoPortInfo invaildRemoteCpuTopoPortInfo{};
    invaildRemoteCpuTopoPortInfo.portStatus = UbseMtiCpuTopoPortStatus::UP;
    invaildRemoteCpuTopoPortInfo.remoteSlotId = "str";
    devTopology[invaildRemoteDev] = {emptyDeviceInfo, {{invaildRemoteName, invaildRemoteCpuTopoPortInfo}}};

    UbseDevName dev("4", "4");
    UbseDevPortName devPortName("4", "4", "4", "4");
    UbseMtiCpuTopoPortInfo mtiCpuTopoPortInfo{};
    mtiCpuTopoPortInfo.portStatus = UbseMtiCpuTopoPortStatus::UP;
    mtiCpuTopoPortInfo.portId = "4";
    mtiCpuTopoPortInfo.remoteSlotId = "5";
    mtiCpuTopoPortInfo.remoteChipId = "5";
    mtiCpuTopoPortInfo.remotePortId = "5";
    devTopology[dev] = {emptyDeviceInfo, {{devPortName, mtiCpuTopoPortInfo}}};
    return UBSE_OK;
}

TEST_F(TestUbseNodeDiscoveryStaticMode, GetClusterPhysicalLinkInfo)
{
    MOCKER(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    std::shared_ptr<UbseLcneModule> nullModule = nullptr;
    std::shared_ptr<UbseLcneModule> module = std::make_shared<UbseLcneModule>();
    MOCKER(&UbseContext::GetModule<UbseLcneModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    std::vector<PhysicalLink> allLinkInfo{};
    EXPECT_EQ(UBSE_ERROR_MODULE_LOAD_FAILED, GetClusterPhysicalLinkInfo(allLinkInfo));

    MOCKER(&UbseLcneModule::UbseGetDevTopology)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(invoke(MockUbseGetDevTopology));

    EXPECT_EQ(UBSE_ERROR, GetClusterPhysicalLinkInfo(allLinkInfo));

    EXPECT_EQ(UBSE_OK, GetClusterPhysicalLinkInfo(allLinkInfo));
    EXPECT_EQ(1, allLinkInfo.size());
    EXPECT_EQ(4, allLinkInfo[0].slotId);
    EXPECT_EQ(4, allLinkInfo[0].chipId);
    EXPECT_EQ(4, allLinkInfo[0].portId);
    EXPECT_EQ(5, allLinkInfo[0].peerSlotId);
    EXPECT_EQ(5, allLinkInfo[0].peerChipId);
    EXPECT_EQ(5, allLinkInfo[0].peerPortId);
}

UbseResult MockUbseGetAllNodeInfos(UbseLcneModule *this_, std::vector<MtiNodeInfo> &ubseNodeInfos)
{
    ubseNodeInfos.push_back({"1", "4245:4944:0000:0000:0000:0000:0100:0001"});
    ubseNodeInfos.push_back({"2", "4245:4944:0000:0000:0000:0000:0100:0002"});
    ubseNodeInfos.push_back({"3", "4245:4944:0000:0000:0000:0000:0100:0003"});
    return UBSE_OK;
}

TEST_F(TestUbseNodeDiscoveryStaticMode, GenerateClusterStaticInfo)
{
    UbseNodeStaticInfo curInfo{};
    curInfo.nodeId = "1";
    curInfo.groupId = 0;
    UbseNodeStaticInfoMgr::GetInstance().SetCurrentNode(curInfo);

    std::shared_ptr<UbseLcneModule> nullModule = nullptr;
    std::shared_ptr<UbseLcneModule> module = std::make_shared<UbseLcneModule>();
    MOCKER(&UbseContext::GetModule<UbseLcneModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(UBSE_ERROR_MODULE_LOAD_FAILED, UbseNodeDiscoveryStaticMode::GetInstance().GenerateClusterStaticInfo());

    MOCKER(&UbseLcneModule::UbseGetAllNodeInfos)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(invoke(MockUbseGetAllNodeInfos));
    EXPECT_EQ(UBSE_ERROR, UbseNodeDiscoveryStaticMode::GetInstance().GenerateClusterStaticInfo());

    UbseDevName curDev("1", "1");
    UbseUrmaEidInfo curEidInfo{};
    curEidInfo.entityId = "1";
    curEidInfo.primaryEid = "4245:4944:0000:0000:0000:0000:0100:0001";
    curEidInfo.portEidList["10"] = "4345:4944:0000:0000:0000:0000:0100:0001";
    module->allSocketComEid[curDev] = curEidInfo;

    UbseDevName node2Dev("2", "2");
    UbseUrmaEidInfo node2EidInfo{};
    node2EidInfo.entityId = "2";
    node2EidInfo.primaryEid = "4245:4944:0000:0000:0000:0000:0100:0002";
    node2EidInfo.portEidList["10"] = "4345:4944:0000:0000:0000:0000:0100:0002";
    module->allSocketComEid[node2Dev] = node2EidInfo;

    UbseDevName node4Dev("4", "4");
    UbseUrmaEidInfo node4EidInfo{};
    node4EidInfo.entityId = "4";
    node4EidInfo.primaryEid = "4245:4944:0000:0000:0000:0000:0100:0004";
    node4EidInfo.portEidList["10"] = "4345:4944:0000:0000:0000:0000:0100:0004";
    module->allSocketComEid[node4Dev] = node2EidInfo;

    EXPECT_EQ(UBSE_OK, UbseNodeDiscoveryStaticMode::GetInstance().GenerateClusterStaticInfo());
}

} // namespace ubse::nodeMgr