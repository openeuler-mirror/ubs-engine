/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_npu_manager_api.h"
#include "ubse_npu_manager_api.cpp"

namespace ubse::npu::controller::ut {

void TestUbseNpuManagerApi::SetUp()
{
    Test::SetUp();
}

void TestUbseNpuManagerApi::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNpuManagerApi, GetInstanceReturnsSingleton)
{
    auto& instance1 = UbseNpuManagerApi::GetInstance();
    auto& instance2 = UbseNpuManagerApi::GetInstance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(TestUbseNpuManagerApi, SetAndGetState)
{
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);
    EXPECT_EQ(manager.GetState(), UbseNpuManagerApi::NpuManagerState::AVAILABLE);

    manager.SetState(UbseNpuManagerApi::NpuManagerState::RUNNING_ALLOC);
    EXPECT_EQ(manager.GetState(), UbseNpuManagerApi::NpuManagerState::RUNNING_ALLOC);

    manager.SetState(UbseNpuManagerApi::NpuManagerState::RUNNING_FREE);
    EXPECT_EQ(manager.GetState(), UbseNpuManagerApi::NpuManagerState::RUNNING_FREE);

    manager.SetState(UbseNpuManagerApi::NpuManagerState::INIT);
    EXPECT_EQ(manager.GetState(), UbseNpuManagerApi::NpuManagerState::INIT);
}

