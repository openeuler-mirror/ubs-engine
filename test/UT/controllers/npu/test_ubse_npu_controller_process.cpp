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

#include "test_ubse_npu_controller_process.h"
#include "ubse_npu_controller_process.cpp"

namespace ubse::npu::controller::ut {

void TestUbseNpuControllerProcess::SetUp()
{
    Test::SetUp();
}

void TestUbseNpuControllerProcess::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNpuControllerProcess, ProcessBusiResourceSuccess)
{
    UbseAllocRequest requestInfo;
    requestInfo.busInstanceGuid = "test-guid";
    UbDevice dev1;
    dev1.type = ResourceType::NPU;
    dev1.slotId = 1;
    dev1.chipId = 2;
    requestInfo.ubDevList.push_back(dev1);

    std::vector<std::shared_ptr<IResource>> devList;
    std::string newBusInstanceGuid = "new-bus-guid";

    UbseResult result = UbseNpuControllerProcess::ProcessBusiResource(requestInfo, devList, newBusInstanceGuid);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(devList.size(), 1);

    auto busiRes = std::dynamic_pointer_cast<BusiResource>(devList[0]);
    EXPECT_NE(busiRes, nullptr);
    EXPECT_EQ(busiRes->guid_, newBusInstanceGuid);
    EXPECT_EQ(busiRes->subDevices_.size(), 1);
    EXPECT_EQ(busiRes->subDevices_[0].type, ResourceType::NPU);
    EXPECT_EQ(busiRes->subDevices_[0].slotId, dev1.slotId);
    EXPECT_EQ(busiRes->subDevices_[0].chipId, dev1.chipId);
}

TEST_F(TestUbseNpuControllerProcess, ProcessBusiResourceWithEmptyDevList)
{
    UbseAllocRequest requestInfo;
    requestInfo.busInstanceGuid = "test-guid";

    std::vector<std::shared_ptr<IResource>> devList;
    std::string newBusInstanceGuid = "new-bus-guid";

    UbseResult result = UbseNpuControllerProcess::ProcessBusiResource(requestInfo, devList, newBusInstanceGuid);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(devList.size(), 1);

    auto busiRes = std::dynamic_pointer_cast<BusiResource>(devList[0]);
    EXPECT_NE(busiRes, nullptr);
    EXPECT_EQ(busiRes->guid_, newBusInstanceGuid);
    EXPECT_EQ(busiRes->subDevices_.size(), 0);
}

TEST_F(TestUbseNpuControllerProcess, ProcessBusiResourceWithMultipleDevices)
{
    UbseAllocRequest requestInfo;
    requestInfo.busInstanceGuid = "test-guid";

    UbDevice dev1;
    dev1.type = ResourceType::NPU;
    dev1.slotId = 1;
    dev1.chipId = 2;
    requestInfo.ubDevList.push_back(dev1);

    UbDevice dev2;
    dev2.type = ResourceType::NIC_PFE;
    dev2.slotId = 1;
    dev2.chipId = 2;
    dev2.pfId = 3;
    requestInfo.ubDevList.push_back(dev2);

    std::vector<std::shared_ptr<IResource>> devList;
    std::string newBusInstanceGuid = "new-bus-guid";

    UbseResult result = UbseNpuControllerProcess::ProcessBusiResource(requestInfo, devList, newBusInstanceGuid);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(devList.size(), 1);

    auto busiRes = std::dynamic_pointer_cast<BusiResource>(devList[0]);
    EXPECT_NE(busiRes, nullptr);
    EXPECT_EQ(busiRes->guid_, newBusInstanceGuid);
    EXPECT_EQ(busiRes->subDevices_.size(), 2);
    EXPECT_EQ(busiRes->subDevices_[0].type, ResourceType::NPU);
    EXPECT_EQ(busiRes->subDevices_[0].slotId, dev1.slotId);
    EXPECT_EQ(busiRes->subDevices_[0].chipId, dev1.chipId);
    EXPECT_EQ(busiRes->subDevices_[1].type, ResourceType::NIC_PFE);
    EXPECT_EQ(busiRes->subDevices_[1].slotId, dev2.slotId);
    EXPECT_EQ(busiRes->subDevices_[1].chipId, dev2.chipId);
    EXPECT_EQ(busiRes->subDevices_[1].pfId, dev2.pfId);
}

