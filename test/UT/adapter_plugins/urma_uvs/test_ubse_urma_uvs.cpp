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

#include "test_ubse_urma_uvs.h"
#include <mockcpp/mokc.h>
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_urma_uvs_module.h"
#include "adapter_plugins/urma/ubse_urma_uvs.h"
#include "securec.h"

namespace ubse::urma::ut {
using namespace ubse::context;
using namespace ubse::urma;

void TestUrmaUvs::SetUp()
{
    Test::SetUp();
}

void TestUrmaUvs::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUrmaUvs, UbsePushTopoAndBondingToUvs_Success)
{
    std::string current_slot_id = "1";
    std::vector<PhysicalLink> allLinkInfo;
    std::vector<UbseUrmaUvsNodeInfo> bondingInfo;
    std::vector<UbcoreTopoNode> nodes;
    auto module = std::make_shared<UbseUrmaUvsModule>();
    module->uvsSetTopoInfo = [](void* topo, uint32_t topo_size, uint32_t topNum) {
        return UBSE_OK;
    };
    nodes.push_back(UbcoreTopoNode{.type = 0, .superNodeId = 0, .id = 1, .is_current = 1});
    nodes.push_back(UbcoreTopoNode{.type = 0, .superNodeId = 0, .id = 2, .is_current = 0});

    MOCKER_CPP(&FillNodeComInfo).stubs().with(any(), any(), any(), outBound(nodes)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(module));

    auto ret = UbsePushTopoAndBondingToUvs(current_slot_id, allLinkInfo, bondingInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUrmaUvs, UbsePushTopoAndBondingToUvs_FillNodeComInfoFailed)
{
    std::string current_slot_id = "1";
    std::vector<PhysicalLink> allLinkInfo;
    std::vector<UbseUrmaUvsNodeInfo> bondingInfo;
    MOCKER_CPP(&FillNodeComInfo).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbsePushTopoAndBondingToUvs(current_slot_id, allLinkInfo, bondingInfo);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUrmaUvs, UbsePushTopoAndBondingToUvs_GetModuleFailed)
{
    std::string current_slot_id = "1";
    std::vector<PhysicalLink> allLinkInfo;
    std::vector<UbseUrmaUvsNodeInfo> bondingInfo;
    MOCKER_CPP(&FillNodeComInfo).stubs().will(returnValue(UBSE_OK));
    auto ret = UbsePushTopoAndBondingToUvs(current_slot_id, allLinkInfo, bondingInfo);
    EXPECT_EQ(ret, UBSE_ERROR_MODULE_LOAD_FAILED);
}

TEST_F(TestUrmaUvs, UbsePushTopoAndBondingToUvs_UvsSetTopoInfoNull)
{
    std::string current_slot_id = "1";
    std::vector<PhysicalLink> allLinkInfo;
    std::vector<UbseUrmaUvsNodeInfo> bondingInfo;
    auto module = std::make_shared<UbseUrmaUvsModule>();
    MOCKER_CPP(&FillNodeComInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(module));

    auto ret = UbsePushTopoAndBondingToUvs(current_slot_id, allLinkInfo, bondingInfo);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUrmaUvs, UbsePushTopoAndBondingToUvs_UvsSetTopoInfoFailed)
{
    std::string current_slot_id = "1";
    std::vector<PhysicalLink> allLinkInfo;
    std::vector<UbseUrmaUvsNodeInfo> bondingInfo;
    auto module = std::make_shared<UbseUrmaUvsModule>();
    module->uvsSetTopoInfo = [](void* topo, uint32_t topo_size, uint32_t topNum) {
        return UBSE_ERROR;
    };
    MOCKER_CPP(&FillNodeComInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(module));

    auto ret = UbsePushTopoAndBondingToUvs(current_slot_id, allLinkInfo, bondingInfo);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUrmaUvs, UbseGetUrmaSubpathByEid_Success)
{
    std::string eid;
    std::string subpath;
    auto module = std::make_shared<UbseUrmaUvsModule>();
    module->uvsGetDeviceNameByUrmaEid = [](char* eid, char* subpath, size_t subpath_size) -> UbseResult {
        strcpy_s(subpath, subpath_size, "/udma/subpath/123456");
        return UBSE_OK;
    };
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(module));

    auto ret = UbseGetUrmaSubpathByEid(eid, subpath);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(subpath, "/udma/subpath/123456");
}