TEST_F(TestUbseNpuManagerApi, CheckBusiWithNullptr)
{
    std::shared_ptr<CollectionDeviceBusi> busInstance = nullptr;
    UbseResult result = CheckBusi(busInstance);
    EXPECT_EQ(result, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNpuManagerApi, CheckBusiWithHostBusInstance)
{
    CollectDeviceLoc busiLoc;
    busiLoc.guid = "busi-guid-12345678901234567890123456";
    busiLoc.upi = "1";

    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::HOST_BUSINSTANCE);
    UbseResult result = CheckBusi(busi);
    EXPECT_EQ(result, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNpuManagerApi, CheckBusiWithVmBusInstance)
{
    CollectDeviceLoc busiLoc;
    busiLoc.guid = "busi-guid-12345678901234567890123456";
    busiLoc.upi = "1";

    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    UbseResult result = CheckBusi(busi);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, CheckResListAllSuccess)
{
    std::vector<bool> resList = {true, true, true, true};
    bool result = CheckResList(resList);
    EXPECT_TRUE(result);
}

TEST_F(TestUbseNpuManagerApi, CheckResListWithFailure)
{
    std::vector<bool> resList = {true, false, true, true};
    bool result = CheckResList(resList);
    EXPECT_FALSE(result);
}

TEST_F(TestUbseNpuManagerApi, CheckResListAllFailure)
{
    std::vector<bool> resList = {false, false, false};
    bool result = CheckResList(resList);
    EXPECT_FALSE(result);
}

TEST_F(TestUbseNpuManagerApi, CheckResListEmpty)
{
    std::vector<bool> resList;
    bool result = CheckResList(resList);
    EXPECT_TRUE(result);
}

TEST_F(TestUbseNpuManagerApi, UpdateFailedNicListPartialFailure)
{
    std::vector<UbseMti1825Vf> vfList;
    UbseMti1825Vf vf1;
    vf1.slotId = 1;
    vf1.chipId = 2;
    vfList.push_back(vf1);

    UbseMti1825Vf vf2;
    vf2.slotId = 3;
    vf2.chipId = 4;
    vfList.push_back(vf2);

    UbseMti1825Vf vf3;
    vf3.slotId = 5;
    vf3.chipId = 6;
    vfList.push_back(vf3);

    std::vector<bool> resList = {true, false, true};
    auto failedList = UpdateFailedNicList(vfList, resList);
    EXPECT_EQ(failedList.size(), 1);
    EXPECT_EQ(failedList[0].slotId, 3);
    EXPECT_EQ(failedList[0].chipId, 4);
}

TEST_F(TestUbseNpuManagerApi, UpdateFailedNicListAllSuccess)
{
    std::vector<UbseMti1825Vf> vfList;
    UbseMti1825Vf vf1;
    vf1.slotId = 1;
    vfList.push_back(vf1);

    std::vector<bool> resList = {true};
    auto failedList = UpdateFailedNicList(vfList, resList);
    EXPECT_EQ(failedList.size(), 0);
}

TEST_F(TestUbseNpuManagerApi, UpdateFailedNicListAllFailure)
{
    std::vector<UbseMti1825Vf> vfList;
    UbseMti1825Vf vf1;
    vf1.slotId = 1;
    vfList.push_back(vf1);

    UbseMti1825Vf vf2;
    vf2.slotId = 2;
    vfList.push_back(vf2);

    std::vector<bool> resList = {false, false};
    auto failedList = UpdateFailedNicList(vfList, resList);
    EXPECT_EQ(failedList.size(), 2);
}

TEST_F(TestUbseNpuManagerApi, UpdateFailedVfeListPartialFailure)
{
    std::vector<UbseMtiIdevVfe> vfeList;
    UbseMtiIdevVfe vfe1;
    vfe1.pfeId = 1;
    vfe1.vfeId = 1;
    vfeList.push_back(vfe1);

    UbseMtiIdevVfe vfe2;
    vfe2.pfeId = 2;
    vfe2.vfeId = 2;
    vfeList.push_back(vfe2);

    std::vector<bool> resList = {true, false};
    auto failedList = UpdateFailedVfeList(vfeList, resList);
    EXPECT_EQ(failedList.size(), 1);
    EXPECT_EQ(failedList[0].pfeId, 2);
    EXPECT_EQ(failedList[0].vfeId, 2);
}

TEST_F(TestUbseNpuManagerApi, UpdateFailedVfeListAllSuccess)
{
    std::vector<UbseMtiIdevVfe> vfeList;
    UbseMtiIdevVfe vfe1;
    vfe1.pfeId = 1;
    vfeList.push_back(vfe1);

    std::vector<bool> resList = {true};
    auto failedList = UpdateFailedVfeList(vfeList, resList);
    EXPECT_EQ(failedList.size(), 0);
}

TEST_F(TestUbseNpuManagerApi, ConvertToUbseMtiDavidSuccess)
{
    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;

    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    UbseMtiDavid mtiDavid = ConvertToUbseMtiDavid(npu);
    EXPECT_EQ(mtiDavid.slotId, npuLoc.slotId);
    EXPECT_EQ(mtiDavid.chipId, npuLoc.chipId);
    EXPECT_EQ(mtiDavid.channelId, 0xff);
}

TEST_F(TestUbseNpuManagerApi, ConvertToUbseMtiBusiSuccess)
{
    CollectDeviceLoc busiLoc;
    busiLoc.guid = "busi-guid-12345678901234567890123456";
    busiLoc.upi = "1";

    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    UbseMtiBusInst mtiBusi = ConvertToUbseMtiBusi(busi);
    EXPECT_EQ(mtiBusi.eid, busi->GetEid());
}

TEST_F(TestUbseNpuManagerApi, ConvertToUbseMtiIdevVfeWithNullptr)
{
    std::shared_ptr<CollectionDeviceIdevVfe> vfe = nullptr;
    UbseMtiIdevVfe mtiVfe = ConvertToUbseMtiIdevVfe(vfe);
    EXPECT_EQ(mtiVfe.pfeId, 0xFF);
    EXPECT_EQ(mtiVfe.vfeId, 0xFF);
}

TEST_F(TestUbseNpuManagerApi, ConvertToUbseMtiIdevVfeWithNullParentPfe)
{
    CollectDeviceLoc idevVfeLoc;
    idevVfeLoc.chipId = 2;
    idevVfeLoc.dieId = 0;
    idevVfeLoc.pfeId = 1;
    idevVfeLoc.vfeId = 1;
    idevVfeLoc.guid = "idev-guid-1234567890123456789012345";

    auto idevVfe = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLoc);
    UbseMtiIdevVfe mtiVfe = ConvertToUbseMtiIdevVfe(idevVfe);
    EXPECT_EQ(mtiVfe.pfeId, 0xFF);
    EXPECT_EQ(mtiVfe.vfeId, 0xFF);
}