TEST_F(TestUbseNpuControllerProcess, DeviceNpuToResourceIdevNull)
{
    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;

    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    auto npuRes = std::make_shared<NpuResource>();

    UbseResult result = UbseNpuControllerProcess::DeviceNpuToResource(npu, npuRes);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuControllerProcess, DeviceNpuToResourceWithPfeIdev)
{
    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;

    CollectDeviceLoc idevPfeLoc;
    idevPfeLoc.chipId = 2;
    idevPfeLoc.dieId = 0;
    idevPfeLoc.pfeId = 1;
    idevPfeLoc.guid = "0123456789abcdef0123456789abcdef";

    CollectDeviceLoc ubctlLoc;
    ubctlLoc.chipId = 2;
    ubctlLoc.dieId = 0;

    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    auto idevPfe = std::make_shared<CollectionDeviceIdevPfe>(idevPfeLoc);
    auto ubctl = std::make_shared<CollectionDeviceUbCtrl>(ubctlLoc);
    auto npuRes = std::make_shared<NpuResource>();

    npu->SetBondingIdev(CollectionDevice::CollectionToBase(idevPfe));
    idevPfe->SetParentUbCtl(ubctl);

    UbseResult result = UbseNpuControllerProcess::DeviceNpuToResource(npu, npuRes);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(npuRes->slotId_, npuLoc.slotId);
    EXPECT_EQ(npuRes->chipId_, npuLoc.chipId);
    EXPECT_EQ(npuRes->busInstanceGuid_, "");
    EXPECT_EQ(npuRes->affinityDevices_.size(), 1);
    EXPECT_EQ(npuRes->affinityDevices_[0].type, ResourceType::UBCONTROLLER);
    EXPECT_EQ(npuRes->affinityDevices_[0].chipId, ubctlLoc.chipId);
    EXPECT_EQ(npuRes->affinityDevices_[0].dieId, ubctlLoc.dieId);
}

TEST_F(TestUbseNpuControllerProcess, DeviceNpuToResourceUbctlNull)
{
    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;

    CollectDeviceLoc idevPfeLoc;
    idevPfeLoc.chipId = 2;
    idevPfeLoc.dieId = 0;
    idevPfeLoc.pfeId = 1;
    idevPfeLoc.guid = "0123456789abcdef0123456789abcdef";

    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    auto idevPfe = std::make_shared<CollectionDeviceIdevPfe>(idevPfeLoc);
    auto npuRes = std::make_shared<NpuResource>();

    npu->SetBondingIdev(CollectionDevice::CollectionToBase(idevPfe));

    UbseResult result = UbseNpuControllerProcess::DeviceNpuToResource(npu, npuRes);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuControllerProcess, DeviceNpuToResourceWithVfeIdevAndBusi)
{
    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;

    CollectDeviceLoc idevVfeLoc;
    idevVfeLoc.chipId = 2;
    idevVfeLoc.dieId = 0;
    idevVfeLoc.pfeId = 1;
    idevVfeLoc.vfeId = 1;
    idevVfeLoc.guid = "0123456789abcdef0123456789abcdef";

    CollectDeviceLoc idevPfeLoc;
    idevPfeLoc.chipId = 2;
    idevPfeLoc.dieId = 0;
    idevPfeLoc.pfeId = 1;

    CollectDeviceLoc ubctlLoc;
    ubctlLoc.chipId = 2;
    ubctlLoc.dieId = 0;

    CollectDeviceLoc busiLoc;
    busiLoc.guid = "busi-guid-12345678901234567890123456";
    busiLoc.upi = "1";

    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    auto idevVfe = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLoc);
    auto idevPfe = std::make_shared<CollectionDeviceIdevPfe>(idevPfeLoc);
    auto ubctl = std::make_shared<CollectionDeviceUbCtrl>(ubctlLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    auto npuRes = std::make_shared<NpuResource>();

    npu->SetBondingIdev(CollectionDevice::CollectionToBase(idevVfe));
    idevPfe->SetSubDevIdev(idevVfe);
    idevVfe->SetParentPfe(idevPfe);
    idevVfe->SetBondingDevBusi(busi);
    idevPfe->SetParentUbCtl(ubctl);

    UbseResult result = UbseNpuControllerProcess::DeviceNpuToResource(npu, npuRes);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(npuRes->slotId_, npuLoc.slotId);
    EXPECT_EQ(npuRes->chipId_, npuLoc.chipId);
    EXPECT_EQ(npuRes->busInstanceGuid_, busiLoc.guid);
    EXPECT_EQ(npuRes->guid_, idevVfeLoc.guid);
    EXPECT_EQ(npuRes->affinityDevices_.size(), 1);
    EXPECT_EQ(npuRes->affinityDevices_[0].type, ResourceType::UBCONTROLLER);
    EXPECT_EQ(npuRes->affinityDevices_[0].chipId, ubctlLoc.chipId);
    EXPECT_EQ(npuRes->affinityDevices_[0].dieId, ubctlLoc.dieId);
}

