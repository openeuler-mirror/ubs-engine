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
#include "out_of_band/ubse_mti_bus_instance_out_of_band.h"
#include "ubse_npu_manager_api.cpp"

namespace ubse::npu::controller::ut {

void TestUbseNpuManagerApi::SetUp()
{
    Test::SetUp();
    topo_ = BuildTopology();
}

void TestUbseNpuManagerApi::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

DeviceTopology TestUbseNpuManagerApi::BuildTopology()
{
    DeviceTopology topo;

    topo.hostBusiLoc.guid = "01234567abcdef01234567abcdef0100";
    topo.hostBusiLoc.upi = "0";

    topo.vmBusiLoc.guid = "01234567abcdef01234567abcdef0200";
    topo.vmBusiLoc.upi = "1";

    topo.npuLoc.slotId = 1;
    topo.npuLoc.chipId = 2;
    topo.npuLoc.guid = "01234567abcdef01234567abcdef0300";

    topo.ubctlLoc.chipId = 2;
    topo.ubctlLoc.dieId = 0;
    topo.ubctlLoc.slotId = 1;

    topo.idevPfeLoc.chipId = 2;
    topo.idevPfeLoc.dieId = 0;
    topo.idevPfeLoc.pfeId = 1;
    topo.idevPfeLoc.guid = "01234567abcdef01234567abcdef0400";

    topo.idevVfeLoc.chipId = 2;
    topo.idevVfeLoc.dieId = 0;
    topo.idevVfeLoc.pfeId = 1;
    topo.idevVfeLoc.vfeId = 3;
    topo.idevVfeLoc.guid = "01234567abcdef01234567abcdef0500";

    topo.nicPfeLoc.slotId = 1;
    topo.nicPfeLoc.chipId = 2;
    topo.nicPfeLoc.pfeId = 3;
    topo.nicPfeLoc.guid = "01234567abcdef01234567abcdef0600";

    topo.nicVfeLoc.slotId = 1;
    topo.nicVfeLoc.chipId = 2;
    topo.nicVfeLoc.pfeId = 3;
    topo.nicVfeLoc.vfeId = 4;
    topo.nicVfeLoc.guid = "01234567abcdef01234567abcdef0700";

    topo.hostBusi = std::make_shared<CollectionDeviceBusi>(
        topo.hostBusiLoc.guid, topo.hostBusiLoc.eid, topo.hostBusiLoc.upi, CollectionDeviceType::HOST_BUSINSTANCE);
    topo.vmBusi = std::make_shared<CollectionDeviceBusi>(topo.vmBusiLoc.guid, topo.vmBusiLoc.eid, topo.vmBusiLoc.upi,
                                                         CollectionDeviceType::VM_BUSINSTANCE);
    topo.npu = std::make_shared<CollectionDeviceDavid>(topo.npuLoc);
    topo.ubctl = std::make_shared<CollectionDeviceUbCtrl>(topo.ubctlLoc);
    topo.idevPfe = std::make_shared<CollectionDeviceIdevPfe>(topo.idevPfeLoc);
    topo.idevVfe = std::make_shared<CollectionDeviceIdevVfe>(topo.idevVfeLoc);
    topo.nicPfe = std::make_shared<CollectionDeviceNicPfe>(topo.nicPfeLoc);
    topo.nicVfe = std::make_shared<CollectionDeviceNicVfe>(topo.nicVfeLoc);

    topo.idevPfe->SetParentUbCtl(topo.ubctl);
    topo.idevPfe->SetSubDevIdev(topo.idevVfe);
    topo.idevVfe->SetParentPfe(topo.idevPfe);
    topo.idevVfe->SetBondingDevDavid(topo.npu);
    topo.npu->SetBondingIdev(CollectionDevice::CollectionToBase(topo.idevVfe));

    topo.hostBusi->SetSubDevNicPfe(topo.nicPfe);
    topo.hostBusi->SetSubDevNicVfe(topo.nicVfe);

    topo.vmBusi->SetSubDevIdev(topo.idevVfe);
    topo.vmBusi->SetSubDevNicPfe(topo.nicPfe);
    topo.vmBusi->SetSubDevNicVfe(topo.nicVfe);
    topo.idevVfe->SetBondingDevBusi(topo.vmBusi);
    topo.nicPfe->SetBondingDevBusi(topo.vmBusi);
    topo.nicVfe->SetBondingDevBusi(topo.vmBusi);

    return topo;
}

void TestUbseNpuManagerApi::PopulateCollection(const DeviceTopology& topo)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();
    auto baseHostBusi = CollectionDevice::CollectionToBase(topo.hostBusi);
    auto baseVmBusi = CollectionDevice::CollectionToBase(topo.vmBusi);
    auto baseNpu = CollectionDevice::CollectionToBase(topo.npu);
    auto baseUbctl = CollectionDevice::CollectionToBase(topo.ubctl);
    auto baseIdevPfe = CollectionDevice::CollectionToBase(topo.idevPfe);
    auto baseIdevVfe = CollectionDevice::CollectionToBase(topo.idevVfe);
    auto baseNicPfe = CollectionDevice::CollectionToBase(topo.nicPfe);
    auto baseNicVfe = CollectionDevice::CollectionToBase(topo.nicVfe);
    collection.SetDevice(baseHostBusi);
    collection.SetDevice(baseVmBusi);
    collection.SetDevice(baseNpu);
    collection.SetDevice(baseUbctl);
    collection.SetDevice(baseIdevPfe);
    collection.SetDevice(baseIdevVfe);
    collection.SetDevice(baseNicPfe);
    collection.SetDevice(baseNicVfe);
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
    UbseResult result = CheckBusi(topo_.hostBusi);
    EXPECT_EQ(result, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNpuManagerApi, CheckBusiWithVmBusInstance)
{
    UbseResult result = CheckBusi(topo_.vmBusi);
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
    UbseMtiDavid mtiDavid = ConvertToUbseMtiDavid(topo_.npu);
    EXPECT_EQ(mtiDavid.slotId, topo_.npuLoc.slotId);
    EXPECT_EQ(mtiDavid.chipId, topo_.npuLoc.chipId);
    EXPECT_EQ(mtiDavid.channelId, 0xff);
}

TEST_F(TestUbseNpuManagerApi, ConvertToUbseMtiBusiSuccess)
{
    UbseMtiBusInst mtiBusi = ConvertToUbseMtiBusi(topo_.vmBusi);
    EXPECT_EQ(mtiBusi.eid, topo_.vmBusi->GetEid());
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
    UbseMtiIdevVfe mtiVfe = ConvertToUbseMtiIdevVfe(topo_.idevVfe);
    EXPECT_EQ(mtiVfe.pfeId, topo_.idevVfeLoc.pfeId);
    EXPECT_EQ(mtiVfe.vfeId, topo_.idevVfeLoc.vfeId);
    EXPECT_EQ(mtiVfe.ubController.chipId, topo_.ubctlLoc.chipId);
    EXPECT_EQ(mtiVfe.ubController.dieId, topo_.ubctlLoc.dieId);
    EXPECT_EQ(mtiVfe.ubController.slotId, topo_.ubctlLoc.slotId);
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

TEST_F(TestUbseNpuManagerApi, QueryUbaTidSizeImplReturnsErrorWhenGuidNotFound)
{
    UbaTidSize info;
    UbseResult result = QueryUbaTidSizeImpl("test-guid", info);
    EXPECT_EQ(result, UBSE_ERROR);
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
    CollectDeviceLoc idevVfeLocNoBusi;
    idevVfeLocNoBusi.chipId = 2;
    idevVfeLocNoBusi.dieId = 0;
    idevVfeLocNoBusi.pfeId = 1;
    idevVfeLocNoBusi.vfeId = 1;

    auto npuNoBusi = std::make_shared<CollectionDeviceDavid>(topo_.npuLoc);
    auto idevVfeNoBusi = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLocNoBusi);
    npuNoBusi->SetBondingIdev(CollectionDevice::CollectionToBase(idevVfeNoBusi));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList = {npuNoBusi};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceDavid>>> occupiedNpuMap;

    UbseResult result = CheckOccupiedNpus(npuList, occupiedNpuMap);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_TRUE(occupiedNpuMap.empty());
}

TEST_F(TestUbseNpuManagerApi, CheckOccupiedNpusWithVfeIdevVmBusi)
{
    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList = {topo_.npu};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceDavid>>> occupiedNpuMap;

    UbseResult result = CheckOccupiedNpus(npuList, occupiedNpuMap);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(occupiedNpuMap.size(), 1);
    EXPECT_EQ(occupiedNpuMap.count(topo_.vmBusiLoc.guid), 1);
    EXPECT_EQ(occupiedNpuMap[topo_.vmBusiLoc.guid].size(), 1);
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

TEST_F(TestUbseNpuManagerApi, CheckOccupiedNpusVIdevCastFailed)
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
    idevPfe->SetType(CollectionDeviceType::V_IDEV);
    npu->SetBondingIdev(CollectionDevice::CollectionToBase(idevPfe));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList = {npu};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceDavid>>> occupiedNpuMap;

    UbseResult result = CheckOccupiedNpus(npuList, occupiedNpuMap);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, CheckOccupiedNpusPIdevCastFailed)
{
    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;

    CollectDeviceLoc idevVfeLoc;
    idevVfeLoc.chipId = 2;
    idevVfeLoc.dieId = 0;
    idevVfeLoc.pfeId = 1;
    idevVfeLoc.vfeId = 3;

    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    auto idevVfe = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLoc);
    idevVfe->SetType(CollectionDeviceType::P_IDEV);
    npu->SetBondingIdev(CollectionDevice::CollectionToBase(idevVfe));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList = {npu};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceDavid>>> occupiedNpuMap;