TEST_F(TestUbseNpuManagerApi, ConvertToUbseMtiIdevVfeWithNullUbCtl)
{
    CollectDeviceLoc idevVfeLoc;
    idevVfeLoc.chipId = 2;
    idevVfeLoc.dieId = 0;
    idevVfeLoc.pfeId = 1;
    idevVfeLoc.vfeId = 1;
    idevVfeLoc.guid = "idev-guid-1234567890123456789012345";

    CollectDeviceLoc idevPfeLoc;
    idevPfeLoc.chipId = 2;
    idevPfeLoc.dieId = 0;
    idevPfeLoc.pfeId = 1;

    auto idevVfe = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLoc);
    auto idevPfe = std::make_shared<CollectionDeviceIdevPfe>(idevPfeLoc);
    idevVfe->SetParentPfe(idevPfe);

    UbseMtiIdevVfe mtiVfe = ConvertToUbseMtiIdevVfe(idevVfe);
    EXPECT_EQ(mtiVfe.pfeId, 0xFF);
    EXPECT_EQ(mtiVfe.vfeId, 0xFF);
}

TEST_F(TestUbseNpuManagerApi, ConvertToUbseMtiIdevVfeSuccess)
{
    CollectDeviceLoc idevVfeLoc;
    idevVfeLoc.chipId = 2;
    idevVfeLoc.dieId = 0;
    idevVfeLoc.pfeId = 1;
    idevVfeLoc.vfeId = 2;
    idevVfeLoc.guid = "idev-guid-1234567890123456789012345";

    CollectDeviceLoc idevPfeLoc;
    idevPfeLoc.chipId = 2;
    idevPfeLoc.dieId = 0;
    idevPfeLoc.pfeId = 1;

    CollectDeviceLoc ubctlLoc;
    ubctlLoc.chipId = 2;
    ubctlLoc.dieId = 0;
    ubctlLoc.slotId = 1;

    auto idevVfe = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLoc);
    auto idevPfe = std::make_shared<CollectionDeviceIdevPfe>(idevPfeLoc);
    auto ubctl = std::make_shared<CollectionDeviceUbCtrl>(ubctlLoc);

    idevVfe->SetParentPfe(idevPfe);
    idevPfe->SetParentUbCtl(ubctl);

    UbseMtiIdevVfe mtiVfe = ConvertToUbseMtiIdevVfe(idevVfe);
    EXPECT_EQ(mtiVfe.pfeId, idevVfeLoc.pfeId);
    EXPECT_EQ(mtiVfe.vfeId, idevVfeLoc.vfeId);
    EXPECT_EQ(mtiVfe.ubController.chipId, ubctlLoc.chipId);
    EXPECT_EQ(mtiVfe.ubController.dieId, ubctlLoc.dieId);
    EXPECT_EQ(mtiVfe.ubController.slotId, ubctlLoc.slotId);
}

TEST_F(TestUbseNpuManagerApi, ConvertToUbseMtiIdevVfeListSuccess)
{
    CollectDeviceLoc idevVfeLoc1;
    idevVfeLoc1.chipId = 2;
    idevVfeLoc1.dieId = 0;
    idevVfeLoc1.pfeId = 1;
    idevVfeLoc1.vfeId = 1;
    idevVfeLoc1.guid = "idev-guid-1234567890123456789012345";

    CollectDeviceLoc idevVfeLoc2;
    idevVfeLoc2.chipId = 3;
    idevVfeLoc2.dieId = 1;
    idevVfeLoc2.pfeId = 2;
    idevVfeLoc2.vfeId = 2;
    idevVfeLoc2.guid = "idev-guid-2234567890123456789012345";

    CollectDeviceLoc idevPfeLoc;
    idevPfeLoc.chipId = 2;
    idevPfeLoc.dieId = 0;
    idevPfeLoc.pfeId = 1;

    CollectDeviceLoc ubctlLoc;
    ubctlLoc.chipId = 2;
    ubctlLoc.dieId = 0;
    ubctlLoc.slotId = 1;

    auto idevVfe1 = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLoc1);
    auto idevVfe2 = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLoc2);
    auto idevPfe = std::make_shared<CollectionDeviceIdevPfe>(idevPfeLoc);
    auto ubctl = std::make_shared<CollectionDeviceUbCtrl>(ubctlLoc);

    idevVfe1->SetParentPfe(idevPfe);
    idevPfe->SetParentUbCtl(ubctl);
    idevVfe2->SetParentPfe(idevPfe);

    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> vfeList = {idevVfe1, idevVfe2};
    auto mtiVfeList = ConvertToUbseMtiIdevVfeList(vfeList);
    EXPECT_EQ(mtiVfeList.size(), 2);
}

