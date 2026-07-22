/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_direct_to_vm_manager.h"
#include <time.h>
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_timer.h"

namespace ubse::ut::ssu {
using namespace ubse::mti::urma;
using namespace ubse::mti::bus_instance;

void TestUbseSsuDirectToVmManager::SetUp()
{
    Test::SetUp();
    MOCKER_CPP(&nanosleep).stubs().will(returnValue(0));
    auto &mgr = UbseSsuDirectToVmManager::GetInstance();
    mgr.state_ = UbseSsuDirectToVmManager::SsuDirectToVmManagerState::AVAILABLE;
}

void TestUbseSsuDirectToVmManager::TearDown()
{
    Test::TearDown();
    auto &mgr = UbseSsuDirectToVmManager::GetInstance();
    mgr.state_ = UbseSsuDirectToVmManager::SsuDirectToVmManagerState::AVAILABLE;
    GlobalMockObject::verify();
}

UbseMtiGuid TestUbseSsuDirectToVmManager::MakeGuid(uint8_t val)
{
    UbseMtiGuid guid{};
    guid[0] = val;
    guid[15] = val;
    return guid;
}

UbseMtiIdevPfe TestUbseSsuDirectToVmManager::MakePfe(uint8_t slotId, uint8_t chipId, uint8_t dieId, uint8_t pfeId)
{
    UbseMtiIdevPfe pfe;
    pfe.ubController.slotId = slotId;
    pfe.ubController.chipId = chipId;
    pfe.ubController.dieId = dieId;
    pfe.pfeId = pfeId;
    pfe.guid = MakeGuid(pfeId);
    return pfe;
}

UbseMtiIdevVfe TestUbseSsuDirectToVmManager::MakeVfe(uint8_t slotId, uint8_t chipId, uint8_t dieId, uint8_t pfeId,
                                                     uint8_t vfeId)
{
    UbseMtiIdevVfe vfe;
    vfe.ubController.slotId = slotId;
    vfe.ubController.chipId = chipId;
    vfe.ubController.dieId = dieId;
    vfe.pfeId = pfeId;
    vfe.vfeId = vfeId;
    vfe.guid = MakeGuid(vfeId + 0x10);
    return vfe;
}

UbseMtiBusInst TestUbseSsuDirectToVmManager::MakeVmBusInst(uint16_t upi, const UbseMtiGuid &guid,
                                                           const std::vector<UbseMtiGuid> &subDeviceGuids)
{
    UbseMtiBusInst busInst;
    busInst.type = UbseMtiBusInstanceType::VM;
    busInst.upi = upi;
    busInst.guid = guid;
    busInst.subDeviceGuids = subDeviceGuids;
    return busInst;
}

// ---- GetState / SetState ----

TEST_F(TestUbseSsuDirectToVmManager, GetState_DefaultAvailable)
{
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().GetState(),
              UbseSsuDirectToVmManager::SsuDirectToVmManagerState::AVAILABLE);
}

TEST_F(TestUbseSsuDirectToVmManager, SetState_RunningAlloc)
{
    auto &mgr = UbseSsuDirectToVmManager::GetInstance();
    mgr.SetState(UbseSsuDirectToVmManager::SsuDirectToVmManagerState::RUNNING_ALLOC);
    EXPECT_EQ(mgr.GetState(), UbseSsuDirectToVmManager::SsuDirectToVmManagerState::RUNNING_ALLOC);
    mgr.SetState(UbseSsuDirectToVmManager::SsuDirectToVmManagerState::AVAILABLE);
    EXPECT_EQ(mgr.GetState(), UbseSsuDirectToVmManager::SsuDirectToVmManagerState::AVAILABLE);
}

// ---- GuidToStr ----

TEST_F(TestUbseSsuDirectToVmManager, GuidToStr_ValidGuid)
{
    auto guid = MakeGuid(0x42);
    auto str = UbseSsuDirectToVmManager::GuidToStr(guid);
    EXPECT_FALSE(str.empty());
    EXPECT_EQ(str.size(), 32u);
}