TEST_F(TestUrmaUvs, UbseGetUrmaSubpathByEid_ConvertEidFailed)
{
    std::string eid;
    std::string subpath;
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_ERROR));

    auto ret = UbseGetUrmaSubpathByEid(eid, subpath);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUrmaUvs, UbseGetUrmaSubpathByEid_GetModuleFailed)
{
    std::string eid;
    std::string subpath;
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_OK));

    auto ret = UbseGetUrmaSubpathByEid(eid, subpath);
    EXPECT_EQ(ret, UBSE_ERROR_MODULE_LOAD_FAILED);
}

TEST_F(TestUrmaUvs, UbseGetUrmaSubpathByEid_Nullptr)
{
    std::string eid;
    std::string subpath;
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_OK));
    auto module = std::make_shared<UbseUrmaUvsModule>();
    module->uvsGetDeviceNameByUrmaEid = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(module));

    auto ret = UbseGetUrmaSubpathByEid(eid, subpath);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUrmaUvs, UbseGetUrmaSubpathByEid_UvsGetDeviceNameByUrmaEidFailed)
{
    std::string eid;
    std::string subpath;
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_OK));
    auto module = std::make_shared<UbseUrmaUvsModule>();
    module->uvsGetDeviceNameByUrmaEid = [](char* eid, char* subpath, size_t subpath_size) -> UbseResult {
        return UBSE_ERROR;
    };
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(module));

    auto ret = UbseGetUrmaSubpathByEid(eid, subpath);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUrmaUvs, UbseGetBondingActiveStateByEid_Active)
{
    std::string eid = "0000:0000:0000:0000:0000:0000:0000:0001";
    bool activeState;
    auto module = std::make_shared<UbseUrmaUvsModule>();
    module->uvsGetDeviceNameByUrmaEid = [](char* eid, char* subpath, size_t subpath_size) -> UbseResult {
        return UBSE_OK;
    };
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(module));

    auto ret = UbseGetBondingActiveStateByEid(eid, activeState);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_TRUE(activeState);
}

TEST_F(TestUrmaUvs, UbseGetBondingActiveStateByEid_NotActive)
{
    std::string eid = "0000:0000:0000:0000:0000:0000:0000:0001";
    bool activeState;
    auto module = std::make_shared<UbseUrmaUvsModule>();
    module->uvsGetDeviceNameByUrmaEid = [](char* eid, char* subpath, size_t subpath_size) -> UbseResult {
        return UBSE_ERROR;
    };
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(module));

    auto ret = UbseGetBondingActiveStateByEid(eid, activeState);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_FALSE(activeState);
}

TEST_F(TestUrmaUvs, UbseGetBondingActiveStateByEid_ConvertEidFailed)
{
    std::string eid = "";
    bool activeState;
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_ERROR));

    auto ret = UbseGetBondingActiveStateByEid(eid, activeState);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUrmaUvs, UbseGetBondingActiveStateByEid_Nullptr)
{
    std::string eid = "0000:0000:0000:0000:0000:0000:0000:0001";
    bool activeState;
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_OK));
    auto module = std::make_shared<UbseUrmaUvsModule>();
    module->uvsGetDeviceNameByUrmaEid = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(module));

    auto ret = UbseGetBondingActiveStateByEid(eid, activeState);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUrmaUvs, UbseGetBondingActiveStateByEid_GetModuleFailed)
{
    std::string eid = "0000:0000:0000:0000:0000:0000:0000:0001";
    bool activeState;
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_OK));

    auto ret = UbseGetBondingActiveStateByEid(eid, activeState);
    EXPECT_EQ(ret, UBSE_ERROR_MODULE_LOAD_FAILED);
}