TEST_F(TestUbseNpuManagerApi, ConvertToUbseMtiIdevVfeListWithNullptr)
{
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> vfeList;
    vfeList.push_back(nullptr);

    CollectDeviceLoc idevVfeLoc;
    idevVfeLoc.pfeId = 1;
    idevVfeLoc.vfeId = 1;
    auto idevVfe = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLoc);
    vfeList.push_back(idevVfe);

    auto mtiVfeList = ConvertToUbseMtiIdevVfeList(vfeList);
    EXPECT_EQ(mtiVfeList.size(), 1);
}

TEST_F(TestUbseNpuManagerApi, QueryUbaTidSizeImplReturnsOk)
{
    UbaTidSize info;
    UbseResult result = QueryUbaTidSizeImpl("test-guid", info);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, StateTransitionsFromInitToAvailable)
{
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::INIT);
    EXPECT_EQ(manager.GetState(), UbseNpuManagerApi::NpuManagerState::INIT);

    manager.SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);
    EXPECT_EQ(manager.GetState(), UbseNpuManagerApi::NpuManagerState::AVAILABLE);
}

/*
 * ============================================================================
 * AllocDevicesPreparation 相关函数测试
 * ============================================================================
 */

/*
 * CheckOccupiedNpus: 检查NPU列表中哪些设备已绑定到VM bus instance
 * 遍历npu列表，获取每个npu绑定的idev(vfe/pfe)，再获取idev绑定的busi
 * 最后将busi与npu的对应关系记录到occupiedNpuMap中
 */
TEST_F(TestUbseNpuManagerApi, CheckOccupiedNpusEmptyList)
{
    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList;
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceDavid>>> occupiedNpuMap;
    UbseResult result = CheckOccupiedNpus(npuList, occupiedNpuMap);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_TRUE(occupiedNpuMap.empty());
}

TEST_F(TestUbseNpuManagerApi, CheckOccupiedNpusWithNullIdev)
{
    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;

    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList = {npu};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceDavid>>> occupiedNpuMap;

    UbseResult result = CheckOccupiedNpus(npuList, occupiedNpuMap);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, CheckOccupiedNpusWithVfeIdevNoBusi)
{
    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;

    CollectDeviceLoc idevVfeLoc;
    idevVfeLoc.chipId = 2;
    idevVfeLoc.dieId = 0;
    idevVfeLoc.pfeId = 1;
    idevVfeLoc.vfeId = 1;
    idevVfeLoc.guid = "idev-guid-1234567890123456789012345";

    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    auto idevVfe = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLoc);

    npu->SetBondingIdev(CollectionDevice::CollectionToBase(idevVfe));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList = {npu};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceDavid>>> occupiedNpuMap;

    UbseResult result = CheckOccupiedNpus(npuList, occupiedNpuMap);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_TRUE(occupiedNpuMap.empty());
}

TEST_F(TestUbseNpuManagerApi, CheckOccupiedNpusWithVfeIdevVmBusi)
{
    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;

    CollectDeviceLoc idevVfeLoc;
    idevVfeLoc.chipId = 2;
    idevVfeLoc.dieId = 0;
    idevVfeLoc.pfeId = 1;
    idevVfeLoc.vfeId = 1;
    idevVfeLoc.guid = "idev-guid-1234567890123456789012345";

    CollectDeviceLoc busiLoc;
    busiLoc.guid = "vm-busi-guid-1234567890123456789012";
    busiLoc.upi = "1";

    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    auto idevVfe = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);

    npu->SetBondingIdev(CollectionDevice::CollectionToBase(idevVfe));
    idevVfe->SetBondingDevBusi(busi);

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList = {npu};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceDavid>>> occupiedNpuMap;

    UbseResult result = CheckOccupiedNpus(npuList, occupiedNpuMap);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(occupiedNpuMap.size(), 1);
    EXPECT_EQ(occupiedNpuMap.count(busiLoc.guid), 1);
    EXPECT_EQ(occupiedNpuMap[busiLoc.guid].size(), 1);
}