    UbseResult result = CheckOccupiedNpus(npuList, occupiedNpuMap);
    EXPECT_EQ(result, UBSE_ERROR);
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
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicList = {topo_.nicPfe};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNicPfe>>> occupiedNicMap;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> recoverNics;

    CheckOccupiedNicPfes(nicList, occupiedNicMap, recoverNics);
    EXPECT_EQ(occupiedNicMap.size(), 1);
    EXPECT_EQ(occupiedNicMap.count(topo_.vmBusiLoc.guid), 1);
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
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicList = {topo_.nicVfe};
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNicVfe>>> occupiedNicMap;

    CheckOccupiedNicVfes(nicList, occupiedNicMap);
    EXPECT_EQ(occupiedNicMap.size(), 1);
    EXPECT_EQ(occupiedNicMap.count(topo_.vmBusiLoc.guid), 1);
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
    auto npu = std::make_shared<CollectionDeviceDavid>(topo_.npuLoc);
    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList = {npu};
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = CheckAndHandleOccupiedDevices(npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, CheckAndHandleOccupiedDevicesSuccess)
{
    PopulateCollection(topo_);

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList = {topo_.npu};
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList = {topo_.nicPfe};
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList = {topo_.nicVfe};

    MOCKER(&UnregisterAndUnbindNpus).stubs().will(returnValue(UBSE_OK));

    MOCKER(&UnregisterAndRegisterNicPfes).stubs().will(returnValue(UBSE_OK));

    MOCKER(&UnregisterNicVfes).stubs().will(returnValue(UBSE_OK));

    UbseResult result = CheckAndHandleOccupiedDevices(npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_OK);
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

/*
 * ============================================================================
 * QueryVmBusInstances 相关函数测试
 * ============================================================================
 */

/*
 * ============================================================================
 * QueryNpuDevices 相关函数测试
 * ============================================================================
 */

TEST_F(TestUbseNpuManagerApi, QueryNpuDevicesEmptyMap)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    auto result = QueryNpuDevices(collection);
    EXPECT_EQ(result.first, UBSE_OK);
    EXPECT_TRUE(result.second.empty());
}

TEST_F(TestUbseNpuManagerApi, QueryNpuDevicesSuccess)
{
    PopulateCollection(topo_);

    auto result = QueryNpuDevices(ResourceCollection::GetInstance());
    EXPECT_EQ(result.first, UBSE_OK);
    EXPECT_EQ(result.second.size(), 1);

    auto npuRes = std::dynamic_pointer_cast<NpuResource>(result.second[0]);
    EXPECT_NE(npuRes, nullptr);
    EXPECT_EQ(npuRes->GetType(), ResourceType::NPU);
    EXPECT_EQ(npuRes->slotId_, topo_.npuLoc.slotId);
    EXPECT_EQ(npuRes->chipId_, topo_.npuLoc.chipId);
    EXPECT_EQ(npuRes->guid_, topo_.idevVfeLoc.guid);
    EXPECT_EQ(npuRes->busInstanceGuid_, topo_.vmBusiLoc.guid);

    bool foundUbctl = false;
    for (const auto& aff : npuRes->affinityDevices_) {
        if (aff.type == ResourceType::UBCONTROLLER && aff.slotId == topo_.ubctlLoc.slotId &&
            aff.chipId == topo_.ubctlLoc.chipId && aff.dieId == topo_.ubctlLoc.dieId) {
            foundUbctl = true;
        }
    }
    EXPECT_TRUE(foundUbctl);
}

/*
 * ============================================================================
 * QueryNicPfeDevices 相关函数测试
 * ============================================================================
 */

TEST_F(TestUbseNpuManagerApi, QueryNicPfeDevicesEmptyMap)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    auto result = QueryNicPfeDevices(collection);
    EXPECT_EQ(result.first, UBSE_OK);
    EXPECT_TRUE(result.second.empty());
}

TEST_F(TestUbseNpuManagerApi, QueryNicPfeDevicesSuccess)
{
    PopulateCollection(topo_);

    auto result = QueryNicPfeDevices(ResourceCollection::GetInstance());
    EXPECT_EQ(result.first, UBSE_OK);
    EXPECT_EQ(result.second.size(), 1);

    auto nicRes = std::dynamic_pointer_cast<NicPfeResource>(result.second[0]);
    EXPECT_NE(nicRes, nullptr);
    EXPECT_EQ(nicRes->GetType(), ResourceType::NIC_PFE);
    EXPECT_EQ(nicRes->slotId_, topo_.nicPfeLoc.slotId);
    EXPECT_EQ(nicRes->chipId_, topo_.nicPfeLoc.chipId);
    EXPECT_EQ(nicRes->pfId_, topo_.nicPfeLoc.pfeId);
    EXPECT_EQ(nicRes->guid_, topo_.nicPfeLoc.guid);
    EXPECT_EQ(nicRes->busInstanceGuid_, topo_.vmBusiLoc.guid);
}

/*
 * ============================================================================
 * QueryNicVfeDevices 相关函数测试
 * ============================================================================
 */

TEST_F(TestUbseNpuManagerApi, QueryNicVfeDevicesEmptyMap)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    auto result = QueryNicVfeDevices(collection);
    EXPECT_EQ(result.first, UBSE_OK);
    EXPECT_TRUE(result.second.empty());
}

TEST_F(TestUbseNpuManagerApi, QueryNicVfeDevicesSuccess)
{
    PopulateCollection(topo_);

    auto result = QueryNicVfeDevices(ResourceCollection::GetInstance());
    EXPECT_EQ(result.first, UBSE_OK);
    EXPECT_EQ(result.second.size(), 1);

    auto nicRes = std::dynamic_pointer_cast<NicVfeResource>(result.second[0]);
    EXPECT_NE(nicRes, nullptr);
    EXPECT_EQ(nicRes->GetType(), ResourceType::NIC_VFE);
    EXPECT_EQ(nicRes->slotId_, topo_.nicVfeLoc.slotId);
    EXPECT_EQ(nicRes->chipId_, topo_.nicVfeLoc.chipId);
    EXPECT_EQ(nicRes->pfId_, topo_.nicVfeLoc.pfeId);
    EXPECT_EQ(nicRes->vfId_, topo_.nicVfeLoc.vfeId);
    EXPECT_EQ(nicRes->guid_, topo_.nicVfeLoc.guid);
    EXPECT_EQ(nicRes->busInstanceGuid_, topo_.vmBusiLoc.guid);
}

/*
 * ============================================================================
 * QueryAllDevicesImpl 相关函数测试
 * ============================================================================
 */