TEST_F(TestUrmaUvs, UbseActiveBonding_Success)
{
    std::string eid = "0000:0000:0000:0000:0000:0000:0000:0001";
    std::string aggrDevName = "aggrDev1";
    bool activeState = false;
    auto module = std::make_shared<UbseUrmaUvsModule>();
    module->uvsCreateAggrDev = [](char* aggrDevEid, const char* aggrDevName) -> UbseResult {
        return UBSE_OK;
    };
    MOCKER_CPP(&UbseGetBondingActiveStateByEid).stubs().with(eid, outBound(activeState)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(module));

    auto ret = UbseActiveBonding(eid, aggrDevName);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUrmaUvs, UbseActiveBonding_AlreadyActive)
{
    std::string eid = "0000:0000:0000:0000:0000:0000:0000:0001";
    std::string aggrDevName = "aggrDev1";
    bool activeState = true;
    MOCKER_CPP(&UbseGetBondingActiveStateByEid).stubs().with(eid, outBound(activeState)).will(returnValue(UBSE_OK));

    auto ret = UbseActiveBonding(eid, aggrDevName);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUrmaUvs, UbseActiveBonding_NameInvalid)
{
    std::string eid = "0000:0000:0000:0000:0000:0000:0000:0001";
    std::string aggrDevName = "";
    auto ret = UbseActiveBonding(eid, aggrDevName);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);

    aggrDevName = std::string(AGGR_DEV_NAME_LEN, 'a');
    ret = UbseActiveBonding(eid, aggrDevName);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUrmaUvs, UbseActiveBonding_CreateAggrDevFailed)
{
    std::string eid = "0000:0000:0000:0000:0000:0000:0000:0001";
    std::string aggrDevName = "aggrDev1";
    bool activeState = false;
    auto module = std::make_shared<UbseUrmaUvsModule>();
    module->uvsCreateAggrDev = [](char* aggrDevEid, const char* aggrDevName) -> UbseResult {
        return UBSE_ERROR;
    };
    MOCKER_CPP(&UbseGetBondingActiveStateByEid).stubs().with(eid, outBound(activeState)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(module));

    auto ret = UbseActiveBonding(eid, aggrDevName);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUrmaUvs, UbseActiveBonding_GetModuleFailed)
{
    std::string eid = "0000:0000:0000:0000:0000:0000:0000:0001";
    std::string aggrDevName = "aggrDev1";
    bool activeState = false;
    MOCKER_CPP(&UbseGetBondingActiveStateByEid).stubs().with(eid, outBound(activeState)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_OK));

    auto ret = UbseActiveBonding(eid, aggrDevName);
    EXPECT_EQ(ret, UBSE_ERROR_MODULE_LOAD_FAILED);
}

