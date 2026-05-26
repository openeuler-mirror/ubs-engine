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

#include "ubse_lcne_ets.h"

#include <memory>
#include <stdexcept>

#include "ubse_error.h"
#include "ubse_http_module.h"
#include "ubse_logger.h"
#include "ubse_pointer_process.h"
#include "ubse_xml.h"

namespace ubse::lcne {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::adapter_plugins::mti;

const std::string LCNE_ETS_URI = "/restconf/data/huawei-ub-qos:ub-qos/ets-profiles";
const std::string LCNE_ETS_YANG_DATA_XML = "application/yang-data+xml";
const std::string LCNE_ETS_XML = "application/xml";
const std::string LCNE_ETS_SCHEDULE_MODE_DWRR = "dwrr";
const std::string LCNE_ETS_SCHEDULE_MODE_SP = "sp";

namespace {
std::string ScheduleModeToString(UbseEtsScheduleMode mode)
{
    return mode == UbseEtsScheduleMode::SP ? LCNE_ETS_SCHEDULE_MODE_SP : LCNE_ETS_SCHEDULE_MODE_DWRR;
}

UbseResult StringToScheduleMode(const std::string &modeStr, UbseEtsScheduleMode &mode)
{
    if (modeStr == LCNE_ETS_SCHEDULE_MODE_DWRR) {
        mode = UbseEtsScheduleMode::DWRR;
        return UBSE_OK;
    }
    if (modeStr == LCNE_ETS_SCHEDULE_MODE_SP) {
        mode = UbseEtsScheduleMode::SP;
        return UBSE_OK;
    }
    UBSE_LOG_ERROR << "[MTI] Unknown ETS schedule mode: " << modeStr;
    return UBSE_ERROR;
}

void AddTextNode(const std::shared_ptr<UbseXml> &xml, const std::string &nodeName, const std::string &text)
{
    xml->AddNode(nodeName);
    xml->Child(nodeName)->Text(text);
}

UbseResult ReadChildText(const std::shared_ptr<UbseXml> &xml, const std::string &nodeName, std::string &text)
{
    if (xml->Next(nodeName) == nullptr) {
        UBSE_LOG_ERROR << "[MTI] XML node " << nodeName << " not found.";
        return UBSE_ERROR;
    }
    text = xml->Text();
    if (xml->Previous() != UbseXmlError::OK) {
        UBSE_LOG_ERROR << "[MTI] XML previous node failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult ReadChildUint32(const std::shared_ptr<UbseXml> &xml, const std::string &nodeName, uint32_t &value)
{
    std::string text;
    auto ret = ReadChildText(xml, nodeName, text);
    if (ret != UBSE_OK) {
        return ret;
    }
    try {
        value = static_cast<uint32_t>(std::stoul(text));
    } catch (const std::invalid_argument &e) {
        UBSE_LOG_ERROR << "[MTI] ETS XML node " << nodeName << " invalid uint32 value.";
        return UBSE_ERROR;
    } catch (const std::out_of_range &e) {
        UBSE_LOG_ERROR << "[MTI] ETS XML node " << nodeName << " uint32 value out of range.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult ReadChildScheduleMode(const std::shared_ptr<UbseXml> &xml, UbseEtsScheduleMode &mode)
{
    std::string modeStr;
    auto ret = ReadChildText(xml, "schedule-mode", modeStr);
    if (ret != UBSE_OK) {
        return ret;
    }
    return StringToScheduleMode(modeStr, mode);
}

void BuildVlXml(const std::shared_ptr<UbseXml> &vlXml, const UbseEtsVl &vl)
{
    AddTextNode(vlXml, "vl-index", std::to_string(vl.vlIndex));
    AddTextNode(vlXml, "priority-group-id", std::to_string(vl.priorityGroupId));
    AddTextNode(vlXml, "schedule-mode", ScheduleModeToString(vl.scheduleMode));
    AddTextNode(vlXml, "weight", std::to_string(vl.weight));
}

void BuildPriorityGroupXml(const std::shared_ptr<UbseXml> &priorityGroupXml, const UbseEtsPriorityGroup &priorityGroup)
{
    AddTextNode(priorityGroupXml, "priority-group-id", std::to_string(priorityGroup.priorityGroupId));
    AddTextNode(priorityGroupXml, "schedule-mode", ScheduleModeToString(priorityGroup.scheduleMode));
    AddTextNode(priorityGroupXml, "weight", std::to_string(priorityGroup.weight));
    AddTextNode(priorityGroupXml, "cir", std::to_string(priorityGroup.cir));
    AddTextNode(priorityGroupXml, "cbs", std::to_string(priorityGroup.cbs));
}

UbseResult BuildEtsProfileVlsContentXml(const std::shared_ptr<UbseXml> &vlsXml, const std::vector<UbseEtsVl> &vls)
{
    for (size_t i = 0; i < vls.size(); ++i) {
        vlsXml->AddNode("vl");
        std::shared_ptr<UbseXml> vlXml = vlsXml->Next("vl", static_cast<int>(i));
        BuildVlXml(vlXml, vls[i]);
        if (vlsXml->Previous() != UbseXmlError::OK) {
            UBSE_LOG_ERROR << "[MTI] XML previous vl node failed.";
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

UbseResult BuildEtsProfilePriorityGroupsContentXml(const std::shared_ptr<UbseXml> &priorityGroupsXml,
                                                   const std::vector<UbseEtsPriorityGroup> &priorityGroups)
{
    for (size_t i = 0; i < priorityGroups.size(); ++i) {
        priorityGroupsXml->AddNode("priority-group");
        std::shared_ptr<UbseXml> priorityGroupXml = priorityGroupsXml->Next("priority-group", static_cast<int>(i));
        BuildPriorityGroupXml(priorityGroupXml, priorityGroups[i]);
        if (priorityGroupsXml->Previous() != UbseXmlError::OK) {
            UBSE_LOG_ERROR << "[MTI] XML previous priority-group node failed.";
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

UbseResult BuildEtsProfileVlsNode(const std::shared_ptr<UbseXml> &etsProfileXml, const std::vector<UbseEtsVl> &vls)
{
    etsProfileXml->AddNode("vls");
    auto ret = BuildEtsProfileVlsContentXml(etsProfileXml->Next("vls"), vls);
    if (ret != UBSE_OK || etsProfileXml->Previous() != UbseXmlError::OK) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult BuildEtsProfilePriorityGroupsNode(const std::shared_ptr<UbseXml> &etsProfileXml,
                                             const std::vector<UbseEtsPriorityGroup> &priorityGroups)
{
    etsProfileXml->AddNode("priority-groups");
    auto ret = BuildEtsProfilePriorityGroupsContentXml(etsProfileXml->Next("priority-groups"), priorityGroups);
    if (ret != UBSE_OK || etsProfileXml->Previous() != UbseXmlError::OK) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult ParseVlXml(const std::shared_ptr<UbseXml> &vlXml, UbseEtsVl &vl)
{
    auto ret = ReadChildUint32(vlXml, "vl-index", vl.vlIndex);
    if (ret != UBSE_OK) {
        return ret;
    }
    ret = ReadChildUint32(vlXml, "priority-group-id", vl.priorityGroupId);
    if (ret != UBSE_OK) {
        return ret;
    }
    ret = ReadChildScheduleMode(vlXml, vl.scheduleMode);
    if (ret != UBSE_OK) {
        return ret;
    }
    return ReadChildUint32(vlXml, "weight", vl.weight);
}

UbseResult ParsePriorityGroupXml(const std::shared_ptr<UbseXml> &priorityGroupXml, UbseEtsPriorityGroup &priorityGroup)
{
    auto ret = ReadChildUint32(priorityGroupXml, "priority-group-id", priorityGroup.priorityGroupId);
    if (ret != UBSE_OK) {
        return ret;
    }
    ret = ReadChildScheduleMode(priorityGroupXml, priorityGroup.scheduleMode);
    if (ret != UBSE_OK) {
        return ret;
    }
    ret = ReadChildUint32(priorityGroupXml, "weight", priorityGroup.weight);
    if (ret != UBSE_OK) {
        return ret;
    }
    ret = ReadChildUint32(priorityGroupXml, "cir", priorityGroup.cir);
    if (ret != UBSE_OK) {
        return ret;
    }
    return ReadChildUint32(priorityGroupXml, "cbs", priorityGroup.cbs);
}

UbseResult ParseEtsLists(const std::shared_ptr<UbseXml> &xml, std::vector<UbseEtsVl> &vls,
                         std::vector<UbseEtsPriorityGroup> &priorityGroups)
{
    if (xml->Next("vls") != nullptr) {
        int vlIndex = 0;
        while (xml->Next("vl", vlIndex) != nullptr) {
            UbseEtsVl vl{};
            auto ret = ParseVlXml(xml, vl);
            if (ret != UBSE_OK) {
                return ret;
            }
            vls.push_back(vl);
            if (xml->Previous() != UbseXmlError::OK) {
                UBSE_LOG_ERROR << "[MTI] XML previous vl node failed.";
                return UBSE_ERROR;
            }
            ++vlIndex;
        }
        if (xml->Previous() != UbseXmlError::OK) {
            UBSE_LOG_ERROR << "[MTI] XML previous vls node failed.";
            return UBSE_ERROR;
        }
    }

    if (xml->Next("priority-groups") != nullptr) {
        int priorityGroupIndex = 0;
        while (xml->Next("priority-group", priorityGroupIndex) != nullptr) {
            UbseEtsPriorityGroup priorityGroup{};
            auto ret = ParsePriorityGroupXml(xml, priorityGroup);
            if (ret != UBSE_OK) {
                return ret;
            }
            priorityGroups.push_back(priorityGroup);
            if (xml->Previous() != UbseXmlError::OK) {
                UBSE_LOG_ERROR << "[MTI] XML previous priority-group node failed.";
                return UBSE_ERROR;
            }
            ++priorityGroupIndex;
        }
        if (xml->Previous() != UbseXmlError::OK) {
            UBSE_LOG_ERROR << "[MTI] XML previous priority-groups node failed.";
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

UbseResult ParseEtsProfileXml(const std::shared_ptr<UbseXml> &xml, UbseMtiEtsProfile &etsProfile)
{
    auto result = ReadChildText(xml, "name", etsProfile.profileName);
    if (result != UBSE_OK) {
        return result;
    }
    return ParseEtsLists(xml, etsProfile.vls, etsProfile.priorityGroups);
}

UbseResult SendEtsRequest(UbseHttpRequest &req, UbseHttpResponse &rsp,
                          const std::string &contentType = LCNE_ETS_YANG_DATA_XML)
{
    req.headers.emplace("Accept", LCNE_ETS_YANG_DATA_XML);
    req.headers.emplace("Content-Type", contentType);
    auto ret = UbseHttpModule::HttpSend(req, rsp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] UbseHttpModule::HttpSend failed. " << FormatRetCode(ret);
    }
    return ret;
}

std::string BuildEtsProfilePath(const std::string &profileName)
{
    return LCNE_ETS_URI + "/ets-profile=" + profileName;
}

std::string BuildEtsProfileVlsPath(const std::string &profileName)
{
    return LCNE_ETS_URI + "/ets-profile=" + profileName + "/vls";
}

std::string BuildEtsProfilePriorityGroupsPath(const std::string &profileName)
{
    return LCNE_ETS_URI + "/ets-profile=" + profileName + "/priority-groups";
}

UbseResult SendPatchNoContent(const std::string &path, const std::string &body, const std::string &operation)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;

    req.method = "PATCH";
    req.path = path;
    req.body = body;

    auto ret = SendEtsRequest(req, rsp, LCNE_ETS_XML);
    if (ret != UBSE_OK) {
        return ret;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_NO_CONTENT)) {
        UBSE_LOG_ERROR << "[MTI] " << operation << " HTTP status error. Status: " << rsp.status;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult SendDeleteNoContent(const std::string &path, const std::string &operation)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;

    req.method = "DELETE";
    req.path = path;

    auto ret = SendEtsRequest(req, rsp, LCNE_ETS_XML);
    if (ret != UBSE_OK) {
        return ret;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_NO_CONTENT)) {
        UBSE_LOG_ERROR << "[MTI] " << operation << " HTTP status error. Status: " << rsp.status;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

} // namespace

UbseResult UbseLcneEts::CreateEtsProfile(const UbseMtiEtsProfile &etsProfile)
{
    std::string body;
    auto ret = BuildEtsProfileXml(etsProfile, body);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] BuildEtsProfileXml failed.";
        return ret;
    }
    return SendPatchNoContent(LCNE_ETS_URI, body, "CreateEtsProfile");
}

UbseResult UbseLcneEts::AddEtsVlsToProfile(const std::string &profileName, const std::vector<UbseEtsVl> &vls)
{
    std::string body;
    UbseMtiEtsProfile etsProfile{};
    etsProfile.profileName = profileName;
    etsProfile.vls = vls;
    auto ret = BuildEtsProfileXml(etsProfile, body);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] BuildEtsProfileXml failed.";
        return ret;
    }
    return SendPatchNoContent(LCNE_ETS_URI, body, "AddEtsVlsToProfile");
}

UbseResult UbseLcneEts::AddEtsPriorityGroupsToProfile(
    const std::string &profileName, const std::vector<UbseEtsPriorityGroup> &priorityGroups)
{
    std::string body;
    UbseMtiEtsProfile etsProfile{};
    etsProfile.profileName = profileName;
    etsProfile.priorityGroups = priorityGroups;
    auto ret = BuildEtsProfileXml(etsProfile, body);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] BuildEtsProfileXml failed.";
        return ret;
    }
    return SendPatchNoContent(LCNE_ETS_URI, body, "AddEtsPriorityGroupsToProfile");
}

UbseResult UbseLcneEts::AddEtsVlsAndPriorityGroupsToProfile(
    const std::string &profileName, const std::vector<UbseEtsVl> &vls,
    const std::vector<UbseEtsPriorityGroup> &priorityGroups)
{
    std::string body;
    UbseMtiEtsProfile etsProfile{};
    etsProfile.profileName = profileName;
    etsProfile.vls = vls;
    etsProfile.priorityGroups = priorityGroups;
    auto ret = BuildEtsProfileXml(etsProfile, body);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] BuildEtsProfileXml failed.";
        return ret;
    }
    return SendPatchNoContent(LCNE_ETS_URI, body, "AddEtsVlsAndPriorityGroupsToProfile");
}

UbseResult UbseLcneEts::DeleteEtsProfile(const std::string &profileName)
{
    return SendDeleteNoContent(BuildEtsProfilePath(profileName), "DeleteEtsProfile");
}

UbseResult UbseLcneEts::RemoveEtsVlsFromProfile(const std::string &profileName)
{
    return SendDeleteNoContent(BuildEtsProfileVlsPath(profileName), "RemoveEtsVlsFromProfile");
}

UbseResult UbseLcneEts::RemoveEtsPriorityGroupsFromProfile(const std::string &profileName)
{
    return SendDeleteNoContent(BuildEtsProfilePriorityGroupsPath(profileName), "RemoveEtsPriorityGroupsFromProfile");
}

UbseResult UbseLcneEts::QueryEtsProfile(const std::string &profileName, UbseMtiEtsProfile &etsProfile)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;

    etsProfile = {};
    req.method = "GET";
    req.path = LCNE_ETS_URI + "/ets-profile=" + profileName;

    auto ret = SendEtsRequest(req, rsp);
    if (ret != UBSE_OK) {
        return ret;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK)) {
        UBSE_LOG_ERROR << "[MTI] QueryEtsProfile HTTP status error. Status: " << rsp.status;
        return UBSE_ERROR;
    }
    if (rsp.body.empty()) {
        UBSE_LOG_WARN << "[MTI] EtsProfile response is empty.";
        return UBSE_MTI_ERROR_NOT_EXIST;
    }
    ret = ParseEtsProfileResponse(rsp.body, etsProfile);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] EtsProfile XML parse failed.";
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseLcneEts::BuildEtsProfileXml(const UbseMtiEtsProfile &etsProfile, std::string &xmlStr)
{
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>();
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Make xml pointer failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }

    ubseXml->AddNode("ets-profiles");
    ubseXml->AddNode("ets-profile");
    std::shared_ptr<UbseXml> etsProfileXml = ubseXml->Next("ets-profile");
    AddTextNode(etsProfileXml, "name", etsProfile.profileName);
    if (!etsProfile.vls.empty()) {
        auto ret = BuildEtsProfileVlsNode(etsProfileXml, etsProfile.vls);
        if (ret != UBSE_OK) {
            return ret;
        }
    }
    if (!etsProfile.priorityGroups.empty()) {
        auto ret = BuildEtsProfilePriorityGroupsNode(etsProfileXml, etsProfile.priorityGroups);
        if (ret != UBSE_OK) {
            return ret;
        }
    }
    if (ubseXml->Previous() != UbseXmlError::OK) {
        UBSE_LOG_ERROR << "[MTI] XML previous ets-profile node failed.";
        return UBSE_ERROR;
    }

    ubseXml->Printer(xmlStr);
    return UBSE_OK;
}

UbseResult UbseLcneEts::ParseEtsProfileResponse(const std::string &body, UbseMtiEtsProfile &etsProfile)
{
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(body);
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Get ubse xml failed.";
        return UBSE_ERROR_NULLPTR;
    }
    const auto ret = ubseXml->Parse();
    if (ret != UbseXmlError::OK) {
        UBSE_LOG_ERROR << "[MTI] EtsProfile response body parse failed.";
        return UBSE_ERROR;
    }

    if (ubseXml->Name() != "ub-qos") {
        UBSE_LOG_ERROR << "[MTI] Unexpected EtsProfile root node: " << ubseXml->Name();
        return UBSE_ERROR;
    }
    if (ubseXml->Next("ets-profiles") == nullptr) {
        UBSE_LOG_WARN << "[MTI] XML node ets-profiles not found.";
        return UBSE_MTI_ERROR_NOT_EXIST;
    }
    if (ubseXml->Next("ets-profile") == nullptr) {
        UBSE_LOG_WARN << "[MTI] XML node ets-profile not found.";
        return UBSE_MTI_ERROR_NOT_EXIST;
    }

    UbseMtiEtsProfile parsedProfile{};
    auto result = ParseEtsProfileXml(ubseXml, parsedProfile);
    if (result != UBSE_OK) {
        return result;
    }
    etsProfile = std::move(parsedProfile);
    return UBSE_OK;
}

} // namespace ubse::lcne