TEST_F(TestUbseNpuManagerApi, QueryAllDevicesImplSuccess)
{
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.collectionReady_ = true;
    manager.state_ = UbseNpuManagerApi::NpuManagerState::AVAILABLE;

    PopulateCollection(topo_);

    std::vector<std::shared_ptr<IResource>> devList;
    UbseResult result = QueryAllDevicesImpl(devList);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_GE(devList.size(), 2);

    bool foundBusi = false;
    bool foundNpu = false;
    for (const auto& res : devList) {
        if (res->GetType() == ResourceType::BUSINSTANCE) {
            auto busiRes = std::dynamic_pointer_cast<BusiResource>(res);
            EXPECT_EQ(busiRes->guid_, topo_.vmBusiLoc.guid);
            foundBusi = true;
        }
        if (res->GetType() == ResourceType::NPU) {
            auto npuRes = std::dynamic_pointer_cast<NpuResource>(res);
            EXPECT_EQ(npuRes->slotId_, topo_.npuLoc.slotId);
            EXPECT_EQ(npuRes->chipId_, topo_.npuLoc.chipId);
            foundNpu = true;
        }
    }
    EXPECT_TRUE(foundBusi);
    EXPECT_TRUE(foundNpu);
}

/*
 * ============================================================================
 * UbseGetAllocDeviceList 相关函数测试
 * ============================================================================
 */

TEST_F(TestUbseNpuManagerApi, UbseGetAllocDeviceListNpuSuccess)
{
    PopulateCollection(topo_);

    UbseAllocRequest requestInfo;
    UbDevice dev;
    dev.type = ResourceType::NPU;
    dev.slotId = topo_.npuLoc.slotId;
    dev.chipId = topo_.npuLoc.chipId;
    requestInfo.ubDevList.push_back(dev);

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = UbseGetAllocDeviceList(requestInfo, npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(npuList.size(), 1);
    EXPECT_TRUE(nicPfeList.empty());
    EXPECT_TRUE(nicVfeList.empty());

    auto npuDev = npuList[0];
    auto npuLoc = npuDev->GetDeviceLoc();
    EXPECT_EQ(npuLoc.slotId, topo_.npuLoc.slotId);
    EXPECT_EQ(npuLoc.chipId, topo_.npuLoc.chipId);
}

TEST_F(TestUbseNpuManagerApi, UbseGetAllocDeviceListNpuNotFound)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    UbseAllocRequest requestInfo;
    UbDevice dev;
    dev.type = ResourceType::NPU;
    dev.slotId = 99;
    dev.chipId = 99;
    requestInfo.ubDevList.push_back(dev);

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = UbseGetAllocDeviceList(requestInfo, npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNpuManagerApi, UbseGetAllocDeviceListInvalidType)
{
    UbseAllocRequest requestInfo;
    UbDevice dev;
    dev.type = static_cast<ResourceType>(99);
    dev.slotId = 1;
    dev.chipId = 2;
    requestInfo.ubDevList.push_back(dev);

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = UbseGetAllocDeviceList(requestInfo, npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNpuManagerApi, UbseGetAllocDeviceListNicPfeSuccess)
{
    PopulateCollection(topo_);

    UbseAllocRequest requestInfo;
    UbDevice dev;
    dev.type = ResourceType::NIC_PFE;
    dev.slotId = topo_.nicPfeLoc.slotId;
    dev.chipId = topo_.nicPfeLoc.chipId;
    dev.pfId = topo_.nicPfeLoc.pfeId;
    requestInfo.ubDevList.push_back(dev);

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = UbseGetAllocDeviceList(requestInfo, npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_TRUE(npuList.empty());
    EXPECT_EQ(nicPfeList.size(), 1);
    EXPECT_TRUE(nicVfeList.empty());

    auto nicDev = nicPfeList[0];
    auto nicLoc = nicDev->GetDeviceLoc();
    EXPECT_EQ(nicLoc.slotId, topo_.nicPfeLoc.slotId);
    EXPECT_EQ(nicLoc.chipId, topo_.nicPfeLoc.chipId);
    EXPECT_EQ(nicLoc.pfeId, topo_.nicPfeLoc.pfeId);
}

TEST_F(TestUbseNpuManagerApi, UbseGetAllocDeviceListNicVfeSuccess)
{
    PopulateCollection(topo_);

    UbseAllocRequest requestInfo;
    UbDevice dev;
    dev.type = ResourceType::NIC_VFE;
    dev.slotId = topo_.nicVfeLoc.slotId;
    dev.chipId = topo_.nicVfeLoc.chipId;
    dev.pfId = topo_.nicVfeLoc.pfeId;
    dev.vfId = topo_.nicVfeLoc.vfeId;
    requestInfo.ubDevList.push_back(dev);

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = UbseGetAllocDeviceList(requestInfo, npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_TRUE(npuList.empty());
    EXPECT_TRUE(nicPfeList.empty());
    EXPECT_EQ(nicVfeList.size(), 1);

    auto nicDev = nicVfeList[0];
    auto nicLoc = nicDev->GetDeviceLoc();
    EXPECT_EQ(nicLoc.slotId, topo_.nicVfeLoc.slotId);
    EXPECT_EQ(nicLoc.chipId, topo_.nicVfeLoc.chipId);
    EXPECT_EQ(nicLoc.pfeId, topo_.nicVfeLoc.pfeId);
    EXPECT_EQ(nicLoc.vfeId, topo_.nicVfeLoc.vfeId);
}

TEST_F(TestUbseNpuManagerApi, UbseGetAllocDeviceListMixedDevices)
{
    PopulateCollection(topo_);

    UbseAllocRequest requestInfo;
    UbDevice dev1;
    dev1.type = ResourceType::NPU;
    dev1.slotId = topo_.npuLoc.slotId;
    dev1.chipId = topo_.npuLoc.chipId;
    requestInfo.ubDevList.push_back(dev1);

    UbDevice dev2;
    dev2.type = ResourceType::NIC_PFE;
    dev2.slotId = topo_.nicPfeLoc.slotId;
    dev2.chipId = topo_.nicPfeLoc.chipId;
    dev2.pfId = topo_.nicPfeLoc.pfeId;
    requestInfo.ubDevList.push_back(dev2);

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = UbseGetAllocDeviceList(requestInfo, npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(npuList.size(), 1);
    EXPECT_EQ(nicPfeList.size(), 1);
    EXPECT_TRUE(nicVfeList.empty());

    auto npuDev = npuList[0];
    auto npuLoc = npuDev->GetDeviceLoc();
    EXPECT_EQ(npuLoc.slotId, topo_.npuLoc.slotId);
    EXPECT_EQ(npuLoc.chipId, topo_.npuLoc.chipId);

    auto nicDev = nicPfeList[0];
    auto nicLoc = nicDev->GetDeviceLoc();
    EXPECT_EQ(nicLoc.slotId, topo_.nicPfeLoc.slotId);
    EXPECT_EQ(nicLoc.chipId, topo_.nicPfeLoc.chipId);
    EXPECT_EQ(nicLoc.pfeId, topo_.nicPfeLoc.pfeId);
}

/*
 * ============================================================================
 * AllocDevicesImpl 相关函数测试

TEST_F(TestUbseNpuManagerApi, AllocDevicesImplCollectionNotReady)
{
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.collectionReady_ = false;

    MOCKER_CPP(&ResourceCollection::CollectStaticResource)
        .stubs()
        .will(returnValue(UBSE_ERROR));

    UbseAllocRequest requestInfo;
    std::vector<std::shared_ptr<IResource>> devList;
    std::string busInstanceGuid;

    UbseResult result = AllocDevicesImpl(requestInfo, busInstanceGuid, devList);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, AllocDevicesImplInvalidBusInstanceGuid)
{
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.collectionReady_ = true;
    manager.state_ = UbseNpuManagerApi::NpuManagerState::AVAILABLE;
    manager.cv_.notify_all();

    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    UbseAllocRequest requestInfo;
    requestInfo.busInstanceGuid = "nonexistent-guid-1234567890123456789012";

    std::vector<std::shared_ptr<IResource>> devList;
    std::string busInstanceGuid;

    UbseResult result = AllocDevicesImpl(requestInfo, busInstanceGuid, devList);
    EXPECT_EQ(result, UBSE_ERROR_INVAL);
}

/*
 * ============================================================================
 * AllocNic 相关函数测试
 * ============================================================================
 */

TEST_F(TestUbseNpuManagerApi, AllocNicEmptyLists)
{
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;
    std::string busInstanceGuid = "vm-busi-guid";

    UbseResult result = AllocNic(nicPfeList, nicVfeList, busInstanceGuid);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, AllocNicPfeNoHostBusi)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    CollectDeviceLoc nicLoc;
    nicLoc.slotId = 1;
    nicLoc.chipId = 2;
    nicLoc.pfeId = 3;
    nicLoc.guid = "nic-pfe-guid-12345678901234567890123456";

    auto nic = std::make_shared<CollectionDeviceNicPfe>(nicLoc);
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList = {nic};
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;
    std::string busInstanceGuid = "vm-busi-guid";

    UbseResult result = AllocNic(nicPfeList, nicVfeList, busInstanceGuid);
    EXPECT_EQ(result, UBSE_ERROR);
}

/*
 * ============================================================================
 * CheckCollection 相关函数测试
 * ============================================================================
 */

TEST_F(TestUbseNpuManagerApi, CheckCollectionReady)
{
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.collectionReady_ = true;

    UbseResult result = CheckCollection("test action");
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, CheckCollectionNotReadyRetrySuccess)
{
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.collectionReady_ = false;

    MOCKER_CPP(&ResourceCollection::CollectStaticResource).stubs().will(returnValue(UBSE_OK));

    UbseResult result = CheckCollection("test action");
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(manager.collectionReady_, true);
    EXPECT_EQ(manager.GetState(), UbseNpuManagerApi::NpuManagerState::AVAILABLE);
}

TEST_F(TestUbseNpuManagerApi, CheckCollectionNotReadyRetryFailed)
{
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.collectionReady_ = false;

    MOCKER_CPP(&ResourceCollection::CollectStaticResource).stubs().will(returnValue(UBSE_ERROR));

    UbseResult result = CheckCollection("test action");
    EXPECT_EQ(result, UBSE_ERROR);
}

/*
 * ============================================================================
 * FreeUbDevicesImpl 相关函数测试
 * ============================================================================
 */

TEST_F(TestUbseNpuManagerApi, FreeUbDevicesImplInvalidUpi)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    CollectDeviceLoc busiLoc;
    busiLoc.guid = "1234567890abcdef1234567890abcdef";
    busiLoc.upi = "not_a_hex_number";

    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    auto baseBusi = CollectionDevice::CollectionToBase(busi);
    collection.SetDevice(baseBusi);

    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.collectionReady_ = true;
    manager.state_ = UbseNpuManagerApi::NpuManagerState::AVAILABLE;

    UbseAllocRequest requestInfo;
    requestInfo.busInstanceGuid = busiLoc.guid;

    UbseResult result = FreeUbDevicesImpl(requestInfo);
    EXPECT_EQ(result, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNpuManagerApi, FreeUbDevicesImplNullBondingDavid)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    CollectDeviceLoc busiLoc;
    busiLoc.guid = "abcdef1234567890abcdef1234567890";
    busiLoc.upi = "1";

    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);

    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 2;
    vfeLoc.dieId = 0;
    vfeLoc.pfeId = 1;
    vfeLoc.vfeId = 3;

    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    vfe->SetIsComSharedFe(false);
    busi->SetSubDevIdev(vfe);

    auto baseBusi = CollectionDevice::CollectionToBase(busi);
    collection.SetDevice(baseBusi);

    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.collectionReady_ = true;
    manager.state_ = UbseNpuManagerApi::NpuManagerState::AVAILABLE;

    UbseAllocRequest requestInfo;
    requestInfo.busInstanceGuid = busiLoc.guid;

    UbseResult result = FreeUbDevicesImpl(requestInfo);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, FreeUbDevicesImplSharedFeOnlySkip)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    CollectDeviceLoc busiLoc;
    busiLoc.guid = "7890abcdef1234567890abcdef123456";
    busiLoc.upi = "1";

    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);

    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 2;
    vfeLoc.dieId = 0;
    vfeLoc.pfeId = 1;
    vfeLoc.vfeId = 3;

    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    vfe->SetIsComSharedFe(true);
    busi->SetSubDevIdev(vfe);

    auto baseBusi = CollectionDevice::CollectionToBase(busi);
    collection.SetDevice(baseBusi);

    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.collectionReady_ = true;
    manager.state_ = UbseNpuManagerApi::NpuManagerState::AVAILABLE;

    UbseAllocRequest requestInfo;
    requestInfo.busInstanceGuid = busiLoc.guid;

    MOCKER(&FreeUbDevicesAction).stubs().will(returnValue(UBSE_OK));

    UbseResult result = FreeUbDevicesImpl(requestInfo);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, FreeUbDevicesImplSuccess)
{
    PopulateCollection(topo_);

    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.collectionReady_ = true;
    manager.state_ = UbseNpuManagerApi::NpuManagerState::AVAILABLE;

    UbseAllocRequest requestInfo;
    requestInfo.busInstanceGuid = topo_.vmBusiLoc.guid;

    MOCKER(&FreeUbDevicesAction).stubs().will(returnValue(UBSE_OK));

    UbseResult result = FreeUbDevicesImpl(requestInfo);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, SetStateRollbackTriggersRollBack)
{
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);
    EXPECT_EQ(manager.retryTime_, COMMON_RETRY_TIME);

    manager.SetState(UbseNpuManagerApi::NpuManagerState::ROLLBACK);
    EXPECT_EQ(manager.state_, UbseNpuManagerApi::NpuManagerState::AVAILABLE);
    EXPECT_EQ(manager.retryTime_, COMMON_RETRY_TIME);
}

