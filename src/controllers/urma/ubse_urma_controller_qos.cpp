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

#include "ubse_urma_controller_qos.h"
#include <algorithm>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "adapter_plugins/mti/ubse_mti_def.h"
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_node_com_urma_collector.h"
#include "ubse_node_controller.h"
#include "ubse_urma_controller_util.h"

namespace ubse::urmaController {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace common::def;
using namespace ubse::log;
using namespace ubse::adapter_plugins::mti;
using namespace ubse::nodeController;

static const std::map<EtsPriority, std::vector<uint8_t>> priorityToVlsMap = {{EtsPriority::PRI_0, {NO_1, NO_2}},
                                                                             {EtsPriority::PRI_1, {NO_8, NO_9}}};

void EtsTemplate::SetEtsProfileState(EtsQosProfileState state)
{
    // 加锁，确保线程安全
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = state;
}

UbseResult EtsTemplate::ValidateConfig(const std::vector<EtsQosConfig> &configs)
{
    if (configs.empty()) {
        UBSE_LOG_ERROR << "ETS configs is empty";
        return UBSE_ERROR_INVAL;
    }
    UbseMtiEtsProfile etsProfile;
    auto ret = UbseMtiInterface::GetInstance().UbseQueryEtsProfile(ETS_QOS_PROFILE_NAME, etsProfile);
    if (ret != UBSE_OK && ret != UBSE_MTI_ERROR_NOT_EXIST) {
        UBSE_LOG_ERROR << "Failed to query ETS profile," << FormatRetCode(ret);
        return UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED;
    }
    std::set<uint8_t> seenPriorities;
    for (const auto &cfg : configs) {
        // 优先级只能为0或1
        if (cfg.priority != EtsPriority::PRI_0 && cfg.priority != EtsPriority::PRI_1) {
            UBSE_LOG_ERROR << "Invalid priority=" << static_cast<uint32_t>(cfg.priority);
            return UBSE_ERROR_INVAL;
        }
        if (cfg.bandwidth == 0) {
            UBSE_LOG_ERROR << "Zero bandwidth for priority=" << static_cast<uint32_t>(cfg.priority);
            return UBSE_ERROR_INVAL;
        }
        if (!seenPriorities.insert(static_cast<uint8_t>(cfg.priority)).second) {
            UBSE_LOG_ERROR << "Duplicate priority=" << static_cast<uint32_t>(cfg.priority);
            return UBSE_ERROR_INVAL;
        }
        if (!etsProfile.priorityGroups.empty()) {
            UBSE_LOG_WARN << "ETS QoS profile has priority groups, not support config priority="
                          << static_cast<uint32_t>(cfg.priority);
            return UBSE_URMACONTRL_ERROR_PRIO_GROUP_EXIST;
        }
    }
    return UBSE_OK;
}

UbseResult EtsTemplate::InitQosEtsRetry()
{
    if (context::g_globalStop) {
        UBSE_LOG_INFO << "Global stop, no need to retry";
        return UBSE_OK;
    }
    if (state_ == EtsQosProfileState::ETS_PROFILE_APPLIED) {
        UBSE_LOG_INFO << "ETS QoS profile applied on all ports, no need to retry";
        return UBSE_OK;
    }
    std::string taskExecutor = "UrmaExecutor";
    std::string taskName = "UrmaQosEtsInitRetryTimer";
    auto task = [this]() {
        if (context::g_globalStop) {
            UBSE_LOG_INFO << "Global stop, no need to retry";
            return UBSE_OK;
        }
        if (this->state_ == EtsQosProfileState::ETS_PROFILE_APPLIED) {
            UBSE_LOG_INFO << "ETS QoS profile applied on all ports, no need to retry";
            return UBSE_OK;
        }
        return this->InitInner(false);
    };
    // 定时器每10s执行一次
    if (auto ret = RegisterUrmaRetryTimer(taskExecutor, taskName, NO_10, task); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to register QoS ETS init retry timer," << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

void EtsTemplate::ClassifyAppliedEtsInterfaces(
    const std::vector<adapter_plugins::mti::UbseMtiInterfaceEtsApplication> &appliedEtsInterfaces,
    std::set<std::string> &allAppliedInterfaceNames, std::set<std::string> &targetEtsAppliedInterfaceNames)
{
    for (const auto &interface : appliedEtsInterfaces) {
        if (interface.interfaceName.empty() || interface.etsProfileName.empty()) {
            UBSE_LOG_WARN << "Invalid ETS interface application, interfaceName=" << interface.interfaceName
                          << ", etsProfileName=" << interface.etsProfileName;
            continue;
        }
        allAppliedInterfaceNames.insert(interface.interfaceName);
        if (interface.etsProfileName == ETS_QOS_PROFILE_NAME) {
            targetEtsAppliedInterfaceNames.insert(interface.interfaceName);
        }
    }
}

UbseResult EtsTemplate::Init()
{
    return InitInner(true);
}

UbseResult EtsTemplate::InitInner(bool isRetry)
{
    // 调用MTI接口查询已应用ETS模板的所有接口
    std::vector<UbseMtiInterfaceEtsApplication> appliedEtsInterfaces;
    auto ret = UbseMtiInterface::GetInstance().UbseQueryAllInterfaceEtsProfile(appliedEtsInterfaces);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query all applied ETS interfaces," << FormatRetCode(ret);
        return isRetry ? this->InitQosEtsRetry() : UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED;
    }
    std::set<std::string> targetEtsAppliedInterfaceNames;
    std::set<std::string> allAppliedInterfaceNames;
    ClassifyAppliedEtsInterfaces(appliedEtsInterfaces, allAppliedInterfaceNames, targetEtsAppliedInterfaceNames);
    // 如果所有port都已应用ETS配置，直接返回
    std::set<std::string> allUbInterfaces;
    ret = GetAllUbInterfaceNameFromMti(allUbInterfaces);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get all ub interfaces";
        return isRetry ? this->InitQosEtsRetry() : UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED;
    }
    if (!allUbInterfaces.empty() &&
        std::includes(targetEtsAppliedInterfaceNames.begin(), targetEtsAppliedInterfaceNames.end(),
                      allUbInterfaces.begin(), allUbInterfaces.end())) {
        UBSE_LOG_INFO << "ETS QoS profile applied on all ports";
        this->SetEtsProfileState(EtsQosProfileState::ETS_PROFILE_APPLIED);
        return UBSE_OK;
    }
    // 确保ETS模板存在，不存在则创建
    ret = this->CreateEtsProfileIfNotExist(isRetry);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 将ETS模板应用到未覆盖的port
    return this->ApplyToRemainingPorts(isRetry, allUbInterfaces, allAppliedInterfaceNames,
                                       targetEtsAppliedInterfaceNames);
}

UbseResult EtsTemplate::CreateEtsProfileIfNotExist(bool isRetry)
{
    UbseMtiEtsProfile etsProfile;
    auto ret = UbseMtiInterface::GetInstance().UbseQueryEtsProfile(ETS_QOS_PROFILE_NAME, etsProfile);
    if (ret != UBSE_OK && ret != UBSE_MTI_ERROR_NOT_EXIST) {
        UBSE_LOG_ERROR << "Failed to query ETS profile," << FormatRetCode(ret);
        return isRetry ? this->InitQosEtsRetry() : UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED;
    }
    if (etsProfile.profileName != ETS_QOS_PROFILE_NAME) {
        UBSE_LOG_INFO << "ETS profile " << ETS_QOS_PROFILE_NAME << " not exist, will create";
        this->SetEtsProfileState(EtsQosProfileState::ETS_PROFILE_NOT_CREATED);
        etsProfile.profileName = ETS_QOS_PROFILE_NAME;
        etsProfile.vls.clear();
        etsProfile.priorityGroups.clear();
        ret = UbseMtiInterface::GetInstance().UbseCreateEtsProfile(etsProfile);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to create ETS profile," << FormatRetCode(ret);
            return isRetry ? this->InitQosEtsRetry() : UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED;
        }
    }
    this->SetEtsProfileState(EtsQosProfileState::ETS_PROFILE_NOT_APPLIED);
    return UBSE_OK;
}