TEST_F(TestUbseNpuControllerProcess, DeviceNicPfeToResourceSuccess)
{
    CollectDeviceLoc nicLoc;
    nicLoc.slotId = 1;
    nicLoc.chipId = 2;
    nicLoc.pfeId = 3;
    nicLoc.guid = "nic-guid-1234567890123456789012345";

    auto nic = std::make_shared<CollectionDeviceNicPfe>(nicLoc);
    auto nicRes = std::make_shared<NicPfeResource>();

    UbseResult result = UbseNpuControllerProcess::DeviceNicPfeToResource(nic, nicRes);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(nicRes->slotId_, nicLoc.slotId);
    EXPECT_EQ(nicRes->chipId_, nicLoc.chipId);
    EXPECT_EQ(nicRes->pfId_, nicLoc.pfeId);
    EXPECT_EQ(nicRes->guid_, nicLoc.guid);
    EXPECT_EQ(nicRes->busInstanceGuid_, "");
    EXPECT_EQ(nicRes->affinityDevices_.size(), 0);
}

TEST_F(TestUbseNpuControllerProcess, DeviceNicPfeToResourceWithAffinityNpu)
{
    CollectDeviceLoc nicLoc;
    nicLoc.slotId = 1;
    nicLoc.chipId = 2;
    nicLoc.pfeId = 3;
    nicLoc.guid = "nic-guid-1234567890123456789012345";

    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 5;
    npuLoc.chipId = 6;

    auto nic = std::make_shared<CollectionDeviceNicPfe>(nicLoc);
    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    auto nicRes = std::make_shared<NicPfeResource>();

    nic->SetAffinityDevDavid(npu);

    UbseResult result = UbseNpuControllerProcess::DeviceNicPfeToResource(nic, nicRes);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(nicRes->slotId_, nicLoc.slotId);
    EXPECT_EQ(nicRes->chipId_, nicLoc.chipId);
    EXPECT_EQ(nicRes->pfId_, nicLoc.pfeId);
    EXPECT_EQ(nicRes->guid_, nicLoc.guid);
    EXPECT_EQ(nicRes->busInstanceGuid_, "");
    EXPECT_EQ(nicRes->affinityDevices_.size(), 1);
    EXPECT_EQ(nicRes->affinityDevices_[0].type, ResourceType::NPU);
    EXPECT_EQ(nicRes->affinityDevices_[0].slotId, npuLoc.slotId);
    EXPECT_EQ(nicRes->affinityDevices_[0].chipId, npuLoc.chipId);
}

TEST_F(TestUbseNpuControllerProcess, DeviceNicVfeToResourceSuccess)
{
    CollectDeviceLoc nicLoc;
    nicLoc.slotId = 1;
    nicLoc.chipId = 2;
    nicLoc.pfeId = 3;
    nicLoc.vfeId = 4;
    nicLoc.guid = "nic-guid-1234567890123456789012345";

    auto nic = std::make_shared<CollectionDeviceNicVfe>(nicLoc);
    auto nicRes = std::make_shared<NicVfeResource>();

    UbseResult result = UbseNpuControllerProcess::DeviceNicVfeToResource(nic, nicRes);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(nicRes->slotId_, nicLoc.slotId);
    EXPECT_EQ(nicRes->chipId_, nicLoc.chipId);
    EXPECT_EQ(nicRes->pfId_, nicLoc.pfeId);
    EXPECT_EQ(nicRes->vfId_, nicLoc.vfeId);
    EXPECT_EQ(nicRes->guid_, nicLoc.guid);
    EXPECT_EQ(nicRes->busInstanceGuid_, "");
    EXPECT_EQ(nicRes->affinityDevices_.size(), 0);
}