TEST_F(TestUbseNpuManagerApi, SetStateRunningAllocClearsHistory)
{
    auto& manager = UbseNpuManagerApi::GetInstance();
    auto op = std::make_shared<OperationHistory>();
    op->operation = []() -> UbseResult {
        return UBSE_OK;
    };
    manager.operationHistory_.push(op);

    manager.SetState(UbseNpuManagerApi::NpuManagerState::RUNNING_ALLOC);
    EXPECT_TRUE(manager.operationHistory_.empty());
    EXPECT_EQ(manager.retryTime_, COMMON_RETRY_TIME);
}

TEST_F(TestUbseNpuManagerApi, SetStateRunningFreeClearsFuture)
{
    auto& manager = UbseNpuManagerApi::GetInstance();
    auto op = std::make_shared<OperationHistory>();
    op->operation = []() -> UbseResult {
        return UBSE_OK;
    };
    manager.futureProcedure_.push(op);

    manager.SetState(UbseNpuManagerApi::NpuManagerState::RUNNING_FREE);
    EXPECT_TRUE(manager.futureProcedure_.empty());
    EXPECT_EQ(manager.retryTime_, COMMON_RETRY_TIME);
}

TEST_F(TestUbseNpuManagerApi, FilterUnregisteredDevicesReturnsNotInBusi)
{
    PopulateCollection(topo_);

    CollectDeviceLoc vfeLoc2;
    vfeLoc2.chipId = 3;
    vfeLoc2.dieId = 1;
    vfeLoc2.pfeId = 2;
    vfeLoc2.vfeId = 2;
    vfeLoc2.guid = "idev-vfe2-guid-1234567890123456789012";
    auto vfe2 = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc2);

    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> devList = {topo_.idevVfe, vfe2};
    auto result = UbseNpuManagerApi::GetInstance().FilterUnregisteredDevices(devList, topo_.vmBusi);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]->GetIdStr(), vfe2->GetIdStr());
}

TEST_F(TestUbseNpuManagerApi, FilterRegisteredDevicesReturnsInBusi)
{
    PopulateCollection(topo_);

    CollectDeviceLoc vfeLoc2;
    vfeLoc2.chipId = 3;
    vfeLoc2.dieId = 1;
    vfeLoc2.pfeId = 2;
    vfeLoc2.vfeId = 2;
    vfeLoc2.guid = "idev-vfe2-guid-1234567890123456789012";
    auto vfe2 = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc2);

    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> devList = {topo_.idevVfe, vfe2};
    auto result = UbseNpuManagerApi::GetInstance().FilterRegisteredDevices(devList, topo_.vmBusi);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]->GetIdStr(), topo_.idevVfe->GetIdStr());
}

TEST_F(TestUbseNpuManagerApi, PrepareRegisterInfosSkipsNull)
{
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 2;
    vfeLoc.dieId = 0;
    vfeLoc.pfeId = 1;
    vfeLoc.vfeId = 3;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);

    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> devList = {nullptr, vfe};
    auto result = UbseNpuManagerApi::GetInstance().PrepareRegisterInfos(devList);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]->GetIdStr(), vfe->GetIdStr());
}