TEST_F(TestUbseSsuDirectToVmManager, GuidToStr_AllZeros)
{
    UbseMtiGuid guid{};
    auto str = UbseSsuDirectToVmManager::GuidToStr(guid);
    EXPECT_EQ(str, std::string(32, '0'));
}

// ---- QueryPfeList ----

TEST_F(TestUbseSsuDirectToVmManager, QueryPfeList_Success)
{
    std::vector<UbseMtiIdevPfe> pfes;
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::GetIdevFeList).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseSsuDirectToVmManager::QueryPfeList(pfes), UBSE_OK);
}

TEST_F(TestUbseSsuDirectToVmManager, QueryPfeList_Failed)
{
    std::vector<UbseMtiIdevPfe> pfes;
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::GetIdevFeList).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseSsuDirectToVmManager::QueryPfeList(pfes), UBSE_ERROR);
}

// ---- ConvertVfe / ConvertFe ----

TEST_F(TestUbseSsuDirectToVmManager, ConvertVfe_FieldsMapped)
{
    auto vfe = MakeVfe(1, 2, 3, 6, 10);
    auto ssuVfe = UbseSsuDirectToVmManager::ConvertVfe(vfe);
    EXPECT_EQ(ssuVfe.slotId, 1);
    EXPECT_EQ(ssuVfe.chipId, 2);
    EXPECT_EQ(ssuVfe.dieId, 3);
    EXPECT_EQ(ssuVfe.pfeId, 6);
    EXPECT_EQ(ssuVfe.vfeId, 10);
    EXPECT_FALSE(ssuVfe.vfeGuid.empty());
}

TEST_F(TestUbseSsuDirectToVmManager, ConvertFe_FieldsMapped)
{
    auto pfe = MakePfe(1, 2, 3, 6);
    pfe.AddVfe(MakeVfe(1, 2, 3, 6, 10));
    auto ssuFe = UbseSsuDirectToVmManager::ConvertFe(pfe);
    EXPECT_EQ(ssuFe.slotId, 1);
    EXPECT_EQ(ssuFe.chipId, 2);
    EXPECT_EQ(ssuFe.dieId, 3);
    EXPECT_EQ(ssuFe.pfeId, 6);
    EXPECT_EQ(ssuFe.vfeList.size(), 1u);
    EXPECT_EQ(ssuFe.vfeList[0].vfeId, 10);
}

// ---- GetFeDeviceList ----

TEST_F(TestUbseSsuDirectToVmManager, GetFeDeviceList_Success)
{
    std::vector<UbseMtiIdevPfe> pfes;
    auto pfe = MakePfe(1, 2, 3, 6);
    pfe.AddVfe(MakeVfe(1, 2, 3, 6, 10));
    pfes.push_back(pfe);

    auto pfe2 = MakePfe(1, 2, 3, 5);
    pfes.push_back(pfe2);

    std::vector<UbseMtiBusInst> busInsts;

    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::GetIdevFeList)
        .stubs()
        .with(outBound(pfes))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .with(outBound(busInsts))
        .will(returnValue(UBSE_OK));

    std::vector<UbseSsuFe> feList;
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().GetFeDeviceList(feList), UBSE_OK);
    EXPECT_EQ(feList.size(), 1u);
    EXPECT_EQ(feList[0].vfeList.size(), 1u);
}

TEST_F(TestUbseSsuDirectToVmManager, GetFeDeviceList_QueryFailed)
{
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::GetIdevFeList).stubs().will(returnValue(UBSE_ERROR));
    std::vector<UbseSsuFe> feList;
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().GetFeDeviceList(feList), UBSE_ERROR);
}