TEST_F(TestUbseNpuControllerProcess, DeviceNicVfeToResourceWithAffinityNpu)
{
    CollectDeviceLoc nicLoc;
    nicLoc.slotId = 1;
    nicLoc.chipId = 2;
    nicLoc.pfeId = 3;
    nicLoc.vfeId = 4;
    nicLoc.guid = "nic-guid-1234567890123456789012345";

    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 5;
    npuLoc.chipId = 6;

    auto nic = std::make_shared<CollectionDeviceNicVfe>(nicLoc);
    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    auto nicRes = std::make_shared<NicVfeResource>();

    nic->SetAffinityDevDavid(npu);

    UbseResult result = UbseNpuControllerProcess::DeviceNicVfeToResource(nic, nicRes);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(nicRes->slotId_, nicLoc.slotId);
    EXPECT_EQ(nicRes->chipId_, nicLoc.chipId);
    EXPECT_EQ(nicRes->pfId_, nicLoc.pfeId);
    EXPECT_EQ(nicRes->vfId_, nicLoc.vfeId);
    EXPECT_EQ(nicRes->guid_, nicLoc.guid);
    EXPECT_EQ(nicRes->busInstanceGuid_, "");
    EXPECT_EQ(nicRes->affinityDevices_.size(), 1);
    EXPECT_EQ(nicRes->affinityDevices_[0].type, ResourceType::NPU);
    EXPECT_EQ(nicRes->affinityDevices_[0].slotId, npuLoc.slotId);
    EXPECT_EQ(nicRes->affinityDevices_[0].chipId, npuLoc.chipId);
}

TEST_F(TestUbseNpuControllerProcess, BusInstanceToResourceSuccess)
{
    CollectDeviceLoc busiLoc;
    busiLoc.guid = "busi-guid-12345678901234567890123456";
    busiLoc.upi = "1";

    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::HOST_BUSINSTANCE);
    auto busRes = std::make_shared<BusiResource>();

    UbseResult result = UbseNpuControllerProcess::BusInstanceToResource(busi, busRes);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(busRes->guid_, busiLoc.guid);
    EXPECT_EQ(busRes->subDevices_.size(), 0);
}

TEST_F(TestUbseNpuControllerProcess, BusInstanceToResourceWithNicPfe)
{
    CollectDeviceLoc busiLoc;
    busiLoc.guid = "busi-guid-12345678901234567890123456";
    busiLoc.upi = "1";

    CollectDeviceLoc nicPfeLoc;
    nicPfeLoc.slotId = 1;
    nicPfeLoc.chipId = 2;
    nicPfeLoc.pfeId = 3;
    nicPfeLoc.guid = "nic-guid-1234567890123456789012345";

    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::HOST_BUSINSTANCE);
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(nicPfeLoc);
    auto busRes = std::make_shared<BusiResource>();

    busi->SetSubDevNicPfe(nicPfe);

    UbseResult result = UbseNpuControllerProcess::BusInstanceToResource(busi, busRes);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(busRes->guid_, busiLoc.guid);
    EXPECT_EQ(busRes->subDevices_.size(), 1);
    EXPECT_EQ(busRes->subDevices_[0].type, ResourceType::NIC_PFE);
    EXPECT_EQ(busRes->subDevices_[0].slotId, nicPfeLoc.slotId);
    EXPECT_EQ(busRes->subDevices_[0].chipId, nicPfeLoc.chipId);
    EXPECT_EQ(busRes->subDevices_[0].pfId, nicPfeLoc.pfeId);
}