UbseResult EtsTemplate::ApplyToRemainingPorts(bool isRetry, const std::set<std::string> &allUbInterfaces,
                                              const std::set<std::string> &allAppliedInterfaceNames,
                                              const std::set<std::string> &targetEtsAppliedInterfaceNames)
{
    for (const auto &interfaceName : allUbInterfaces) {
        if (targetEtsAppliedInterfaceNames.find(interfaceName) != targetEtsAppliedInterfaceNames.end()) {
            continue;
        }
        if (allAppliedInterfaceNames.find(interfaceName) != allAppliedInterfaceNames.end()) {
            auto ret = UbseMtiInterface::GetInstance().UbseRemoveEtsProfileFromInterface(interfaceName);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to delete ETS profile from interface," << FormatRetCode(ret);
                return isRetry ? this->InitQosEtsRetry() : UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED;
            }
        }
        auto ret = UbseMtiInterface::GetInstance().UbseApplyEtsProfileToInterface(interfaceName, ETS_QOS_PROFILE_NAME);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to apply ETS profile to interface," << FormatRetCode(ret);
            return isRetry ? this->InitQosEtsRetry() : UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED;
        }
    }
    this->SetEtsProfileState(EtsQosProfileState::ETS_PROFILE_APPLIED);
    return UBSE_OK;
}

