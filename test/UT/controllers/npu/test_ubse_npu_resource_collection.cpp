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

#include "test_ubse_npu_resource_collection.h"
#include "ubse_error.h"
#include "ubse_npu_resource_collection.cpp"

namespace ubse::npu::controller::ut {

void TestUbseNpuResourceCollection::SetUp() {}

void TestUbseNpuResourceCollection::TearDown()
{
    GlobalMockObject::verify();
}

TEST_F(TestUbseNpuResourceCollection, ValidateGuidValidTest)
{
    EXPECT_TRUE(ValidateGuid("0123456789abcdef0123456789abcdef"));
}

TEST_F(TestUbseNpuResourceCollection, ValidateGuidWithDashesValidTest)
{
    EXPECT_TRUE(ValidateGuid("0123456789abcdef0123456789-abcde"));
}

TEST_F(TestUbseNpuResourceCollection, ValidateGuidInvalidLengthTest)
{
    EXPECT_FALSE(ValidateGuid("0123456789abcdef"));
    EXPECT_FALSE(ValidateGuid(""));
}

TEST_F(TestUbseNpuResourceCollection, ValidateGuidInvalidCharsTest)
{
    EXPECT_FALSE(ValidateGuid("0123456789abcdef0123456789abcdex"));
}

TEST_F(TestUbseNpuResourceCollection, IsBusInstanceTypeSharedPtrTest)
{
    UbseMtiEid eid{};
    auto busi = std::make_shared<CollectionDeviceBusi>("guid", eid, "100", CollectionDeviceType::VM_BUSINSTANCE);
    EXPECT_TRUE(IsBusInstanceType(busi));

    CollectDeviceLoc loc;
    loc.chipId = 1;
    loc.dieId = 2;
    loc.pfeId = 3;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(loc);
    EXPECT_FALSE(IsBusInstanceType(pfe));
}

TEST_F(TestUbseNpuResourceCollection, IsIdevFeTypeSharedPtrTest)
{
    CollectDeviceLoc loc;
    loc.chipId = 1;
    loc.dieId = 2;
    loc.pfeId = 3;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(loc);
    EXPECT_TRUE(IsIdevFeType(pfe));

    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 1;
    vfeLoc.dieId = 2;
    vfeLoc.pfeId = 3;
    vfeLoc.vfeId = 4;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    EXPECT_TRUE(IsIdevFeType(vfe));

    UbseMtiEid eid{};
    auto busi = std::make_shared<CollectionDeviceBusi>("guid", eid, "100", CollectionDeviceType::VM_BUSINSTANCE);
    EXPECT_FALSE(IsIdevFeType(busi));
}

TEST_F(TestUbseNpuResourceCollection, GetBindDevTypeBusiVfeTest)
{
    UbseMtiEid eid{};
    auto busi = std::make_shared<CollectionDeviceBusi>("guid", eid, "100", CollectionDeviceType::VM_BUSINSTANCE);
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 1;
    vfeLoc.dieId = 2;
    vfeLoc.pfeId = 3;
    vfeLoc.vfeId = 4;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    EXPECT_EQ(GetBindDevType(busi, vfe), BindDevType::BUSI_VFE);
    EXPECT_EQ(GetBindDevType(vfe, busi), BindDevType::BUSI_VFE);
}

TEST_F(TestUbseNpuResourceCollection, GetBindDevTypeBusiNicPfeTest)
{
    UbseMtiEid eid{};
    auto busi = std::make_shared<CollectionDeviceBusi>("guid", eid, "100", CollectionDeviceType::VM_BUSINSTANCE);
    CollectDeviceLoc nicPfeLoc;
    nicPfeLoc.slotId = 1;
    nicPfeLoc.chipId = 2;
    nicPfeLoc.pfeId = 3;
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(nicPfeLoc);
    EXPECT_EQ(GetBindDevType(busi, nicPfe), BindDevType::BUSI_NIC_PFE);
}

TEST_F(TestUbseNpuResourceCollection, GetBindDevTypeBusiNicVfeTest)
{
    UbseMtiEid eid{};
    auto busi = std::make_shared<CollectionDeviceBusi>("guid", eid, "100", CollectionDeviceType::VM_BUSINSTANCE);
    CollectDeviceLoc nicVfeLoc;
    nicVfeLoc.slotId = 1;
    nicVfeLoc.chipId = 2;
    nicVfeLoc.pfeId = 3;
    nicVfeLoc.vfeId = 4;
    auto nicVfe = std::make_shared<CollectionDeviceNicVfe>(nicVfeLoc);
    EXPECT_EQ(GetBindDevType(busi, nicVfe), BindDevType::BUSI_NIC_VFE);
}

TEST_F(TestUbseNpuResourceCollection, GetBindDevTypeIdevDavidTest)
{
    CollectDeviceLoc pfeLoc;
    pfeLoc.chipId = 1;
    pfeLoc.dieId = 2;
    pfeLoc.pfeId = 3;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(pfeLoc);
    CollectDeviceLoc davidLoc;
    davidLoc.slotId = 4;
    davidLoc.chipId = 5;
    auto david = std::make_shared<CollectionDeviceDavid>(davidLoc);
    EXPECT_EQ(GetBindDevType(pfe, david), BindDevType::IDEV_DAVID);
}

TEST_F(TestUbseNpuResourceCollection, GetBindDevTypeErrorTypeTest)
{
    UbseMtiEid eid{};
    auto busi1 = std::make_shared<CollectionDeviceBusi>("guid1", eid, "100", CollectionDeviceType::VM_BUSINSTANCE);
    auto busi2 = std::make_shared<CollectionDeviceBusi>("guid2", eid, "200", CollectionDeviceType::HOST_BUSINSTANCE);
    EXPECT_EQ(GetBindDevType(busi1, busi2), BindDevType::ERROR_TYPE);
}

TEST_F(TestUbseNpuResourceCollection, BindDeviceNullDev1Test)
{
    UbseMtiEid eid{};
    auto busi = std::make_shared<CollectionDeviceBusi>("guid", eid, "100", CollectionDeviceType::VM_BUSINSTANCE);
    EXPECT_EQ(ResourceCollection::BindDevice(nullptr, busi), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNpuResourceCollection, BindDeviceNullDev2Test)
{
    UbseMtiEid eid{};
    auto busi = std::make_shared<CollectionDeviceBusi>("guid", eid, "100", CollectionDeviceType::VM_BUSINSTANCE);
    EXPECT_EQ(ResourceCollection::BindDevice(busi, nullptr), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNpuResourceCollection, BindDeviceBothNullTest)
{
    EXPECT_EQ(ResourceCollection::BindDevice(nullptr, nullptr), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNpuResourceCollection, BindDeviceErrorTypeTest)
{
    UbseMtiEid eid{};
    auto busi1 = std::make_shared<CollectionDeviceBusi>("guid1", eid, "100", CollectionDeviceType::VM_BUSINSTANCE);
    auto busi2 = std::make_shared<CollectionDeviceBusi>("guid2", eid, "200", CollectionDeviceType::HOST_BUSINSTANCE);
    EXPECT_EQ(ResourceCollection::BindDevice(busi1, busi2), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNpuResourceCollection, UnbindDeviceNullDev1Test)
{
    UbseMtiEid eid{};
    auto busi = std::make_shared<CollectionDeviceBusi>("guid", eid, "100", CollectionDeviceType::VM_BUSINSTANCE);
    EXPECT_EQ(ResourceCollection::UnbindDevice(nullptr, busi), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNpuResourceCollection, UnbindDeviceNullDev2Test)
{
    UbseMtiEid eid{};
    auto busi = std::make_shared<CollectionDeviceBusi>("guid", eid, "100", CollectionDeviceType::VM_BUSINSTANCE);
    EXPECT_EQ(ResourceCollection::UnbindDevice(busi, nullptr), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNpuResourceCollection, UnbindDeviceErrorTypeTest)
{
    UbseMtiEid eid{};
    auto busi1 = std::make_shared<CollectionDeviceBusi>("guid1", eid, "100", CollectionDeviceType::VM_BUSINSTANCE);
    auto busi2 = std::make_shared<CollectionDeviceBusi>("guid2", eid, "200", CollectionDeviceType::HOST_BUSINSTANCE);
    EXPECT_EQ(ResourceCollection::UnbindDevice(busi1, busi2), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNpuResourceCollection, GetDevicesByTypeInvalidTypeTest)
{
    auto& rc = ResourceCollection::GetInstance();
    CollectionDevIdToDevice devices;
    EXPECT_EQ(rc.GetDevicesByType(CollectionDeviceType::COLLECTION_DEVICE_TYPE_COUNT, devices), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNpuResourceCollection, SplitLinesTest)
{
    auto lines = ResourceCollection::GetInstance().SplitLines("line1\nline2\nline3");
    EXPECT_EQ(lines.size(), 3);
    EXPECT_EQ(lines[0], "line1");
    EXPECT_EQ(lines[1], "line2");
    EXPECT_EQ(lines[2], "line3");
}

TEST_F(TestUbseNpuResourceCollection, SplitLinesEmptyTest)
{
    auto lines = ResourceCollection::GetInstance().SplitLines("");
    EXPECT_TRUE(lines.empty());
}

TEST_F(TestUbseNpuResourceCollection, SplitFieldsTest)
{
    std::vector<std::string> input = {"aa bb cc dd"};
    auto fields = ResourceCollection::GetInstance().SplitFields(input);
    EXPECT_EQ(fields.size(), 4);
    EXPECT_EQ(fields[0], "aa");
    EXPECT_EQ(fields[1], "bb");
    EXPECT_EQ(fields[2], "cc");
    EXPECT_EQ(fields[3], "dd");
}

TEST_F(TestUbseNpuResourceCollection, SplitFieldsEmptyTest)
{
    std::vector<std::string> input;
    auto fields = ResourceCollection::GetInstance().SplitFields(input);
    EXPECT_TRUE(fields.empty());
}

TEST_F(TestUbseNpuResourceCollection, IsBusInstanceTypeEnumTest)
{
    EXPECT_TRUE(IsBusInstanceType(CollectionDeviceType::VM_BUSINSTANCE));
    EXPECT_TRUE(IsBusInstanceType(CollectionDeviceType::HOST_BUSINSTANCE));
    EXPECT_FALSE(IsBusInstanceType(CollectionDeviceType::NPU));
}

TEST_F(TestUbseNpuResourceCollection, ResourceCollectionValidateDeviceNull)
{
    auto& rc = ResourceCollection::GetInstance();
    std::shared_ptr<CollectionDevice> nullDev;
    auto device = nullDev;
    EXPECT_EQ(rc.SetDevice(device), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNpuResourceCollection, SetDeviceWithDevIdExistingDeviceTest)
{
    UbseMtiEid eid{};
    auto dev1 = std::make_shared<CollectionDeviceBusi>("testguid1234567890abcdef01234567", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    std::shared_ptr<CollectionDevice> device1 = CollectionDevice::CollectionToBase(dev1);
    CollectionDevIdToDevice devIdToDeviceMap;
    devIdToDeviceMap[device1->GetIdStr()] = device1;
    auto dev2 = std::make_shared<CollectionDeviceBusi>("testguid1234567890abcdef01234567", eid, "200",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    std::shared_ptr<CollectionDevice> device2 = CollectionDevice::CollectionToBase(dev2);
    EXPECT_EQ(SetDeviceWithDevId(device2, devIdToDeviceMap), UBSE_OK);
    EXPECT_EQ(device2.get(), device1.get());
}

TEST_F(TestUbseNpuResourceCollection, SetDeviceWithDevIdNewDeviceTest)
{
    UbseMtiEid eid{};
    auto dev = std::make_shared<CollectionDeviceBusi>("newguid1234567890abcdef012345678", eid, "100",
                                                      CollectionDeviceType::VM_BUSINSTANCE);
    std::shared_ptr<CollectionDevice> device = CollectionDevice::CollectionToBase(dev);
    CollectionDevIdToDevice devIdToDeviceMap;
    EXPECT_EQ(SetDeviceWithDevId(device, devIdToDeviceMap), UBSE_OK);
    EXPECT_NE(devIdToDeviceMap.find(device->GetIdStr()), devIdToDeviceMap.end());
}

TEST_F(TestUbseNpuResourceCollection, SetDeviceWithDevIdInvalidTypeTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc loc;
    loc.guid = "guid1234567890abcdef012345678901";
    auto dev = std::make_shared<CollectionDeviceBusi>("guid1234567890abcdef012345678901", eid, "100",
                                                      CollectionDeviceType::COLLECTION_DEVICE_TYPE_COUNT);
    std::shared_ptr<CollectionDevice> device = CollectionDevice::CollectionToBase(dev);
    CollectionDevIdToDevice devIdToDeviceMap;
    EXPECT_EQ(SetDeviceWithDevId(device, devIdToDeviceMap), UBSE_ERROR);
}

TEST_F(TestUbseNpuResourceCollection, SetDeviceWithGuidInvalidGuidTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc loc;
    loc.guid = "shortguid";
    auto dev = std::make_shared<CollectionDeviceNicPfe>(loc);
    std::shared_ptr<CollectionDevice> device = CollectionDevice::CollectionToBase(dev);
    CollectionGuidToDevice guidToDevice;
    EXPECT_EQ(SetDeviceWithGuid(device, guidToDevice), UBSE_ERROR);
}

TEST_F(TestUbseNpuResourceCollection, SetDeviceWithGuidInvalidTypeTest)
{
    CollectDeviceLoc loc;
    loc.guid = "0123456789abcdef0123456789abcdef";
    auto dev = std::make_shared<CollectionDeviceUbCtrl>(loc);
    std::shared_ptr<CollectionDevice> device = CollectionDevice::CollectionToBase(dev);
    CollectionGuidToDevice guidToDevice;
    EXPECT_EQ(SetDeviceWithGuid(device, guidToDevice), UBSE_OK);
    EXPECT_TRUE(guidToDevice.empty());
}

TEST_F(TestUbseNpuResourceCollection, SetDeviceWithGuidExistingDeviceTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc loc;
    loc.guid = "0123456789abcdef0123456789abcdef";
    auto dev1 = std::make_shared<CollectionDeviceNicPfe>(loc);
    std::shared_ptr<CollectionDevice> device1 = CollectionDevice::CollectionToBase(dev1);
    CollectionGuidToDevice guidToDevice;
    guidToDevice[device1->GetGuid()] = device1;
    auto dev2 = std::make_shared<CollectionDeviceNicPfe>(loc);
    std::shared_ptr<CollectionDevice> device2 = CollectionDevice::CollectionToBase(dev2);
    EXPECT_EQ(SetDeviceWithGuid(device2, guidToDevice), UBSE_OK);
}

TEST_F(TestUbseNpuResourceCollection, SetDeviceWithGuidNewDeviceTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc loc;
    loc.guid = "0123456789abcdef0123456789abcdef";
    auto dev = std::make_shared<CollectionDeviceNicPfe>(loc);
    std::shared_ptr<CollectionDevice> device = CollectionDevice::CollectionToBase(dev);
    CollectionGuidToDevice guidToDevice;
    EXPECT_EQ(SetDeviceWithGuid(device, guidToDevice), UBSE_OK);
    EXPECT_NE(guidToDevice.find(device->GetGuid()), guidToDevice.end());
}

TEST_F(TestUbseNpuResourceCollection, SetDeviceWithGuidSkipTypeTest)
{
    CollectDeviceLoc loc;
    loc.guid = "0123456789abcdef0123456789abcdef";
    auto devPfe = std::make_shared<CollectionDeviceIdevPfe>(loc);
    std::shared_ptr<CollectionDevice> device = CollectionDevice::CollectionToBase(devPfe);
    CollectionGuidToDevice guidToDevice;
    EXPECT_EQ(SetDeviceWithGuid(device, guidToDevice), UBSE_OK);
    EXPECT_TRUE(guidToDevice.empty());
}

TEST_F(TestUbseNpuResourceCollection, ValidateDeviceInvalidTypeTest)
{
    auto& rc = ResourceCollection::GetInstance();
    UbseMtiEid eid{};
    CollectDeviceLoc loc;
    loc.guid = "guid1234567890abcdef012345678901";
    auto dev = std::make_shared<CollectionDeviceBusi>("guid1234567890abcdef012345678901", eid, "100",
                                                      CollectionDeviceType::COLLECTION_DEVICE_TYPE_COUNT);
    auto device = CollectionDevice::CollectionToBase(dev);
    EXPECT_EQ(rc.ValidateDevice(device), UBSE_ERROR);
}

TEST_F(TestUbseNpuResourceCollection, SetDevicePfeSkipGuidTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    CollectDeviceLoc loc;
    loc.chipId = 1;
    loc.dieId = 2;
    loc.pfeId = 3;
    auto dev = std::make_shared<CollectionDeviceIdevPfe>(loc);
    auto device = CollectionDevice::CollectionToBase(dev);
    EXPECT_EQ(rc.SetDevice(device), UBSE_OK);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, SetDeviceVfeSkipGuidTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    CollectDeviceLoc loc;
    loc.chipId = 1;
    loc.dieId = 2;
    loc.pfeId = 3;
    loc.vfeId = 4;
    auto dev = std::make_shared<CollectionDeviceIdevVfe>(loc);
    auto device = CollectionDevice::CollectionToBase(dev);
    EXPECT_EQ(rc.SetDevice(device), UBSE_OK);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, SetDeviceValidGuidTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    UbseMtiEid eid{};
    CollectDeviceLoc loc;
    loc.guid = "0123456789abcdef0123456789abcdef";
    auto dev = std::make_shared<CollectionDeviceNicPfe>(loc);
    auto device = CollectionDevice::CollectionToBase(dev);
    EXPECT_EQ(rc.SetDevice(device), UBSE_OK);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, SetDeviceWithGuidFailedTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    UbseMtiEid eid{};
    CollectDeviceLoc loc;
    loc.guid = "shortguid";
    auto dev = std::make_shared<CollectionDeviceNicPfe>(loc);
    auto device = CollectionDevice::CollectionToBase(dev);
    EXPECT_EQ(rc.SetDevice(device), UBSE_OK);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, ClearAllDevicesTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    UbseMtiEid eid{};
    CollectDeviceLoc loc;
    loc.guid = "0123456789abcdef0123456789abcdef";
    auto dev = std::make_shared<CollectionDeviceNicPfe>(loc);
    auto device = CollectionDevice::CollectionToBase(dev);
    rc.SetDevice(device);
    rc.ClearAllDevices();
    CollectionDevIdToDevice devices;
    EXPECT_EQ(rc.GetDevicesByType(CollectionDeviceType::NIC_PFE, devices), UBSE_OK);
    EXPECT_TRUE(devices.empty());
}

TEST_F(TestUbseNpuResourceCollection, GetSpecifyTypeDevFromTwoDevTest)
{
    UbseMtiEid eid{};
    auto busi = std::make_shared<CollectionDeviceBusi>("guid1234567890abcdef0123456789012", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 1;
    vfeLoc.dieId = 2;
    vfeLoc.pfeId = 3;
    vfeLoc.vfeId = 4;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);

    auto result = GetSpecifyTypeDevFromTwoDev(busi, vfe, CollectionDeviceType::VM_BUSINSTANCE);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->GetType(), CollectionDeviceType::VM_BUSINSTANCE);

    result = GetSpecifyTypeDevFromTwoDev(busi, vfe, CollectionDeviceType::V_IDEV);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->GetType(), CollectionDeviceType::V_IDEV);

    result = GetSpecifyTypeDevFromTwoDev(busi, vfe, CollectionDeviceType::NPU);
    EXPECT_EQ(result, nullptr);
}

TEST_F(TestUbseNpuResourceCollection, RemovePreviousBondingBusiFromVfeEmptyTest)
{
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 1;
    vfeLoc.dieId = 2;
    vfeLoc.pfeId = 3;
    vfeLoc.vfeId = 4;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    EXPECT_EQ(RemovePreviousBondingBusiFromVfe(vfe), UBSE_OK);
}

TEST_F(TestUbseNpuResourceCollection, RemovePreviousBondingBusiFromVfeSingleBusiTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 1;
    vfeLoc.dieId = 2;
    vfeLoc.pfeId = 3;
    vfeLoc.vfeId = 4;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234567", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    vfe->SetBondingDevBusi(busi);
    EXPECT_EQ(RemovePreviousBondingBusiFromVfe(vfe), UBSE_OK);
    EXPECT_TRUE(busi->GetSubDevIdev().empty());
}

TEST_F(TestUbseNpuResourceCollection, RemovePreviousBondingBusiFromVfeNullBusiTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 1;
    vfeLoc.dieId = 2;
    vfeLoc.pfeId = 3;
    vfeLoc.vfeId = 4;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234567", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    vfe->SetBondingDevBusi(busi);
    busi.reset();
    auto bondingBusiVec = vfe->GetBondingDevBusi();
    if (!bondingBusiVec.empty() && bondingBusiVec[0] == nullptr) {
        EXPECT_EQ(RemovePreviousBondingBusiFromVfe(vfe), UBSE_ERROR);
    }
}

TEST_F(TestUbseNpuResourceCollection, RemovePreviousBondingBusiFromVfeTooManyBusiTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 1;
    vfeLoc.dieId = 2;
    vfeLoc.pfeId = 3;
    vfeLoc.vfeId = 4;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    auto busi1 = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234561", eid, "100",
                                                        CollectionDeviceType::VM_BUSINSTANCE);
    auto busi2 = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234562", eid, "200",
                                                        CollectionDeviceType::VM_BUSINSTANCE);
    vfe->SetBondingDevBusi(busi1);
    vfe->SetBondingDevBusi(busi2);
    EXPECT_EQ(RemovePreviousBondingBusiFromVfe(vfe), UBSE_ERROR);
}

TEST_F(TestUbseNpuResourceCollection, ManageBusiAndVfeBindingTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 1;
    vfeLoc.dieId = 2;
    vfeLoc.pfeId = 3;
    vfeLoc.vfeId = 4;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234567", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    EXPECT_EQ(ManageBusiAndVfeBinding(busi, vfe, true), UBSE_OK);
    EXPECT_EQ(busi->GetSubDevIdev().size(), 1);
    EXPECT_EQ(vfe->GetBondingDevBusi().size(), 1);

    EXPECT_EQ(ManageBusiAndVfeBinding(busi, vfe, false), UBSE_OK);
    EXPECT_TRUE(busi->GetSubDevIdev().empty());
}

TEST_F(TestUbseNpuResourceCollection, ManageBusiAndVfeBindingNotSharedTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 1;
    vfeLoc.dieId = 2;
    vfeLoc.pfeId = 3;
    vfeLoc.vfeId = 4;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    auto busi1 = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234561", eid, "100",
                                                        CollectionDeviceType::VM_BUSINSTANCE);
    auto busi2 = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234562", eid, "200",
                                                        CollectionDeviceType::VM_BUSINSTANCE);
    vfe->SetBondingDevBusi(busi1);
    EXPECT_EQ(ManageBusiAndVfeBinding(busi2, vfe, true), UBSE_OK);
    EXPECT_EQ(busi1->GetSubDevIdev().size(), 0);
}

TEST_F(TestUbseNpuResourceCollection, ManageBusiAndNicPfeBindingTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc nicPfeLoc;
    nicPfeLoc.slotId = 1;
    nicPfeLoc.chipId = 2;
    nicPfeLoc.pfeId = 3;
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(nicPfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234567", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    EXPECT_EQ(ManageBusiAndNicPfeBinding(busi, nicPfe, true), UBSE_OK);
    EXPECT_EQ(busi->GetSubDevNicPfe().size(), 1);

    EXPECT_EQ(ManageBusiAndNicPfeBinding(busi, nicPfe, false), UBSE_OK);
    EXPECT_TRUE(busi->GetSubDevNicPfe().empty());
}

TEST_F(TestUbseNpuResourceCollection, ManageBusiAndNicPfeBindingWithPreviousBusiTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc nicPfeLoc;
    nicPfeLoc.slotId = 1;
    nicPfeLoc.chipId = 2;
    nicPfeLoc.pfeId = 3;
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(nicPfeLoc);
    auto busi1 = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234561", eid, "100",
                                                        CollectionDeviceType::VM_BUSINSTANCE);
    auto busi2 = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234562", eid, "200",
                                                        CollectionDeviceType::VM_BUSINSTANCE);
    nicPfe->SetBondingDevBusi(busi1);
    EXPECT_EQ(ManageBusiAndNicPfeBinding(busi2, nicPfe, true), UBSE_OK);
    EXPECT_TRUE(busi1->GetSubDevNicPfe().empty());
    EXPECT_EQ(busi2->GetSubDevNicPfe().size(), 1);
}

TEST_F(TestUbseNpuResourceCollection, ManageBusiAndNicPfeBindingHostBusiTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc nicPfeLoc;
    nicPfeLoc.slotId = 1;
    nicPfeLoc.chipId = 2;
    nicPfeLoc.pfeId = 3;
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(nicPfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234567", eid, "100",
                                                       CollectionDeviceType::HOST_BUSINSTANCE);
    EXPECT_EQ(ManageBusiAndNicPfeBinding(busi, nicPfe, true), UBSE_OK);
    EXPECT_EQ(busi->GetSubDevNicPfe().size(), 1);
}

TEST_F(TestUbseNpuResourceCollection, ManageBusiAndNicVfeBindingTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc nicVfeLoc;
    nicVfeLoc.slotId = 1;
    nicVfeLoc.chipId = 2;
    nicVfeLoc.pfeId = 3;
    nicVfeLoc.vfeId = 4;
    auto nicVfe = std::make_shared<CollectionDeviceNicVfe>(nicVfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234567", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    EXPECT_EQ(ManageBusiAndNicVfeBinding(busi, nicVfe, true), UBSE_OK);
    EXPECT_EQ(busi->GetSubDevNicVfe().size(), 1);

    EXPECT_EQ(ManageBusiAndNicVfeBinding(busi, nicVfe, false), UBSE_OK);
    EXPECT_TRUE(busi->GetSubDevNicVfe().empty());
}

TEST_F(TestUbseNpuResourceCollection, ManageBusiAndNicVfeBindingWithPreviousBusiTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc nicVfeLoc;
    nicVfeLoc.slotId = 1;
    nicVfeLoc.chipId = 2;
    nicVfeLoc.pfeId = 3;
    nicVfeLoc.vfeId = 4;
    auto nicVfe = std::make_shared<CollectionDeviceNicVfe>(nicVfeLoc);
    auto busi1 = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234561", eid, "100",
                                                        CollectionDeviceType::VM_BUSINSTANCE);
    auto busi2 = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234562", eid, "200",
                                                        CollectionDeviceType::VM_BUSINSTANCE);
    nicVfe->SetBondingDevBusi(busi1);
    EXPECT_EQ(ManageBusiAndNicVfeBinding(busi2, nicVfe, true), UBSE_OK);
    EXPECT_TRUE(busi1->GetSubDevNicVfe().empty());
}

TEST_F(TestUbseNpuResourceCollection, ManageIdevAndDavidBindingTest)
{
    CollectDeviceLoc pfeLoc;
    pfeLoc.chipId = 1;
    pfeLoc.dieId = 2;
    pfeLoc.pfeId = 3;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(pfeLoc);
    CollectDeviceLoc davidLoc;
    davidLoc.slotId = 4;
    davidLoc.chipId = 5;
    auto david = std::make_shared<CollectionDeviceDavid>(davidLoc);

    EXPECT_EQ(ManageIdevAndDavidBinding(pfe, david, true), UBSE_OK);
    EXPECT_NE(pfe->GetBondingDevDavid(), nullptr);
    EXPECT_NE(david->GetBondingIdev(), nullptr);

    EXPECT_EQ(ManageIdevAndDavidBinding(pfe, david, false), UBSE_OK);
    EXPECT_EQ(pfe->GetBondingDevDavid(), nullptr);
    EXPECT_EQ(david->GetBondingIdev(), nullptr);
}

TEST_F(TestUbseNpuResourceCollection, ManageIdevAndDavidBindingWithPreviousBondingTest)
{
    CollectDeviceLoc pfeLoc;
    pfeLoc.chipId = 1;
    pfeLoc.dieId = 2;
    pfeLoc.pfeId = 3;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(pfeLoc);
    CollectDeviceLoc davidLoc1;
    davidLoc1.slotId = 4;
    davidLoc1.chipId = 5;
    auto david1 = std::make_shared<CollectionDeviceDavid>(davidLoc1);
    CollectDeviceLoc davidLoc2;
    davidLoc2.slotId = 6;
    davidLoc2.chipId = 7;
    auto david2 = std::make_shared<CollectionDeviceDavid>(davidLoc2);
    CollectDeviceLoc pfeLoc2;
    pfeLoc2.chipId = 8;
    pfeLoc2.dieId = 9;
    pfeLoc2.pfeId = 10;
    auto pfe2 = std::make_shared<CollectionDeviceIdevPfe>(pfeLoc2);

    pfe->SetBondingDevDavid(david1);
    david2->SetBondingIdev(CollectionDevice::CollectionToBase(pfe2));
    EXPECT_EQ(ManageIdevAndDavidBinding(pfe, david2, true), UBSE_OK);
    EXPECT_EQ(pfe->GetBondingDevDavid().get(), david2.get());
    EXPECT_EQ(david2->GetBondingIdev()->GetIdStr(), pfe->GetIdStr());
}

TEST_F(TestUbseNpuResourceCollection, ManageIdevAndDavidBindingVfeTypeTest)
{
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 1;
    vfeLoc.dieId = 2;
    vfeLoc.pfeId = 3;
    vfeLoc.vfeId = 4;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    CollectDeviceLoc davidLoc;
    davidLoc.slotId = 5;
    davidLoc.chipId = 6;
    auto david = std::make_shared<CollectionDeviceDavid>(davidLoc);

    EXPECT_EQ(ManageIdevAndDavidBinding(vfe, david, true), UBSE_OK);
    EXPECT_NE(vfe->GetBondingDevDavid(), nullptr);
    EXPECT_NE(david->GetBondingIdev(), nullptr);
}

TEST_F(TestUbseNpuResourceCollection, BindDeviceBusiVfeSuccessTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 1;
    vfeLoc.dieId = 2;
    vfeLoc.pfeId = 3;
    vfeLoc.vfeId = 4;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234567", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    EXPECT_EQ(ResourceCollection::BindDevice(busi, vfe), UBSE_OK);
}

TEST_F(TestUbseNpuResourceCollection, BindDeviceBusiNicPfeSuccessTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc nicPfeLoc;
    nicPfeLoc.slotId = 1;
    nicPfeLoc.chipId = 2;
    nicPfeLoc.pfeId = 3;
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(nicPfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234567", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    EXPECT_EQ(ResourceCollection::BindDevice(busi, nicPfe), UBSE_OK);
}

TEST_F(TestUbseNpuResourceCollection, BindDeviceBusiNicVfeSuccessTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc nicVfeLoc;
    nicVfeLoc.slotId = 1;
    nicVfeLoc.chipId = 2;
    nicVfeLoc.pfeId = 3;
    nicVfeLoc.vfeId = 4;
    auto nicVfe = std::make_shared<CollectionDeviceNicVfe>(nicVfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234567", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    EXPECT_EQ(ResourceCollection::BindDevice(busi, nicVfe), UBSE_OK);
}

TEST_F(TestUbseNpuResourceCollection, BindDeviceIdevDavidSuccessTest)
{
    CollectDeviceLoc pfeLoc;
    pfeLoc.chipId = 1;
    pfeLoc.dieId = 2;
    pfeLoc.pfeId = 3;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(pfeLoc);
    CollectDeviceLoc davidLoc;
    davidLoc.slotId = 4;
    davidLoc.chipId = 5;
    auto david = std::make_shared<CollectionDeviceDavid>(davidLoc);
    EXPECT_EQ(ResourceCollection::BindDevice(pfe, david), UBSE_OK);
}

TEST_F(TestUbseNpuResourceCollection, UnbindDeviceBusiVfeSuccessTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 1;
    vfeLoc.dieId = 2;
    vfeLoc.pfeId = 3;
    vfeLoc.vfeId = 4;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234567", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    ResourceCollection::BindDevice(busi, vfe);
    EXPECT_EQ(ResourceCollection::UnbindDevice(busi, vfe), UBSE_OK);
    EXPECT_TRUE(busi->GetSubDevIdev().empty());
}

TEST_F(TestUbseNpuResourceCollection, UnbindDeviceBusiNicPfeSuccessTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc nicPfeLoc;
    nicPfeLoc.slotId = 1;
    nicPfeLoc.chipId = 2;
    nicPfeLoc.pfeId = 3;
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(nicPfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234567", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    ResourceCollection::BindDevice(busi, nicPfe);
    EXPECT_EQ(ResourceCollection::UnbindDevice(busi, nicPfe), UBSE_OK);
    EXPECT_TRUE(busi->GetSubDevNicPfe().empty());
}

TEST_F(TestUbseNpuResourceCollection, UnbindDeviceBusiNicVfeSuccessTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc nicVfeLoc;
    nicVfeLoc.slotId = 1;
    nicVfeLoc.chipId = 2;
    nicVfeLoc.pfeId = 3;
    nicVfeLoc.vfeId = 4;
    auto nicVfe = std::make_shared<CollectionDeviceNicVfe>(nicVfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>("busiguid1234567890abcdef01234567", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    ResourceCollection::BindDevice(busi, nicVfe);
    EXPECT_EQ(ResourceCollection::UnbindDevice(busi, nicVfe), UBSE_OK);
    EXPECT_TRUE(busi->GetSubDevNicVfe().empty());
}

TEST_F(TestUbseNpuResourceCollection, UnbindDeviceIdevDavidSuccessTest)
{
    CollectDeviceLoc pfeLoc;
    pfeLoc.chipId = 1;
    pfeLoc.dieId = 2;
    pfeLoc.pfeId = 3;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(pfeLoc);
    CollectDeviceLoc davidLoc;
    davidLoc.slotId = 4;
    davidLoc.chipId = 5;
    auto david = std::make_shared<CollectionDeviceDavid>(davidLoc);
    ResourceCollection::BindDevice(pfe, david);
    EXPECT_EQ(ResourceCollection::UnbindDevice(pfe, david), UBSE_OK);
    EXPECT_EQ(pfe->GetBondingDevDavid(), nullptr);
    EXPECT_EQ(david->GetBondingIdev(), nullptr);
}

TEST_F(TestUbseNpuResourceCollection, GetDeviceByDevIdEmptyDevIdTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    EXPECT_EQ(rc.GetDeviceByDevId("", CollectionDeviceType::VM_BUSINSTANCE), nullptr);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetDeviceByDevIdInvalidTypeTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    EXPECT_EQ(rc.GetDeviceByDevId("someid", CollectionDeviceType::COLLECTION_DEVICE_TYPE_COUNT), nullptr);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetDeviceByDevIdDeviceNotFoundTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    EXPECT_EQ(rc.GetDeviceByDevId("nonexistent", CollectionDeviceType::VM_BUSINSTANCE), nullptr);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetDeviceByDevIdDeviceFoundTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    UbseMtiEid eid{};
    CollectDeviceLoc loc;
    loc.guid = "0123456789abcdef0123456789abcdef";
    auto dev = std::make_shared<CollectionDeviceNicPfe>(loc);
    auto device = CollectionDevice::CollectionToBase(dev);
    rc.SetDevice(device);
    auto found = rc.GetDeviceByDevId(dev->GetIdStr(), CollectionDeviceType::NIC_PFE);
    EXPECT_NE(found, nullptr);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetDeviceByGuidInvalidGuidTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    EXPECT_EQ(rc.GetDeviceByGuid("shortguid"), nullptr);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetDeviceByGuidDeviceNotFoundTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    EXPECT_EQ(rc.GetDeviceByGuid("0123456789abcdef0123456789abcdef"), nullptr);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetDeviceByGuidDeviceFoundTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    UbseMtiEid eid{};
    CollectDeviceLoc loc;
    loc.guid = "0123456789abcdef0123456789abcdef";
    auto dev = std::make_shared<CollectionDeviceNicPfe>(loc);
    auto device = CollectionDevice::CollectionToBase(dev);
    rc.SetDevice(device);
    auto found = rc.GetDeviceByGuid("0123456789abcdef0123456789abcdef");
    EXPECT_NE(found, nullptr);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetDevicesByTypeValidTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    UbseMtiEid eid{};
    CollectDeviceLoc loc;
    loc.guid = "0123456789abcdef0123456789abcdef";
    auto dev = std::make_shared<CollectionDeviceNicPfe>(loc);
    auto device = CollectionDevice::CollectionToBase(dev);
    rc.SetDevice(device);
    CollectionDevIdToDevice devices;
    EXPECT_EQ(rc.GetDevicesByType(CollectionDeviceType::NIC_PFE, devices), UBSE_OK);
    EXPECT_EQ(devices.size(), 1);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetDeviceHostBusInstanceEmptyTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    EXPECT_EQ(rc.GetDeviceHostBusInstance(), nullptr);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetDeviceHostBusInstanceFoundTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    UbseMtiEid eid{};
    CollectDeviceLoc loc;
    loc.guid = "hostbusi1234567890abcdef012345678";
    auto dev = std::make_shared<CollectionDeviceBusi>("hostbusi1234567890abcdef012345678", eid, "0",
                                                      CollectionDeviceType::HOST_BUSINSTANCE);
    auto device = CollectionDevice::CollectionToBase(dev);
    rc.SetDevice(device);
    auto hostBusi = rc.GetDeviceHostBusInstance();
    EXPECT_NE(hostBusi, nullptr);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetDeviceAllComSharedIdevVfeTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    auto result = rc.GetDeviceAllComSharedIdevVfe();
    EXPECT_TRUE(result.empty());
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetDeviceAllComSharedIdevVfeWithSharedVfeTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 1;
    vfeLoc.dieId = 2;
    vfeLoc.pfeId = 3;
    vfeLoc.vfeId = 4;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    vfe->SetIsComSharedFe(true);
    auto device = CollectionDevice::CollectionToBase(vfe);
    rc.SetDevice(device);
    auto result = rc.GetDeviceAllComSharedIdevVfe();
    EXPECT_EQ(result.size(), 1);
    rc.ClearAllDevices();
}

} // namespace ubse::npu::controller::ut