TEST_F(TestUbseNpuControllerProcess, BusInstanceToResourceWithNicVfe)
{
    CollectDeviceLoc busiLoc;
    busiLoc.guid = "busi-guid-12345678901234567890123456";
    busiLoc.upi = "1";

    CollectDeviceLoc nicVfeLoc;
    nicVfeLoc.slotId = 1;
    nicVfeLoc.chipId = 2;
    nicVfeLoc.pfeId = 3;
    nicVfeLoc.vfeId = 4;
    nicVfeLoc.guid = "nic-guid-1234567890123456789012345";

    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::HOST_BUSINSTANCE);
    auto nicVfe = std::make_shared<CollectionDeviceNicVfe>(nicVfeLoc);
    auto busRes = std::make_shared<BusiResource>();

    busi->SetSubDevNicVfe(nicVfe);

    UbseResult result = UbseNpuControllerProcess::BusInstanceToResource(busi, busRes);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(busRes->guid_, busiLoc.guid);
    EXPECT_EQ(busRes->subDevices_.size(), 1);
    EXPECT_EQ(busRes->subDevices_[0].type, ResourceType::NIC_VFE);
    EXPECT_EQ(busRes->subDevices_[0].slotId, nicVfeLoc.slotId);
    EXPECT_EQ(busRes->subDevices_[0].chipId, nicVfeLoc.chipId);
    EXPECT_EQ(busRes->subDevices_[0].pfId, nicVfeLoc.pfeId);
    EXPECT_EQ(busRes->subDevices_[0].vfId, nicVfeLoc.vfeId);
}

TEST_F(TestUbseNpuControllerProcess, BusInstanceToResourceWithIdevVfe)
{
    CollectDeviceLoc busiLoc;
    busiLoc.guid = "busi-guid-12345678901234567890123456";
    busiLoc.upi = "1";

    CollectDeviceLoc idevVfeLoc;
    idevVfeLoc.chipId = 2;
    idevVfeLoc.dieId = 0;
    idevVfeLoc.pfeId = 1;
    idevVfeLoc.vfeId = 1;
    idevVfeLoc.guid = "idev-guid-1234567890123456789012345";

    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;
    npuLoc.dieId = 0;

    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    auto idevVfe = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLoc);
    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    auto busRes = std::make_shared<BusiResource>();

    busi->SetSubDevIdev(idevVfe);
    idevVfe->SetBondingDevDavid(npu);

    UbseResult result = UbseNpuControllerProcess::BusInstanceToResource(busi, busRes);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(busRes->guid_, busiLoc.guid);
    EXPECT_EQ(busRes->subDevices_.size(), 1);
    EXPECT_EQ(busRes->subDevices_[0].type, ResourceType::NPU);
    EXPECT_EQ(busRes->subDevices_[0].slotId, npuLoc.slotId);
    EXPECT_EQ(busRes->subDevices_[0].chipId, npuLoc.chipId);
    EXPECT_EQ(busRes->subDevices_[0].dieId, npuLoc.dieId);
}

TEST_F(TestUbseNpuControllerProcess, BusInstanceToResourceWithComSharedVfe)
{
    CollectDeviceLoc busiLoc;
    busiLoc.guid = "busi-guid-12345678901234567890123456";
    busiLoc.upi = "1";

    CollectDeviceLoc idevVfeLoc;
    idevVfeLoc.chipId = 2;
    idevVfeLoc.dieId = 0;
    idevVfeLoc.pfeId = 1;
    idevVfeLoc.vfeId = 1;
    idevVfeLoc.guid = "idev-guid-1234567890123456789012345";

    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    auto idevVfe = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLoc);
    auto busRes = std::make_shared<BusiResource>();

    busi->SetSubDevIdev(idevVfe);
    idevVfe->SetIsComSharedFe(true);

    UbseResult result = UbseNpuControllerProcess::BusInstanceToResource(busi, busRes);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(busRes->guid_, busiLoc.guid);
    EXPECT_EQ(busRes->subDevices_.size(), 0);
}

TEST_F(TestUbseNpuControllerProcess, BusInstanceToResourceWithNullVfe)
{
    CollectDeviceLoc busiLoc;
    busiLoc.guid = "busi-guid-12345678901234567890123456";
    busiLoc.upi = "1";

    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    auto busRes = std::make_shared<BusiResource>();

    UbseResult result = UbseNpuControllerProcess::BusInstanceToResource(busi, busRes);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(busRes->guid_, busiLoc.guid);
    EXPECT_EQ(busRes->subDevices_.size(), 0);
}