TEST_F(TestUbseNpuManagerApi, PrepareUnRegisterInfosSkipsNull)
{
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 2;
    vfeLoc.dieId = 0;
    vfeLoc.pfeId = 1;
    vfeLoc.vfeId = 3;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);

    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> devList = {nullptr, vfe};
    auto result = UbseNpuManagerApi::GetInstance().PrepareUnRegisterInfos(devList);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]->GetIdStr(), vfe->GetIdStr());
}

TEST_F(TestUbseNpuManagerApi, ConvertToUbseMti1825VfNicPfe)
{
    auto result = ConvertToUbseMti1825Vf<CollectionDeviceNicPfe>({topo_.nicPfe});
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].slotId, topo_.nicPfeLoc.slotId);
    EXPECT_EQ(result[0].chipId, topo_.nicPfeLoc.chipId);
    EXPECT_EQ(result[0].pfId, topo_.nicPfeLoc.pfeId);
}

TEST_F(TestUbseNpuManagerApi, ConvertToUbseMti1825VfNicVfe)
{
    auto result = ConvertToUbseMti1825Vf<CollectionDeviceNicVfe>({topo_.nicVfe});
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].slotId, topo_.nicVfeLoc.slotId);
    EXPECT_EQ(result[0].chipId, topo_.nicVfeLoc.chipId);
    EXPECT_EQ(result[0].pfId, topo_.nicVfeLoc.pfeId);
    EXPECT_EQ(result[0].vfId, topo_.nicVfeLoc.vfeId);
}

TEST_F(TestUbseNpuManagerApi, ConvertToUbseMti1825VfSkipsNull)
{
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> devList = {nullptr};
    auto result = ConvertToUbseMti1825Vf<CollectionDeviceNicPfe>(devList);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(TestUbseNpuManagerApi, ResetNpuDavidListSuccess)
{
    MOCKER_CPP(&ubse::npu::vm_monitor::ResetNpu).stubs().will(returnValue(UBSE_OK));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList = {topo_.npu};
    UbseResult result = UbseNpuManagerApi::GetInstance().ResetNpu(devList);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, ResetNpuDavidListFailure)
{
    MOCKER_CPP(&ubse::npu::vm_monitor::ResetNpu).stubs().will(returnValue(UBSE_ERROR));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList = {topo_.npu};
    UbseResult result = UbseNpuManagerApi::GetInstance().ResetNpu(devList);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, ResetNpuUbDeviceListSuccess)
{
    MOCKER_CPP(&ubse::npu::vm_monitor::ResetNpu).stubs().will(returnValue(UBSE_OK));

    std::vector<UbDevice> ubDevList;
    UbDevice dev;
    dev.type = ResourceType::NPU;
    dev.chipId = 2;
    ubDevList.push_back(dev);

    UbseResult result = UbseNpuManagerApi::GetInstance().ResetNpu(ubDevList);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, ResetNpuUbDeviceListSkipsNonNpu)
{
    std::vector<UbDevice> ubDevList;
    UbDevice dev;
    dev.type = ResourceType::NIC_PFE;
    dev.chipId = 2;
    ubDevList.push_back(dev);

    UbseResult result = UbseNpuManagerApi::GetInstance().ResetNpu(ubDevList);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, ResetNpuUbDeviceListFailure)
{
    MOCKER_CPP(&ubse::npu::vm_monitor::ResetNpu).stubs().will(returnValue(UBSE_ERROR));

    std::vector<UbDevice> ubDevList;
    UbDevice dev;
    dev.type = ResourceType::NPU;
    dev.chipId = 2;
    ubDevList.push_back(dev);

    UbseResult result = UbseNpuManagerApi::GetInstance().ResetNpu(ubDevList);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, HandleOccupiedFilterDeviceVMBusiEmptyAll)
{
    PopulateCollection(topo_);
    UbseNpuManagerApi::GetInstance().SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = HandleOccupiedFilterDeviceVMBusi(topo_.vmBusi, npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, HandleOccupiedFilterDeviceVMBusiNpuUnbindFail)
{
    PopulateCollection(topo_);
    UbseNpuManagerApi::GetInstance().SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);

    MOCKER_CPP(&UbseNpuManagerApi::UnbindVfeDavid).stubs().will(returnValue(UBSE_ERROR));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList = {topo_.npu};
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = HandleOccupiedFilterDeviceVMBusi(topo_.vmBusi, npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, HandleOccupiedFilterDeviceVMBusiNicPfeUnregFail)
{
    PopulateCollection(topo_);
    UbseNpuManagerApi::GetInstance().SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);

    MOCKER_CPP(&UbseNpuManagerApi::UnbindVfeDavid).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNpuManagerApi::UnRegisterIDevFromBusi).stubs().will(returnValue(UBSE_OK));
    MOCKER(&RegisterNicPfesToHost).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNpuManagerApi::UnRegisterNicFromBusi<CollectionDeviceNicPfe>).stubs().will(returnValue(UBSE_ERROR));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList = {topo_.npu};
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList = {topo_.nicPfe};
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = HandleOccupiedFilterDeviceVMBusi(topo_.vmBusi, npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, ClearEmptyVMBusInstanceNoNicPfeNoIdev)
{
    PopulateCollection(topo_);
    UbseNpuManagerApi::GetInstance().SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);

    CollectDeviceLoc emptyBusiLoc;
    emptyBusiLoc.guid = "empty-busi-guid-1234567890123456789012";
    emptyBusiLoc.upi = "1";
    auto emptyBusi = std::make_shared<CollectionDeviceBusi>(emptyBusiLoc.guid, emptyBusiLoc.eid, emptyBusiLoc.upi,
                                                            CollectionDeviceType::VM_BUSINSTANCE);
    auto baseEmptyBusi = CollectionDevice::CollectionToBase(emptyBusi);
    ResourceCollection::GetInstance().SetDevice(baseEmptyBusi);

    MOCKER_CPP(&UbseNpuManagerApi::DestroyVMBusi).stubs().will(returnValue(UBSE_OK));

    ClearEmptyVMBusInstance();
}

TEST_F(TestUbseNpuManagerApi, ClearEmptyVMBusInstanceHasNicPfeNoDestroy)
{
    PopulateCollection(topo_);

    CollectDeviceLoc busiLoc;
    busiLoc.guid = "nic-pfe-busi-guid-1234567890123456789012";
    busiLoc.upi = "1";
    auto busiWithPfe = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                              CollectionDeviceType::VM_BUSINSTANCE);
    busiWithPfe->SetSubDevNicPfe(topo_.nicPfe);
    auto baseBusi = CollectionDevice::CollectionToBase(busiWithPfe);
    ResourceCollection::GetInstance().SetDevice(baseBusi);

    MOCKER_CPP(&UbseNpuManagerApi::DestroyVMBusi).expects(never());

    ClearEmptyVMBusInstance();
}

TEST_F(TestUbseNpuManagerApi, ClearEmptyVMBusInstanceHasNonSharedVfeNoDestroy)
{
    PopulateCollection(topo_);

    CollectDeviceLoc busiLoc;
    busiLoc.guid = "vfe-busi-guid-1234567890123456789012";
    busiLoc.upi = "1";
    auto busiWithVfe = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                              CollectionDeviceType::VM_BUSINSTANCE);
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 2;
    vfeLoc.dieId = 0;
    vfeLoc.pfeId = 1;
    vfeLoc.vfeId = 3;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    vfe->SetIsComSharedFe(false);
    busiWithVfe->SetSubDevIdev(vfe);
    auto baseBusi = CollectionDevice::CollectionToBase(busiWithVfe);
    ResourceCollection::GetInstance().SetDevice(baseBusi);

    MOCKER_CPP(&UbseNpuManagerApi::DestroyVMBusi).expects(never());

    ClearEmptyVMBusInstance();
}