TEST_F(TestUbseSsuDirectToVmManager, GetFeDeviceList_FillBusInstanceGuidFailed)
{
    std::vector<UbseMtiIdevPfe> pfes;
    auto pfe = MakePfe(1, 2, 3, 6);
    pfe.AddVfe(MakeVfe(1, 2, 3, 6, 10));
    pfes.push_back(pfe);
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::GetIdevFeList)
        .stubs()
        .with(outBound(pfes))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    std::vector<UbseSsuFe> feList;
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().GetFeDeviceList(feList), UBSE_ERROR);
}

// ---- FindVmBusInst ----

TEST_F(TestUbseSsuDirectToVmManager, FindVmBusInst_Found)
{
    auto guid = MakeGuid(0x99);
    auto busInst = MakeVmBusInst(100, guid);
    std::vector<UbseMtiBusInst> list{busInst};
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .with(outBound(list))
        .will(returnValue(UBSE_OK));
    UbseMtiBusInst result;
    auto guidStr = UbseSsuDirectToVmManager::GuidToStr(guid);
    EXPECT_EQ(UbseSsuDirectToVmManager::FindVmBusInst(guidStr, result), UBSE_OK);
    EXPECT_EQ(result.upi, 100);
}

TEST_F(TestUbseSsuDirectToVmManager, FindVmBusInst_NotFound)
{
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .will(returnValue(UBSE_OK));
    UbseMtiBusInst result;
    EXPECT_EQ(UbseSsuDirectToVmManager::FindVmBusInst("nonexistent", result), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseSsuDirectToVmManager, FindVmBusInst_GetListFailed)
{
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    UbseMtiBusInst result;
    EXPECT_EQ(UbseSsuDirectToVmManager::FindVmBusInst("any", result), UBSE_ERROR);
}

// ---- FindVfeInPfeList ----

TEST_F(TestUbseSsuDirectToVmManager, FindVfeInPfeList_Found)
{
    auto pfe = MakePfe(1, 2, 3, 6);
    auto vfe = MakeVfe(1, 2, 3, 6, 10);
    pfe.AddVfe(vfe);
    std::vector<UbseMtiIdevPfe> pfes{pfe};
    UbseSsuVfe ssuVfe;
    ssuVfe.slotId = 1;
    ssuVfe.chipId = 2;
    ssuVfe.dieId = 3;
    ssuVfe.pfeId = 6;
    ssuVfe.vfeId = 10;
    UbseMtiIdevVfe result;
    EXPECT_EQ(UbseSsuDirectToVmManager::FindVfeInPfeList(ssuVfe, pfes, result), UBSE_OK);
    EXPECT_EQ(result.vfeId, 10);
}

TEST_F(TestUbseSsuDirectToVmManager, FindVfeInPfeList_NotFound)
{
    auto pfe = MakePfe(1, 2, 3, 6);
    pfe.AddVfe(MakeVfe(1, 2, 3, 6, 10));
    std::vector<UbseMtiIdevPfe> pfes{pfe};
    UbseSsuVfe ssuVfe;
    ssuVfe.slotId = 1;
    ssuVfe.chipId = 2;
    ssuVfe.dieId = 3;
    ssuVfe.pfeId = 6;
    ssuVfe.vfeId = 99;
    UbseMtiIdevVfe result;
    EXPECT_EQ(UbseSsuDirectToVmManager::FindVfeInPfeList(ssuVfe, pfes, result), UBSE_ERROR_INVAL);
}

// ---- CheckVfeOccupied ----

TEST_F(TestUbseSsuDirectToVmManager, CheckVfeOccupied_NotOccupied)
{
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .will(returnValue(UBSE_OK));
    std::string occupied;
    EXPECT_EQ(UbseSsuDirectToVmManager::CheckVfeOccupied("anyguid", occupied), UBSE_OK);
    EXPECT_TRUE(occupied.empty());
}

TEST_F(TestUbseSsuDirectToVmManager, CheckVfeOccupied_Occupied)
{
    auto vfeGuid = MakeGuid(0x20);
    auto busGuid = MakeGuid(0x30);
    auto busInst = MakeVmBusInst(100, busGuid, {vfeGuid});
    std::vector<UbseMtiBusInst> list{busInst};
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .with(outBound(list))
        .will(returnValue(UBSE_OK));
    std::string vfeGuidStr = UbseSsuDirectToVmManager::GuidToStr(vfeGuid);
    std::string occupied;
    EXPECT_EQ(UbseSsuDirectToVmManager::CheckVfeOccupied(vfeGuidStr, occupied), UBSE_OK);
    EXPECT_FALSE(occupied.empty());
}

// ---- HasOtherVfe ----

TEST_F(TestUbseSsuDirectToVmManager, HasOtherVfe_NoOther)
{
    auto vfeGuid = MakeGuid(0x20);
    auto busInst = MakeVmBusInst(100, MakeGuid(0x30), {vfeGuid});
    std::string vfeGuidStr = UbseSsuDirectToVmManager::GuidToStr(vfeGuid);
    EXPECT_FALSE(UbseSsuDirectToVmManager::HasOtherVfe(busInst, vfeGuidStr));
}

TEST_F(TestUbseSsuDirectToVmManager, HasOtherVfe_HasOther)
{
    auto vfeGuid1 = MakeGuid(0x20);
    auto vfeGuid2 = MakeGuid(0x21);
    auto busInst = MakeVmBusInst(100, MakeGuid(0x30), {vfeGuid1, vfeGuid2});
    std::string vfeGuidStr = UbseSsuDirectToVmManager::GuidToStr(vfeGuid1);
    EXPECT_TRUE(UbseSsuDirectToVmManager::HasOtherVfe(busInst, vfeGuidStr));
}

// ---- CreateVmBusInst / DestroyVmBusInst ----

TEST_F(TestUbseSsuDirectToVmManager, CreateVmBusInst_Success)
{
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::CreateVmBusInstance)
        .stubs()
        .will(returnValue(UBSE_OK));
    UbseMtiBusInst busInst;
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().CreateVmBusInst(100, busInst), UBSE_OK);
}