UbseResult EtsTemplate::GetAllUbInterfaceNameFromMti(std::set<std::string> &allUbInterfaceNames)
{
    std::vector<PhysicalLink> allLinkInfos;
    auto ret = UbseNodeComUrmaCollector::GetInstance().GetCurNodePorts(allLinkInfos);
    allUbInterfaceNames.clear();
    std::transform(allLinkInfos.begin(), allLinkInfos.end(),
                   std::inserter(allUbInterfaceNames, allUbInterfaceNames.end()),
                   [](const PhysicalLink &link) { return link.interfaceName; });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get all ub interface name";
        return UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED;
    }
    for (auto &name : allUbInterfaceNames) {
        UBSE_LOG_DEBUG << "Get interface name=" << name;
    }
    return UBSE_OK;
}

UbseResult EtsTemplate::CreatePreset(const std::vector<EtsQosConfig> &configs)
{
    auto ret = this->InitInner(false);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to init ETS template," << FormatRetCode(ret);
        return ret;
    }
    ret = ValidateConfig(configs);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to validate ETS QoS configs," << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

UbseResult EtsTemplate::Create(const std::vector<EtsQosConfig> &configs)
{
    UBSE_LOG_INFO << "Will create ETS template with " << configs.size()
                  << " priority groups, ETS QoS profile state=" << static_cast<int>(state_);
    if (auto ret = CreatePreset(configs); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to do ETS cretion preset," << FormatRetCode(ret) << ", cannot create priority groups";
        return ret;
    }
    UbseMtiEtsProfile etsProfiles;
    etsProfiles.profileName = ETS_QOS_PROFILE_NAME;
    etsProfiles.vls.clear();
    etsProfiles.priorityGroups.clear();
    for (const auto &config : configs) {
        UbseEtsPriorityGroup prioGroup{.priorityGroupId = static_cast<uint8_t>(config.priority),
                                       .scheduleMode = UbseEtsScheduleMode::SP,
                                       .weight = NO_1,
                                       .cir = config.bandwidth};
        auto it = priorityToVlsMap.find(config.priority);
        if (it == priorityToVlsMap.end()) {
            UBSE_LOG_ERROR << "No VL mapping found for priority=" << static_cast<uint32_t>(config.priority);
            return UBSE_ERROR_INVAL;
        }

        std::string vlIndicesStr;
        for (const auto &vlIndex : it->second) {
            UbseEtsVl vl{.vlIndex = vlIndex,
                         .priorityGroupId = static_cast<uint8_t>(config.priority),
                         .scheduleMode = UbseEtsScheduleMode::SP,
                         .weight = NO_1};
            etsProfiles.vls.push_back(vl);
            if (!vlIndicesStr.empty()) {
                vlIndicesStr += ", ";
            }
            vlIndicesStr += std::to_string(vlIndex);
        }
        UBSE_LOG_INFO << "Will Create ETS priority group " << prioGroup.priorityGroupId << " with VLS " << vlIndicesStr;
        etsProfiles.priorityGroups.push_back(prioGroup);
        UBSE_LOG_INFO << "Will Create ETS priority group " << prioGroup.priorityGroupId << " with bandwidth "
                      << prioGroup.cir;
    }
    auto ret = UbseMtiInterface::GetInstance().UbseCreateEtsProfile(etsProfiles);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to create ETS profile," << FormatRetCode(ret);
        return UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED;
    }
    UBSE_LOG_INFO << "ETS template created";
    return UBSE_OK;
}

