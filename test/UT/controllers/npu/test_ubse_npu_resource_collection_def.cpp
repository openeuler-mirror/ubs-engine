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

#include "test_ubse_npu_resource_collection_def.h"
#include "ubse_error.h"

namespace ubse::npu::controller::ut {

void TestUbseNpuResourceCollectionDef::SetUp() {}

void TestUbseNpuResourceCollectionDef::TearDown()
{
    GlobalMockObject::verify();
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectDeviceLocDefaultConstructor)
{
    CollectDeviceLoc loc;
    EXPECT_EQ(loc.slotId, 0xff);
    EXPECT_EQ(loc.chipId, 0xff);
    EXPECT_EQ(loc.dieId, 0xff);
    EXPECT_EQ(loc.pfeId, 0xff);
    EXPECT_EQ(loc.vfeId, 0xff);
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectDeviceLocThreeArgConstructor)
{
    UbseMtiEid eid{};
    eid[0] = 1;
    CollectionGuid guid = "test-guid";
    CollectionUpi upi = "100";
    CollectDeviceLoc loc(eid, guid, upi);
    EXPECT_EQ(loc.slotId, 0xff);
    EXPECT_EQ(loc.chipId, 0xff);
    EXPECT_EQ(loc.eid[0], 1);
    EXPECT_EQ(loc.guid, "test-guid");
    EXPECT_EQ(loc.upi, "100");
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectionJoinStrSingleArg)
{
    auto result = CollectionStringUtil::CollectionJoinStr(static_cast<uint8_t>(5));
    EXPECT_EQ(result, "5");
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectionJoinStrMultipleArgs)
{
    auto result = CollectionStringUtil::CollectionJoinStr(static_cast<uint8_t>(6), static_cast<uint8_t>(1),
                                                          static_cast<uint8_t>(2));
    EXPECT_EQ(result, "6-1-2");
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectionJoinStrUint16Args)
{
    auto result = CollectionStringUtil::CollectionJoinStr(static_cast<uint16_t>(100));
    EXPECT_EQ(result, "100");
}

TEST_F(TestUbseNpuResourceCollectionDef, GuidToStrTest)
{
    UbseMtiGuid guid{};
    guid[0] = 0x01;
    guid[1] = 0x23;
    for (size_t i = 2; i < guid.size(); i++) {
        guid[i] = 0x00;
    }
    auto result = CollectionStringUtil::GuidToStr(guid);
    EXPECT_EQ(result.size(), 32);
    EXPECT_EQ(result.substr(30, 2), "01");
    EXPECT_EQ(result.substr(28, 2), "23");
}

TEST_F(TestUbseNpuResourceCollectionDef, GetDevIdByDevLocBusi)
{
    CollectDeviceLoc loc;
    loc.guid = "abc123";
    auto devId = CollectionDevice::GetDevIdByDevLoc(loc, CollectionDeviceType::VM_BUSINSTANCE);
    EXPECT_EQ(devId, "abc123");
}

TEST_F(TestUbseNpuResourceCollectionDef, GetDevIdByDevLocNpu)
{
    CollectDeviceLoc loc;
    loc.slotId = 1;
    loc.chipId = 2;
    auto devId = CollectionDevice::GetDevIdByDevLoc(loc, CollectionDeviceType::NPU);
    EXPECT_EQ(devId, "2-1-2");
}

TEST_F(TestUbseNpuResourceCollectionDef, GetDevIdByDevLocUbCtrl)
{
    CollectDeviceLoc loc;
    loc.chipId = 3;
    loc.dieId = 4;
    auto devId = CollectionDevice::GetDevIdByDevLoc(loc, CollectionDeviceType::UBCONTROLLER);
    EXPECT_EQ(devId, "5-3-4");
}

TEST_F(TestUbseNpuResourceCollectionDef, GetDevIdByDevLocIdevPfe)
{
    CollectDeviceLoc loc;
    loc.chipId = 1;
    loc.dieId = 2;
    loc.pfeId = 3;
    auto devId = CollectionDevice::GetDevIdByDevLoc(loc, CollectionDeviceType::P_IDEV);
    EXPECT_EQ(devId, "6-1-2-3");
}

TEST_F(TestUbseNpuResourceCollectionDef, GetDevIdByDevLocInvalidType)
{
    CollectDeviceLoc loc;
    auto devId = CollectionDevice::GetDevIdByDevLoc(loc, CollectionDeviceType::COLLECTION_DEVICE_TYPE_COUNT);
    EXPECT_EQ(devId, "");
}

TEST_F(TestUbseNpuResourceCollectionDef, IsValidHexStringValid)
{
    EXPECT_TRUE(IsValidHexString("0123456789abcdef"));
    EXPECT_TRUE(IsValidHexString("0123456789ABCDEF"));
}

TEST_F(TestUbseNpuResourceCollectionDef, IsValidHexStringWithDashes)
{
    EXPECT_TRUE(IsValidHexString("0123-4567-89ab-cdef", true));
}

TEST_F(TestUbseNpuResourceCollectionDef, IsValidHexStringInvalid)
{
    EXPECT_FALSE(IsValidHexString("0123xyz"));
    EXPECT_FALSE(IsValidHexString("0123-4567", false));
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectionDeviceUbCtrlTest)
{
    CollectDeviceLoc loc;
    loc.chipId = 1;
    loc.dieId = 2;
    auto dev = std::make_shared<CollectionDeviceUbCtrl>(loc);
    EXPECT_EQ(dev->GetType(), CollectionDeviceType::UBCONTROLLER);
    EXPECT_EQ(dev->GetIdStr(), "5-1-2");
    EXPECT_EQ(dev->GetDeviceLoc().chipId, 1);
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectionDeviceBusiTest)
{
    UbseMtiEid eid{};
    auto dev = std::make_shared<CollectionDeviceBusi>("testguid1234567890abcdef01234567", eid, "100",
                                                      CollectionDeviceType::VM_BUSINSTANCE);
    EXPECT_EQ(dev->GetType(), CollectionDeviceType::VM_BUSINSTANCE);
    EXPECT_EQ(dev->GetIdStr(), "testguid1234567890abcdef01234567");
    EXPECT_EQ(dev->GetUpiStr(), "100");
    dev->SetUpiStr("200");
    EXPECT_EQ(dev->GetUpiStr(), "200");
    EXPECT_TRUE(dev->GetSubDevNicPfe().empty());
    EXPECT_TRUE(dev->GetSubDevNicVfe().empty());
    EXPECT_TRUE(dev->GetSubDevIdev().empty());
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectionDeviceBusiSubDevNicPfe)
{
    UbseMtiEid eid{};
    auto busi = std::make_shared<CollectionDeviceBusi>("testguid1234567890abcdef01234567", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    CollectDeviceLoc nicLoc;
    nicLoc.slotId = 1;
    nicLoc.chipId = 2;
    nicLoc.pfeId = 3;
    nicLoc.guid = "nicpfe1234567890abcdef01234567";
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(nicLoc);
    busi->SetSubDevNicPfe(nicPfe);
    EXPECT_EQ(busi->GetSubDevNicPfe().size(), 1);
    busi->RemoveSubDevNicPfe(nicPfe);
    EXPECT_TRUE(busi->GetSubDevNicPfe().empty());
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectionDeviceBusiSubDevIdev)
{
    UbseMtiEid eid{};
    auto busi = std::make_shared<CollectionDeviceBusi>("testguid1234567890abcdef01234567", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    CollectDeviceLoc vfeLoc;
    vfeLoc.chipId = 1;
    vfeLoc.dieId = 2;
    vfeLoc.pfeId = 3;
    vfeLoc.vfeId = 4;
    vfeLoc.guid = "vfe1234567890abcdef012345678901";
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(vfeLoc);
    busi->SetSubDevIdev(vfe);
    EXPECT_EQ(busi->GetSubDevIdev().size(), 1);
    busi->RemoveSubDevIdev(vfe);
    EXPECT_TRUE(busi->GetSubDevIdev().empty());
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectionDeviceIdevPfeTest)
{
    CollectDeviceLoc loc;
    loc.chipId = 1;
    loc.dieId = 2;
    loc.pfeId = 3;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(loc);
    EXPECT_EQ(pfe->GetType(), CollectionDeviceType::P_IDEV);
    EXPECT_EQ(pfe->GetIdStr(), "6-1-2-3");

    CollectDeviceLoc ubCtrlLoc;
    ubCtrlLoc.chipId = 1;
    ubCtrlLoc.dieId = 2;
    auto ubCtrl = std::make_shared<CollectionDeviceUbCtrl>(ubCtrlLoc);
    pfe->SetParentUbCtl(ubCtrl);
    EXPECT_NE(pfe->GetParentUbCtl(), nullptr);
    EXPECT_EQ(pfe->GetParentUbCtl()->GetIdStr(), "5-1-2");

    EXPECT_FALSE(pfe->GetIsComSharedFe());
    pfe->SetIsComSharedFe(true);
    EXPECT_TRUE(pfe->GetIsComSharedFe());
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectionDeviceIdevVfeTest)
{
    CollectDeviceLoc loc;
    loc.chipId = 1;
    loc.dieId = 2;
    loc.pfeId = 3;
    loc.vfeId = 4;
    auto vfe = std::make_shared<CollectionDeviceIdevVfe>(loc);
    EXPECT_EQ(vfe->GetType(), CollectionDeviceType::V_IDEV);
    EXPECT_EQ(vfe->GetIdStr(), "7-1-2-3-4");

    CollectDeviceLoc pfeLoc;
    pfeLoc.chipId = 1;
    pfeLoc.dieId = 2;
    pfeLoc.pfeId = 3;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(pfeLoc);
    vfe->SetParentPfe(pfe);
    EXPECT_NE(vfe->GetParentPfe(), nullptr);

    UbseMtiEid eid{};
    auto busi = std::make_shared<CollectionDeviceBusi>("busi1234567890abcdef012345678901", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    vfe->SetBondingDevBusi(busi);
    auto bondingBusi = vfe->GetBondingDevBusi();
    EXPECT_EQ(bondingBusi.size(), 1);
    vfe->RemoveBondingDevBusi(busi);
    EXPECT_TRUE(vfe->GetBondingDevBusi().empty());
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectionDeviceIdevBondingDavid)
{
    CollectDeviceLoc loc;
    loc.chipId = 1;
    loc.dieId = 2;
    loc.pfeId = 3;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(loc);
    EXPECT_EQ(pfe->GetBondingDevDavid(), nullptr);

    CollectDeviceLoc davidLoc;
    davidLoc.slotId = 4;
    davidLoc.chipId = 5;
    auto david = std::make_shared<CollectionDeviceDavid>(davidLoc);
    pfe->SetBondingDevDavid(david);
    EXPECT_NE(pfe->GetBondingDevDavid(), nullptr);
    pfe->RemoveBondingDevDavid();
    EXPECT_EQ(pfe->GetBondingDevDavid(), nullptr);
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectionDeviceDavidTest)
{
    CollectDeviceLoc loc;
    loc.slotId = 1;
    loc.chipId = 2;
    auto david = std::make_shared<CollectionDeviceDavid>(loc);
    EXPECT_EQ(david->GetType(), CollectionDeviceType::NPU);
    EXPECT_EQ(david->GetIdStr(), "2-1-2");

    EXPECT_EQ(david->GetBondingIdev(), nullptr);
    CollectDeviceLoc pfeLoc;
    pfeLoc.chipId = 3;
    pfeLoc.dieId = 4;
    pfeLoc.pfeId = 5;
    auto pfeBase = std::make_shared<CollectionDeviceIdevPfe>(pfeLoc);
    auto pfeDev = CollectionDevice::CollectionToBase(pfeBase);
    david->SetBondingIdev(pfeDev);
    EXPECT_NE(david->GetBondingIdev(), nullptr);
    david->RemoveBondingIdev();
    EXPECT_EQ(david->GetBondingIdev(), nullptr);
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectionDeviceDavidAffinityNic)
{
    CollectDeviceLoc loc;
    loc.slotId = 1;
    loc.chipId = 2;
    auto david = std::make_shared<CollectionDeviceDavid>(loc);

    CollectDeviceLoc nicPfeLoc;
    nicPfeLoc.slotId = 3;
    nicPfeLoc.chipId = 4;
    nicPfeLoc.pfeId = 5;
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(nicPfeLoc);
    david->SetAffinityDevNicPfe(nicPfe);
    EXPECT_NE(david->GetAffinityDevNicPfe(), nullptr);

    CollectDeviceLoc nicVfeLoc;
    nicVfeLoc.slotId = 3;
    nicVfeLoc.chipId = 4;
    nicVfeLoc.pfeId = 5;
    nicVfeLoc.vfeId = 6;
    auto nicVfe = std::make_shared<CollectionDeviceNicVfe>(nicVfeLoc);
    david->SetAffinityDevNicVfe(nicVfe);
    EXPECT_NE(david->GetAffinityDevNicVfe(), nullptr);
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectionDeviceNicPfeTest)
{
    CollectDeviceLoc loc;
    loc.slotId = 1;
    loc.chipId = 2;
    loc.pfeId = 3;
    loc.guid = "nicpfe1234567890abcdef01234567";
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(loc);
    EXPECT_EQ(nicPfe->GetType(), CollectionDeviceType::NIC_PFE);
    EXPECT_EQ(nicPfe->GetIdStr(), "3-1-2-3");

    CollectDeviceLoc nicVfeLoc;
    nicVfeLoc.slotId = 1;
    nicVfeLoc.chipId = 2;
    nicVfeLoc.pfeId = 3;
    nicVfeLoc.vfeId = 4;
    nicVfeLoc.guid = "nicvfe1234567890abcdef01234567";
    auto nicVfe = std::make_shared<CollectionDeviceNicVfe>(nicVfeLoc);
    nicPfe->SetSubNicVfe(nicVfe);
    EXPECT_EQ(nicPfe->GetSubNicVfe().size(), 1);
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectionDeviceNicVfeTest)
{
    CollectDeviceLoc loc;
    loc.slotId = 1;
    loc.chipId = 2;
    loc.pfeId = 3;
    loc.vfeId = 4;
    loc.guid = "nicvfe1234567890abcdef01234567";
    auto nicVfe = std::make_shared<CollectionDeviceNicVfe>(loc);
    EXPECT_EQ(nicVfe->GetType(), CollectionDeviceType::NIC_VFE);
    EXPECT_EQ(nicVfe->GetIdStr(), "4-1-2-3-4");

    CollectDeviceLoc nicPfeLoc;
    nicPfeLoc.slotId = 1;
    nicPfeLoc.chipId = 2;
    nicPfeLoc.pfeId = 3;
    nicPfeLoc.guid = "nicpfe1234567890abcdef01234567";
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(nicPfeLoc);
    nicVfe->SetParentNicPfe(nicPfe);
    EXPECT_NE(nicVfe->GetParentNicPfe(), nullptr);
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectionDeviceNicBondingBusi)
{
    CollectDeviceLoc loc;
    loc.slotId = 1;
    loc.chipId = 2;
    loc.pfeId = 3;
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(loc);
    EXPECT_EQ(nicPfe->GetBondingDevBusi(), nullptr);

    UbseMtiEid eid{};
    auto busi = std::make_shared<CollectionDeviceBusi>("busi1234567890abcdef012345678901", eid, "100",
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    nicPfe->SetBondingDevBusi(busi);
    EXPECT_NE(nicPfe->GetBondingDevBusi(), nullptr);
    nicPfe->RemoveBondingDevBusi();
    EXPECT_EQ(nicPfe->GetBondingDevBusi(), nullptr);
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectionDeviceNicAffinityDavid)
{
    CollectDeviceLoc loc;
    loc.slotId = 1;
    loc.chipId = 2;
    loc.pfeId = 3;
    auto nicPfe = std::make_shared<CollectionDeviceNicPfe>(loc);
    EXPECT_EQ(nicPfe->GetAffinityDevDavid(), nullptr);

    CollectDeviceLoc davidLoc;
    davidLoc.slotId = 4;
    davidLoc.chipId = 5;
    auto david = std::make_shared<CollectionDeviceDavid>(davidLoc);
    nicPfe->SetAffinityDevDavid(david);
    EXPECT_NE(nicPfe->GetAffinityDevDavid(), nullptr);
}

TEST_F(TestUbseNpuResourceCollectionDef, CollectionDeviceSetEidGuidType)
{
    CollectDeviceLoc loc;
    loc.chipId = 1;
    loc.dieId = 2;
    loc.pfeId = 3;
    auto pfe = std::make_shared<CollectionDeviceIdevPfe>(loc);

    UbseMtiEid eid{};
    eid[0] = 0xAA;
    pfe->SetEid(eid);
    EXPECT_EQ(pfe->GetEid()[0], 0xAA);

    pfe->SetGuid("newguid");
    EXPECT_EQ(pfe->GetGuid(), "newguid");

    pfe->SetType(CollectionDeviceType::V_IDEV);
    EXPECT_EQ(pfe->GetType(), CollectionDeviceType::V_IDEV);
}

} // namespace ubse::npu::controller::ut
