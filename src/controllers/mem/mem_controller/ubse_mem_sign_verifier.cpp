/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_mem_sign_verifier.h"

#include <chrono>
#include <thread>
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_json_util.h"
#include "ubse_logger.h"
#include "ubse_sign_token_bucket.h"
#include "ubse_str_util.h"
#include "src/framework/vscok/ubse_vsock_client.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace rapidjson;
using namespace log;
using namespace ubse::utils;
constexpr uint32_t REQ_TYPE_SIGN = 0x10;
constexpr uint32_t REQ_TYPE_VERIFY_AND_SIGN = 0x0012;
constexpr uint32_t TO_SIGN_DATA_SIZE = 20; // 8 + 8 + 4
constexpr uint32_t RETRY_INTERVAL_MS = 5;  // 重试间隔
constexpr uint32_t MAX_RETRY_COUNT = 3;    // 最多重试3次
static uint32_t g_requestId = 0;
ReadWriteLock requestIdLock;

class Base64 {
public:
    Base64() : bufferSize(0) {}

    void Begin(const size_t n)
    {
        buffer.clear();
        buffer.reserve(n);
        bufferSize = n;
    }

    void Append(const uint32_t value)
    {
        if (buffer.size() > bufferSize || 4 > bufferSize - buffer.size()) {
            throw std::overflow_error("Buffer overflow");
        }
        for (int i = 3; i >= 0; --i) {
            buffer.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }

    void Append(const uint64_t value)
    {
        if (buffer.size() + 8 > bufferSize) {
            throw std::overflow_error("Buffer overflow");
        }
        for (int i = 7; i >= 0; --i) {
            buffer.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }

    void Append(const std::string_view s)
    {
        if (buffer.size() + s.size() > bufferSize) {
            throw std::overflow_error("Buffer overflow");
        }
        buffer.insert(buffer.end(),
                      reinterpret_cast<const uint8_t*>(s.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                      reinterpret_cast<const uint8_t*>(s.data()) +
                          s.size()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }

    [[nodiscard]] std::string Generate() const
    {
        static constexpr char kBase64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                               "abcdefghijklmnopqrstuvwxyz"
                                               "0123456789+/";
        const size_t len = buffer.size();
        std::string encoded;
        encoded.reserve(((len + 2) / 3) * 4);

        size_t i = 0;
        while (i < len) {
            const uint32_t b0 = buffer[i++];
            const bool has_b1 = (i < len);
            const uint32_t b1 = has_b1 ? buffer[i++] : 0;
            const bool has_b2 = (i < len);
            const uint32_t b2 = has_b2 ? buffer[i++] : 0;

            const uint32_t triplet = (b0 << 16) | (b1 << 8) | b2;

            encoded.push_back(kBase64Chars[(triplet >> 18) & 0x3F]);
            encoded.push_back(kBase64Chars[(triplet >> 12) & 0x3F]);

            encoded.push_back(has_b1 ? kBase64Chars[(triplet >> 6) & 0x3F] : '=');
            encoded.push_back(has_b2 ? kBase64Chars[triplet & 0x3F] : '=');
        }

        return encoded;
    }

private:
    std::vector<uint8_t> buffer;
    size_t bufferSize;
};

UbseResult GetRequestId()
{
    requestIdLock.LockWrite();
    const uint32_t id = g_requestId++;
    requestIdLock.UnLock();
    return id;
}

UbseResult ParseResponse(const std::string& rspJson, std::string& signedData, std::string& trustRingId,
                         bool isShared = false)
{
    Document doc;
    doc.Parse(rspJson.c_str());
    if (doc.HasParseError()) {
        UBSE_LOG_ERROR << "Parse response json failed, error str is " << rspJson.c_str() << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    if (!isShared) {
        int result{};
        if (const UbseResult res = UbseJsonUtil::GetIntFromJsonPtr(doc, "result", result); res != UBSE_OK) {
            UBSE_LOG_ERROR << "[MTI_MEM] Parse result failed, " << FormatRetCode(res);
            return res;
        }
        if (result != 0) {
            UBSE_LOG_ERROR << "[MTI_MEM] Trust operation failed, the result of response is " << result << ", "
                           << FormatRetCode(UBSE_ERROR);
            return UBSE_ERROR;
        }
    }

    if (const UbseResult res = UbseJsonUtil::GetStrFromJsonPtr(doc, "signed_data", signedData) != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse signed_data failed, " << FormatRetCode(res);
        return res;
    }

    if (const UbseResult res = UbseJsonUtil::GetStrFromJsonPtr(doc, "id", trustRingId) != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse id failed, " << FormatRetCode(res);
        return res;
    }

    return UBSE_OK;
}

std::string StrToBase64(const std::string_view s)
{
    Base64 enc{};
    enc.Begin(s.size());
    enc.Append(s);
    return enc.Generate();
}

UbseResult UbseMemSignVerifier::Sign(const std::string& type, std::string& signedData, std::string& trustRingId)
{
    auto token = UbseSignTokenBucket::GetInstance().Acquire();
    vsock::UbseVsockClient client;
    if (!client.Connect()) {
        return UBSE_ERROR;
    }
    for (uint32_t retryCount = 0; retryCount <= MAX_RETRY_COUNT; ++retryCount) {
        Document doc;
        doc.SetObject();
        Document::AllocatorType& allocator = doc.GetAllocator();
        Value toSign(kObjectType);
        std::string base64Type = StrToBase64(type);
        toSign.AddMember("data", Value(base64Type.c_str(), allocator), allocator);
        doc.AddMember("to-sign", toSign, allocator);
        StringBuffer buffer;
        Writer writer(buffer);
        doc.Accept(writer);

        vsock::UbseSignReq signReq{GetRequestId(), REQ_TYPE_SIGN, buffer.GetString()};
        vsock::UbseSignRsp rsp{};

        if (const auto ret = client.UbseVsockSend(signReq, rsp); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[MTI_MEM] Send request failed, " << FormatRetCode(ret);
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_INTERVAL_MS));
            continue;
        }
        if (const auto ret = ParseResponse(rsp.signedData, signedData, trustRingId); ret == UBSE_OK) {
            return UBSE_OK;
        }
    }
    return UBSE_ERR_DAEMON_BUSY;
}

std::string BuildRawData(const std::string_view type, const UbseMemObmmInfo& exportObmmInfo)
{
    Base64 enc{};
    enc.Begin(type.size() + TO_SIGN_DATA_SIZE);
    enc.Append(type);
    enc.Append(exportObmmInfo.desc.addr);
    enc.Append(exportObmmInfo.desc.length);
    enc.Append(exportObmmInfo.desc.tokenid);

    return enc.Generate();
}

UbseResult UbseMemSignVerifier::SignAndVerify(const UbseExportSignReq& signReq,
                                              std::vector<std::string>& lendSignedDatas)
{
    auto token = UbseSignTokenBucket::GetInstance().Acquire();
    vsock::UbseVsockClient client;
    if (!client.Connect()) {
        return UBSE_ERROR;
    }
    for (const auto exportObmmInfo : signReq.exportObmmInfo) {
        std::string lendSignedData{};
        // 构造请求
        Document doc;
        doc.SetObject();
        Document::AllocatorType& allocator = doc.GetAllocator();

        Value toVerify(kObjectType);
        std::string base64Type = StrToBase64(signReq.type);
        toVerify.AddMember("data", Value(base64Type.c_str(), allocator), allocator);
        toVerify.AddMember("signed_data", Value(signReq.reqSignedData.c_str(), allocator), allocator);
        toVerify.AddMember("id", Value(signReq.trustRingId.c_str(), allocator), allocator);
        doc.AddMember("to-verify", toVerify, allocator);

        Value toSign(kObjectType);
        std::string rawData = BuildRawData(signReq.type, exportObmmInfo);
        toSign.AddMember("data", Value(rawData.c_str(), allocator), allocator);
        toSign.AddMember("id", Value(signReq.trustRingId.c_str(), allocator), allocator);
        doc.AddMember("to-sign", toSign, allocator);

        StringBuffer buffer;
        Writer writer(buffer);
        doc.Accept(writer);

        // 发送请求
        vsock::UbseSignReq vsockReq{GetRequestId(), REQ_TYPE_VERIFY_AND_SIGN, buffer.GetString()};
        auto ret = UBSE_ERROR;
        for (uint32_t retryCount = 0; retryCount <= MAX_RETRY_COUNT; ++retryCount) {
            vsock::UbseSignRsp rsp{};
            if (ret = client.UbseVsockSend(vsockReq, rsp); ret != UBSE_OK) {
                UBSE_LOG_ERROR << "[MTI_MEM] Send request failed, " << FormatRetCode(ret);
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_INTERVAL_MS));
                continue;
            }
            std::string trustChainId;
            if (ret = ParseResponse(rsp.signedData, lendSignedData, trustChainId); ret != UBSE_OK) {
                continue;
            }
            lendSignedDatas.emplace_back(lendSignedData);
            break;
        }
        if (ret != UBSE_OK) {
            return ret;
        }
    }
    return UBSE_OK;
}

bool IsHighSafety()
{
    auto ubseConfModule = context::UbseContext::GetInstance().GetModule<config::UbseConfModule>();
    if (ubseConfModule == nullptr) {
        UBSE_LOG_ERROR << "Get config info failed, " << FormatRetCode(UBSE_ERROR_MODULE_LOAD_FAILED);
        return false;
    }

    uint32_t intCid;
    if (auto ret = ubseConfModule->GetConf<uint32_t>("ubse.ubfm", "ubm.server.cid", intCid); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to get safe config, will use default value false, " << FormatRetCode(ret);
        return false;
    }

    return true;
}
} // namespace ubse::mem::controller