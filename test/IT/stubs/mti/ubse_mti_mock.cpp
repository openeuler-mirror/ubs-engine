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

#include <mutex>
#include <sstream>

#include "ubse_error.h"
#include "ubse_mti_interface_default.h"
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "lcne/ubse_lcne_ets.h"

namespace {

std::string BuildEtsProfileResponseXml(const ubse::adapter_plugins::mti::UbseMtiEtsProfile& profile)
{
    std::ostringstream xml;
    xml << "<ub-qos><ets-profiles><ets-profile>";
    xml << "<name>" << profile.profileName << "</name>";
    if (!profile.vls.empty()) {
        xml << "<vls>";
        for (const auto& vl : profile.vls) {
            xml << "<vl><vl-index>" << vl.vlIndex << "</vl-index><priority-group-id>" << vl.priorityGroupId
                << "</priority-group-id></vl>";
        }
        xml << "</vls>";
    }
    if (!profile.priorityGroups.empty()) {
        xml << "<priority-groups>";
        for (const auto& pg : profile.priorityGroups) {
            xml << "<priority-group><priority-group-id>" << pg.priorityGroupId << "</priority-group-id><cir>" << pg.cir
                << "</cir><cbs>" << pg.cbs << "</cbs></priority-group>";
        }
        xml << "</priority-groups>";
    }
    xml << "</ets-profile></ets-profiles></ub-qos>";
    return xml.str();
}

std::string BuildAllInterfaceEtsApplicationXml(const std::map<std::string, std::string>& applications)
{
    std::ostringstream xml;
    xml << "<interfaces>";
    for (const auto& [ifName, profileName] : applications) {
        xml << "<interface><name>" << ifName << "</name>"
            << "<ets-application><ets-profile-name>" << profileName
            << "</ets-profile-name></ets-application></interface>";
    }
    xml << "</interfaces>";
    return xml.str();
}

std::string BuildInterfaceEtsApplicationXml(const std::string& interfaceName, const std::string& profileName)
{
    std::ostringstream xml;
    xml << "<ets-application><ets-profile-name>" << profileName << "</ets-profile-name></ets-application>";
    return xml.str();
}

} // namespace

namespace ubse::adapter_plugins::mti {

class UbseMtiInterfaceMock : public UbseMtiInterface {
public:
    // Non-ETS methods: delegate to UbseMtiInterfaceDefault
    // (which calls UbseLcneModule → HTTP → MockLcneServer)

    common::def::UbseResult GetLocalNodeInfo(UbseMtiNodeInfo& nodeInfo) override
    {
        return defaultImpl_.GetLocalNodeInfo(nodeInfo);
    }

    common::def::UbseResult GetCurNodeTopo(UbseDevTopology& topo) override
    {
        return defaultImpl_.GetCurNodeTopo(topo);
    }

    common::def::UbseResult GetClusterNodeInfoList(std::vector<UbseMtiNodeInfo>& nodeInfoList) override
    {
        return defaultImpl_.GetClusterNodeInfoList(nodeInfoList);
    }

    common::def::UbseResult GetClusterCpuTopo(UbseMtiCpuTopoInfoMap& topo) override
    {
        return defaultImpl_.GetClusterCpuTopo(topo);
    }

    common::def::UbseResult GetLocalIp(std::string& localIp) override
    {
        const char* envIp = std::getenv("UBSE_IT_NODE_IP");
        if (envIp != nullptr && envIp[0] != '\0') {
            localIp = envIp;
            return UBSE_OK;
        }
        return defaultImpl_.GetLocalIp(localIp);
    }

    common::def::UbseResult GetClusterIpList(std::vector<std::string>& ipList) override
    {
        const char* envIps = std::getenv("UBSE_IT_CLUSTER_IPS");
        if (envIps != nullptr && envIps[0] != '\0') {
            ipList.clear();
            std::string ips(envIps);
            size_t start = 0;
            while (start <= ips.size()) {
                size_t end = ips.find(',', start);
                std::string ip = ips.substr(start, end == std::string::npos ? std::string::npos : end - start);
                if (!ip.empty()) {
                    ipList.push_back(ip);
                }
                if (end == std::string::npos) {
                    break;
                }
                start = end + 1;
            }
            return UBSE_OK;
        }
        return defaultImpl_.GetClusterIpList(ipList);
    }

