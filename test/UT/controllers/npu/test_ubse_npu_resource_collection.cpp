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

} // namespace ubse::npu::controller::ut
