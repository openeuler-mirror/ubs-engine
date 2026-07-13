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

#include "ubse_ssu_http_handler.h"

#include <string>
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "ubse_error.h"
#include "ubse_http_common.h"
#include "ubse_json_util.h"
#include "ubse_logger.h"
#include "ubse_ssu_service.h"
#include "framework/vip/ubse_vip_http_server.h"

UBSE_DEFINE_THIS_MODULE("ubse");

namespace ubse::ssu::http_handler {
using namespace ubse::common::def;
using namespace ubse::http;
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::plugin::service::ssu;

namespace {
constexpr const char *SSU_SPACES_URL = "/ubse/v1/ssu/spaces";
constexpr const char *SSU_SPACES_PREFIX_URL = "/ubse/v1/ssu/spaces/";
// SSU_NAME_MAX_LEN/SSU_TENANT_MAX_LEN 为底层C风格缓冲区大小，含'\0'，实际有效字符数需减1
constexpr uint32_t SSU_NAME_MAX_LEN = 48;
constexpr uint32_t SSU_TENANT_MAX_LEN = 17;
constexpr uint32_t SSU_CN_MAX_LEN = 64;

// 校验cn(用户名)字符集：小写字母(a-z)、数字(0-9)、连字符(-)，最大64字符
bool IsValidUserName(const std::string &name)
{
    if (name.empty() || name.size() > SSU_CN_MAX_LEN) {
        return false;
    }
    for (char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            return false;
        }
    }
    return true;
}

// 从mTLS对端证书中提取调用方身份信息
bool BuildIdentityFromCert(const UbseHttpRequest &req, UbseSsuAllocIdentityInfo &identity)
{
    if (!req.peerCert.present) {
        UBSE_LOG_ERROR << "Peer certificate not present, cannot build identity";
        return false;
    }

    const auto &cn = req.peerCert.cn;
    const auto &ou = req.peerCert.ou;
    bool hasCn = !cn.empty();
    bool hasOu = !ou.empty();

    if (!hasCn && !hasOu) {
        UBSE_LOG_ERROR << "Both CN and OU are empty in peer certificate";
        return false;
    }

    // cn作为username，校验字符集
    if (hasCn) {
        if (!IsValidUserName(cn)) {
            UBSE_LOG_ERROR << "Invalid CN format (allowed: a-z, 0-9, '-', max " << SSU_CN_MAX_LEN << ")";
            return false;
        }
        identity.userName = cn;
    }

    // ou作为uid（整数）
    if (hasOu) {
        try {
            identity.uid = static_cast<uid_t>(std::stoul(ou));
        } catch (...) {
            UBSE_LOG_ERROR << "Failed to parse OU as uid, ou=" << ou;
            return false;
        }
    }
    return true;
}

// 构建错误响应JSON: {"code":<code>, "msg":"<msg>"}
void BuildErrorResponse(UbseHttpResponse &resp, int httpStatus, uint32_t code, const std::string &msg)
{
    rapidjson::Document doc;
    doc.SetObject();
    auto &allocator = doc.GetAllocator();
    doc.AddMember("code", code, allocator);
    doc.AddMember("msg", rapidjson::Value(msg.c_str(), allocator), allocator);
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    resp.body = buffer.GetString();
    resp.status = httpStatus;
}

// 序列化命名空间信息为JSON对象
void SerializeNameSpaceInfo(rapidjson::Value &nsObj, rapidjson::Document::AllocatorType &allocator,
                            const UbseSsuNameSpaceInfo &ns)
{
    nsObj.AddMember("tgtEid", rapidjson::Value(ns.tgtEid.c_str(), allocator), allocator);
    nsObj.AddMember("tgtNqn", rapidjson::Value(ns.tgtNqn.c_str(), allocator), allocator);
    // defaultHostNqn: 取允许主机列表中的第一个，无则为空
    std::string defaultHostNqn;
    if (!ns.allowHostNqnList.empty()) {
        defaultHostNqn = ns.allowHostNqnList.front();
    }
    nsObj.AddMember("defaultHostNqn", rapidjson::Value(defaultHostNqn.c_str(), allocator), allocator);
    nsObj.AddMember("nsUuid", rapidjson::Value(ns.nsUuid.c_str(), allocator), allocator);
    nsObj.AddMember("namespaceId", ns.namespaceId, allocator);
    nsObj.AddMember("nsDevPath", rapidjson::Value(ns.nsDevPath.c_str(), allocator), allocator);
    nsObj.AddMember("nsSize", ns.nsSize, allocator);
    nsObj.AddMember("lbaFormat", static_cast<uint32_t>(ns.lbaFormat), allocator);
}

// 解析并校验AllocSpace请求体，成功返回UBSE_OK，失败返回错误码并填充errMsg
uint32_t ParseAllocSpaceBody(const rapidjson::Document &doc, UbseSsuAllocSpaceReq &allocReq, std::string &errMsg)
{
    uint32_t lbaFormat = 0;
    uint8_t strategy = 0;

    if (UbseJsonUtil::GetStrFromJsonPtr(doc, "name", allocReq.name) != UBSE_OK) {
        errMsg = "missing or invalid field: name";
        return UBSE_ERR_INVALID_ARG;
    }
    // SSU_NAME_MAX_LEN含'\0'，实际有效字符上限为SSU_NAME_MAX_LEN-1
    if (allocReq.name.size() > SSU_NAME_MAX_LEN - 1) {
        errMsg = "name exceeds max length 47";
        return UBSE_ERR_INVALID_ARG;
    }
    if (UbseJsonUtil::GetUint64FromJsonPtr(doc, "nsSize", allocReq.nsSize) != UBSE_OK) {
        errMsg = "missing or invalid field: nsSize";
        return UBSE_ERR_INVALID_ARG;
    }
    if (UbseJsonUtil::GetUintFromJsonPtr(doc, "nsNum", allocReq.nsNum) != UBSE_OK || allocReq.nsNum == 0) {
        errMsg = "missing or invalid field: nsNum";
        return UBSE_ERR_INVALID_ARG;
    }
    if (UbseJsonUtil::GetUintFromJsonPtr(doc, "lbaFormat", lbaFormat) != UBSE_OK) {
        errMsg = "missing or invalid field: lbaFormat";
        return UBSE_ERR_INVALID_ARG;
    }
    if (lbaFormat != static_cast<uint32_t>(UbseSsuLBAFormat::LBA_FORMAT_512) &&
        lbaFormat != static_cast<uint32_t>(UbseSsuLBAFormat::LBA_FORMAT_4K)) {
        errMsg = "lbaFormat must be 512 or 4096";
        return UBSE_ERR_INVALID_ARG;
    }
    if (UbseJsonUtil::GetUint8FromJsonPtr(doc, "strategy", strategy) != UBSE_OK) {
        errMsg = "missing or invalid field: strategy";
        return UBSE_ERR_INVALID_ARG;
    }
    if (strategy > static_cast<uint8_t>(UbseSsuAllocStrategy::LINEAR)) {
        errMsg = "strategy must be 0 or 1";
        return UBSE_ERR_INVALID_ARG;
    }
    allocReq.lbaFormat = static_cast<UbseSsuLBAFormat>(lbaFormat);
    allocReq.strategy = static_cast<UbseSsuAllocStrategy>(strategy);

    // 可选字段 tenant
    if (doc.HasMember("tenant") && doc["tenant"].IsString()) {
        allocReq.tenant = doc["tenant"].GetString();
        // SSU_TENANT_MAX_LEN含'\0'，实际有效字符上限为SSU_TENANT_MAX_LEN-1
        if (allocReq.tenant.size() > SSU_TENANT_MAX_LEN - 1) {
            errMsg = "tenant exceeds max length 16";
            return UBSE_ERR_INVALID_ARG;
        }
    }
    return UBSE_OK;
}

// 构建AllocSpace成功响应: 201 Created
void BuildAllocSpaceSuccessResp(UbseHttpResponse &resp, const UbseSsuAllocResult &result)
{
    rapidjson::Document respDoc;
    respDoc.SetObject();
    auto &allocator = respDoc.GetAllocator();
    respDoc.AddMember("code", static_cast<uint32_t>(UBSE_OK), allocator);
    respDoc.AddMember("msg", rapidjson::Value("success", allocator), allocator);

    rapidjson::Value data(rapidjson::kObjectType);
    data.AddMember("name", rapidjson::Value(result.name.c_str(), allocator), allocator);
    data.AddMember("strategy", static_cast<uint8_t>(result.strategy), allocator);

    rapidjson::Value nsList(rapidjson::kArrayType);
    for (const auto &ns : result.nameSpaceList) {
        rapidjson::Value nsObj(rapidjson::kObjectType);
        SerializeNameSpaceInfo(nsObj, allocator, ns);
        nsList.PushBack(nsObj, allocator);
    }
    data.AddMember("nameSpaceList", nsList, allocator);
    respDoc.AddMember("data", data, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    respDoc.Accept(writer);
    resp.body = buffer.GetString();
    resp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_CREATED);
}

// AllocSpace: POST /ubse/v1/ssu/spaces
uint32_t AllocSpaceHandler(const UbseHttpRequest &req, UbseHttpResponse &resp)
{
    UBSE_LOG_INFO << "AllocSpace request received, path=" << req.path;

    // 身份认证：从mTLS对端证书提取调用方信息
    UbseSsuAllocIdentityInfo identity;
    if (!BuildIdentityFromCert(req, identity)) {
        BuildErrorResponse(resp, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_UNAUTH),
                           UBSE_ERR_AUTHENTICATION_FAILED, "authentication failed: invalid peer certificate");
        return UBSE_ERR_AUTHENTICATION_FAILED;
    }