    common::def::UbseResult AddDecoderEntry(const mami::UbseMamiMemImportInfo& importInfo,
                                            mami::UbseMamiMemImportResult& importResult,
                                            const UbseDecoderTrustRingData& trustRingData) override
    {
        return defaultImpl_.AddDecoderEntry(importInfo, importResult, trustRingData);
    }

    common::def::UbseResult DeleteDecoderEntry(const mami::UbseMamiMemWithdraw& drawInfo) override
    {
        return defaultImpl_.DeleteDecoderEntry(drawInfo);
    }

    common::def::UbseResult InvalidateDecoderEntry(const mami::UbseMamiMemWithdraw& drawInfo) override
    {
        return defaultImpl_.InvalidateDecoderEntry(drawInfo);
    }

    common::def::UbseResult GetAllMemHandles(const mami::UbseMamiMemHandleQueryInfo& queryInfo,
                                             std::vector<mami::UbseMamiMemHandleValue>& handleValues) override
    {
        return defaultImpl_.GetAllMemHandles(queryInfo, handleValues);
    }

    common::def::UbseResult GetMtiComEid(std::map<UbseMtiIouInfo, UbseMtiEidGroup>& comUrmaInfoMap) override
    {
        return defaultImpl_.GetMtiComEid(comUrmaInfoMap);
    }

    common::def::UbseResult UbseGetFeEid(UbseMtiIouInfo iouInfo, std::vector<UbseMtiFeInfo>& allFeInfos) override
    {
        return defaultImpl_.UbseGetFeEid(iouInfo, allFeInfos);
    }

    // ETS methods: stateful mock backed by in-memory profile + applied interfaces

    common::def::UbseResult UbseCreateEtsProfile(const UbseMtiEtsProfile& etsProfile) override
    {
        std::lock_guard<std::mutex> lock(etsMutex_);
        storedProfile_ = etsProfile;
        return UBSE_OK;
    }

    common::def::UbseResult UbseAddEtsVlsToProfile(const std::string& profileName,
                                                   const std::vector<UbseEtsVl>& vls) override
    {
        std::lock_guard<std::mutex> lock(etsMutex_);
        if (storedProfile_.profileName != profileName) {
            return UBSE_MTI_ERROR_NOT_EXIST;
        }
        storedProfile_.vls.insert(storedProfile_.vls.end(), vls.begin(), vls.end());
        return UBSE_OK;
    }

    common::def::UbseResult UbseAddEtsPriorityGroupsToProfile(
        const std::string& profileName, const std::vector<UbseEtsPriorityGroup>& priorityGroups) override
    {
        std::lock_guard<std::mutex> lock(etsMutex_);
        if (storedProfile_.profileName != profileName) {
            return UBSE_MTI_ERROR_NOT_EXIST;
        }
        storedProfile_.priorityGroups.insert(storedProfile_.priorityGroups.end(), priorityGroups.begin(),
                                             priorityGroups.end());
        return UBSE_OK;
    }

    common::def::UbseResult UbseAddEtsVlsAndPriorityGroupsToProfile(
        const std::string& profileName, const std::vector<UbseEtsVl>& vls,
        const std::vector<UbseEtsPriorityGroup>& priorityGroups) override
    {
        std::lock_guard<std::mutex> lock(etsMutex_);
        if (storedProfile_.profileName != profileName) {
            return UBSE_MTI_ERROR_NOT_EXIST;
        }
        storedProfile_.vls.insert(storedProfile_.vls.end(), vls.begin(), vls.end());
        storedProfile_.priorityGroups.insert(storedProfile_.priorityGroups.end(), priorityGroups.begin(),
                                             priorityGroups.end());
        return UBSE_OK;
    }