TEST_F(TestUrmaUvs, UbseActiveBonding_ConvertEidFailed)
{
    std::string eid = "0000:0000:0000:0000:0000:0000:0000:0001";
    std::string aggrDevName = "aggrDev1";
    bool activeState = false;
    MOCKER_CPP(&UbseGetBondingActiveStateByEid).stubs().with(eid, outBound(activeState)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_ERROR));

    auto ret = UbseActiveBonding(eid, aggrDevName);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUrmaUvs, UbseActiveBonding_Nullptr)
{
    std::string eid = "0000:0000:0000:0000:0000:0000:0000:0001";
    std::string aggrDevName = "aggrDev1";
    bool activeState = false;
    auto module = std::make_shared<UbseUrmaUvsModule>();
    module->uvsCreateAggrDev = nullptr;
    MOCKER_CPP(&UbseGetBondingActiveStateByEid).stubs().with(eid, outBound(activeState)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(module));

    auto ret = UbseActiveBonding(eid, aggrDevName);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUrmaUvs, UbseDeactiveBonding_Success)
{
    std::string eid = "0000:0000:0000:0000:0000:0000:0000:0001";
    auto module = std::make_shared<UbseUrmaUvsModule>();
    module->uvsDeleteAggrDev = [](char* aggrDevEid) -> UbseResult {
        return UBSE_OK;
    };
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(module));

    auto ret = UbseDeactiveBonding(eid);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUrmaUvs, UbseDeactiveBonding_ConvertEidFailed)
{
    std::string eid = "invalid_eid";
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_ERROR));

    auto ret = UbseDeactiveBonding(eid);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUrmaUvs, UbseDeactiveBonding_GetModuleFailed)
{
    std::string eid = "0000:0000:0000:0000:0000:0000:0000:0001";
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_OK));

    auto ret = UbseDeactiveBonding(eid);
    EXPECT_EQ(ret, UBSE_ERROR_MODULE_LOAD_FAILED);
}

TEST_F(TestUrmaUvs, UbseDeactiveBonding_Nullptr)
{
    std::string eid = "0000:0000:0000:0000:0000:0000:0000:0001";
    auto module = std::make_shared<UbseUrmaUvsModule>();
    module->uvsDeleteAggrDev = nullptr;
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(module));

    auto ret = UbseDeactiveBonding(eid);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUrmaUvs, UbseDeactiveBonding_DestroyAggrDevFailed)
{
    std::string eid = "0000:0000:0000:0000:0000:0000:0000:0001";
    auto module = std::make_shared<UbseUrmaUvsModule>();
    module->uvsDeleteAggrDev = [](char* aggrDevEid) -> UbseResult {
        return UBSE_ERROR;
    };
    MOCKER_CPP(&ConvertEidStrToHexCharList).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(module));

    auto ret = UbseDeactiveBonding(eid);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUrmaUvs, FillNodeComInfo)
{
    std::vector<PhysicalLink> allLinkInfo;
    allLinkInfo.push_back(
        PhysicalLink{.slotId = 1, .chipId = 1, .portId = 1, .peerSlotId = 2, .peerChipId = 1, .peerPortId = 1});

    std::vector<UbseUrmaUvsNodeInfo> bondingInfo;
    std::vector<UbcoreTopoNode> nodes;
    std::vector<UbseUrmaUvsAggrDev> devList;
    std::vector<UbseUrmaUvsFe> feList;
    std::map<std::string, std::string> portEid;

    portEid["1"] = "0000:0000:0000:0000:0000:0000:0000:0002";
    portEid["2"] = "0000:0000:0000:0000:0000:0000:0000:0003";
    feList.push_back(UbseUrmaUvsFe{
        .ubpuId = "1", .entityId = "1", .primaryEid = "0000:0000:0000:0000:0000:0000:0000:0001", .portEid = portEid});

    std::map<std::string, std::string> portEid2;

    portEid2["1"] = "0000:0000:0000:0000:0000:0000:0000:0004";
    portEid2["2"] = "0000:0000:0000:0000:0000:0000:0000:0005";
    feList.push_back(UbseUrmaUvsFe{
        .ubpuId = "2", .entityId = "1", .primaryEid = "0000:0000:0000:0000:0000:0000:0000:0006", .portEid = portEid2});
    devList.push_back(UbseUrmaUvsAggrDev{.urmaDevEid = "4225:0000:0000:0000:0000:0000:0000:0001", .feList = feList});
    bondingInfo.push_back(UbseUrmaUvsNodeInfo{.nodeId = "1", .devList = devList});

    auto ret = FillNodeComInfo("1", allLinkInfo, bondingInfo, nodes);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUrmaUvs, FillNodeComInfo_NoLink)
{
    std::vector<PhysicalLink> allLinkInfo;
    std::vector<UbseUrmaUvsNodeInfo> bondingInfo;
    std::vector<UbcoreTopoNode> nodes;
    std::vector<UbseUrmaUvsAggrDev> devList;
    std::vector<UbseUrmaUvsFe> feList;
    std::map<std::string, std::string> portEid;

    portEid["1"] = "0000:0000:0000:0000:0000:0000:0000:0002";
    portEid["2"] = "0000:0000:0000:0000:0000:0000:0000:0003";
    feList.push_back(UbseUrmaUvsFe{
        .ubpuId = "1", .entityId = "1", .primaryEid = "0000:0000:0000:0000:0000:0000:0000:0001", .portEid = portEid});

    std::map<std::string, std::string> portEid2;

    portEid2["1"] = "0000:0000:0000:0000:0000:0000:0000:0004";
    portEid2["2"] = "0000:0000:0000:0000:0000:0000:0000:0005";
    feList.push_back(UbseUrmaUvsFe{
        .ubpuId = "2", .entityId = "1", .primaryEid = "0000:0000:0000:0000:0000:0000:0000:0006", .portEid = portEid2});
    devList.push_back(UbseUrmaUvsAggrDev{.urmaDevEid = "4225:0000:0000:0000:0000:0000:0000:0001", .feList = feList});
    bondingInfo.push_back(UbseUrmaUvsNodeInfo{.nodeId = "1", .devList = devList});

    auto ret = FillNodeComInfo("1", allLinkInfo, bondingInfo, nodes);
    EXPECT_EQ(ret, UBSE_OK);
}