TEST_F(TestUbseNpuControllerProcess, SetNicPfeLocationSuccess)
{
    CollectDeviceLoc nicLoc;
    nicLoc.slotId = 1;
    nicLoc.chipId = 2;
    nicLoc.pfeId = 3;
    nicLoc.guid = "nic-guid-1234567890123456789012345";

    auto nic = std::make_shared<CollectionDeviceNicPfe>(nicLoc);
    auto nicRes = std::make_shared<NicPfeResource>();

    UbseNpuControllerProcess::SetNicPfeLocation(nic, nicRes);
    EXPECT_EQ(nicRes->slotId_, nicLoc.slotId);
    EXPECT_EQ(nicRes->chipId_, nicLoc.chipId);
    EXPECT_EQ(nicRes->pfId_, nicLoc.pfeId);
}

TEST_F(TestUbseNpuControllerProcess, SetNicVfeLocationSuccess)
{
    CollectDeviceLoc nicLoc;
    nicLoc.slotId = 1;
    nicLoc.chipId = 2;
    nicLoc.pfeId = 3;
    nicLoc.vfeId = 4;
    nicLoc.guid = "nic-guid-1234567890123456789012345";

    auto nic = std::make_shared<CollectionDeviceNicVfe>(nicLoc);
    auto nicRes = std::make_shared<NicVfeResource>();

    UbseNpuControllerProcess::SetNicVfeLocation(nic, nicRes);
    EXPECT_EQ(nicRes->slotId_, nicLoc.slotId);
    EXPECT_EQ(nicRes->chipId_, nicLoc.chipId);
    EXPECT_EQ(nicRes->pfId_, nicLoc.pfeId);
    EXPECT_EQ(nicRes->vfId_, nicLoc.vfeId);
}

TEST_F(TestUbseNpuControllerProcess, DeviceNpuToResourceWithVfeIdevNoBusi)
{
    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;

    CollectDeviceLoc idevVfeLoc;
    idevVfeLoc.chipId = 2;
    idevVfeLoc.dieId = 0;
    idevVfeLoc.pfeId = 1;
    idevVfeLoc.vfeId = 1;
    idevVfeLoc.guid = "0123456789abcdef0123456789abcdef";

    CollectDeviceLoc idevPfeLoc;
    idevPfeLoc.chipId = 2;
    idevPfeLoc.dieId = 0;
    idevPfeLoc.pfeId = 1;

    CollectDeviceLoc ubctlLoc;
    ubctlLoc.chipId = 2;
    ubctlLoc.dieId = 0;

    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    auto idevVfe = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLoc);
    auto idevPfe = std::make_shared<CollectionDeviceIdevPfe>(idevPfeLoc);
    auto ubctl = std::make_shared<CollectionDeviceUbCtrl>(ubctlLoc);
    auto npuRes = std::make_shared<NpuResource>();

    npu->SetBondingIdev(CollectionDevice::CollectionToBase(idevVfe));
    idevVfe->SetParentPfe(idevPfe);
    idevPfe->SetParentUbCtl(ubctl);

    UbseResult result = UbseNpuControllerProcess::DeviceNpuToResource(npu, npuRes);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseNpuControllerProcess, DeviceNpuToResourceWithAffinityNicPfeAndNicVfe)
{
    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;

    CollectDeviceLoc idevVfeLoc;
    idevVfeLoc.chipId = 2;
    idevVfeLoc.dieId = 0;
    idevVfeLoc.pfeId = 1;
    idevVfeLoc.vfeId = 1;
    idevVfeLoc.guid = "0123456789abcdef0123456789abcdef";

    CollectDeviceLoc idevPfeLoc;
    idevPfeLoc.chipId = 2;
    idevPfeLoc.dieId = 0;
    idevPfeLoc.pfeId = 1;

    CollectDeviceLoc busiLoc;
    busiLoc.guid = "busi-guid-12345678901234567890123456";
    busiLoc.upi = "1";

    CollectDeviceLoc ubctlLoc;
    ubctlLoc.chipId = 2;
    ubctlLoc.dieId = 0;

    CollectDeviceLoc nicPfeLoc;
    nicPfeLoc.slotId = 5;
    nicPfeLoc.chipId = 6;
    nicPfeLoc.pfeId = 3;

    CollectDeviceLoc nicVfeLoc;
    nicVfeLoc.slotId = 7;
    nicVfeLoc.chipId = 8;
    nicVfeLoc.pfeId = 4;
    nicVfeLoc.vfeId = 1;

    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    auto idevVfe = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLoc);
    auto idevPfe = std::make_shared<CollectionDeviceIdevPfe>(idevPfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    auto ubctl = std::make_shared<CollectionDeviceUbCtrl>(ubctlLoc);
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(nicPfeLoc);
    auto nicVfe = std::make_shared<CollectionDeviceNicVfe>(nicVfeLoc);
    auto npuRes = std::make_shared<NpuResource>();

    npu->SetBondingIdev(CollectionDevice::CollectionToBase(idevVfe));
    idevPfe->SetSubDevIdev(idevVfe);
    idevVfe->SetParentPfe(idevPfe);
    idevVfe->SetBondingDevBusi(busi);
    idevPfe->SetParentUbCtl(ubctl);
    npu->SetAffinityDevNicPfe(nicPfe);
    npu->SetAffinityDevNicVfe(nicVfe);

    UbseResult result = UbseNpuControllerProcess::DeviceNpuToResource(npu, npuRes);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(npuRes->slotId_, npuLoc.slotId);
    EXPECT_EQ(npuRes->chipId_, npuLoc.chipId);
    EXPECT_EQ(npuRes->busInstanceGuid_, busiLoc.guid);
    EXPECT_EQ(npuRes->guid_, idevVfeLoc.guid);
    EXPECT_EQ(npuRes->affinityDevices_.size(), 3);
    EXPECT_EQ(npuRes->affinityDevices_[0].type, ResourceType::UBCONTROLLER);
    EXPECT_EQ(npuRes->affinityDevices_[1].type, ResourceType::NIC_PFE);
    EXPECT_EQ(npuRes->affinityDevices_[1].slotId, nicPfeLoc.slotId);
    EXPECT_EQ(npuRes->affinityDevices_[1].chipId, nicPfeLoc.chipId);
    EXPECT_EQ(npuRes->affinityDevices_[1].pfId, nicPfeLoc.pfeId);
    EXPECT_EQ(npuRes->affinityDevices_[2].type, ResourceType::NIC_VFE);
    EXPECT_EQ(npuRes->affinityDevices_[2].slotId, nicVfeLoc.slotId);
    EXPECT_EQ(npuRes->affinityDevices_[2].chipId, nicVfeLoc.chipId);
    EXPECT_EQ(npuRes->affinityDevices_[2].pfId, nicVfeLoc.pfeId);
    EXPECT_EQ(npuRes->affinityDevices_[2].vfId, nicVfeLoc.vfeId);
}