    // 解析请求体
    rapidjson::Document doc;
    doc.Parse(req.body.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        UBSE_LOG_ERROR << "AllocSpace: invalid request body, parse error";
        BuildErrorResponse(resp, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_BAD_REQ),
                           UBSE_ERR_INVALID_ARG, "invalid request body");
        return UBSE_ERR_INVALID_ARG;
    }

    UbseSsuAllocSpaceReq allocReq{};
    std::string errMsg;
    auto ret = ParseAllocSpaceBody(doc, allocReq, errMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AllocSpace: " << errMsg;
        BuildErrorResponse(resp, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_BAD_REQ), ret, errMsg);
        return ret;
    }

    // 获取SSU服务并调用AllocSpace
    auto ssuService = GetSsuService();
    if (ssuService == nullptr) {
        UBSE_LOG_ERROR << "AllocSpace: ssu service is not available";
        BuildErrorResponse(resp, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_SERVICE_UNVAILABLE),
                           UBSE_ERR_IPC_SERVICE_UNAVAILABLE, "ssu service unavailable");
        return UBSE_ERR_IPC_SERVICE_UNAVAILABLE;
    }

    UbseSsuAllocResult result;
    ret = ssuService->AllocSpace(allocReq, identity, result);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AllocSpace failed, name=" << allocReq.name << ", ret=" << ret;
        int httpStatus = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_INTERNAL_SVR_ERR);
        if (ret == UBSE_ERR_EXISTED) {
            httpStatus = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_CONFICT);
        }
        BuildErrorResponse(resp, httpStatus, ret, "alloc space failed");
        return ret;
    }

    BuildAllocSpaceSuccessResp(resp, result);
    UBSE_LOG_INFO << "AllocSpace success, name=" << allocReq.name;
    return UBSE_OK;
}