TEST_F(TestUbseNpuManagerApi, CheckOccupiedNpusWithPfeIdevVmBusi)
{
    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;

    CollectDeviceLoc idevPfeLoc;
    idevPfeLoc.chipId = 2;
    idevPfeLoc.dieId = 0;
    idevPfeLoc.pfeId = 1;

    CollectDeviceLoc idevVfeLoc;
    idevVfeLoc.chipId = 2;
    idevVfeLoc.dieId = 0;
    idevVfeLoc.pfeId = 1;
    idevVfeLoc.vfeId = 1;
    idevVfeLoc.guid = "idev-vfe-guid-1234567890123456789012";

    CollectDeviceLoc busiLoc;
    busiLoc.guid = "vm-busi-guid-1234567890123456789012";
    busiLoc.upi = "1";

    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    auto idevPfe = std::make_shared<CollectionDeviceIdevPfe>(idevPfeLoc);
    auto idevVfe = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);

    npu->SetBondingIdev(CollectionDevice::CollectionToBase(idevPfe));
    idevPfe->SetSubDevIdev(idevVfe);
    idevVfe->SetBondingDevBusi(busi);

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList = {npu};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceDavid>>> occupiedNpuMap;

    UbseResult result = CheckOccupiedNpus(npuList, occupiedNpuMap);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(occupiedNpuMap.size(), 1);
    EXPECT_EQ(occupiedNpuMap.count(busiLoc.guid), 1);
}

TEST_F(TestUbseNpuManagerApi, CheckOccupiedNpusWithPfeIdevNullVfe)
{
    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;

    CollectDeviceLoc idevPfeLoc;
    idevPfeLoc.chipId = 2;
    idevPfeLoc.dieId = 0;
    idevPfeLoc.pfeId = 1;

    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    auto idevPfe = std::make_shared<CollectionDeviceIdevPfe>(idevPfeLoc);

    npu->SetBondingIdev(CollectionDevice::CollectionToBase(idevPfe));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList = {npu};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceDavid>>> occupiedNpuMap;

    UbseResult result = CheckOccupiedNpus(npuList, occupiedNpuMap);
    EXPECT_EQ(result, UBSE_ERROR);
}

/*
 * CheckOccupiedNicPfes: 检查NIC PFE列表中哪些设备已绑定到VM bus instance
 * 遍历nic列表，获取每个nic绑定的busi
 * 如果busi为nullptr，加入recoverNics列表(需要注册到host bus instance)
 * 如果busi是VM_BUSINSTANCE类型，则将busi与nic的对应关系记录到occupiedNicMap中
 */
TEST_F(TestUbseNpuManagerApi, CheckOccupiedNicPfesEmptyList)
{
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicList;
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNicPfe>>> occupiedNicMap;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> recoverNics;

    CheckOccupiedNicPfes(nicList, occupiedNicMap, recoverNics);
    EXPECT_TRUE(occupiedNicMap.empty());
    EXPECT_TRUE(recoverNics.empty());
}

TEST_F(TestUbseNpuManagerApi, CheckOccupiedNicPfesWithNullptr)
{
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicList = {nullptr};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNicPfe>>> occupiedNicMap;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> recoverNics;

    CheckOccupiedNicPfes(nicList, occupiedNicMap, recoverNics);
    EXPECT_TRUE(occupiedNicMap.empty());
    EXPECT_TRUE(recoverNics.empty());
}

TEST_F(TestUbseNpuManagerApi, CheckOccupiedNicPfesWithNullBusi)
{
    CollectDeviceLoc nicLoc;
    nicLoc.slotId = 1;
    nicLoc.chipId = 2;
    nicLoc.pfeId = 3;
    nicLoc.guid = "nic-guid-1234567890123456789012345";

    auto nic = std::make_shared<CollectionDeviceNicPfe>(nicLoc);
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicList = {nic};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNicPfe>>> occupiedNicMap;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> recoverNics;

    CheckOccupiedNicPfes(nicList, occupiedNicMap, recoverNics);
    EXPECT_TRUE(occupiedNicMap.empty());
    EXPECT_EQ(recoverNics.size(), 1);
}