namespace {
uint32_t PortIndex(uint32_t chipId, uint32_t portId)
{
    return (chipId - 1) * PORT_NUM + portId;
}

UbcoreTopoNode MakeTopoNode(uint32_t nodeId, bool isCurrent)
{
    UbcoreTopoNode node{};
    node.id = nodeId;
    node.type = 1;
    node.is_current = isCurrent ? 1 : 0;
    return node;
}

UbseUrmaTopoConfig MakeNonCrossTopoConfig()
{
    UbseUrmaTopoConfig config{};
    config.version = "1.0";
    config.nodeType = "normal";
    config.linkType = "non-cross";
    config.nodePorts = {
        {1, 1, 1},
        {1, 1, 2},
        {2, 1, 1},
        {2, 1, 2},
    };
    config.links = {
        {{1, 1, 1}, {1, 1, 1}},
        {{1, 1, 2}, {1, 1, 2}},
        {{2, 1, 1}, {2, 1, 1}},
        {{2, 1, 2}, {2, 1, 2}},
    };
    return config;
}

UbseUrmaTopoConfig MakeHccsCrossTopoConfig()
{
    UbseUrmaTopoConfig config{};
    config.version = "1.0";
    config.nodeType = "normal";
    config.linkType = "hccs-cross";
    config.nodePorts = {
        {1, 1, 1},
        {1, 1, 2},
        {2, 1, 1},
        {2, 1, 2},
    };
    config.links = {
        {{1, 1, 1}, {1, 1, 1}}, {{1, 1, 1}, {2, 1, 1}}, {{1, 1, 2}, {1, 1, 2}}, {{1, 1, 2}, {2, 1, 2}},
        {{2, 1, 1}, {2, 1, 1}}, {{2, 1, 1}, {1, 1, 1}}, {{2, 1, 2}, {2, 1, 2}}, {{2, 1, 2}, {1, 1, 2}},
    };
    return config;
}
} // namespace

/*
 * 用例描述：按 non-cross 静态拓扑配置填充 CLOS 拓扑矩阵。
 * 预期结果：当前节点和非当前节点均按配置填充同 chip 同 port 链路。
 */
