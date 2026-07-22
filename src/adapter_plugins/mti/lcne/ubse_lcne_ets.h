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

#ifndef UBSE_LCNE_ETS_H
#define UBSE_LCNE_ETS_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_http_common.h"
#include "ubse_lcne_def.h"
#include "adapter_plugins/mti/ubse_mti_def.h"

namespace ubse::lcne {
using namespace common::def;
using namespace ubse::http;
using namespace ubse::adapter_plugins::mti;

class UbseLcneEts {
public:
    static UbseLcneEts& GetInstance()
    {
        static UbseLcneEts instance("127.0.0.1", LcneServer::realPort);
        return instance;
    }

    UbseResult CreateEtsProfile(const UbseMtiEtsProfile& etsProfile);
    UbseResult AddEtsVlsToProfile(const std::string& profileName, const std::vector<UbseEtsVl>& vls);
    UbseResult AddEtsPriorityGroupsToProfile(const std::string& profileName,
                                             const std::vector<UbseEtsPriorityGroup>& priorityGroups);
    UbseResult AddEtsVlsAndPriorityGroupsToProfile(const std::string& profileName, const std::vector<UbseEtsVl>& vls,
                                                   const std::vector<UbseEtsPriorityGroup>& priorityGroups);
    UbseResult DeleteEtsProfile(const std::string& profileName);
    UbseResult RemoveEtsVlsFromProfile(const std::string& profileName);
    UbseResult RemoveEtsPriorityGroupsFromProfile(const std::string& profileName);
    UbseResult QueryEtsProfile(const std::string& profileName, UbseMtiEtsProfile& etsProfile);
    UbseResult QueryAllEtsProfiles(std::vector<UbseMtiEtsProfile>& etsProfiles);
    UbseResult ApplyEtsProfileToInterface(const std::string& interfaceName, const std::string& profileName);
    UbseResult RemoveEtsProfileFromInterface(const std::string& interfaceName);
    UbseResult QueryAllInterfaceEtsProfile(std::vector<UbseMtiInterfaceEtsApplication>& applications);
    UbseResult QueryInterfaceEtsProfile(const std::string& interfaceName, std::string& profileName);
    UbseResult SaveEtsProfile();

    // XML parsing methods exposed for IT mock integration
    static UbseResult ParseEtsProfileResponse(const std::string& body, UbseMtiEtsProfile& etsProfile);
    static UbseResult ParseAllEtsProfilesResponse(const std::string& body, std::vector<UbseMtiEtsProfile>& etsProfiles);
    static UbseResult ParseInterfaceEtsProfileResponse(const std::string& body, std::string& profileName);
    static UbseResult ParseAllInterfaceEtsProfileResponse(const std::string& body,
                                                          std::vector<UbseMtiInterfaceEtsApplication>& applications);

private:
    UbseLcneEts(std::string host, int port) : host(std::move(host)), port(port) {}
    UbseLcneEts(const UbseLcneEts&) = delete;
    UbseLcneEts& operator=(const UbseLcneEts&) = delete;

    UbseResult BuildEtsProfileXml(const UbseMtiEtsProfile& etsProfile, std::string& xmlStr);
    UbseResult BuildInterfaceEtsApplicationXml(const std::string& profileName, std::string& xmlStr);
    UbseResult BuildSaveEtsProfileXml(std::string& xmlStr);

    std::string host;
    int port;
};
} // namespace ubse::lcne

#endif // UBSE_LCNE_ETS_H