TEST_F(TestUbseNpuManagerApi, AllocDevicesPreparationEmptyBusInstanceGuid)
{
    PopulateCollection(topo_);
    UbseNpuManagerApi::GetInstance().SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);

    MOCKER(&CheckAndHandleOccupiedDevices).stubs().will(returnValue(UBSE_OK));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = AllocDevicesPreparation("", npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, AllocDevicesPreparationWithBusInstanceGuidBusiNull)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = AllocDevicesPreparation("nonexistent-guid", npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNpuManagerApi, AllocDevicesPreparationFilterFail)
{
    PopulateCollection(topo_);

    MOCKER(&FilterDeviceVMBusi).stubs().will(returnValue(UBSE_ERROR));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = AllocDevicesPreparation(topo_.vmBusiLoc.guid, npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, AllocDevicesPreparationOccupiedFail)
{
    PopulateCollection(topo_);

    MOCKER(&CheckAndHandleOccupiedDevices).stubs().will(returnValue(UBSE_ERROR));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;

    UbseResult result = AllocDevicesPreparation("", npuList, nicPfeList, nicVfeList);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, QueryVmBusInstancesEmpty)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    auto result = QueryVmBusInstances(collection);
    EXPECT_EQ(result.first, UBSE_OK);
    EXPECT_TRUE(result.second.empty());
}

TEST_F(TestUbseNpuManagerApi, QueryVmBusInstancesSuccess)
{
    PopulateCollection(topo_);

    auto result = QueryVmBusInstances(ResourceCollection::GetInstance());
    EXPECT_EQ(result.first, UBSE_OK);
    EXPECT_EQ(result.second.size(), 1);

    auto busiRes = std::dynamic_pointer_cast<BusiResource>(result.second[0]);
    EXPECT_NE(busiRes, nullptr);
    EXPECT_EQ(busiRes->guid_, topo_.vmBusiLoc.guid);
}

TEST_F(TestUbseNpuManagerApi, RollBackSuccess)
{
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);

    auto op = std::make_shared<OperationHistory>();
    op->operation = []() -> UbseResult {
        return UBSE_OK;
    };
    manager.operationHistory_.push(op);

    UbseResult result = manager.RollBack();
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_TRUE(manager.operationHistory_.empty());
    EXPECT_EQ(manager.GetState(), UbseNpuManagerApi::NpuManagerState::AVAILABLE);
}

TEST_F(TestUbseNpuManagerApi, RollBackFailureCreatesBgThread)
{
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);

    auto op = std::make_shared<OperationHistory>();
    op->operation = []() -> UbseResult {
        return UBSE_ERROR;
    };
    manager.operationHistory_.push(op);

    UbseResult result = manager.RollBack();
    EXPECT_EQ(result, UBSE_ERROR);
    EXPECT_EQ(manager.GetState(), UbseNpuManagerApi::NpuManagerState::ROLLBACK_BG);
}

TEST_F(TestUbseNpuManagerApi, RollBackEmptyHistory)
{
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);

    while (!manager.operationHistory_.empty()) {
        manager.operationHistory_.pop();
    }

    UbseResult result = manager.RollBack();
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(manager.GetState(), UbseNpuManagerApi::NpuManagerState::AVAILABLE);
}

TEST_F(TestUbseNpuManagerApi, ExecuteFreeQueueSuccess)
{
    auto& manager = UbseNpuManagerApi::GetInstance();

    auto op = std::make_shared<OperationHistory>();
    op->operation = []() -> UbseResult {
        return UBSE_OK;
    };
    manager.futureProcedure_.push(op);

    UbseResult result = manager.ExecuteFreeQueue();
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_TRUE(manager.futureProcedure_.empty());
}

TEST_F(TestUbseNpuManagerApi, ExecuteFreeQueueFailure)
{
    auto& manager = UbseNpuManagerApi::GetInstance();

    auto op = std::make_shared<OperationHistory>();
    op->operation = []() -> UbseResult {
        return UBSE_ERROR;
    };
    manager.futureProcedure_.push(op);

    UbseResult result = manager.ExecuteFreeQueue();
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, UnRegisterIDevFromBusiBusiNotFound)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList = {topo_.npu};
    UbseResult result = UbseNpuManagerApi::GetInstance().UnRegisterIDevFromBusi(devList, "nonexistent-guid");
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, UnRegisterIDevFromBusiNullDavid)
{
    PopulateCollection(topo_);

    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList = {nullptr};
    UbseResult result = UbseNpuManagerApi::GetInstance().UnRegisterIDevFromBusi(devList, topo_.vmBusiLoc.guid);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, UnRegisterIDevFromBusiNullIdev)
{
    PopulateCollection(topo_);

    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 99;
    npuLoc.chipId = 99;
    auto npuNoIdev = std::make_shared<CollectionDeviceDavid>(npuLoc);

    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList = {npuNoIdev};
    UbseResult result = UbseNpuManagerApi::GetInstance().UnRegisterIDevFromBusi(devList, topo_.vmBusiLoc.guid);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, UnRegisterIDevFromBusiVfeMismatchSize)
{
    PopulateCollection(topo_);

    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;
    auto npuPfeIdev = std::make_shared<CollectionDeviceDavid>(npuLoc);

    CollectDeviceLoc pfeLoc;
    pfeLoc.chipId = 2;
    pfeLoc.dieId = 0;
    pfeLoc.pfeId = 1;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(pfeLoc);
    npuPfeIdev->SetBondingIdev(CollectionDevice::CollectionToBase(pfe));

    MOCKER_CPP(&UbseNpuManagerApi::UnRegisterVfeFromBusi).stubs().will(returnValue(UBSE_OK));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList = {npuPfeIdev};
    UbseResult result = UbseNpuManagerApi::GetInstance().UnRegisterIDevFromBusi(devList, topo_.vmBusiLoc.guid);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, RegisterIDevToBusiBusiNotFound)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList = {topo_.npu};
    UbseResult result = UbseNpuManagerApi::GetInstance().RegisterIDevToBusi(devList, "nonexistent-guid");
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, RegisterIDevToBusiWithPfeIdevEmptyVfe)
{
    PopulateCollection(topo_);

    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;
    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);

    CollectDeviceLoc pfeLoc;
    pfeLoc.chipId = 2;
    pfeLoc.dieId = 0;
    pfeLoc.pfeId = 1;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(pfeLoc);
    npu->SetBondingIdev(CollectionDevice::CollectionToBase(pfe));

    MOCKER_CPP(&UbseNpuManagerApi::RegisterVfeToBusi).stubs().will(returnValue(UBSE_OK));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList = {npu};
    UbseResult result = UbseNpuManagerApi::GetInstance().RegisterIDevToBusi(devList, topo_.vmBusiLoc.guid);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, RegisterIDevToBusiMismatchSize)
{
    PopulateCollection(topo_);

    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 99;
    npuLoc.chipId = 99;
    auto npuNoIdev = std::make_shared<CollectionDeviceDavid>(npuLoc);

    MOCKER_CPP(&UbseNpuManagerApi::RegisterVfeToBusi).stubs().will(returnValue(UBSE_OK));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList = {npuNoIdev};
    UbseResult result = UbseNpuManagerApi::GetInstance().RegisterIDevToBusi(devList, topo_.vmBusiLoc.guid);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, UnRegisterNicFromBusiPfeBusiNotFound)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> devList = {topo_.nicPfe};
    UbseResult result = UbseNpuManagerApi::GetInstance().UnRegisterNicFromBusi(devList, "nonexistent-guid");
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, RegisterNicToBusiPfeBusiNotFound)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> devList = {topo_.nicPfe};
    UbseResult result = UbseNpuManagerApi::GetInstance().RegisterNicToBusi(devList, "nonexistent-guid");
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, CreateVMBusiSuccess)
{
    UbseMtiBusInst mtiBusi;
    mtiBusi.eid = {1, 2, 3, 4, 5, 6, 7, 8};
    for (int i = 0; i < 16; i++) {
        mtiBusi.guid[i] = static_cast<uint8_t>(i);
    }

    auto& busInstance = UbseMtiBusInstance::GetInstance();
    MOCKER_CPP_VIRTUAL(busInstance, &UbseMtiBusInstance::CreateVmBusInstance).stubs().will(returnValue(UBSE_OK));

    CollectionGuid busiGuid;
    UbseResult result = UbseNpuManagerApi::GetInstance().CreateVMBusi(1, busiGuid);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_FALSE(busiGuid.empty());
}