TEST_F(TestUbseSsuDirectToVmManager, CreateVmBusInst_Failed)
{
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::CreateVmBusInstance)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    UbseMtiBusInst busInst;
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().CreateVmBusInst(100, busInst), UBSE_ERROR);
}

TEST_F(TestUbseSsuDirectToVmManager, DestroyVmBusInst_Success)
{
    auto busInst = MakeVmBusInst(100, MakeGuid(0x50));
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::DestroyVmBusInstance)
        .stubs()
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().DestroyVmBusInst(busInst), UBSE_OK);
}

TEST_F(TestUbseSsuDirectToVmManager, DestroyVmBusInst_Failed)
{
    auto busInst = MakeVmBusInst(100, MakeGuid(0x50));
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::DestroyVmBusInstance)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().DestroyVmBusInst(busInst), UBSE_ERROR);
}

// ---- RegVfeToVmBusInst / UnRegVfeFromVmBusInst ----

TEST_F(TestUbseSsuDirectToVmManager, RegVfeToVmBusInst_Success)
{
    auto vfe = MakeVfe(1, 2, 3, 6, 10);
    auto busInst = MakeVmBusInst(100, MakeGuid(0x50));
    std::vector<bool> resList{true};
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::RegDavidFeToVmBusInstance)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(resList))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().RegVfeToVmBusInst(busInst, vfe), UBSE_OK);
}

TEST_F(TestUbseSsuDirectToVmManager, RegVfeToVmBusInst_ResListFalse)
{
    auto vfe = MakeVfe(1, 2, 3, 6, 10);
    auto busInst = MakeVmBusInst(100, MakeGuid(0x50));
    std::vector<bool> resList{false};
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::RegDavidFeToVmBusInstance)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(resList))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().RegVfeToVmBusInst(busInst, vfe), UBSE_ERROR);
}

