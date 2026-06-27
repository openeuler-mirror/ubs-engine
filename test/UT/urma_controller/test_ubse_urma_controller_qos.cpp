/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_urma_controller_qos.h"
#include <set>
#include <string>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_mti_def.h"
#include "ubse_mti_interface.h"
#include "ubse_node_com_urma_collector.h"
#include "ubse_node_controller.h"
#include "ubse_smbios.h"
#define private public
#include "ubse_urma_controller_qos.h"
#undef private

namespace ubse::urmaControllerQos::ut {

using namespace ubse::urmaController;
using namespace ubse::common::def;
using namespace adapter_plugins::mti;
using namespace ubse::nodeController;
using namespace ubse::adapter_plugins::smbios;

/* helper to get a fresh MTI instance reference */
static UbseMtiInterface& GetMti()
{
    return UbseMtiInterface::GetInstance();
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_SetEtsProfileState)
{
    EtsTemplate tmpl;
    tmpl.SetEtsProfileState(EtsQosProfileState::ETS_PROFILE_NOT_CREATED);
    tmpl.SetEtsProfileState(EtsQosProfileState::ETS_PROFILE_NOT_APPLIED);
    tmpl.SetEtsProfileState(EtsQosProfileState::ETS_PROFILE_APPLIED);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_ValidateConfig_EmptyConfigs)
{
    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    auto ret = tmpl.ValidateConfig(configs);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_ValidateConfig_InvalidPriority)
{
    auto& mti = GetMti();
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile).stubs().will(returnValue(UBSE_MTI_ERROR_NOT_EXIST));
    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    EtsQosConfig cfg;
    cfg.priority = static_cast<EtsPriority>(99);
    cfg.bandwidth = 100;
    configs.push_back(cfg);
    auto ret = tmpl.ValidateConfig(configs);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_ValidateConfig_ZeroBandwidth)
{
    auto& mti = GetMti();
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile).stubs().will(returnValue(UBSE_MTI_ERROR_NOT_EXIST));
    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    EtsQosConfig cfg;
    cfg.priority = EtsPriority::PRI_0;
    cfg.bandwidth = 0;
    configs.push_back(cfg);
    auto ret = tmpl.ValidateConfig(configs);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_ValidateConfig_DuplicatePriority)
{
    auto& mti = GetMti();
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile).stubs().will(returnValue(UBSE_MTI_ERROR_NOT_EXIST));
    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    EtsQosConfig cfg1{.priority = EtsPriority::PRI_0, .bandwidth = 100};
    EtsQosConfig cfg2{.priority = EtsPriority::PRI_0, .bandwidth = 200};
    configs.push_back(cfg1);
    configs.push_back(cfg2);
    auto ret = tmpl.ValidateConfig(configs);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_ValidateConfig_Success)
{
    auto& mti = GetMti();
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile).stubs().will(returnValue(UBSE_MTI_ERROR_NOT_EXIST));
    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    EtsQosConfig cfg{.priority = EtsPriority::PRI_0, .bandwidth = 100};
    configs.push_back(cfg);
    auto ret = tmpl.ValidateConfig(configs);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_ValidateConfig_QueryError)
{
    auto& mti = GetMti();
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile).stubs().will(returnValue(UBSE_ERROR));
    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    EtsQosConfig cfg{.priority = EtsPriority::PRI_0, .bandwidth = 100};
    configs.push_back(cfg);
    auto ret = tmpl.ValidateConfig(configs);
    EXPECT_EQ(ret, UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_ClassifyAppliedEtsInterfaces)
{
    EtsTemplate tmpl;
    std::vector<UbseMtiInterfaceEtsApplication> applied;
    std::set<std::string> allNames, targetNames;
    tmpl.ClassifyAppliedEtsInterfaces(applied, allNames, targetNames);
    EXPECT_TRUE(allNames.empty());
    EXPECT_TRUE(targetNames.empty());

    UbseMtiInterfaceEtsApplication app1{.interfaceName = "iface1", .etsProfileName = ETS_QOS_PROFILE_NAME};
    UbseMtiInterfaceEtsApplication app2{.interfaceName = "iface2", .etsProfileName = "OTHER_PROFILE"};
    applied = {app1, app2};
    tmpl.ClassifyAppliedEtsInterfaces(applied, allNames, targetNames);
    EXPECT_EQ(allNames.size(), 2);
    EXPECT_EQ(targetNames.size(), 1);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_GetAllUbInterfaceNameFromMti)
{
    std::vector<PhysicalLink> links{{.interfaceName = "eth0"}, {.interfaceName = "eth1"}};
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodePorts).stubs().with(outBound(links)).will(returnValue(UBSE_OK));
    EtsTemplate tmpl;
    std::set<std::string> names;
    EXPECT_EQ(tmpl.GetAllUbInterfaceNameFromMti(names), UBSE_OK);
    EXPECT_EQ(names.size(), 2);
    GlobalMockObject::verify();

    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodePorts).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(tmpl.GetAllUbInterfaceNameFromMti(names), UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_Delete_QueryInterfacesError)
{
    auto& mti = GetMti();
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile).stubs().will(returnValue(UBSE_ERROR));
    EtsTemplate tmpl;
    auto ret = tmpl.TryDelete();
    EXPECT_EQ(ret, UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_Delete_NoMatchingInterfaces)
{
    auto& mti = GetMti();
    std::vector<UbseMtiInterfaceEtsApplication> applied;
    UbseMtiInterfaceEtsApplication app;
    app.interfaceName = "iface";
    app.etsProfileName = "OTHER";
    applied.push_back(app);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(applied))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseSaveEtsProfile).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile).stubs().will(returnValue(UBSE_MTI_ERROR_NOT_EXIST));
    EtsTemplate tmpl;
    auto ret = tmpl.Delete();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_Delete_RemoveInterfaceFails)
{
    auto& mti = GetMti();
    std::vector<UbseMtiInterfaceEtsApplication> applied;
    UbseMtiInterfaceEtsApplication app;
    app.interfaceName = "iface";
    app.etsProfileName = ETS_QOS_PROFILE_NAME;
    applied.push_back(app);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(applied))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsProfileFromInterface).stubs().will(returnValue(UBSE_ERROR));
    EtsTemplate tmpl;
    auto ret = tmpl.TryDelete();
    EXPECT_EQ(ret, UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_Delete_RemoveVlsFails)
{
    auto& mti = GetMti();
    std::vector<UbseMtiInterfaceEtsApplication> applied;
    UbseMtiInterfaceEtsApplication app;
    app.interfaceName = "iface";
    app.etsProfileName = ETS_QOS_PROFILE_NAME;
    applied.push_back(app);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(applied))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsProfileFromInterface).stubs().will(returnValue(UBSE_OK));
    UbseMtiEtsProfile profile;
    profile.profileName = ETS_QOS_PROFILE_NAME;
    profile.vls.resize(1);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile)
        .stubs()
        .with(_, outBound(profile))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsVlsFromProfile).stubs().will(returnValue(UBSE_ERROR));
    EtsTemplate tmpl;
    auto ret = tmpl.TryDelete();
    EXPECT_EQ(ret, UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_Delete_RemovePriorityGroupsFails)
{
    auto& mti = GetMti();
    std::vector<UbseMtiInterfaceEtsApplication> applied;
    UbseMtiInterfaceEtsApplication app;
    app.interfaceName = "iface";
    app.etsProfileName = ETS_QOS_PROFILE_NAME;
    applied.push_back(app);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(applied))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsProfileFromInterface).stubs().will(returnValue(UBSE_OK));
    UbseMtiEtsProfile profile;
    profile.profileName = ETS_QOS_PROFILE_NAME;
    profile.priorityGroups.resize(1);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile)
        .stubs()
        .with(_, outBound(profile))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsVlsFromProfile).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsPriorityGroupsFromProfile)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    EtsTemplate tmpl;
    auto ret = tmpl.TryDelete();
    EXPECT_EQ(ret, UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_Delete_Success)
{
    auto& mti = GetMti();
    std::vector<UbseMtiInterfaceEtsApplication> applied;
    UbseMtiInterfaceEtsApplication app;
    app.interfaceName = "iface";
    app.etsProfileName = ETS_QOS_PROFILE_NAME;
    applied.push_back(app);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(applied))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsProfileFromInterface).stubs().will(returnValue(UBSE_OK));
    UbseMtiEtsProfile profile;
    profile.profileName = ETS_QOS_PROFILE_NAME;
    profile.vls.resize(1);
    profile.priorityGroups.resize(1);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile)
        .stubs()
        .with(_, outBound(profile))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsVlsFromProfile).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsPriorityGroupsFromProfile)
        .stubs()
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseSaveEtsProfile).stubs().will(returnValue(UBSE_OK));
    EtsTemplate tmpl;
    auto ret = tmpl.Delete();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_Query_ProfileNotFound)
{
    auto& mti = GetMti();
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile).stubs().will(returnValue(UBSE_MTI_ERROR_NOT_EXIST));
    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    auto ret = tmpl.Query(configs);
    EXPECT_EQ(ret, UBSE_URMACONTRL_ERROR_ETS_TEMPLATE_NOT_EXISTED);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_Query_QueryError)
{
    auto& mti = GetMti();
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile).stubs().will(returnValue(UBSE_ERROR));
    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    auto ret = tmpl.Query(configs);
    EXPECT_EQ(ret, UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_Query_GetPortsFails)
{
    auto& mti = GetMti();
    UbseMtiEtsProfile profile;
    profile.profileName = ETS_QOS_PROFILE_NAME;
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile)
        .stubs()
        .with(_, outBound(profile))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodePorts).stubs().will(returnValue(UBSE_ERROR));
    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    auto ret = tmpl.Query(configs);
    EXPECT_EQ(ret, UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_Query_NotAllApplied)
{
    auto& mti = GetMti();
    UbseMtiEtsProfile profile;
    profile.profileName = ETS_QOS_PROFILE_NAME;
    profile.priorityGroups.resize(1);
    profile.priorityGroups[0].priorityGroupId = 0;
    profile.priorityGroups[0].cir = 100;
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile)
        .stubs()
        .with(_, outBound(profile))
        .will(returnValue(UBSE_OK));
    std::vector<PhysicalLink> links;
    PhysicalLink link;
    link.interfaceName = "eth0";
    links.push_back(link);
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodePorts).stubs().with(outBound(links)).will(returnValue(UBSE_OK));
    std::vector<UbseMtiInterfaceEtsApplication> applied;
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(applied))
        .will(returnValue(UBSE_OK));
    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    auto ret = tmpl.Query(configs);
    EXPECT_EQ(ret, UBSE_URMACONTRL_ERROR_ETS_TEMPLATE_NOT_APPLIED);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_Query_Success)
{
    auto& mti = GetMti();
    UbseMtiEtsProfile profile;
    profile.profileName = ETS_QOS_PROFILE_NAME;
    profile.priorityGroups.resize(1);
    profile.priorityGroups[0].priorityGroupId = 0;
    profile.priorityGroups[0].cir = 100;
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile)
        .stubs()
        .with(_, outBound(profile))
        .will(returnValue(UBSE_OK));
    std::vector<PhysicalLink> links;
    PhysicalLink link;
    link.interfaceName = "eth0";
    links.push_back(link);
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodePorts).stubs().with(outBound(links)).will(returnValue(UBSE_OK));
    std::vector<UbseMtiInterfaceEtsApplication> applied;
    UbseMtiInterfaceEtsApplication app;
    app.interfaceName = "eth0";
    app.etsProfileName = ETS_QOS_PROFILE_NAME;
    applied.push_back(app);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(applied))
        .will(returnValue(UBSE_OK));
    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    auto ret = tmpl.Query(configs);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_ValidateAndPrepare_InitInnerFails)
{
    auto& mti = GetMti();
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile).stubs().will(returnValue(UBSE_ERROR));
    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    auto ret = tmpl.ValidateAndPrepare(configs);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_TryCreate_InitInnerFails)
{
    auto& mti = GetMti();
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile).stubs().will(returnValue(UBSE_ERROR));
    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    EtsQosConfig cfg{.priority = EtsPriority::PRI_0, .bandwidth = 100};
    configs.push_back(cfg);
    auto ret = tmpl.TryCreate(configs);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_CreateEtsProfileIfNotExist)
{
    auto& mti = GetMti();
    EtsTemplate tmpl;
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile).stubs().will(returnValue(UBSE_MTI_ERROR_NOT_EXIST));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseCreateEtsProfile).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(tmpl.CreateEtsProfileIfNotExist(), UBSE_OK);
    GlobalMockObject::verify();

    UbseMtiEtsProfile profile{.profileName = ETS_QOS_PROFILE_NAME};
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile)
        .stubs()
        .with(_, outBound(profile))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(tmpl.CreateEtsProfileIfNotExist(), UBSE_OK);
    GlobalMockObject::verify();

    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(tmpl.CreateEtsProfileIfNotExist(), UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_ApplyToRemainingPorts_Success)
{
    auto& mti = GetMti();
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseApplyEtsProfileToInterface).stubs().will(returnValue(UBSE_OK));
    EtsTemplate tmpl;
    std::set<std::string> allPorts{"eth0"};
    std::set<std::string> allApplied{"eth0"};
    std::set<std::string> targetApplied{"eth0"};
    auto ret = tmpl.ApplyToRemainingPorts(allPorts, allApplied, targetApplied);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_ApplyToRemainingPorts_RemoveFromInterface)
{
    auto& mti = GetMti();
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsProfileFromInterface).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseApplyEtsProfileToInterface).stubs().will(returnValue(UBSE_OK));
    EtsTemplate tmpl;
    std::set<std::string> allPorts{"eth0"};
    std::set<std::string> allApplied{"eth0"};
    std::set<std::string> targetApplied{"eth1"}; // not in targetApplied
    auto ret = tmpl.ApplyToRemainingPorts(allPorts, allApplied, targetApplied);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_ValidateConfig_PriorityGroupExists)
{
    auto& mti = GetMti();
    UbseMtiEtsProfile profile;
    profile.profileName = ETS_QOS_PROFILE_NAME;
    profile.priorityGroups.resize(1);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile)
        .stubs()
        .with(_, outBound(profile))
        .will(returnValue(UBSE_OK));
    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    EtsQosConfig cfg{.priority = EtsPriority::PRI_0, .bandwidth = 100};
    configs.push_back(cfg);
    auto ret = tmpl.ValidateConfig(configs);
    EXPECT_EQ(ret, UBSE_URMACONTRL_ERROR_PRIO_GROUP_EXIST);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_ApplyToRemainingPorts_RemoveFails)
{
    auto& mti = GetMti();
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsProfileFromInterface).stubs().will(returnValue(UBSE_ERROR));
    EtsTemplate tmpl;
    std::set<std::string> allPorts{"eth0"};
    std::set<std::string> allApplied{"eth0"};
    std::set<std::string> targetApplied{};
    auto ret = tmpl.ApplyToRemainingPorts(allPorts, allApplied, targetApplied);
    EXPECT_EQ(ret, UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_ApplyToRemainingPorts_ApplyFails)
{
    auto& mti = GetMti();
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseApplyEtsProfileToInterface).stubs().will(returnValue(UBSE_ERROR));
    EtsTemplate tmpl;
    std::set<std::string> allPorts{"eth0"};
    std::set<std::string> allApplied{};
    std::set<std::string> targetApplied{};
    auto ret = tmpl.ApplyToRemainingPorts(allPorts, allApplied, targetApplied);
    EXPECT_EQ(ret, UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_Init)
{
    // No ports → all applied vacuously, but CreateEtsProfileIfNotExist is still entered;
    // mock QueryEtsProfile to return existing profile so we short-circuit without calling InitQosEtsRetry
    auto& mti = GetMti();
    std::vector<UbseMtiInterfaceEtsApplication> emptyApplied;
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(emptyApplied))
        .will(returnValue(UBSE_OK));
    std::vector<PhysicalLink> emptyLinks;
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodePorts)
        .stubs()
        .with(outBound(emptyLinks))
        .will(returnValue(UBSE_OK));
    UbseMtiEtsProfile existingProfile{.profileName = ETS_QOS_PROFILE_NAME};
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile)
        .stubs()
        .with(_, outBound(existingProfile))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseSaveEtsProfile).stubs().will(returnValue(UBSE_OK));
    EtsTemplate tmpl;
    EXPECT_EQ(tmpl.Init(), UBSE_OK);
    GlobalMockObject::verify();

    // All ports already applied with ETS profile → early return success
    std::vector<UbseMtiInterfaceEtsApplication> applied;
    UbseMtiInterfaceEtsApplication app{.interfaceName = "eth0", .etsProfileName = ETS_QOS_PROFILE_NAME};
    applied.push_back(app);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(applied))
        .will(returnValue(UBSE_OK));
    std::vector<PhysicalLink> links{{.interfaceName = "eth0"}};
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodePorts).stubs().with(outBound(links)).will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile)
        .stubs()
        .with(_, outBound(existingProfile))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseSaveEtsProfile).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(tmpl.Init(), UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_Create_AllConditions)
{
    auto& mti = GetMti();
    std::vector<UbseMtiInterfaceEtsApplication> emptyApplied;
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(emptyApplied))
        .will(returnValue(UBSE_OK));
    std::vector<PhysicalLink> links;
    PhysicalLink link;
    link.interfaceName = "eth0";
    links.push_back(link);
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodePorts).stubs().with(outBound(links)).will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile).stubs().will(returnValue(UBSE_MTI_ERROR_NOT_EXIST));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseCreateEtsProfile).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseApplyEtsProfileToInterface).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseSaveEtsProfile).stubs().will(returnValue(UBSE_OK));
    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    EtsQosConfig cfg{.priority = EtsPriority::PRI_0, .bandwidth = 100};
    configs.push_back(cfg);
    auto ret = tmpl.Create(configs);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, UbseUrmaControllerQos_Init_NonClos)
{
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    auto ret = UbseUrmaControllerQos<EtsQosConfig>::GetInstance().UbseUrmaQosInit();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, UbseUrmaControllerQos_Init_Clos)
{
    auto& mti = GetMti();
    std::vector<UbseMtiInterfaceEtsApplication> emptyApplied;
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(emptyApplied))
        .will(returnValue(UBSE_OK));
    std::vector<PhysicalLink> links;
    PhysicalLink link;
    link.interfaceName = "eth0";
    links.push_back(link);
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodePorts).stubs().with(outBound(links)).will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile).stubs().will(returnValue(UBSE_MTI_ERROR_NOT_EXIST));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseCreateEtsProfile).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseApplyEtsProfileToInterface).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseSaveEtsProfile).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    auto ret = UbseUrmaControllerQos<EtsQosConfig>::GetInstance().UbseUrmaQosInit();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, UbseUrmaControllerQos_NullTemplate)
{
    auto& inst = UbseUrmaControllerQos<EtsQosConfig>::GetInstance();
    inst.qosTemplate_ = nullptr;
    std::vector<EtsQosConfig> configs;
    EXPECT_EQ(inst.UbseUrmaQosCreate(configs), UBSE_ERROR_NULLPTR);
    EXPECT_EQ(inst.UbseUrmaQosDelete(), UBSE_ERROR_NULLPTR);
    EXPECT_EQ(inst.UbseUrmaQosQuery(configs), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaControllerQos, UbseUrmaControllerQos_Create_Success)
{
    auto& mti = GetMti();
    std::vector<UbseMtiInterfaceEtsApplication> emptyApplied;
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(emptyApplied))
        .will(returnValue(UBSE_OK));
    std::vector<PhysicalLink> links;
    PhysicalLink link;
    link.interfaceName = "eth0";
    links.push_back(link);
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodePorts).stubs().with(outBound(links)).will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile).stubs().will(returnValue(UBSE_MTI_ERROR_NOT_EXIST));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseCreateEtsProfile).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseApplyEtsProfileToInterface).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseSaveEtsProfile).stubs().will(returnValue(UBSE_OK));
    UbseUrmaControllerQos<EtsQosConfig>::GetInstance().qosTemplate_ = std::make_unique<EtsTemplate>();
    std::vector<EtsQosConfig> configs;
    EtsQosConfig cfg{.priority = EtsPriority::PRI_0, .bandwidth = 100};
    configs.push_back(cfg);
    auto ret = UbseUrmaControllerQos<EtsQosConfig>::GetInstance().UbseUrmaQosCreate(configs);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, UbseUrmaControllerQos_Delete_Success)
{
    auto& mti = GetMti();
    std::vector<UbseMtiInterfaceEtsApplication> applied;
    UbseMtiInterfaceEtsApplication app;
    app.interfaceName = "iface";
    app.etsProfileName = ETS_QOS_PROFILE_NAME;
    applied.push_back(app);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(applied))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsProfileFromInterface).stubs().will(returnValue(UBSE_OK));
    UbseMtiEtsProfile profile;
    profile.profileName = ETS_QOS_PROFILE_NAME;
    profile.vls.resize(1);
    profile.priorityGroups.resize(1);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile)
        .stubs()
        .with(_, outBound(profile))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsVlsFromProfile).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsPriorityGroupsFromProfile)
        .stubs()
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseSaveEtsProfile).stubs().will(returnValue(UBSE_OK));
    UbseUrmaControllerQos<EtsQosConfig>::GetInstance().qosTemplate_ = std::make_unique<EtsTemplate>();
    auto ret = UbseUrmaControllerQos<EtsQosConfig>::GetInstance().UbseUrmaQosDelete();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, UbseUrmaControllerQos_Query_Success)
{
    auto& mti = GetMti();
    UbseMtiEtsProfile profile;
    profile.profileName = ETS_QOS_PROFILE_NAME;
    profile.priorityGroups.resize(1);
    profile.priorityGroups[0].priorityGroupId = 0;
    profile.priorityGroups[0].cir = 100;
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile)
        .stubs()
        .with(_, outBound(profile))
        .will(returnValue(UBSE_OK));
    std::vector<PhysicalLink> links;
    PhysicalLink link;
    link.interfaceName = "eth0";
    links.push_back(link);
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodePorts).stubs().with(outBound(links)).will(returnValue(UBSE_OK));
    std::vector<UbseMtiInterfaceEtsApplication> applied;
    UbseMtiInterfaceEtsApplication app;
    app.interfaceName = "eth0";
    app.etsProfileName = ETS_QOS_PROFILE_NAME;
    applied.push_back(app);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(applied))
        .will(returnValue(UBSE_OK));
    UbseUrmaControllerQos<EtsQosConfig>::GetInstance().qosTemplate_ = std::make_unique<EtsTemplate>();
    std::vector<EtsQosConfig> configs;
    auto ret = UbseUrmaControllerQos<EtsQosConfig>::GetInstance().UbseUrmaQosQuery(configs);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_TryCreate_BothPriorities)
{
    auto& mti = GetMti();
    // InitInner: no ports → create profile infra
    std::vector<UbseMtiInterfaceEtsApplication> emptyApplied;
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(emptyApplied))
        .will(returnValue(UBSE_OK));
    std::vector<PhysicalLink> emptyLinks;
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodePorts)
        .stubs()
        .with(outBound(emptyLinks))
        .will(returnValue(UBSE_OK));
    // ValidateConfig: no existing profile → passes
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile).stubs().will(returnValue(UBSE_MTI_ERROR_NOT_EXIST));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseCreateEtsProfile).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseSaveEtsProfile).stubs().will(returnValue(UBSE_OK));

    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    configs.push_back({.priority = EtsPriority::PRI_0, .bandwidth = 100});
    configs.push_back({.priority = EtsPriority::PRI_1, .bandwidth = 200});
    auto ret = tmpl.TryCreate(configs);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_TryCreate_UbseCreateEtsProfileFails)
{
    auto& mti = GetMti();
    // InitInner: all ports already applied → early return
    std::vector<UbseMtiInterfaceEtsApplication> applied;
    UbseMtiInterfaceEtsApplication app{.interfaceName = "eth0", .etsProfileName = ETS_QOS_PROFILE_NAME};
    applied.push_back(app);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(applied))
        .will(returnValue(UBSE_OK));
    std::vector<PhysicalLink> links{{.interfaceName = "eth0"}};
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodePorts).stubs().with(outBound(links)).will(returnValue(UBSE_OK));
    // ValidateConfig: no existing profile → passes
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile).stubs().will(returnValue(UBSE_MTI_ERROR_NOT_EXIST));
    // TryCreate: UbseCreateEtsProfile fails
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseCreateEtsProfile).stubs().will(returnValue(UBSE_ERROR));

    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    configs.push_back({.priority = EtsPriority::PRI_0, .bandwidth = 100});
    auto ret = tmpl.TryCreate(configs);
    EXPECT_EQ(ret, UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_TryCreate_UbseSaveEtsProfileFails)
{
    auto& mti = GetMti();
    // InitInner: all ports already applied → early return
    std::vector<UbseMtiInterfaceEtsApplication> applied;
    UbseMtiInterfaceEtsApplication app{.interfaceName = "eth0", .etsProfileName = ETS_QOS_PROFILE_NAME};
    applied.push_back(app);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(applied))
        .will(returnValue(UBSE_OK));
    std::vector<PhysicalLink> links{{.interfaceName = "eth0"}};
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodePorts).stubs().with(outBound(links)).will(returnValue(UBSE_OK));
    // ValidateConfig: no existing profile → passes
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile).stubs().will(returnValue(UBSE_MTI_ERROR_NOT_EXIST));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseCreateEtsProfile).stubs().will(returnValue(UBSE_OK));
    // TryCreate: UbseSaveEtsProfile fails
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseSaveEtsProfile).stubs().will(returnValue(UBSE_ERROR));

    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    configs.push_back({.priority = EtsPriority::PRI_0, .bandwidth = 100});
    auto ret = tmpl.TryCreate(configs);
    EXPECT_EQ(ret, UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_TryDelete_NoVls)
{
    auto& mti = GetMti();
    std::vector<UbseMtiInterfaceEtsApplication> applied;
    UbseMtiInterfaceEtsApplication app{.interfaceName = "iface", .etsProfileName = ETS_QOS_PROFILE_NAME};
    applied.push_back(app);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(applied))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsProfileFromInterface).stubs().will(returnValue(UBSE_OK));
    // Profile with priority groups but no VLs
    UbseMtiEtsProfile profile;
    profile.profileName = ETS_QOS_PROFILE_NAME;
    profile.priorityGroups.resize(1);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile)
        .stubs()
        .with(_, outBound(profile))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsPriorityGroupsFromProfile)
        .stubs()
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseSaveEtsProfile).stubs().will(returnValue(UBSE_OK));

    EtsTemplate tmpl;
    auto ret = tmpl.TryDelete();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_TryDelete_NoPriorityGroups)
{
    auto& mti = GetMti();
    std::vector<UbseMtiInterfaceEtsApplication> applied;
    UbseMtiInterfaceEtsApplication app{.interfaceName = "iface", .etsProfileName = ETS_QOS_PROFILE_NAME};
    applied.push_back(app);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(applied))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsProfileFromInterface).stubs().will(returnValue(UBSE_OK));
    // Profile with VLs but no priority groups
    UbseMtiEtsProfile profile;
    profile.profileName = ETS_QOS_PROFILE_NAME;
    profile.vls.resize(1);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile)
        .stubs()
        .with(_, outBound(profile))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsVlsFromProfile).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseSaveEtsProfile).stubs().will(returnValue(UBSE_OK));

    EtsTemplate tmpl;
    auto ret = tmpl.TryDelete();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_TryDelete_UbseSaveEtsProfileFails)
{
    auto& mti = GetMti();
    std::vector<UbseMtiInterfaceEtsApplication> applied;
    UbseMtiInterfaceEtsApplication app{.interfaceName = "iface", .etsProfileName = ETS_QOS_PROFILE_NAME};
    applied.push_back(app);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryAllInterfaceEtsProfile)
        .stubs()
        .with(outBound(applied))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsProfileFromInterface).stubs().will(returnValue(UBSE_OK));
    UbseMtiEtsProfile profile;
    profile.profileName = ETS_QOS_PROFILE_NAME;
    profile.vls.resize(1);
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile)
        .stubs()
        .with(_, outBound(profile))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseRemoveEtsVlsFromProfile).stubs().will(returnValue(UBSE_OK));
    // TryDelete: UbseSaveEtsProfile fails
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseSaveEtsProfile).stubs().will(returnValue(UBSE_ERROR));

    EtsTemplate tmpl;
    auto ret = tmpl.TryDelete();
    EXPECT_EQ(ret, UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED);
}

TEST_F(TestUbseUrmaControllerQos, EtsTemplate_ValidateConfig_ConfigMatchProfile)
{
    auto& mti = GetMti();
    UbseMtiEtsProfile profile;
    profile.profileName = ETS_QOS_PROFILE_NAME;
    profile.priorityGroups.resize(1);
    profile.priorityGroups[0].priorityGroupId = 0;
    profile.priorityGroups[0].cir = 100;
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::UbseQueryEtsProfile)
        .stubs()
        .with(_, outBound(profile))
        .will(returnValue(UBSE_OK));

    EtsTemplate tmpl;
    std::vector<EtsQosConfig> configs;
    configs.push_back({.priority = EtsPriority::PRI_0, .bandwidth = 100});
    auto ret = tmpl.ValidateConfig(configs);
    EXPECT_EQ(ret, UBSE_OK);
}

} // namespace ubse::urmaControllerQos::ut