TEST_F(TestUbseNpuManagerApi, CreateVMBusiFailure)
{
    auto& busInstance = UbseMtiBusInstance::GetInstance();
    MOCKER_CPP_VIRTUAL(busInstance, &UbseMtiBusInstance::CreateVmBusInstance).stubs().will(returnValue(UBSE_ERROR));

    CollectionGuid busiGuid;
    UbseResult result = UbseNpuManagerApi::GetInstance().CreateVMBusi(1, busiGuid);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, DestroyVMBusiBusiNotFound)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    UbseResult result = UbseNpuManagerApi::GetInstance().DestroyVMBusi("nonexistent-guid");
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, DestroyVMBusiSuccess)
{
    PopulateCollection(topo_);

    auto& busInstance = UbseMtiBusInstance::GetInstance();
    MOCKER_CPP_VIRTUAL(busInstance, &UbseMtiBusInstance::DestroyVmBusInstance).stubs().will(returnValue(UBSE_OK));

    UbseResult result = UbseNpuManagerApi::GetInstance().DestroyVMBusi(topo_.vmBusiLoc.guid);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, DestroyVMBusiFailure)
{
    PopulateCollection(topo_);

    auto& busInstance = UbseMtiBusInstance::GetInstance();
    MOCKER_CPP_VIRTUAL(busInstance, &UbseMtiBusInstance::DestroyVmBusInstance).stubs().will(returnValue(UBSE_ERROR));

    UbseResult result = UbseNpuManagerApi::GetInstance().DestroyVMBusi(topo_.vmBusiLoc.guid);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, RegisterVfeToBusiBusiNotFound)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> devList = {topo_.idevVfe};
    UbseResult result = UbseNpuManagerApi::GetInstance().RegisterVfeToBusi(devList, "nonexistent-guid");
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, RegisterVfeToBusiEmptyAfterFilter)
{
    PopulateCollection(topo_);

    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> devList = {topo_.idevVfe};
    UbseResult result = UbseNpuManagerApi::GetInstance().RegisterVfeToBusi(devList, topo_.vmBusiLoc.guid);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, UnRegisterVfeFromBusiBusiNotFound)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> devList = {topo_.idevVfe};
    UbseResult result = UbseNpuManagerApi::GetInstance().UnRegisterVfeFromBusi(devList, "nonexistent-guid");
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, UnRegisterVfeFromBusiEmptyAfterFilter)
{
    PopulateCollection(topo_);

    CollectDeviceLoc vfeLoc2;
    vfeLoc2.chipId = 99;
    vfeLoc2.dieId = 1;
    vfeLoc2.pfeId = 2;
    vfeLoc2.vfeId = 2;
    vfeLoc2.guid = "not-in-busi-vfe-guid-1234567890123456789012";
    auto vfeNotInBusi = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc2);

    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> devList = {vfeNotInBusi};
    UbseResult result = UbseNpuManagerApi::GetInstance().UnRegisterVfeFromBusi(devList, topo_.vmBusiLoc.guid);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, UnRegisterNicFromBusiVfeBusiNotFound)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> devList = {topo_.nicVfe};
    UbseResult result = UbseNpuManagerApi::GetInstance().UnRegisterNicFromBusi(devList, "nonexistent-guid");
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, GetCollectionReadyDefaultFalse)
{
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.collectionReady_ = false;
    EXPECT_FALSE(manager.GetCollectionReady());

    manager.SetCollectionReady(true);
    EXPECT_TRUE(manager.GetCollectionReady());
}

TEST_F(TestUbseNpuManagerApi, AllocNicWithNicPfeSuccess)
{
    PopulateCollection(topo_);
    UbseNpuManagerApi::GetInstance().SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);

    MOCKER_CPP(&UbseNpuManagerApi::UnRegisterNicFromBusi<CollectionDeviceNicPfe>).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNpuManagerApi::RegisterNicToBusi<CollectionDeviceNicPfe>).stubs().will(returnValue(UBSE_OK));

    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList = {topo_.nicPfe};
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;
    std::string busInstanceGuid = topo_.vmBusiLoc.guid;

    UbseResult result = AllocNic(nicPfeList, nicVfeList, busInstanceGuid);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, AllocNicWithNicPfeUnregFail)
{
    PopulateCollection(topo_);
    UbseNpuManagerApi::GetInstance().SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);

    MOCKER_CPP(&UbseNpuManagerApi::UnRegisterNicFromBusi<CollectionDeviceNicPfe>).stubs().will(returnValue(UBSE_ERROR));

    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList = {topo_.nicPfe};
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList;
    std::string busInstanceGuid = topo_.vmBusiLoc.guid;

    UbseResult result = AllocNic(nicPfeList, nicVfeList, busInstanceGuid);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, AllocNicWithNicVfeRegisterFail)
{
    PopulateCollection(topo_);
    UbseNpuManagerApi::GetInstance().SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);

    MOCKER_CPP(&UbseNpuManagerApi::RegisterNicToBusi<CollectionDeviceNicVfe>).stubs().will(returnValue(UBSE_ERROR));

    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfeList;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfeList = {topo_.nicVfe};
    std::string busInstanceGuid = topo_.vmBusiLoc.guid;

    UbseResult result = AllocNic(nicPfeList, nicVfeList, busInstanceGuid);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, UnbindVfeDavidEmptyList)
{
    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList;
    UbseResult result = UbseNpuManagerApi::GetInstance().UnbindVfeDavid(1, devList);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, UnbindVfeDavidNullDev)
{
    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList = {nullptr};
    UbseResult result = UbseNpuManagerApi::GetInstance().UnbindVfeDavid(1, devList);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, UnbindVfeDavidSuccess)
{
    PopulateCollection(topo_);
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::RUNNING_ALLOC);

    MOCKER_CPP(&UbseNpuManagerApi::SendUnbindRequest).stubs().will(returnValue(UBSE_OK));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList = {topo_.npu};
    UbseResult result = manager.UnbindVfeDavid(1, devList);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_FALSE(manager.operationHistory_.empty());
}

TEST_F(TestUbseNpuManagerApi, UnbindVfeDavidFailRollback)
{
    PopulateCollection(topo_);
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::RUNNING_ALLOC);

    MOCKER_CPP(&UbseNpuManagerApi::SendUnbindRequest).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(&UbseNpuManagerApi::SendBindRequest).stubs().will(returnValue(UBSE_OK));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList = {topo_.npu};
    UbseResult result = manager.UnbindVfeDavid(1, devList);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, BindVfeDavidSuccess)
{
    PopulateCollection(topo_);
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::RUNNING_ALLOC);

    MOCKER_CPP(&UbseNpuManagerApi::SendBindRequest).stubs().will(returnValue(UBSE_OK));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList = {topo_.npu};
    UbseResult result = manager.BindVfeDavid(1, devList);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_FALSE(manager.operationHistory_.empty());
}

TEST_F(TestUbseNpuManagerApi, BindVfeDavidFailRollback)
{
    PopulateCollection(topo_);
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::RUNNING_ALLOC);

    MOCKER_CPP(&UbseNpuManagerApi::SendBindRequest).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(&UbseNpuManagerApi::SendUnbindRequest).stubs().will(returnValue(UBSE_OK));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList = {topo_.npu};
    UbseResult result = manager.BindVfeDavid(1, devList);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, BindVfeDavidNullDev)
{
    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList = {nullptr};
    UbseResult result = UbseNpuManagerApi::GetInstance().BindVfeDavid(1, devList);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, FreeQueueBuildsFutureProcedures)
{
    PopulateCollection(topo_);
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);
    while (!manager.futureProcedure_.empty()) {
        manager.futureProcedure_.pop();
    }

    UbseAllocRequest req;
    req.upiStr = 1;
    req.busInstanceGuid = topo_.vmBusiLoc.guid;
    UbDevice ubDev;
    ubDev.chipId = 1;
    ubDev.type = ResourceType::NPU;
    req.ubDevList = {ubDev};

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npus = {topo_.npu};
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfes = {topo_.nicPfe};
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfes = {topo_.nicVfe};

    manager.FreeQueue(req, topo_.vmBusiLoc.guid, npus, nicPfes, nicVfes);
    EXPECT_EQ(manager.futureProcedure_.size(), 7);
}

TEST_F(TestUbseNpuManagerApi, SendUnbindRequestEmpty)
{
    std::vector<UbseMtiIdevVfeDavidPair> emptyList;
    UbseResult result = UbseNpuManagerApi::GetInstance().SendUnbindRequest(1, emptyList);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, SendBindRequestEmpty)
{
    std::vector<UbseMtiIdevVfeDavidPair> emptyList;
    UbseResult result = UbseNpuManagerApi::GetInstance().SendBindRequest(1, emptyList);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, SendUnRegisterNicRequestEmpty)
{
    bool needRollback = false;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> emptyList;
    UbseResult result =
        UbseNpuManagerApi::GetInstance().SendUnRegisterNicRequest<CollectionDeviceNicPfe>(emptyList, needRollback);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_FALSE(needRollback);
}