TEST_F(TestUbseSsuDirectToVmManager, UnRegVfeFromVmBusInst_Success)
{
    auto vfe = MakeVfe(1, 2, 3, 6, 10);
    auto busInst = MakeVmBusInst(100, MakeGuid(0x50));
    std::vector<bool> resList{true};
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::UnRegDavidFeFromVmBusInstance)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(resList))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().UnRegVfeFromVmBusInst(busInst, vfe), UBSE_OK);
}

TEST_F(TestUbseSsuDirectToVmManager, UnRegVfeFromVmBusInst_Failed)
{
    auto vfe = MakeVfe(1, 2, 3, 6, 10);
    auto busInst = MakeVmBusInst(100, MakeGuid(0x50));
    std::vector<bool> resList{false};
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::UnRegDavidFeFromVmBusInstance)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(resList))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().UnRegVfeFromVmBusInst(busInst, vfe), UBSE_ERROR);
}

// ---- FeDeviceAlloc ----

TEST_F(TestUbseSsuDirectToVmManager, FeDeviceAlloc_InvalidUpi)
{
    UbseSsuVfe vfe;
    std::string guid;
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().FeDeviceAlloc(70000, vfe, guid), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseSsuDirectToVmManager, FeDeviceAlloc_PreparationQueryFailed)
{
    UbseSsuVfe vfe;
    vfe.slotId = 1;
    vfe.chipId = 2;
    vfe.dieId = 3;
    vfe.pfeId = 6;
    vfe.vfeId = 10;
    std::string guid;
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::GetIdevFeList).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().FeDeviceAlloc(100, vfe, guid), UBSE_ERROR);
}

TEST_F(TestUbseSsuDirectToVmManager, FeDeviceAlloc_VfeNotFound)
{
    std::vector<UbseMtiIdevPfe> pfes;
    auto pfe = MakePfe(1, 2, 3, 6);
    pfe.AddVfe(MakeVfe(1, 2, 3, 6, 10));
    pfes.push_back(pfe);
    UbseSsuVfe vfe;
    vfe.slotId = 1;
    vfe.chipId = 2;
    vfe.dieId = 3;
    vfe.pfeId = 6;
    vfe.vfeId = 99;
    std::string guid;
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::GetIdevFeList)
        .stubs()
        .with(outBound(pfes))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().FeDeviceAlloc(100, vfe, guid), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseSsuDirectToVmManager, FeDeviceAlloc_AllocExecutionFailed)
{
    auto vfe = MakeVfe(1, 2, 3, 6, 10);
    auto pfe = MakePfe(1, 2, 3, 6);
    pfe.AddVfe(vfe);
    std::vector<UbseMtiIdevPfe> pfes{pfe};
    std::vector<UbseMtiBusInst> busInsts;
    UbseSsuVfe ssuVfe;
    ssuVfe.slotId = 1;
    ssuVfe.chipId = 2;
    ssuVfe.dieId = 3;
    ssuVfe.pfeId = 6;
    ssuVfe.vfeId = 10;
    std::string guid = "nonexistent";
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::GetIdevFeList)
        .stubs()
        .with(outBound(pfes))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .with(outBound(busInsts))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().FeDeviceAlloc(100, ssuVfe, guid), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseSsuDirectToVmManager, FeDeviceAlloc_Success)
{
    auto vfe = MakeVfe(1, 2, 3, 6, 10);
    auto pfe = MakePfe(1, 2, 3, 6);
    pfe.AddVfe(vfe);
    std::vector<UbseMtiIdevPfe> pfes{pfe};
    std::vector<UbseMtiBusInst> busInsts;
    std::vector<bool> resList{true};
    UbseSsuVfe ssuVfe;
    ssuVfe.slotId = 1;
    ssuVfe.chipId = 2;
    ssuVfe.dieId = 3;
    ssuVfe.pfeId = 6;
    ssuVfe.vfeId = 10;
    std::string guid;
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::GetIdevFeList)
        .stubs()
        .with(outBound(pfes))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .with(outBound(busInsts))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::CreateVmBusInstance)
        .stubs()
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::RegDavidFeToVmBusInstance)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(resList))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::DestroyVmBusInstance)
        .stubs()
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().FeDeviceAlloc(100, ssuVfe, guid), UBSE_OK);
    EXPECT_FALSE(guid.empty());
}