TEST_F(TestUbseNpuManagerApi, CheckOccupiedNicPfesWithHostBusi)
{
    CollectDeviceLoc nicLoc;
    nicLoc.slotId = 1;
    nicLoc.chipId = 2;
    nicLoc.pfeId = 3;
    nicLoc.guid = "nic-guid-1234567890123456789012345";

    CollectDeviceLoc busiLoc;
    busiLoc.guid = "host-busi-guid-1234567890123456789012";
    busiLoc.upi = "0";

    auto nic = std::make_shared<CollectionDeviceNicPfe>(nicLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::HOST_BUSINSTANCE);

    nic->SetBondingDevBusi(busi);

    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicList = {nic};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNicPfe>>> occupiedNicMap;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> recoverNics;

    CheckOccupiedNicPfes(nicList, occupiedNicMap, recoverNics);
    EXPECT_TRUE(occupiedNicMap.empty());
    EXPECT_TRUE(recoverNics.empty());
}

TEST_F(TestUbseNpuManagerApi, CheckOccupiedNicPfesWithVmBusi)
{
    CollectDeviceLoc nicLoc;
    nicLoc.slotId = 1;
    nicLoc.chipId = 2;
    nicLoc.pfeId = 3;
    nicLoc.guid = "nic-guid-1234567890123456789012345";

    CollectDeviceLoc busiLoc;
    busiLoc.guid = "vm-busi-guid-1234567890123456789012";
    busiLoc.upi = "1";

    auto nic = std::make_shared<CollectionDeviceNicPfe>(nicLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);

    nic->SetBondingDevBusi(busi);

    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicList = {nic};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNicPfe>>> occupiedNicMap;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> recoverNics;

    CheckOccupiedNicPfes(nicList, occupiedNicMap, recoverNics);
    EXPECT_EQ(occupiedNicMap.size(), 1);
    EXPECT_EQ(occupiedNicMap.count(busiLoc.guid), 1);
    EXPECT_TRUE(recoverNics.empty());
}

/*
 * CheckOccupiedNicVfes: 检查NIC VFE列表中哪些设备已绑定到VM bus instance
 * 遍历nic列表，获取每个nic绑定的busi
 * 如果busi是VM_BUSINSTANCE类型，则将busi与nic的对应关系记录到occupiedNicMap中
 * (与CheckOccupiedNicPfes不同，busi为nullptr时不加入recover列表)
 */
TEST_F(TestUbseNpuManagerApi, CheckOccupiedNicVfesEmptyList)
{
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicList;
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNicVfe>>> occupiedNicMap;

    CheckOccupiedNicVfes(nicList, occupiedNicMap);
    EXPECT_TRUE(occupiedNicMap.empty());
}

TEST_F(TestUbseNpuManagerApi, CheckOccupiedNicVfesWithNullptr)
{
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicList = {nullptr};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNicVfe>>> occupiedNicMap;

    CheckOccupiedNicVfes(nicList, occupiedNicMap);
    EXPECT_TRUE(occupiedNicMap.empty());
}

TEST_F(TestUbseNpuManagerApi, CheckOccupiedNicVfesWithNullBusi)
{
    CollectDeviceLoc nicLoc;
    nicLoc.slotId = 1;
    nicLoc.chipId = 2;
    nicLoc.pfeId = 3;
    nicLoc.vfeId = 4;
    nicLoc.guid = "nic-vfe-guid-1234567890123456789012";

    auto nic = std::make_shared<CollectionDeviceNicVfe>(nicLoc);
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicList = {nic};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNicVfe>>> occupiedNicMap;

    CheckOccupiedNicVfes(nicList, occupiedNicMap);
    EXPECT_TRUE(occupiedNicMap.empty());
}

