/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "ubse_lcne_qos.h"
#include "ubse_error.h"
#include "ubse_http_module.h" // for UbseHttpModule
#include "ubse_logger.h"
#include "ubse_pointer_process.h"
#include "ubse_xml.h"

namespace ubse::lcne {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::utils;

const std::string LCNE_QOS_MODE = "dwrr";
const std::string LCNE_QOS_URI = "/restconf/data/huawei-vbussw-service:"
                                 "vbussw-service/ub-function/tqos-entity-profile-applys";
const std::string LCNE_QOS_PROFILE_URI = "/restconf/data/huawei-ub-qos:ub-qos/tqos-profiles";
const std::string LCNE_QOS_ACCEPT = "application/yang-data+xml";
const std::string LCNE_QOS_CONTENT_TYPE = "application/yang-data+xml";
const std::string LCNE_QOS_XML = "urn:huawei:yang:huawei-vbussw-service";
const std::string LCNE_QOS_PROFILE_XML = "urn:huawei:yang:huawei-ub-qos";
const uint32_t MAX_LCNE_QOS_CBS = 4;
const uint32_t MAX_LCNE_QOS_PBS = 4;
const uint32_t LCNE_QOS_WEIGHT = 12;
const uint32_t ONE_K = 1024;
const uint32_t BYTE_TO_BIT = 8;

UbseResult UbseLcneQos::CreateQosProfile(UbseMtiQosProfile ubseLcneQosProfile)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    std::string body;

    req.method = "POST";
    req.path = LCNE_QOS_PROFILE_URI;
    req.headers.emplace("Accept", LCNE_QOS_ACCEPT);
    req.headers.emplace("Content-Type", LCNE_QOS_CONTENT_TYPE);
    auto ret = BuildQoSProfileXml(ubseLcneQosProfile, body);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] buildQoSProfileXml failed. ";
        return ret;
    }
    req.body = body;

    ret = UbseHttpModule::HttpSend(req, rsp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] UbseHttpModule::HttpSend failed. " << FormatRetCode(ret);
        return ret;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_CREATED)) {
        UBSE_LOG_ERROR << "[MTI] UbseHttpStatusCode is error."
                       << "The HTTP status code is " << rsp.status;
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

UbseResult UbseLcneQos::DeleteQosProfile(std::string profileName)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;

    req.method = "DELETE";
    req.path = LCNE_QOS_PROFILE_URI + "/tqos-profile=" + profileName;
    req.headers.emplace("Accept", LCNE_QOS_ACCEPT);
    req.headers.emplace("Content-Type", LCNE_QOS_CONTENT_TYPE);

    auto ret = UbseHttpModule::HttpSend(req, rsp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] UbseHttpModule::HttpSend failed. " << FormatRetCode(ret);
        return ret;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_NO_CONTENT)) {
        UBSE_LOG_ERROR << "[MTI] UbseHttpStatusCode is error."
                       << "The HTTP status code is " << rsp.status;
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

UbseResult UbseLcneQos::QueryQosProfile(std::string profileName, UbseMtiQosProfile &ubseLcneQosProfile)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;

    req.method = "GET";
    req.path = LCNE_QOS_PROFILE_URI + "/tqos-profile=" + profileName;
    req.headers.emplace("Accept", LCNE_QOS_ACCEPT);
    req.headers.emplace("Content-Type", LCNE_QOS_CONTENT_TYPE);

    auto ret = UbseHttpModule::HttpSend(req, rsp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] UbseHttpModule::HttpSend failed. " << FormatRetCode(ret);
        return ret;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK)) {
        UBSE_LOG_ERROR << "[MTI] UbseHttpStatusCode is error."
                       << "The HTTP status code is " << rsp.status;
        return UBSE_ERROR;
    }
    if (rsp.body.empty()) {
        UBSE_LOG_ERROR << "[MTI] QosProfile response is empty.";
        return UBSE_ERROR;
    }
    // 解析xml响应
    ret = ParseQosProfileResponse(rsp.body, ubseLcneQosProfile);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] QosProfile Info Parse XML data is failed.";
        return UBSE_ERROR;
    }
    ubseLcneQosProfile.profileName = profileName;
    return UBSE_OK;
}