UbseResult EtsTemplate::Delete()
{
    // 取消所有接口上的ETS配置
    std::vector<UbseMtiInterfaceEtsApplication> appliedEtsInterfaces;
    auto ret = UbseMtiInterface::GetInstance().UbseQueryAllInterfaceEtsProfile(appliedEtsInterfaces);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query all applied ETS interfaces," << FormatRetCode(ret);
        return UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED;
    }
    for (const auto &interface : appliedEtsInterfaces) {
        if (interface.interfaceName.empty() || interface.etsProfileName.empty()) {
            UBSE_LOG_WARN << "Invalid ETS interface application, interfaceName=" << interface.interfaceName
                          << ", etsProfileName=" << interface.etsProfileName;
            continue;
        }
        if (interface.etsProfileName != ETS_QOS_PROFILE_NAME) {
            continue;
        }
        auto ret = UbseMtiInterface::GetInstance().UbseRemoveEtsProfileFromInterface(interface.interfaceName);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to delete ETS profile from interface," << FormatRetCode(ret);
            return UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED;
        }
    }
    UbseMtiEtsProfile etsProfiles;
    ret = UbseMtiInterface::GetInstance().UbseQueryEtsProfile(ETS_QOS_PROFILE_NAME, etsProfiles);
    if (ret != UBSE_OK && ret != UBSE_MTI_ERROR_NOT_EXIST) {
        UBSE_LOG_ERROR << "Failed to query QoS ETS profile," << FormatRetCode(ret);
        return UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED;
    }
    if (etsProfiles.profileName != ETS_QOS_PROFILE_NAME) {
        UBSE_LOG_WARN << "ETS profile name not match," << etsProfiles.profileName << ", or no priority groups";
        return UBSE_OK;
    }
    if (!etsProfiles.vls.empty()) {
        ret = UbseMtiInterface::GetInstance().UbseRemoveEtsVlsFromProfile(ETS_QOS_PROFILE_NAME);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to delete ETS VLs," << FormatRetCode(ret);
            return UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED;
        }
    }
    if (!etsProfiles.priorityGroups.empty()) {
        ret = UbseMtiInterface::GetInstance().UbseRemoveEtsPriorityGroupsFromProfile(ETS_QOS_PROFILE_NAME);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to delete ETS priority groups," << FormatRetCode(ret);
            return UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED;
        }
    }
    UBSE_LOG_INFO << "ETS template deleted";
    return UBSE_OK;
}