// FreeSpace: DELETE /ubse/v1/ssu/spaces/{name}
uint32_t FreeSpaceHandler(const UbseHttpRequest &req, UbseHttpResponse &resp)
{
    // 从路径提取name: /ubse/v1/ssu/spaces/{name}
    std::string name;
    if (req.path.size() > strlen(SSU_SPACES_PREFIX_URL)) {
        name = req.path.substr(strlen(SSU_SPACES_PREFIX_URL));
    }
    UBSE_LOG_INFO << "FreeSpace request received, name=" << name;

    if (name.empty()) {
        BuildErrorResponse(resp, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_BAD_REQ),
                           UBSE_ERR_INVALID_ARG, "missing path parameter: name");
        return UBSE_ERR_INVALID_ARG;
    }
    // 与AllocSpaceHandler保持一致：SSU_NAME_MAX_LEN含'\0'，实际有效字符上限为SSU_NAME_MAX_LEN-1
    if (name.size() > SSU_NAME_MAX_LEN - 1) {
        BuildErrorResponse(resp, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_BAD_REQ),
                           UBSE_ERR_INVALID_ARG, "name exceeds max length 47");
        return UBSE_ERR_INVALID_ARG;
    }

    // 身份认证：从mTLS对端证书提取调用方信息
    UbseSsuAllocIdentityInfo identity;
    if (!BuildIdentityFromCert(req, identity)) {
        BuildErrorResponse(resp, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_UNAUTH),
                           UBSE_ERR_AUTHENTICATION_FAILED, "authentication failed: invalid peer certificate");
        return UBSE_ERR_AUTHENTICATION_FAILED;
    }

    // 获取SSU服务并调用FreeSpace
    auto ssuService = GetSsuService();
    if (ssuService == nullptr) {
        UBSE_LOG_ERROR << "FreeSpace: ssu service is not available";
        BuildErrorResponse(resp, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_SERVICE_UNVAILABLE),
                           UBSE_ERR_IPC_SERVICE_UNAVAILABLE, "ssu service unavailable");
        return UBSE_ERR_IPC_SERVICE_UNAVAILABLE;
    }

    auto ret = ssuService->FreeSpace(name, identity);
    // 释放不存在的空间返回成功（幂等性保证）
    if (ret != UBSE_OK && ret != UBSE_ERR_NOT_EXIST) {
        UBSE_LOG_ERROR << "FreeSpace failed, name=" << name << ", ret=" << ret;
        BuildErrorResponse(resp, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_INTERNAL_SVR_ERR),
                           ret, "free space failed");
        return ret;
    }

    // 构建成功响应: 200 OK
    rapidjson::Document respDoc;
    respDoc.SetObject();
    auto &allocator = respDoc.GetAllocator();
    respDoc.AddMember("code", static_cast<uint32_t>(UBSE_OK), allocator);
    respDoc.AddMember("msg", rapidjson::Value("success", allocator), allocator);
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    respDoc.Accept(writer);
    resp.body = buffer.GetString();
    resp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    UBSE_LOG_INFO << "FreeSpace success, name=" << name;
    return UBSE_OK;
}
} // namespace

UbseResult RegisterSsuHttpHandlers()
{
    // 注册分配存储空间接口: POST /ubse/v1/ssu/spaces
    // 北向HTTP走VIP HTTP Server，路由在主节点切换时由VipManager应用到UbseHttpServer
    auto ret = ubse::vip::RegVipHttpService(UbseHttpMethod::UBSE_HTTP_METHOD_POST, SSU_SPACES_URL, AllocSpaceHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to register AllocSpace handler, ret=" << ret;
        return ret;
    }

    // 注册释放存储空间接口: DELETE /ubse/v1/ssu/spaces/{name}
    // 使用前缀路由（路径以'/'结尾），由HTTP Server的prefix匹配机制处理动态路径参数
    ret = ubse::vip::RegVipHttpService(UbseHttpMethod::UBSE_HTTP_METHOD_DELETE, SSU_SPACES_PREFIX_URL,
                                       FreeSpaceHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to register FreeSpace handler, ret=" << ret;
        return ret;
    }

    UBSE_LOG_INFO << "SSU http handlers registered successfully";
    return UBSE_OK;
}
} // namespace ubse::ssu::http_handler