UbseResult UbseLcneQos::ApplyVfeQos(UbseMtiFeInfo ubseFeInfo, std::string profileName)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    std::string body;

    req.method = "POST";
    req.path = LCNE_QOS_URI;
    req.headers.emplace("Accept", LCNE_QOS_ACCEPT);
    req.headers.emplace("Content-Type", LCNE_QOS_CONTENT_TYPE);
    auto ret = BuildQoSXml(ubseFeInfo, profileName, body);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] buildQoSProfileXml failed. ";
        return ret;
    }
    req.body = body;

    ret = UbseHttpModule::HttpSend(req, rsp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] UbseHttpModule::HttpSend failed. " << FormatRetCode(ret);
        return ret;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_CREATED)) {
        UBSE_LOG_ERROR << "[MTI] UbseHttpStatusCode is error."
                       << "The HTTP status code is " << rsp.status;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseLcneQos::DeleteVfeQos(UbseMtiFeInfo ubseFeInfo)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;

    std::string profileApply = "/tqos-entity-profile-apply=" + ubseFeInfo.slotId + "," + ubseFeInfo.ubpuId + "," +
                               ubseFeInfo.iouId + "," + ubseFeInfo.entityId;
    req.method = "DELETE";
    req.path = LCNE_QOS_URI + profileApply;
    req.headers.emplace("Accept", LCNE_QOS_ACCEPT);
    req.headers.emplace("Content-Type", LCNE_QOS_CONTENT_TYPE);

    auto ret = UbseHttpModule::HttpSend(req, rsp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] UbseHttpModule::HttpSend failed. " << FormatRetCode(ret);
        return ret;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_NO_CONTENT)) {
        UBSE_LOG_ERROR << "[MTI] UbseHttpStatusCode is error."
                       << "The HTTP status code is " << rsp.status;
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

UbseResult UbseLcneQos::QueryVfeQos(UbseMtiFeInfo ubseFeInfo, std::string &profileName)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;

    req.method = "GET";
    req.path = LCNE_QOS_URI + "/tqos-entity-profile-apply=" + ubseFeInfo.slotId + "," + ubseFeInfo.ubpuId + "," +
               ubseFeInfo.iouId + "," + ubseFeInfo.entityId;
    req.headers.emplace("Accept", LCNE_QOS_ACCEPT);
    req.headers.emplace("Content-Type", LCNE_QOS_CONTENT_TYPE);

    auto ret = UbseHttpModule::HttpSend(req, rsp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] UbseHttpModule::HttpSend failed. " << FormatRetCode(ret);
        return ret;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK)) {
        UBSE_LOG_ERROR << "[MTI] UbseHttpStatusCode is error."
                       << "The HTTP status code is " << rsp.status;
        return UBSE_ERROR;
    }
    if (rsp.body.empty()) {
        UBSE_LOG_ERROR << "[MTI] VfeQos response is empty.";
        return UBSE_ERROR;
    }
    // 解析xml响应
    ret = ParseVfeQosResponse(rsp.body, profileName);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] VfeQos Info Parse XML data is failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseLcneQos::BuildQoSProfileXml(UbseMtiQosProfile ubseLcneQosProfile, std::string &xmlStr)
{
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>();
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI_MEM] Make xml pointer failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    ubseXml->AddNode("tqos-profile");
    ubseXml->Attr("xmlns", LCNE_QOS_PROFILE_XML);
    ubseXml->AddNode("name");
    ubseXml->Child("name")->Text(ubseLcneQosProfile.profileName);
    ubseXml->AddNode("schedule");
    std::shared_ptr<UbseXml> scheduleXml = ubseXml->Next("schedule");
    scheduleXml->AddNode("mode");
    scheduleXml->Child("mode")->Text(LCNE_QOS_MODE);
    scheduleXml->AddNode("weight");
    scheduleXml->Child("weight")->Text(std::to_string(LCNE_QOS_WEIGHT));
    ubseXml->Previous();
    ubseXml->AddNode("shaper");
    std::shared_ptr<UbseXml> shaperXml = ubseXml->Next("shaper");
    // 北向传入带宽为Mbps， 南向单位为MBps
    uint32_t cir = ubseLcneQosProfile.minBandWidth / BYTE_TO_BIT;
    uint32_t pir = ubseLcneQosProfile.maxBandWidth / BYTE_TO_BIT;

    // 如果cir/pir小于4MBps时,cbs/pbs等于cir/pir，否则取4M
    uint32_t cbs = MAX_LCNE_QOS_CBS * ONE_K * ONE_K;
    if (cir < MAX_LCNE_QOS_CBS) {
        cbs = cir * ONE_K * ONE_K;
    }
    uint32_t pbs = MAX_LCNE_QOS_CBS * ONE_K * ONE_K;
    if (pir < MAX_LCNE_QOS_CBS) {
        pbs = pir * ONE_K * ONE_K;
    }
    shaperXml->AddNode("cir");
    shaperXml->Child("cir")->Text(std::to_string(cir));
    shaperXml->AddNode("cbs");
    shaperXml->Child("cbs")->Text(std::to_string(cbs));
    shaperXml->AddNode("pir");
    shaperXml->Child("pir")->Text(std::to_string(pir));
    shaperXml->AddNode("pbs");
    shaperXml->Child("pbs")->Text(std::to_string(pbs));
    ubseXml->Printer(xmlStr);

    return UBSE_OK;
}