UbseResult EtsTemplate::Query(std::vector<EtsQosConfig> &configs)
{
    // 模板是否存在
    UbseMtiEtsProfile etsProfiles;
    auto ret = UbseMtiInterface::GetInstance().UbseQueryEtsProfile(ETS_QOS_PROFILE_NAME, etsProfiles);
    if (ret != UBSE_OK && ret != UBSE_MTI_ERROR_NOT_EXIST) {
        UBSE_LOG_ERROR << "Failed to query QoS ETS profile," << FormatRetCode(ret);
        return UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED;
    }
    if (etsProfiles.profileName != ETS_QOS_PROFILE_NAME) {
        UBSE_LOG_WARN << "ETS profile name not match, profileName=" << etsProfiles.profileName;
        return UBSE_URMACONTRL_ERROR_ETS_TEMPLATE_NOT_EXISTED;
    }
    // 模板是否应用
    std::set<std::string> allUbInterfaces;
    ret = GetAllUbInterfaceNameFromMti(allUbInterfaces);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get all ub interfaces";
        return UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED;
    }
    // 调用MTI接口查询已应用ETS模板的所有接口
    std::vector<UbseMtiInterfaceEtsApplication> appliedEtsInterfaces;
    ret = UbseMtiInterface::GetInstance().UbseQueryAllInterfaceEtsProfile(appliedEtsInterfaces);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query all applied ETS interfaces," << FormatRetCode(ret);
        return UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED;
    }
    std::set<std::string> targetEtsAppliedInterfaceNames;
    std::set<std::string> allAppliedInterfaceNames;
    ClassifyAppliedEtsInterfaces(appliedEtsInterfaces, allAppliedInterfaceNames, targetEtsAppliedInterfaceNames);
    // 如果存在port未应用ETS配置，直接返回模板微应用错误
    if (allUbInterfaces.empty() ||
        !std::includes(targetEtsAppliedInterfaceNames.begin(), targetEtsAppliedInterfaceNames.end(),
                       allUbInterfaces.begin(), allUbInterfaces.end())) {
        UBSE_LOG_WARN << "ETS profile not applied on all ports," << targetEtsAppliedInterfaceNames.size() << "/"
                      << allUbInterfaces.size();
        return UBSE_URMACONTRL_ERROR_ETS_TEMPLATE_NOT_APPLIED;
    }
    // 否则，返回已应用的ETS配置
    for (const auto &prioGroup : etsProfiles.priorityGroups) {
        configs.push_back(
            {.priority = static_cast<EtsPriority>(prioGroup.priorityGroupId), .bandwidth = prioGroup.cir});
    }
    return UBSE_OK;
}

template <>
std::unique_ptr<QosTemplateBase<EtsQosConfig>> CreateTemplate<EtsQosConfig>()
{
    return std::make_unique<EtsTemplate>();
}

template <>
UbseUrmaControllerQos<EtsQosConfig>::UbseUrmaControllerQos() : qosTemplate_(CreateTemplate<EtsQosConfig>())
{
}

template <>
UbseResult UbseUrmaControllerQos<EtsQosConfig>::UbseUrmaQosInit()
{
    UBSE_LOG_INFO << "Start to init ETS QoS";
    auto ret = qosTemplate_->Init();
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to init ETS QoS," << FormatRetCode(ret) << ", will retry";
        return ret;
    }
    return UBSE_OK;
}

template <>
UbseResult UbseUrmaControllerQos<EtsQosConfig>::UbseUrmaQosCreate(const std::vector<EtsQosConfig> &configs)
{
    if (qosTemplate_ == nullptr) {
        UBSE_LOG_ERROR << "QoS template not created";
        return UBSE_ERROR_NULLPTR;
    }
    return qosTemplate_->Create(configs);
}

template <>
UbseResult UbseUrmaControllerQos<EtsQosConfig>::UbseUrmaQosDelete()
{
    if (qosTemplate_ == nullptr) {
        UBSE_LOG_ERROR << "QoS template not created";
        return UBSE_ERROR_NULLPTR;
    }
    return qosTemplate_->Delete();
}

template <>
UbseResult UbseUrmaControllerQos<EtsQosConfig>::UbseUrmaQosQuery(std::vector<EtsQosConfig> &configs)
{
    if (qosTemplate_ == nullptr) {
        UBSE_LOG_ERROR << "QoS template not created";
        return UBSE_ERROR_NULLPTR;
    }
    return qosTemplate_->Query(configs);
}
} // namespace ubse::urmaController
