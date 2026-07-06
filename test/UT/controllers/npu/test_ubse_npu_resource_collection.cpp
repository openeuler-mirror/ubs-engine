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

TEST_F(TestUbseNpuResourceCollection, RemoveDeviceEmptyVmBusiNullTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    std::shared_ptr<CollectionDevice> nullDev;
    EXPECT_EQ(rc.RemoveDeviceEmptyVmBusi(nullDev), UBSE_ERROR_INVAL);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, RemoveDeviceEmptyVmBusiNotBusiTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    CollectDeviceLoc loc;
    loc.chipId = 1;
    loc.dieId = 2;
    auto dev = std::make_shared<CollectionDeviceUbCtrl>(loc);
    auto device = CollectionDevice::CollectionToBase(dev);
    EXPECT_EQ(rc.RemoveDeviceEmptyVmBusi(device), UBSE_ERROR_INVAL);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, RemoveDeviceEmptyVmBusiSubNicPfeNotEmptyTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    UbseMtiEid eid{};
    CollectDeviceLoc nicPfeLoc;
    nicPfeLoc.slotId = 1;
    nicPfeLoc.chipId = 2;
    nicPfeLoc.pfeId = 3;
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(nicPfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>("vmbusi1234567890abcdef012345678", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    busi->SetSubDevNicPfe(nicPfe);
    auto device = CollectionDevice::CollectionToBase(busi);
    EXPECT_EQ(rc.RemoveDeviceEmptyVmBusi(device), UBSE_ERROR);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, RemoveDeviceEmptyVmBusiSubNicVfeNotEmptyTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    UbseMtiEid eid{};
    CollectDeviceLoc nicVfeLoc;
    nicVfeLoc.slotId = 1;
    nicVfeLoc.chipId = 2;
    nicVfeLoc.pfeId = 3;
    nicVfeLoc.vfeId = 4;
    auto nicVfe = std::make_shared<CollectionDeviceNicVfe>(nicVfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>("vmbusi1234567890abcdef012345678", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    busi->SetSubDevNicVfe(nicVfe);
    auto device = CollectionDevice::CollectionToBase(busi);
    EXPECT_EQ(rc.RemoveDeviceEmptyVmBusi(device), UBSE_ERROR);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, RemoveDeviceEmptyVmBusiSubIdevNotEmptyTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    UbseMtiEid eid{};
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 1;
    vfeLoc.dieId = 2;
    vfeLoc.pfeId = 3;
    vfeLoc.vfeId = 4;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    auto busi = std::make_shared<CollectionDeviceBusi>("vmbusi1234567890abcdef012345678", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    busi->SetSubDevIdev(vfe);
    auto device = CollectionDevice::CollectionToBase(busi);
    EXPECT_EQ(rc.RemoveDeviceEmptyVmBusi(device), UBSE_ERROR);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, RemoveDeviceEmptyVmBusiSuccessTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    UbseMtiEid eid{};
    auto busi = std::make_shared<CollectionDeviceBusi>("vmbusi1234567890abcdef012345678", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    auto device = CollectionDevice::CollectionToBase(busi);
    rc.SetDevice(device);
    EXPECT_EQ(rc.RemoveDeviceEmptyVmBusi(device), UBSE_OK);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, RemoveDeviceEmptyVmBusiNotInMapTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    UbseMtiEid eid{};
    auto busi = std::make_shared<CollectionDeviceBusi>("0123456789abcdef0123456789abcdef", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    auto device = CollectionDevice::CollectionToBase(busi);
    EXPECT_EQ(rc.RemoveDeviceEmptyVmBusi(device), UBSE_ERROR);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, ConstructUbCtrlObjectTest)
{
    UbseMtiUbController ubController;
    ubController.chipId = 1;
    ubController.dieId = 2;
    auto dev = ConstructUbCtrlObject(ubController);
    EXPECT_NE(dev, nullptr);
    EXPECT_EQ(dev->GetType(), CollectionDeviceType::UBCONTROLLER);
    EXPECT_EQ(dev->GetDeviceLoc().chipId, 1);
    EXPECT_EQ(dev->GetDeviceLoc().dieId, 2);
}

TEST_F(TestUbseNpuResourceCollection, ConstructIdevPfeTest)
{
    UbseMtiIdevPfe idevPfe;
    idevPfe.ubController.chipId = 1;
    idevPfe.ubController.dieId = 2;
    idevPfe.pfeId = 3;
    idevPfe.guid[0] = 0xAB;
    auto dev = ConstructIdevPfe(idevPfe);
    EXPECT_NE(dev, nullptr);
    EXPECT_EQ(dev->GetType(), CollectionDeviceType::P_IDEV);
    EXPECT_EQ(dev->GetDeviceLoc().chipId, 1);
    EXPECT_EQ(dev->GetDeviceLoc().dieId, 2);
}

TEST_F(TestUbseNpuResourceCollection, ConstructIdevVfeTest)
{
    UbseMtiIdevVfe idevVfe;
    idevVfe.ubController.chipId = 1;
    idevVfe.ubController.dieId = 2;
    idevVfe.pfeId = 3;
    idevVfe.vfeId = 4;
    idevVfe.guid[0] = 0xCD;
    auto dev = ConstructIdevVfe(idevVfe);
    EXPECT_NE(dev, nullptr);
    EXPECT_EQ(dev->GetType(), CollectionDeviceType::V_IDEV);
    EXPECT_EQ(dev->GetDeviceLoc().chipId, 1);
    EXPECT_EQ(dev->GetDeviceLoc().dieId, 2);
    EXPECT_EQ(dev->GetDeviceLoc().vfeId, 4);
}

TEST_F(TestUbseNpuResourceCollection, GetDavidPfeLocTest)
{
    UbseMtiDavid david;
    david.slotId = 1;
    david.chipId = 2;
    UbseMtiIdevPfe idevPfe;
    idevPfe.ubController.chipId = 3;
    idevPfe.ubController.dieId = 4;
    idevPfe.pfeId = 5;
    CollectDeviceLoc davidDevLoc;
    CollectDeviceLoc pfeDevLoc;
    GetDavidPfeLoc(david, idevPfe, davidDevLoc, pfeDevLoc);
    EXPECT_EQ(davidDevLoc.slotId, 1);
    EXPECT_EQ(davidDevLoc.chipId, 2);
    EXPECT_EQ(pfeDevLoc.chipId, 3);
    EXPECT_EQ(pfeDevLoc.dieId, 4);
    EXPECT_EQ(pfeDevLoc.pfeId, 5);
}

TEST_F(TestUbseNpuResourceCollection, GetNicPfeLocTest)
{
    UbseMti1825Pf mti1825Pf;
    mti1825Pf.slotId = 1;
    mti1825Pf.chipId = 2;
    mti1825Pf.dieId = 3;
    mti1825Pf.pfId = 4;
    mti1825Pf.guid[0] = 0xAA;
    CollectDeviceLoc nicFeLoc;
    GetNicPfeLoc(mti1825Pf, nicFeLoc);
    EXPECT_EQ(nicFeLoc.slotId, 1);
    EXPECT_EQ(nicFeLoc.chipId, 2);
    EXPECT_EQ(nicFeLoc.dieId, 3);
    EXPECT_EQ(nicFeLoc.pfeId, 4);
}

TEST_F(TestUbseNpuResourceCollection, GetNicVfeLocTest)
{
    UbseMti1825Vf mti1825Vf;
    mti1825Vf.slotId = 1;
    mti1825Vf.chipId = 2;
    mti1825Vf.dieId = 3;
    mti1825Vf.pfId = 4;
    mti1825Vf.vfId = 5;
    mti1825Vf.guid[0] = 0xBB;
    CollectDeviceLoc nicFeLoc;
    GetNicVfeLoc(mti1825Vf, nicFeLoc);
    EXPECT_EQ(nicFeLoc.slotId, 1);
    EXPECT_EQ(nicFeLoc.chipId, 2);
    EXPECT_EQ(nicFeLoc.dieId, 3);
    EXPECT_EQ(nicFeLoc.pfeId, 4);
    EXPECT_EQ(nicFeLoc.vfeId, 5);
}

TEST_F(TestUbseNpuResourceCollection, AddDevIdevPfeTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    CollectDeviceLoc ubCtrlLoc;
    ubCtrlLoc.chipId = 1;
    ubCtrlLoc.dieId = 2;
    auto ubCtrl = std::make_shared<CollectionDeviceUbCtrl>(ubCtrlLoc);
    CollectDeviceLoc pfeLoc;
    pfeLoc.chipId = 1;
    pfeLoc.dieId = 2;
    pfeLoc.pfeId = 3;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(pfeLoc);
    EXPECT_EQ(rc.AddDevIdevPfe(ubCtrl, pfe), UBSE_OK);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, AddDevIdevVfeTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    CollectDeviceLoc pfeLoc;
    pfeLoc.chipId = 1;
    pfeLoc.dieId = 2;
    pfeLoc.pfeId = 3;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(pfeLoc);
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 1;
    vfeLoc.dieId = 2;
    vfeLoc.pfeId = 3;
    vfeLoc.vfeId = 4;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    EXPECT_EQ(rc.AddDevIdevVfe(pfe, vfe), UBSE_OK);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, CollectStaticResourceAlreadyStartedTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    rc.state_ = CollectionState::RUNNING;
    EXPECT_EQ(rc.CollectStaticResource(), UBSE_ERROR);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, SplitLinesWhitespaceTest)
{
    auto lines = ResourceCollection::GetInstance().SplitLines("  line1  \n  line2  \n\n  line3  ");
    EXPECT_EQ(lines.size(), 3);
    EXPECT_EQ(lines[0], "line1");
    EXPECT_EQ(lines[1], "line2");
    EXPECT_EQ(lines[2], "line3");
}

TEST_F(TestUbseNpuResourceCollection, GetIdevVfeByGuidNotFoundTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    EXPECT_EQ(rc.GetIdevVfeByGuid("0123456789abcdef0123456789abcdef"), nullptr);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetIdevVfeByGuidFoundTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 1;
    vfeLoc.dieId = 2;
    vfeLoc.pfeId = 3;
    vfeLoc.vfeId = 4;
    vfeLoc.guid = "abcd1234567890abcdef01234567890";
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    auto device = CollectionDevice::CollectionToBase(vfe);
    rc.SetDevice(device);
    auto found = rc.GetIdevVfeByGuid("abcd1234567890abcdef01234567890");
    EXPECT_NE(found, nullptr);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, AddDavidAndBindToIdevPfeComSharedTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    CollectDeviceLoc pfeLoc;
    pfeLoc.chipId = 1;
    pfeLoc.dieId = 2;
    pfeLoc.pfeId = 3;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(pfeLoc);
    auto device = CollectionDevice::CollectionToBase(pfe);
    rc.SetDevice(device);
    CollectDeviceLoc davidDevLoc;
    davidDevLoc.slotId = SPECIAL_FE_VAL;
    davidDevLoc.chipId = SPECIAL_FE_VAL;
    CollectDeviceLoc pfeDevLoc;
    pfeDevLoc.chipId = 1;
    pfeDevLoc.dieId = 2;
    pfeDevLoc.pfeId = 3;
    EXPECT_EQ(rc.AddDavidAndBindToIdevPfe(davidDevLoc, pfeDevLoc), UBSE_OK);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, AddDavidAndBindToIdevPfeNormalTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    CollectDeviceLoc pfeLoc;
    pfeLoc.chipId = 1;
    pfeLoc.dieId = 2;
    pfeLoc.pfeId = 3;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(pfeLoc);
    auto device = CollectionDevice::CollectionToBase(pfe);
    rc.SetDevice(device);
    CollectDeviceLoc davidDevLoc;
    davidDevLoc.slotId = 4;
    davidDevLoc.chipId = 5;
    CollectDeviceLoc pfeDevLoc;
    pfeDevLoc.chipId = 1;
    pfeDevLoc.dieId = 2;
    pfeDevLoc.pfeId = 3;
    EXPECT_EQ(rc.AddDavidAndBindToIdevPfe(davidDevLoc, pfeDevLoc), UBSE_OK);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, AddDavidAndBindToIdevPfeNoPfeTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    CollectDeviceLoc davidDevLoc;
    davidDevLoc.slotId = 4;
    davidDevLoc.chipId = 5;
    CollectDeviceLoc pfeDevLoc;
    pfeDevLoc.chipId = 1;
    pfeDevLoc.dieId = 2;
    pfeDevLoc.pfeId = 3;
    EXPECT_EQ(rc.AddDavidAndBindToIdevPfe(davidDevLoc, pfeDevLoc), UBSE_ERROR);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetDavidSlotIdEmptyTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    uint8_t slotId = 0;
    EXPECT_EQ(rc.GetDavidSlotId(slotId), UBSE_ERROR);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetDavidSlotIdFoundTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    CollectDeviceLoc davidLoc;
    davidLoc.slotId = 4;
    davidLoc.chipId = 5;
    auto david = std::make_shared<CollectionDeviceDavid>(davidLoc);
    auto device = CollectionDevice::CollectionToBase(david);
    rc.SetDevice(device);
    uint8_t slotId = 0;
    EXPECT_EQ(rc.GetDavidSlotId(slotId), UBSE_OK);
    EXPECT_EQ(slotId, 4);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, ResourceCollectionConstructorTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    EXPECT_EQ(rc.devIdToDevice_.size(), static_cast<uint32_t>(CollectionDeviceType::COLLECTION_DEVICE_TYPE_COUNT));
    EXPECT_EQ(rc.state_, CollectionState::WAIT_INIT);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetProductTypeExecFailedTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    const auto execFunc = &ubse::utils::UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().will(returnValue(UBSE_ERROR));
    ProductType productType;
    EXPECT_EQ(rc.GetProductType(productType), UBSE_ERROR);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GenerateDavidNicMapServerTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    CollectDeviceLoc davidLoc;
    davidLoc.slotId = 1;
    davidLoc.chipId = 2;
    auto david = std::make_shared<CollectionDeviceDavid>(davidLoc);
    auto device = CollectionDevice::CollectionToBase(david);
    rc.SetDevice(device);
    CollectionDavidDevIdTo1825DevId davidDevIdTo1825DevId;
    EXPECT_EQ(rc.GenerateDavidNicMap(ProductType::SERVER, davidDevIdTo1825DevId), UBSE_OK);
    EXPECT_FALSE(davidDevIdTo1825DevId.empty());
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, SetDavidAffinityNicNoDavidTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    CollectionDavidDevIdTo1825DevId davidDevIdTo1825DevId;
    EXPECT_EQ(rc.SetDavidAffinityNic(davidDevIdTo1825DevId), UBSE_OK);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, SetDavidAffinityNicWithDevicesTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    CollectDeviceLoc davidLoc;
    davidLoc.slotId = 1;
    davidLoc.chipId = 1;
    auto david = std::make_shared<CollectionDeviceDavid>(davidLoc);
    auto davidDev = CollectionDevice::CollectionToBase(david);
    rc.SetDevice(davidDev);

    CollectDeviceLoc nicPfeLoc;
    nicPfeLoc.slotId = 1;
    nicPfeLoc.chipId = 11;
    nicPfeLoc.pfeId = 3;
    nicPfeLoc.guid = "0123456789abcdef0123456789abcdef";
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(nicPfeLoc);
    auto nicDev = CollectionDevice::CollectionToBase(nicPfe);
    rc.SetDevice(nicDev);

    CollectionDavidDevIdTo1825DevId davidDevIdTo1825DevId;
    davidDevIdTo1825DevId[david->GetIdStr()] = nicPfe->GetIdStr();
    EXPECT_EQ(rc.SetDavidAffinityNic(davidDevIdTo1825DevId), UBSE_OK);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, BindVfeToNpuNoBusiTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    EXPECT_EQ(rc.BindVfeToNpu(), UBSE_OK);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, ManageBusiAndNicPfeBindingHostBusiFallbackTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc nicPfeLoc;
    nicPfeLoc.slotId = 1;
    nicPfeLoc.chipId = 2;
    nicPfeLoc.pfeId = 3;
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(nicPfeLoc);
    auto hostBusi = std::make_shared<CollectionDeviceBusi>("hostbusiguid1234567890abcdef0123456", eid, "0",
                                                           CollectionDeviceType::HOST_BUSINSTANCE);
    EXPECT_EQ(ManageBusiAndNicPfeBinding(hostBusi, nicPfe, true), UBSE_OK);
    EXPECT_EQ(hostBusi->GetSubDevNicPfe().size(), 1);
}

TEST_F(TestUbseNpuResourceCollection, ManageBusiAndNicVfeBindingHostBusiFallbackTest)
{
    UbseMtiEid eid{};
    CollectDeviceLoc nicVfeLoc;
    nicVfeLoc.slotId = 1;
    nicVfeLoc.chipId = 2;
    nicVfeLoc.pfeId = 3;
    nicVfeLoc.vfeId = 4;
    auto nicVfe = std::make_shared<CollectionDeviceNicVfe>(nicVfeLoc);
    auto hostBusi = std::make_shared<CollectionDeviceBusi>("hostbusiguid1234567890abcdef0123456", eid, "0",
                                                           CollectionDeviceType::HOST_BUSINSTANCE);
    EXPECT_EQ(ManageBusiAndNicVfeBinding(hostBusi, nicVfe, true), UBSE_OK);
    EXPECT_EQ(hostBusi->GetSubDevNicVfe().size(), 1);
}

TEST_F(TestUbseNpuResourceCollection, ManageIdevAndDavidBindingPfeVfeTypeTest)
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
    EXPECT_NE(david->GetBondingIdev(), nullptr);
}

// === GetProductType tests ===

TEST_F(TestUbseNpuResourceCollection, GetProductTypeServerTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    std::string output = "00 00 00 00 00 00 00 00 00 00 ff";
    const auto execFunc = &ubse::utils::UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().with(_, outBound(output)).will(returnValue(UBSE_OK));
    ProductType productType;
    EXPECT_EQ(rc.GetProductType(productType), UBSE_OK);
    EXPECT_EQ(productType, ProductType::SERVER);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetProductTypePod16Test)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    std::string output = "00 00 00 00 00 00 00 00 00 00 00\n10 00";
    const auto execFunc = &ubse::utils::UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().with(_, outBound(output)).will(returnValue(UBSE_OK));
    ProductType productType;
    EXPECT_EQ(rc.GetProductType(productType), UBSE_OK);
    EXPECT_EQ(productType, ProductType::POD_16_1825);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetProductTypePod32Test)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    std::string output = "00 00 00 00 00 00 00 00 00 00 00\n20 00";
    const auto execFunc = &ubse::utils::UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().with(_, outBound(output)).will(returnValue(UBSE_OK));
    ProductType productType;
    EXPECT_EQ(rc.GetProductType(productType), UBSE_OK);
    EXPECT_EQ(productType, ProductType::POD_32_1825);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetProductTypeUnknownFieldTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    std::string output = "00 00 00 00 00 00 00 00 00 00 01";
    const auto execFunc = &ubse::utils::UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().with(_, outBound(output)).will(returnValue(UBSE_OK));
    ProductType productType;
    EXPECT_EQ(rc.GetProductType(productType), UBSE_ERROR);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetProductTypeEmptyLinesTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    std::string output = "";
    const auto execFunc = &ubse::utils::UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().with(_, outBound(output)).will(returnValue(UBSE_OK));
    ProductType productType;
    EXPECT_EQ(rc.GetProductType(productType), UBSE_ERROR);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetProductTypeNotEnoughFieldsTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    std::string output = "00 00 00";
    const auto execFunc = &ubse::utils::UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().with(_, outBound(output)).will(returnValue(UBSE_OK));
    ProductType productType;
    EXPECT_EQ(rc.GetProductType(productType), UBSE_ERROR);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, GetProductTypePodOnlyOneLineTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    std::string output = "00 00 00 00 00 00 00 00 00 00 00";
    const auto execFunc = &ubse::utils::UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().with(_, outBound(output)).will(returnValue(UBSE_OK));
    ProductType productType;
    EXPECT_EQ(rc.GetProductType(productType), UBSE_ERROR);
    rc.ClearAllDevices();
}

// === CollectUbCtrlIdev tests ===

TEST_F(TestUbseNpuResourceCollection, CollectUbCtrlIdevGetFeListFailTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    rc.state_ = CollectionState::WAIT_INIT;
    auto& urma = ubse::mti::urma::UbseMtiUrma::GetInstance();
    MOCKER_CPP_VIRTUAL(urma, &ubse::mti::urma::UbseMtiUrma::GetIdevFeList).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(rc.CollectUbCtrlIdev(), UBSE_ERROR);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, CollectUbCtrlIdevSuccessTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    rc.state_ = CollectionState::WAIT_INIT;
    UbseMtiIdevVfe idevVfe;
    idevVfe.ubController.chipId = 1;
    idevVfe.ubController.dieId = 2;
    idevVfe.pfeId = 3;
    idevVfe.vfeId = 4;
    UbseMtiGuid vfeGuid{};
    vfeGuid[0] = 0x01;
    idevVfe.guid = vfeGuid;
    UbseMtiIdevPfe idevPfe;
    idevPfe.ubController.chipId = 1;
    idevPfe.ubController.dieId = 2;
    idevPfe.pfeId = 3;
    UbseMtiGuid pfeGuid{};
    pfeGuid[0] = 0x02;
    idevPfe.guid = pfeGuid;
    idevPfe.vfeList.push_back(idevVfe);
    std::vector<UbseMtiIdevPfe> feList;
    feList.push_back(idevPfe);
    auto& urma = ubse::mti::urma::UbseMtiUrma::GetInstance();
    MOCKER_CPP_VIRTUAL(urma, &ubse::mti::urma::UbseMtiUrma::GetIdevFeList)
        .stubs()
        .with(outBound(feList))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(rc.CollectUbCtrlIdev(), UBSE_OK);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, CollectUbCtrlIdevSetUbCtrlFailTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    rc.state_ = CollectionState::WAIT_INIT;
    UbseMtiIdevPfe idevPfe;
    idevPfe.ubController.chipId = 1;
    idevPfe.ubController.dieId = 2;
    idevPfe.pfeId = 3;
    std::vector<UbseMtiIdevPfe> feList;
    feList.push_back(idevPfe);
    auto& urma = ubse::mti::urma::UbseMtiUrma::GetInstance();
    MOCKER_CPP_VIRTUAL(urma, &ubse::mti::urma::UbseMtiUrma::GetIdevFeList)
        .stubs()
        .with(outBound(feList))
        .will(returnValue(UBSE_OK));
    UbseMtiEid eid{};
    auto invalidDev =
        std::make_shared<CollectionDeviceBusi>("guid", eid, "100", CollectionDeviceType::COLLECTION_DEVICE_TYPE_COUNT);
    rc.devIdToDevice_[DeviceTypeToUint8(CollectionDeviceType::UBCONTROLLER)].clear();
    EXPECT_EQ(rc.CollectUbCtrlIdev(), UBSE_OK);
    rc.ClearAllDevices();
}

// === CollectIdevPfeDavid tests ===

TEST_F(TestUbseNpuResourceCollection, CollectIdevPfeDavidGetMappingFailTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    rc.state_ = CollectionState::WAIT_INIT;
    auto& urma = ubse::mti::urma::UbseMtiUrma::GetInstance();
    MOCKER_CPP_VIRTUAL(urma, &ubse::mti::urma::UbseMtiUrma::GetIdevFeDavidMapping)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(rc.CollectIdevPfeDavid(), UBSE_ERROR);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, CollectIdevPfeDavidSuccessTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    rc.state_ = CollectionState::WAIT_INIT;
    CollectDeviceLoc ubCtrlLoc;
    ubCtrlLoc.chipId = 1;
    ubCtrlLoc.dieId = 2;
    auto ubCtrl = std::make_shared<CollectionDeviceUbCtrl>(ubCtrlLoc);
    auto ubCtrlDev = CollectionDevice::CollectionToBase(ubCtrl);
    rc.SetDevice(ubCtrlDev);
    CollectDeviceLoc pfeLoc;
    pfeLoc.chipId = 1;
    pfeLoc.dieId = 2;
    pfeLoc.pfeId = 3;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(pfeLoc);
    auto pfeDev = CollectionDevice::CollectionToBase(pfe);
    rc.AddDevIdevPfe(ubCtrl, pfe);
    UbseMtiIdevFeDavidMapping mapping;
    UbseMtiDavid david;
    david.slotId = 4;
    david.chipId = 5;
    UbseMtiIdevPfe idevPfe;
    idevPfe.ubController.chipId = 1;
    idevPfe.ubController.dieId = 2;
    idevPfe.pfeId = 3;
    mapping[david] = idevPfe;
    auto& urma = ubse::mti::urma::UbseMtiUrma::GetInstance();
    MOCKER_CPP_VIRTUAL(urma, &ubse::mti::urma::UbseMtiUrma::GetIdevFeDavidMapping)
        .stubs()
        .with(outBound(mapping))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(rc.CollectIdevPfeDavid(), UBSE_OK);
    rc.ClearAllDevices();
}

// === AddNicFe tests ===

TEST_F(TestUbseNpuResourceCollection, AddNicFeSuccessWithVfeTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    rc.state_ = CollectionState::WAIT_INIT;
    UbseMti1825Pf mti1825Pf;
    mti1825Pf.slotId = 1;
    mti1825Pf.chipId = 2;
    mti1825Pf.dieId = 3;
    mti1825Pf.pfId = 4;
    UbseMtiGuid pfeGuid{};
    pfeGuid[0] = 0x0a;
    mti1825Pf.guid = pfeGuid;
    UbseMti1825Vf mti1825Vf;
    mti1825Vf.slotId = 1;
    mti1825Vf.chipId = 2;
    mti1825Vf.dieId = 3;
    mti1825Vf.pfId = 4;
    mti1825Vf.vfId = 5;
    UbseMtiGuid vfeGuid{};
    vfeGuid[0] = 0x0b;
    mti1825Vf.guid = vfeGuid;
    mti1825Pf.vfList.push_back(mti1825Vf);
    EXPECT_EQ(rc.AddNicFe(mti1825Pf), UBSE_OK);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, AddNicFeSuccessNoVfeTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    rc.state_ = CollectionState::WAIT_INIT;
    UbseMti1825Pf mti1825Pf;
    mti1825Pf.slotId = 1;
    mti1825Pf.chipId = 2;
    mti1825Pf.dieId = 3;
    mti1825Pf.pfId = 4;
    UbseMtiGuid pfeGuid{};
    pfeGuid[0] = 0x0c;
    mti1825Pf.guid = pfeGuid;
    EXPECT_EQ(rc.AddNicFe(mti1825Pf), UBSE_OK);
    rc.ClearAllDevices();
}

// === CollectNic tests ===

TEST_F(TestUbseNpuResourceCollection, CollectNicGetFeListFailTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    rc.state_ = CollectionState::WAIT_INIT;
    auto& mti1825 = ubse::mti::_1825::UbseMti1825::GetInstance();
    MOCKER_CPP_VIRTUAL(mti1825, &ubse::mti::_1825::UbseMti1825::Get1825FeList).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(rc.CollectNic(), UBSE_ERROR);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, CollectNicSuccessTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    rc.state_ = CollectionState::WAIT_INIT;
    UbseMti1825Pf mti1825Pf;
    mti1825Pf.slotId = 1;
    mti1825Pf.chipId = 2;
    mti1825Pf.dieId = 3;
    mti1825Pf.pfId = 4;
    UbseMtiGuid pfeGuid{};
    pfeGuid[0] = 0x0d;
    mti1825Pf.guid = pfeGuid;
    std::vector<UbseMti1825Pf> pfList;
    pfList.push_back(mti1825Pf);
    auto& mti1825 = ubse::mti::_1825::UbseMti1825::GetInstance();
    MOCKER_CPP_VIRTUAL(mti1825, &ubse::mti::_1825::UbseMti1825::Get1825FeList)
        .stubs()
        .with(outBound(pfList))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(rc.CollectNic(), UBSE_OK);
    rc.ClearAllDevices();
}

TEST_F(TestUbseNpuResourceCollection, CollectNicAddNicFeFailTest)
{
    auto& rc = ResourceCollection::GetInstance();
    rc.ClearAllDevices();
    rc.state_ = CollectionState::WAIT_INIT;
    UbseMti1825Pf mti1825Pf;
    mti1825Pf.slotId = 1;
    mti1825Pf.chipId = 2;
    mti1825Pf.dieId = 3;
    mti1825Pf.pfId = 4;
    UbseMtiGuid pfeGuid{};
    pfeGuid[0] = 0x0e;
    mti1825Pf.guid = pfeGuid;
    std::vector<UbseMti1825Pf> pfList;
    pfList.push_back(mti1825Pf);
    auto& mti1825 = ubse::mti::_1825::UbseMti1825::GetInstance();
    MOCKER_CPP_VIRTUAL(mti1825, &ubse::mti::_1825::UbseMti1825::Get1825FeList)
        .stubs()
        .with(outBound(pfList))
        .will(returnValue(UBSE_OK));
    UbseMtiEid eid{};
    auto existNicPfe = std::make_shared<CollectionDeviceNicPfe>(CollectDeviceLoc{});
    auto existNicPfeBase = CollectionDevice::CollectionToBase(existNicPfe);
    rc.devIdToDevice_[DeviceTypeToUint8(CollectionDeviceType::NIC_PFE)][existNicPfeBase->GetIdStr()] = existNicPfeBase;
    EXPECT_EQ(rc.CollectNic(), UBSE_OK);
    rc.ClearAllDevices();
}

} // namespace ubse::npu::controller::ut