UbseResult UbseLcneQos::BuildQoSXml(UbseMtiFeInfo ubseFeInfo, std::string profileName, std::string &xmlStr)
{
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>();
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI_MEM] Make xml pointer failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    ubseXml->AddNode("tqos-entity-profile-apply");
    ubseXml->Attr("xmlns", LCNE_QOS_XML);
    ubseXml->AddNode("slot-id");
    ubseXml->Child("slot-id")->Text(ubseFeInfo.slotId);
    ubseXml->AddNode("ubpu-id");
    ubseXml->Child("ubpu-id")->Text(ubseFeInfo.ubpuId);
    ubseXml->AddNode("iou-id");
    ubseXml->Child("iou-id")->Text(ubseFeInfo.iouId);
    ubseXml->AddNode("entity-id");
    ubseXml->Child("entity-id")->Text(ubseFeInfo.entityId);
    ubseXml->AddNode("profile-name");
    ubseXml->Child("profile-name")->Text(profileName);
    ubseXml->Printer(xmlStr);

    return UBSE_OK;
}
UbseResult UbseLcneQos::ParseQosProfileResponse(std::string body, UbseMtiQosProfile &ubseLcneQosProfile)
{
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(body);
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Get ubse xml failed.";
        return UBSE_ERROR;
    }
    const auto ret = ubseXml->Parse();
    if (ret != UbseXmlError::OK) {
        UBSE_LOG_ERROR << "[MTI] QosProfile resBody parse failed.";
        return UBSE_ERROR;
    }
    ubseXml = ubseXml->Next("shaper");
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Xml parse cir failed.";
        return UBSE_ERROR;
    }
    if (ubseXml->Next("cir") == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Xml parse cir failed.";
        return UBSE_ERROR;
    }
    std::string minBandWidthStr = ubseXml->Text();
    ubseXml->Previous();
    if (ubseXml->Next("pir") == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Xml parse pir failed.";
        return UBSE_ERROR;
    }
    std::string maxBandWidthStr = ubseXml->Text();
    try {
        ubseLcneQosProfile.minBandWidth = std::stoul(minBandWidthStr) * BYTE_TO_BIT;
        ubseLcneQosProfile.maxBandWidth = std::stoul(maxBandWidthStr) * BYTE_TO_BIT;
    } catch (const std::invalid_argument &e) {
        return UBSE_ERROR;
    } catch (const std::out_of_range &e) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseLcneQos::ParseVfeQosResponse(std::string body, std::string &profileName)
{
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(body);
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Get ubse xml failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }
    const auto ret = ubseXml->Parse();
    if (ret != UbseXmlError::OK) {
        UBSE_LOG_ERROR << "[MTI] VfeQos resBody parse failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    ubseXml = ubseXml->Next("profile-name");
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Xml parse cir failed," << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    profileName = ubseXml->Text();
    return UBSE_OK;
}
} // namespace ubse::lcne