TEST_F(TestUbseNpuManagerApi, SendRegisterNicRequestEmpty)
{
    bool needRollback = false;
    auto busi = topo_.vmBusi;
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> emptyList;
    UbseResult result =
        UbseNpuManagerApi::GetInstance().SendRegisterNicRequest<CollectionDeviceNicPfe>(busi, emptyList, needRollback);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_FALSE(needRollback);
}

TEST_F(TestUbseNpuManagerApi, SendUnRegisterVfeRequestEmpty)
{
    bool needRollback = false;
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> emptyList;
    UbseResult result = UbseNpuManagerApi::GetInstance().SendUnRegisterVfeRequest(emptyList, needRollback);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_FALSE(needRollback);
}

TEST_F(TestUbseNpuManagerApi, SendRegisterVfeRequestEmpty)
{
    bool needRollback = false;
    auto busi = topo_.vmBusi;
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> emptyList;
    UbseResult result = UbseNpuManagerApi::GetInstance().SendRegisterVfeRequest(busi, emptyList, needRollback);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_FALSE(needRollback);
}

TEST_F(TestUbseNpuManagerApi, UnregisterAndUnbindNpusSuccess)
{
    PopulateCollection(topo_);
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);

    MOCKER_CPP(&UbseNpuManagerApi::UnbindVfeDavid).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNpuManagerApi::UnRegisterIDevFromBusi).stubs().will(returnValue(UBSE_OK));

    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceDavid>>> npuMap;
    npuMap[topo_.vmBusiLoc.guid] = {topo_.npu};

    UbseResult result = UnregisterAndUnbindNpus(npuMap);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, UnregisterAndUnbindNpusBusiNotFound)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceDavid>>> npuMap;
    npuMap["invalid-guid"] = {topo_.npu};

    UbseResult result = UnregisterAndUnbindNpus(npuMap);
    EXPECT_EQ(result, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNpuManagerApi, RegisterNicPfesToHostSuccess)
{
    PopulateCollection(topo_);

    MOCKER_CPP(&UbseNpuManagerApi::RegisterNicToBusi<CollectionDeviceNicPfe>).stubs().will(returnValue(UBSE_OK));

    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfes = {topo_.nicPfe};
    UbseResult result = RegisterNicPfesToHost(nicPfes);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, RegisterNicPfesToHostNoHostBusi)
{
    auto& collection = ResourceCollection::GetInstance();
    collection.ClearAllDevices();

    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfes = {topo_.nicPfe};
    UbseResult result = RegisterNicPfesToHost(nicPfes);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuManagerApi, UnregisterAndRegisterNicPfesSuccess)
{
    PopulateCollection(topo_);

    MOCKER_CPP(&UbseNpuManagerApi::UnRegisterNicFromBusi<CollectionDeviceNicPfe>).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNpuManagerApi::RegisterNicToBusi<CollectionDeviceNicPfe>).stubs().will(returnValue(UBSE_OK));

    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNicPfe>>> nicMap;
    nicMap[topo_.vmBusiLoc.guid] = {topo_.nicPfe};

    UbseResult result = UnregisterAndRegisterNicPfes(nicMap);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, UnregisterNicVfesSuccess)
{
    PopulateCollection(topo_);

    MOCKER_CPP(&UbseNpuManagerApi::UnRegisterNicFromBusi<CollectionDeviceNicVfe>).stubs().will(returnValue(UBSE_OK));

    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNicVfe>>> nicMap;
    nicMap[topo_.vmBusiLoc.guid] = {topo_.nicVfe};

    UbseResult result = UnregisterNicVfes(nicMap);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, RegisterIDevToBusiSuccess)
{
    PopulateCollection(topo_);

    MOCKER_CPP(&UbseNpuManagerApi::RegisterVfeToBusi).stubs().will(returnValue(UBSE_OK));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList = {topo_.npu};
    UbseResult result = UbseNpuManagerApi::GetInstance().RegisterIDevToBusi(devList, topo_.vmBusiLoc.guid);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, UnRegisterIDevFromBusiSuccess)
{
    PopulateCollection(topo_);

    MOCKER_CPP(&UbseNpuManagerApi::UnRegisterVfeFromBusi).stubs().will(returnValue(UBSE_OK));

    std::vector<std::shared_ptr<CollectionDeviceDavid>> devList = {topo_.npu};
    UbseResult result = UbseNpuManagerApi::GetInstance().UnRegisterIDevFromBusi(devList, topo_.vmBusiLoc.guid);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, UnRegisterNicFromBusiPfeSuccess)
{
    PopulateCollection(topo_);
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::RUNNING_ALLOC);

    MOCKER_CPP(&UbseNpuManagerApi::SendUnRegisterNicRequest<CollectionDeviceNicPfe>).stubs().will(returnValue(UBSE_OK));

    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> devList = {topo_.nicPfe};
    UbseResult result = manager.UnRegisterNicFromBusi<CollectionDeviceNicPfe>(devList, topo_.vmBusiLoc.guid);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_FALSE(manager.operationHistory_.empty());
}

TEST_F(TestUbseNpuManagerApi, RegisterNicToBusiPfeSuccess)
{
    PopulateCollection(topo_);
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::RUNNING_ALLOC);

    MOCKER_CPP(&UbseNpuManagerApi::SendRegisterNicRequest<CollectionDeviceNicPfe>).stubs().will(returnValue(UBSE_OK));

    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> devList = {topo_.nicPfe};
    UbseResult result = manager.RegisterNicToBusi<CollectionDeviceNicPfe>(devList, topo_.vmBusiLoc.guid);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_FALSE(manager.operationHistory_.empty());
}

TEST_F(TestUbseNpuManagerApi, RegisterVfeToBusiSuccess)
{
    PopulateCollection(topo_);
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::RUNNING_ALLOC);

    MOCKER_CPP(&UbseNpuManagerApi::SendRegisterVfeRequest).stubs().will(returnValue(UBSE_OK));

    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> devList;
    auto freshVfe = std::make_shared<CollectionDeviceIdevVfe>(topo_.idevVfeLoc);
    devList.push_back(freshVfe);
    UbseResult result = manager.RegisterVfeToBusi(devList, topo_.vmBusiLoc.guid);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseNpuManagerApi, UnRegisterVfeFromBusiSuccess)
{
    PopulateCollection(topo_);
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::RUNNING_ALLOC);

    MOCKER_CPP(&UbseNpuManagerApi::SendUnRegisterVfeRequest).stubs().will(returnValue(UBSE_OK));

    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> devList = {topo_.idevVfe};
    UbseResult result = manager.UnRegisterVfeFromBusi(devList, topo_.vmBusiLoc.guid);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_FALSE(manager.operationHistory_.empty());
}

TEST_F(TestUbseNpuManagerApi, ExecuteFreeQueueBackGroundRunsQueue)
{
    PopulateCollection(topo_);
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);

    while (!manager.futureProcedure_.empty()) {
        manager.futureProcedure_.pop();
    }

    auto op = std::make_shared<OperationHistory>();
    op->operation = []() -> UbseResult {
        return UBSE_OK;
    };
    manager.futureProcedure_.push(op);

    EXPECT_EQ(manager.futureProcedure_.size(), 1);
    manager.SetState(UbseNpuManagerApi::NpuManagerState::FREE_BG);

    std::unique_lock<std::mutex> lock(manager.mtx_);
    ASSERT_TRUE(manager.cv_.wait_for(lock, std::chrono::seconds(5), [&manager]() {
        return manager.state_ == UbseNpuManagerApi::NpuManagerState::AVAILABLE;
    }));
    EXPECT_TRUE(manager.futureProcedure_.empty());
}

TEST_F(TestUbseNpuManagerApi, SetStateRollbackBg)
{
    auto& manager = UbseNpuManagerApi::GetInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::ROLLBACK_BG);
    EXPECT_EQ(manager.retryTime_, COMMON_RETRY_TIME);
}

} // namespace ubse::npu::controller::ut