// ---- FeDeviceFree ----

TEST_F(TestUbseSsuDirectToVmManager, FeDeviceFree_InvalidUpi)
{
    UbseSsuVfe vfe;
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().FeDeviceFree(70000, vfe), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseSsuDirectToVmManager, FeDeviceFree_BusInstNotFound)
{
    UbseSsuVfe vfe;
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().FeDeviceFree(100, vfe), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseSsuDirectToVmManager, FeDeviceFree_UpiMismatch)
{
    auto busInst = MakeVmBusInst(200, MakeGuid(0x50));
    std::vector<UbseMtiBusInst> list{busInst};
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .with(outBound(list))
        .will(returnValue(UBSE_OK));
    UbseSsuVfe vfe;
    auto guidStr = UbseSsuDirectToVmManager::GuidToStr(busInst.guid);
    vfe.bindBusInstanceGuid = guidStr;
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().FeDeviceFree(100, vfe), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseSsuDirectToVmManager, FeDeviceFree_PreparationQueryFailed)
{
    auto busInst = MakeVmBusInst(100, MakeGuid(0x50));
    std::vector<UbseMtiBusInst> list{busInst};
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .with(outBound(list))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::GetIdevFeList).stubs().will(returnValue(UBSE_ERROR));
    UbseSsuVfe vfe;
    auto guidStr = UbseSsuDirectToVmManager::GuidToStr(busInst.guid);
    vfe.bindBusInstanceGuid = guidStr;
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().FeDeviceFree(100, vfe), UBSE_ERROR);
}

TEST_F(TestUbseSsuDirectToVmManager, FeDeviceFree_Success_WithDestroy)
{
    auto vfe = MakeVfe(1, 2, 3, 6, 10);
    auto pfe = MakePfe(1, 2, 3, 6);
    pfe.AddVfe(vfe);
    std::vector<UbseMtiIdevPfe> pfes{pfe};

    auto vfeGuid = MakeGuid(0x20);
    auto busGuid = MakeGuid(0x50);
    auto busInst = MakeVmBusInst(100, busGuid, {vfeGuid});
    std::vector<UbseMtiBusInst> list{busInst};
    std::vector<bool> resList{true};

    UbseSsuVfe ssuVfe;
    ssuVfe.slotId = 1;
    ssuVfe.chipId = 2;
    ssuVfe.dieId = 3;
    ssuVfe.pfeId = 6;
    ssuVfe.vfeId = 10;
    ssuVfe.vfeGuid = UbseSsuDirectToVmManager::GuidToStr(vfeGuid);

    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .with(outBound(list))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::GetIdevFeList)
        .stubs()
        .with(outBound(pfes))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::UnRegDavidFeFromVmBusInstance)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(resList))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::DestroyVmBusInstance)
        .stubs()
        .will(returnValue(UBSE_OK));
    auto guidStr = UbseSsuDirectToVmManager::GuidToStr(busInst.guid);
    ssuVfe.bindBusInstanceGuid = guidStr;
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().FeDeviceFree(100, ssuVfe), UBSE_OK);
}