TEST_F(TestUbseNpuManagerApi, CheckOccupiedNicVfesWithVmBusi)
{
    CollectDeviceLoc nicLoc;
    nicLoc.slotId = 1;
    nicLoc.chipId = 2;
    nicLoc.pfeId = 3;
    nicLoc.vfeId = 4;
    nicLoc.guid = "nic-vfe-guid-1234567890123456789012";

    CollectDeviceLoc busiLoc;
    busiLoc.guid = "vm-busi-guid-1234567890123456789012";
    busiLoc.upi = "1";

    auto nic = std::make_shared<CollectionDeviceNicVfe>(nicLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);

    nic->SetBondingDevBusi(busi);

    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicList = {nic};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNicVfe>>> occupiedNicMap;

    CheckOccupiedNicVfes(nicList, occupiedNicMap);
    EXPECT_EQ(occupiedNicMap.size(), 1);
    EXPECT_EQ(occupiedNicMap.count(busiLoc.guid), 1);
}

/*
 * RegisterNicPfesToHost: 将NIC PFE列表注册到Host bus instance
 * 如果列表不为空，获取host bus instance并调用RegisterNicToBusi进行注册
 */
TEST_F(TestUbseNpuManagerApi, RegisterNicPfesToHostEmptyList)
{
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> recoverNics;
    UbseResult result = RegisterNicPfesToHost(recoverNics);
    EXPECT_EQ(result, UBSE_OK);
}

/*
 * UnregisterNicVfes: 将NIC VFE列表从各自的VM bus instance解注册
 * 遍历occupiedNicMap，对每个busi下的nic列表调用UnRegisterNicFromBusi
 */
TEST_F(TestUbseNpuManagerApi, UnregisterNicVfesEmptyMap)
{
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNicVfe>>> occupiedNicMap;
    UbseResult result = UnregisterNicVfes(occupiedNicMap);
    EXPECT_EQ(result, UBSE_OK);
}

/*
 * UnregisterAndRegisterNicPfes: 将NIC PFE从VM bus instance解注册后注册到Host bus instance
 * 遍历occupiedNicMap，先解注册再注册到host
 */
TEST_F(TestUbseNpuManagerApi, UnregisterAndRegisterNicPfesEmptyMap)
{
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNicPfe>>> occupiedNicMap;
    UbseResult result = UnregisterAndRegisterNicPfes(occupiedNicMap);
    EXPECT_EQ(result, UBSE_OK);
}

/*
 * UnregisterAndUnbindNpus: 将NPU从VM bus instance解注册并解绑定Vfe
 * 遍历occupiedNpuMap，获取bus instance信息，调用UnbindVfeDavid和UnRegisterIDevFromBusi
 */
TEST_F(TestUbseNpuManagerApi, UnregisterAndUnbindNpusEmptyMap)
{
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceDavid>>> occupiedNpuMap;
    UbseResult result = UnregisterAndUnbindNpus(occupiedNpuMap);
    EXPECT_EQ(result, UBSE_OK);
}

/*
 * CheckAndHandleOccupiedDevices: 检查并处理所有已占用的设备(NPU/NIC PFE/NIC VFE)
 * 综合调用上述检查函数，并执行解注册解绑定操作
 */