TEST_F(TestUrmaUvs, FillClosTopoByConfigNonCrossSuccess)
{
    std::unordered_map<std::string, UbcoreTopoNode> nodeMap{
        {"1", MakeTopoNode(1, true)},
        {"2", MakeTopoNode(2, false)},
    };

    EXPECT_EQ(FillClosTopoByConfig(MakeNonCrossTopoConfig(), nodeMap), UBSE_OK);

    EXPECT_TRUE(nodeMap["1"].links[PortIndex(1, 1)][PortIndex(1, 1)]);
    EXPECT_TRUE(nodeMap["1"].links[PortIndex(1, 2)][PortIndex(1, 2)]);
    EXPECT_TRUE(nodeMap["1"].links[PortIndex(2, 1)][PortIndex(2, 1)]);
    EXPECT_TRUE(nodeMap["1"].links[PortIndex(2, 2)][PortIndex(2, 2)]);
    EXPECT_FALSE(nodeMap["1"].links[PortIndex(1, 3)][PortIndex(1, 3)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(1, 1)][PortIndex(1, 1)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(1, 2)][PortIndex(1, 2)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(2, 1)][PortIndex(2, 1)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(2, 2)][PortIndex(2, 2)]);
    EXPECT_FALSE(nodeMap["2"].links[PortIndex(1, 1)][PortIndex(2, 1)]);
}

/*
 * 用例描述：按 hccs-cross 静态拓扑配置填充 CLOS 拓扑矩阵。
 * 预期结果：当前节点和非当前节点均按配置填充同 chip 同 port 和跨 chip 链路。
 */
TEST_F(TestUrmaUvs, FillClosTopoByConfigHccsCrossSuccess)
{
    std::unordered_map<std::string, UbcoreTopoNode> nodeMap{
        {"1", MakeTopoNode(1, true)},
        {"2", MakeTopoNode(2, false)},
    };

    EXPECT_EQ(FillClosTopoByConfig(MakeHccsCrossTopoConfig(), nodeMap), UBSE_OK);

    EXPECT_TRUE(nodeMap["1"].links[PortIndex(1, 1)][PortIndex(1, 1)]);
    EXPECT_TRUE(nodeMap["1"].links[PortIndex(1, 2)][PortIndex(1, 2)]);
    EXPECT_TRUE(nodeMap["1"].links[PortIndex(2, 1)][PortIndex(2, 1)]);
    EXPECT_TRUE(nodeMap["1"].links[PortIndex(2, 2)][PortIndex(2, 2)]);
    EXPECT_TRUE(nodeMap["1"].links[PortIndex(1, 1)][PortIndex(2, 1)]);
    EXPECT_TRUE(nodeMap["1"].links[PortIndex(1, 2)][PortIndex(2, 2)]);
    EXPECT_TRUE(nodeMap["1"].links[PortIndex(2, 1)][PortIndex(1, 1)]);
    EXPECT_TRUE(nodeMap["1"].links[PortIndex(2, 2)][PortIndex(1, 2)]);
    EXPECT_FALSE(nodeMap["1"].links[PortIndex(1, 1)][PortIndex(2, 2)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(1, 1)][PortIndex(1, 1)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(1, 1)][PortIndex(2, 1)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(1, 2)][PortIndex(2, 2)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(2, 1)][PortIndex(1, 1)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(2, 2)][PortIndex(1, 2)]);
    EXPECT_FALSE(nodeMap["2"].links[PortIndex(1, 1)][PortIndex(2, 2)]);
}

/*
 * 用例描述：CLOS 静态拓扑配置中端口 chipId 非法。
 * 预期结果：填充失败并返回 UBSE_ERROR。
 */
TEST_F(TestUrmaUvs, FillClosTopoByConfigFailsWhenChipInvalid)
{
    auto config = MakeNonCrossTopoConfig();
    config.links[0].localPort.chipId = 0;
    std::unordered_map<std::string, UbcoreTopoNode> nodeMap{
        {"2", MakeTopoNode(2, false)},
    };

    EXPECT_EQ(FillClosTopoByConfig(config, nodeMap), UBSE_ERROR);
}
} // namespace ubse::urma::ut