TEST_F(TestUbseNpuControllerProcess, DeviceNpuToResourceWithOnlyAffinityNicPfe)
{
    CollectDeviceLoc npuLoc;
    npuLoc.slotId = 1;
    npuLoc.chipId = 2;

    CollectDeviceLoc idevPfeLoc;
    idevPfeLoc.chipId = 2;
    idevPfeLoc.dieId = 0;
    idevPfeLoc.pfeId = 1;
    idevPfeLoc.guid = "0123456789abcdef0123456789abcdef";

    CollectDeviceLoc ubctlLoc;
    ubctlLoc.chipId = 2;
    ubctlLoc.dieId = 0;

    CollectDeviceLoc nicPfeLoc;
    nicPfeLoc.slotId = 5;
    nicPfeLoc.chipId = 6;
    nicPfeLoc.pfeId = 3;

    auto npu = std::make_shared<CollectionDeviceDavid>(npuLoc);
    auto idevPfe = std::make_shared<CollectionDeviceIdevPfe>(idevPfeLoc);
    auto ubctl = std::make_shared<CollectionDeviceUbCtrl>(ubctlLoc);
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(nicPfeLoc);
    auto npuRes = std::make_shared<NpuResource>();

    npu->SetBondingIdev(CollectionDevice::CollectionToBase(idevPfe));
    idevPfe->SetParentUbCtl(ubctl);
    npu->SetAffinityDevNicPfe(nicPfe);

    UbseResult result = UbseNpuControllerProcess::DeviceNpuToResource(npu, npuRes);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(npuRes->affinityDevices_.size(), 2);
    EXPECT_EQ(npuRes->affinityDevices_[0].type, ResourceType::UBCONTROLLER);
    EXPECT_EQ(npuRes->affinityDevices_[1].type, ResourceType::NIC_PFE);
    EXPECT_EQ(npuRes->affinityDevices_[1].slotId, nicPfeLoc.slotId);
    EXPECT_EQ(npuRes->affinityDevices_[1].chipId, nicPfeLoc.chipId);
    EXPECT_EQ(npuRes->affinityDevices_[1].pfId, nicPfeLoc.pfeId);
}