TEST_F(TestUbseNpuManagerApi, CheckAndHandleOccupiedDevicesEmptyLists)
{
    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = CheckAndHandleOccupiedDevices(npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, CheckAndHandleOccupiedDevicesWithNullIdev)
{
    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;

    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList = {npu};
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = CheckAndHandleOccupiedDevices(npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_ERROR);
}

/*
 * FilterDeviceVMBusi: 过滤VM bus instance上已注册的设备
 * 对比请求列表与bus instance已有设备的差异：
 * 1. 找出需要从bus instance移除的设备
 * 2. 找出需要新注册到bus instance的设备
 * 3. 调用HandleOccupiedFilterDeviceVMBusi处理需要从busi中移除的设备
 */
TEST_F(TestUbseNpuManagerApi, FilterDeviceVMBusiWithEmptyLists)
{
    CollectDeviceLoc busiLoc;
    busiLoc.guid = "vm-busi-guid-1234567890123456789012";
    busiLoc.upi = "1";

    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = FilterDeviceVMBusi(busi, npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, FilterDeviceVMBusiNicPfeDiff)
{
    CollectDeviceLoc busiLoc;
    busiLoc.guid = "vm-busi-guid-1234567890123456789012";
    busiLoc.upi = "1";

    CollectDeviceLoc nicPfeLoc1;
    nicPfeLoc1.slotId = 1;
    nicPfeLoc1.chipId = 2;
    nicPfeLoc1.pfeId = 3;
    nicPfeLoc1.guid = "nic-pfe-guid-1234567890123456789012";

    CollectDeviceLoc nicPfeLoc2;
    nicPfeLoc2.slotId = 1;
    nicPfeLoc2.chipId = 2;
    nicPfeLoc2.pfeId = 4;
    nicPfeLoc2.guid = "nic-pfe-guid-2234567890123456789012";

    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    auto nicPfe1 = std::make_shared<CollectionDeviceNicPfe>(nicPfeLoc1);
    auto nicPfe2 = std::make_shared<CollectionDeviceNicPfe>(nicPfeLoc2);

    busi->SetSubDevNicPfe(nicPfe1);
    nicPfe1->SetBondingDevBusi(busi);

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList = {nicPfe2};
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = FilterDeviceVMBusi(busi, npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(nicPfeList.size(), 1);
    EXPECT_EQ(nicPfeList[0], nicPfe2);
}

TEST_F(TestUbseNpuManagerApi, FilterDeviceVMBusiNicVfeDiff)
{
    CollectDeviceLoc busiLoc;
    busiLoc.guid = "vm-busi-guid-1234567890123456789012";
    busiLoc.upi = "1";

    CollectDeviceLoc nicVfeLoc1;
    nicVfeLoc1.slotId = 1;
    nicVfeLoc1.chipId = 2;
    nicVfeLoc1.pfeId = 3;
    nicVfeLoc1.vfeId = 1;
    nicVfeLoc1.guid = "nic-vfe-guid-1234567890123456789012";

    CollectDeviceLoc nicVfeLoc2;
    nicVfeLoc2.slotId = 1;
    nicVfeLoc2.chipId = 2;
    nicVfeLoc2.pfeId = 3;
    nicVfeLoc2.vfeId = 2;
    nicVfeLoc2.guid = "nic-vfe-guid-2234567890123456789012";

    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    auto nicVfe1 = std::make_shared<CollectionDeviceNicVfe>(nicVfeLoc1);
    auto nicVfe2 = std::make_shared<CollectionDeviceNicVfe>(nicVfeLoc2);

    busi->SetSubDevNicVfe(nicVfe1);
    nicVfe1->SetBondingDevBusi(busi);

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList = {nicVfe2};

    UbseResult result = FilterDeviceVMBusi(busi, npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(nicVfeList.size(), 1);
    EXPECT_EQ(nicVfeList[0], nicVfe2);
}

TEST_F(TestUbseNpuManagerApi, FilterDeviceVMBusiNpuDiff)
{
    CollectDeviceLoc busiLoc;
    busiLoc.guid = "vm-busi-guid-1234567890123456789012";
    busiLoc.upi = "1";

    CollectDeviceLoc npuLoc1;
    npuLoc1.slotId = 1;
    npuLoc1.chipId = 2;

    CollectDeviceLoc npuLoc2;
    npuLoc2.slotId = 1;
    npuLoc2.chipId = 3;

    CollectDeviceLoc idevVfeLoc1;
    idevVfeLoc1.chipId = 2;
    idevVfeLoc1.dieId = 0;
    idevVfeLoc1.pfeId = 1;
    idevVfeLoc1.vfeId = 1;
    idevVfeLoc1.guid = "idev-vfe-guid-1234567890123456789012";

    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    auto npu1 = std::make_shared<CollectionDeviceDavid>(npuLoc1);
    auto npu2 = std::make_shared<CollectionDeviceDavid>(npuLoc2);
    auto idevVfe1 = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLoc1);

    busi->SetSubDevIdev(idevVfe1);
    idevVfe1->SetBondingDevBusi(busi);
    idevVfe1->SetBondingDevDavid(npu1);
    npu1->SetBondingIdev(CollectionDevice::CollectionToBase(idevVfe1));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList = {npu2};
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = FilterDeviceVMBusi(busi, npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(npuList.size(), 1);
    EXPECT_EQ(npuList[0], npu2);
}

} // namespace ubse::npu::controller::ut