    common::def::UbseResult UbseDeleteEtsProfile(const std::string& profileName) override
    {
        std::lock_guard<std::mutex> lock(etsMutex_);
        if (storedProfile_.profileName != profileName) {
            return UBSE_MTI_ERROR_NOT_EXIST;
        }
        storedProfile_ = UbseMtiEtsProfile{};
        appliedInterfaces_.clear();
        return UBSE_OK;
    }

    common::def::UbseResult UbseRemoveEtsVlsFromProfile(const std::string& profileName) override
    {
        std::lock_guard<std::mutex> lock(etsMutex_);
        if (storedProfile_.profileName != profileName) {
            return UBSE_MTI_ERROR_NOT_EXIST;
        }
        storedProfile_.vls.clear();
        return UBSE_OK;
    }

    common::def::UbseResult UbseRemoveEtsPriorityGroupsFromProfile(const std::string& profileName) override
    {
        std::lock_guard<std::mutex> lock(etsMutex_);
        if (storedProfile_.profileName != profileName) {
            return UBSE_MTI_ERROR_NOT_EXIST;
        }
        storedProfile_.priorityGroups.clear();
        return UBSE_OK;
    }

    common::def::UbseResult UbseQueryEtsProfile(const std::string& profileName, UbseMtiEtsProfile& etsProfile) override
    {
        std::lock_guard<std::mutex> lock(etsMutex_);
        if (storedProfile_.profileName != profileName) {
            return UBSE_MTI_ERROR_NOT_EXIST;
        }
        std::string xml = BuildEtsProfileResponseXml(storedProfile_);
        return lcne::UbseLcneEts::ParseEtsProfileResponse(xml, etsProfile);
    }

    common::def::UbseResult UbseQueryAllEtsProfiles(std::vector<UbseMtiEtsProfile>& etsProfiles) override
    {
        std::lock_guard<std::mutex> lock(etsMutex_);
        if (storedProfile_.profileName.empty()) {
            etsProfiles.clear();
            return UBSE_OK;
        }
        std::string xml = BuildEtsProfileResponseXml(storedProfile_);
        return lcne::UbseLcneEts::ParseAllEtsProfilesResponse(xml, etsProfiles);
    }

    common::def::UbseResult UbseApplyEtsProfileToInterface(const std::string& interfaceName,
                                                           const std::string& profileName) override
    {
        std::lock_guard<std::mutex> lock(etsMutex_);
        appliedInterfaces_[interfaceName] = profileName;
        return UBSE_OK;
    }

    common::def::UbseResult UbseRemoveEtsProfileFromInterface(const std::string& interfaceName) override
    {
        std::lock_guard<std::mutex> lock(etsMutex_);
        appliedInterfaces_.erase(interfaceName);
        return UBSE_OK;
    }

    common::def::UbseResult UbseQueryAllInterfaceEtsProfile(
        std::vector<UbseMtiInterfaceEtsApplication>& applications) override
    {
        std::lock_guard<std::mutex> lock(etsMutex_);
        std::string xml = BuildAllInterfaceEtsApplicationXml(appliedInterfaces_);
        return lcne::UbseLcneEts::ParseAllInterfaceEtsProfileResponse(xml, applications);
    }

    common::def::UbseResult UbseQueryInterfaceEtsProfile(const std::string& interfaceName,
                                                         std::string& profileName) override
    {
        std::lock_guard<std::mutex> lock(etsMutex_);
        auto it = appliedInterfaces_.find(interfaceName);
        if (it == appliedInterfaces_.end()) {
            profileName.clear();
            return UBSE_OK;
        }
        profileName = it->second;
        return UBSE_OK;
    }

    common::def::UbseResult UbseSaveEtsProfile() override
    {
        return UBSE_OK;
    }

private:
    UbseMtiInterfaceDefault defaultImpl_;
    UbseMtiEtsProfile storedProfile_;
    std::map<std::string, std::string> appliedInterfaces_;
    std::mutex etsMutex_;
};

UbseMtiInterface& UbseMtiInterface::GetInstance()
{
    static UbseMtiInterfaceMock instance;
    return instance;
}

} // namespace ubse::adapter_plugins::mti