TEST_F(TestUbseSsuDirectToVmManager, FeDeviceFree_Success_SkipDestroy)
{
    auto vfe = MakeVfe(1, 2, 3, 6, 10);
    auto pfe = MakePfe(1, 2, 3, 6);
    pfe.AddVfe(vfe);
    std::vector<UbseMtiIdevPfe> pfes{pfe};

    auto vfeGuid1 = MakeGuid(0x20);
    auto vfeGuid2 = MakeGuid(0x21);
    auto busGuid = MakeGuid(0x50);
    auto busInst = MakeVmBusInst(100, busGuid, {vfeGuid1, vfeGuid2});
    std::vector<UbseMtiBusInst> list{busInst};
    std::vector<bool> resList{true};

    UbseSsuVfe ssuVfe;
    ssuVfe.slotId = 1;
    ssuVfe.chipId = 2;
    ssuVfe.dieId = 3;
    ssuVfe.pfeId = 6;
    ssuVfe.vfeId = 10;
    ssuVfe.vfeGuid = UbseSsuDirectToVmManager::GuidToStr(vfeGuid1);

    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .with(outBound(list))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::GetIdevFeList)
        .stubs()
        .with(outBound(pfes))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::UnRegDavidFeFromVmBusInstance)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(resList))
        .will(returnValue(UBSE_OK));
    auto guidStr = UbseSsuDirectToVmManager::GuidToStr(busInst.guid);
    ssuVfe.bindBusInstanceGuid = guidStr;
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().FeDeviceFree(100, ssuVfe), UBSE_OK);
}

TEST_F(TestUbseSsuDirectToVmManager, FeDeviceFree_VfeNotFound)
{
    auto busGuid = MakeGuid(0x50);
    auto busInst = MakeVmBusInst(100, busGuid);
    std::vector<UbseMtiBusInst> list{busInst};
    auto pfe = MakePfe(1, 2, 3, 6);
    pfe.AddVfe(MakeVfe(1, 2, 3, 6, 10));
    std::vector<UbseMtiIdevPfe> pfes{pfe};

    UbseSsuVfe ssuVfe;
    ssuVfe.slotId = 1;
    ssuVfe.chipId = 2;
    ssuVfe.dieId = 3;
    ssuVfe.pfeId = 6;
    ssuVfe.vfeId = 99;

    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .with(outBound(list))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(UbseMtiUrma::GetInstance(), &UbseMtiUrma::GetIdevFeList)
        .stubs()
        .with(outBound(pfes))
        .will(returnValue(UBSE_OK));
    auto guidStr = UbseSsuDirectToVmManager::GuidToStr(busInst.guid);
    ssuVfe.bindBusInstanceGuid = guidStr;
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().FeDeviceFree(100, ssuVfe), UBSE_ERROR_INVAL);
}

// ---- ClearEmptyVMBusInstance ----

TEST_F(TestUbseSsuDirectToVmManager, ClearEmptyVMBusInstance_NoVmBusInst)
{
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .will(returnValue(UBSE_OK));
    UbseSsuDirectToVmManager::GetInstance().ClearEmptyVMBusInstance();
}

TEST_F(TestUbseSsuDirectToVmManager, ClearEmptyVMBusInstance_WithEmpty)
{
    auto busInst = MakeVmBusInst(100, MakeGuid(0x50));
    std::vector<UbseMtiBusInst> list{busInst};
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .with(outBound(list))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::DestroyVmBusInstance)
        .stubs()
        .will(returnValue(UBSE_OK));
    UbseSsuDirectToVmManager::GetInstance().ClearEmptyVMBusInstance();
}

TEST_F(TestUbseSsuDirectToVmManager, ClearEmptyVMBusInstance_GetListFailed)
{
    MOCKER_CPP_VIRTUAL(UbseMtiBusInstance::GetInstance(), &UbseMtiBusInstance::GetBusInstanceList)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    UbseSsuDirectToVmManager::GetInstance().ClearEmptyVMBusInstance();
}

// ---- StartClearTimer / StopClearTimer ----

TEST_F(TestUbseSsuDirectToVmManager, StartClearTimer_Success)
{
    MOCKER_CPP(&ubse::timer::UbseTimerHandlerRegister).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    EXPECT_EQ(UbseSsuDirectToVmManager::GetInstance().StartClearTimer(), UBSE_OK);
    UbseSsuDirectToVmManager::GetInstance().StopClearTimer();
}

TEST_F(TestUbseSsuDirectToVmManager, StopClearTimer_NotStarted)
{
    UbseSsuDirectToVmManager::GetInstance().StopClearTimer();
    SUCCEED();
}

} // namespace ubse::ut::ssu