TEST_F(TestUbseNpuControllerProcess, BusInstanceToResourceWithIdevVfeNoBondingDavid)
{
    CollectDeviceLoc busiLoc;
    busiLoc.guid = "busi-guid-12345678901234567890123456";
    busiLoc.upi = "1";

    CollectDeviceLoc idevVfeLoc;
    idevVfeLoc.chipId = 2;
    idevVfeLoc.dieId = 0;
    idevVfeLoc.pfeId = 1;
    idevVfeLoc.vfeId = 1;
    idevVfeLoc.guid = "idev-guid-1234567890123456789012345";

    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    auto idevVfe = std::make_shared<CollectionDeviceIdevVfe>(idevVfeLoc);
    auto busRes = std::make_shared<BusiResource>();

    busi->SetSubDevIdev(idevVfe);

    UbseResult result = UbseNpuControllerProcess::BusInstanceToResource(busi, busRes);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(busRes->guid_, busiLoc.guid);
    EXPECT_EQ(busRes->subDevices_.size(), 0);
}

TEST_F(TestUbseNpuControllerProcess, DeviceNicPfeToResourceWithBusi)
{
    CollectDeviceLoc nicLoc;
    nicLoc.slotId = 1;
    nicLoc.chipId = 2;
    nicLoc.pfeId = 3;
    nicLoc.guid = "nic-guid-1234567890123456789012345";

    CollectDeviceLoc busiLoc;
    busiLoc.guid = "busi-guid-12345678901234567890123456";
    busiLoc.upi = "1";

    auto nic = std::make_shared<CollectionDeviceNicPfe>(nicLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::HOST_BUSINSTANCE);
    auto nicRes = std::make_shared<NicPfeResource>();

    nic->SetBondingDevBusi(busi);

    UbseResult result = UbseNpuControllerProcess::DeviceNicPfeToResource(nic, nicRes);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(nicRes->slotId_, nicLoc.slotId);
    EXPECT_EQ(nicRes->chipId_, nicLoc.chipId);
    EXPECT_EQ(nicRes->pfId_, nicLoc.pfeId);
    EXPECT_EQ(nicRes->guid_, nicLoc.guid);
    EXPECT_EQ(nicRes->busInstanceGuid_, busiLoc.guid);
    EXPECT_EQ(nicRes->affinityDevices_.size(), 0);
}

TEST_F(TestUbseNpuControllerProcess, DeviceNicVfeToResourceWithBusi)
{
    CollectDeviceLoc nicLoc;
    nicLoc.slotId = 1;
    nicLoc.chipId = 2;
    nicLoc.pfeId = 3;
    nicLoc.vfeId = 4;
    nicLoc.guid = "nic-guid-1234567890123456789012345";

    CollectDeviceLoc busiLoc;
    busiLoc.guid = "busi-guid-12345678901234567890123456";
    busiLoc.upi = "1";

    auto nic = std::make_shared<CollectionDeviceNicVfe>(nicLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::HOST_BUSINSTANCE);
    auto nicRes = std::make_shared<NicVfeResource>();

    nic->SetBondingDevBusi(busi);

    UbseResult result = UbseNpuControllerProcess::DeviceNicVfeToResource(nic, nicRes);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(nicRes->slotId_, nicLoc.slotId);
    EXPECT_EQ(nicRes->chipId_, nicLoc.chipId);
    EXPECT_EQ(nicRes->pfId_, nicLoc.pfeId);
    EXPECT_EQ(nicRes->vfId_, nicLoc.vfeId);
    EXPECT_EQ(nicRes->guid_, nicLoc.guid);
    EXPECT_EQ(nicRes->busInstanceGuid_, busiLoc.guid);
    EXPECT_EQ(nicRes->affinityDevices_.size(), 0);
}

} // namespace ubse::npu::controller::ut