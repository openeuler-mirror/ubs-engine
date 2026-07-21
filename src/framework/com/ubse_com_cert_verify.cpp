/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_com_cert_verify.h"

#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/safestack.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <cstdio>
#include <cstring>
#include <string>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "securec.h"

namespace ubse::com {
using namespace ubse::log;
using namespace ubse::common::def;
UBSE_DEFINE_THIS_MODULE("ubse");

constexpr int32_t VERIFY_SUCCESS = 1;
constexpr int32_t VERIFY_FAILED = -1;

// 本节点期望的 nodeid，由 RegisterTLSCallbacks 在初始化时设置，供 CertVerifyCallback 做本端证书自校验。
static std::string g_expectedLocalNodeId;

bool GetLocalCertOtherName(const std::string& certPath, std::string& otherName)
{
    if (certPath.empty()) {
        UBSE_LOG_ERROR << "GetLocalCertOtherName: certPath is empty";
        return false;
    }

    FILE* fp = fopen(certPath.c_str(), "r");
    if (fp == nullptr) {
        UBSE_LOG_ERROR << "GetLocalCertOtherName: failed to open cert file: " << certPath;
        return false;
    }

    X509* cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    if (fclose(fp) != 0) {
        UBSE_LOG_ERROR << "GetLocalCertOtherName: failed to close cert file: " << certPath;
    }
    if (cert == nullptr) {
        UBSE_LOG_ERROR << "GetLocalCertOtherName: failed to read X509 certificate from: " << certPath;
        return false;
    }

    char value[CERT_OTHER_NAME_MAX_LEN] = {0};
    bool ret = GetCertSanOtherName(cert, CERT_OTHER_NAME_OID, value, CERT_OTHER_NAME_MAX_LEN);
    X509_free(cert);

    if (ret) {
        otherName = value;
        UBSE_LOG_INFO << "GetLocalCertOtherName: extracted otherName=" << otherName << " from " << certPath;
    } else {
        UBSE_LOG_ERROR << "GetLocalCertOtherName: failed to extract otherName from certificate";
    }
    return ret;
}

bool LoadCrlToStore(X509_STORE_CTX* ctx, const char* crlPath)
{
    FILE* crlFile = fopen(crlPath, "r");
    if (crlFile == nullptr) {
        UBSE_LOG_ERROR << "LoadCrlToStore: failed to open CRL file: " << crlPath;
        return false;
    }

    X509_CRL* crl = PEM_read_X509_CRL(crlFile, nullptr, nullptr, nullptr);
    if (fclose(crlFile) != 0) {
        UBSE_LOG_ERROR << "LoadCrlToStore: failed to close CRL file: " << crlPath;
    }
    if (crl == nullptr) {
        char errBuf[256] = {0};
        ERR_error_string_n(ERR_get_error(), errBuf, sizeof(errBuf));
        UBSE_LOG_ERROR << "LoadCrlToStore: failed to parse CRL file: " << crlPath << ", err=" << errBuf;
        return false;
    }

    X509_STORE* store = X509_STORE_CTX_get0_store(ctx);
    if (store == nullptr) {
        UBSE_LOG_ERROR << "LoadCrlToStore: failed to get X509 store";
        X509_CRL_free(crl);
        return false;
    }

    X509_STORE_CTX_set_flags(ctx, X509_V_FLAG_CRL_CHECK);
    if (X509_STORE_add_crl(store, crl) != 1) {
        UBSE_LOG_ERROR << "LoadCrlToStore: failed to add CRL to store";
        X509_CRL_free(crl);
        return false;
    }

    X509_CRL_free(crl);
    UBSE_LOG_INFO << "LoadCrlToStore: CRL loaded successfully";
    return true;
}

void SetExpectedLocalNodeId(const std::string& nodeId)
{
    g_expectedLocalNodeId = nodeId;
}

// CertVerifyCallback 在 TLS 握手中被调用（VERIFY_BY_CUSTOM_FUNC 模式下 hcom 全权委托本回调做校验）。
// 仅做证书链 + CRL 校验（mTLS 的密码学基线，确保对端证书由可信 CA 签发且未吊销）。
// 本端证书 otherName 与本节点 id 的一致性，由建链前的自校验（VerifyLocalCertOtherName）保证，
// 此处不再比对任何证书 otherName。

int CertVerifyCallback(void* x509ctx, const char* crlPath)
{
    UBSE_LOG_INFO << "CertVerifyCallback: start, crlPath=" << (crlPath != nullptr ? crlPath : "(none)");

    // 本端证书自校验：本端导入的证书 otherName 必须与本节点 nodeid 一致，
    // 不一致说明证书与本节点不匹配，直接拒绝本次握手，避免使用错误身份的证书建链。
    if (!VerifyLocalCertOtherName(SERVER_CERT_FILENAME, g_expectedLocalNodeId)) {
        UBSE_LOG_ERROR << "CertVerifyCallback: local cert otherName mismatch with node id, reject handshake, nodeId="
                       << g_expectedLocalNodeId;
        return VERIFY_FAILED;
    }

    if (x509ctx == nullptr) {
        UBSE_LOG_ERROR << "CertVerifyCallback: x509ctx is null";
        return VERIFY_FAILED;
    }

    X509_STORE_CTX* ctx = static_cast<X509_STORE_CTX*>(x509ctx);

    if (crlPath != nullptr && crlPath[0] != '\0') {
        if (!LoadCrlToStore(ctx, crlPath)) {
            UBSE_LOG_ERROR << "CertVerifyCallback: CRL check failed, reject handshake";
            return VERIFY_FAILED;
        }
    }

    int verifyResult = X509_verify_cert(ctx);
    if (verifyResult != 1) {
        int err = X509_STORE_CTX_get_error(ctx);
        UBSE_LOG_ERROR << "CertVerifyCallback: chain verification failed, error=" << err
                       << ", depth=" << X509_STORE_CTX_get_error_depth(ctx);
        return VERIFY_FAILED;
    }

    UBSE_LOG_INFO << "CertVerifyCallback: peer certificate chain verification passed";
    return VERIFY_SUCCESS;
}

bool VerifyLocalCertOtherName(const std::string& certPath, const std::string& expectedNodeId)
{
    std::string otherName;
    if (!GetLocalCertOtherName(certPath, otherName)) {
        UBSE_LOG_ERROR << "VerifyLocalCertOtherName: failed to extract otherName from local cert: " << certPath;
        return false;
    }
    if (otherName != expectedNodeId) {
        UBSE_LOG_ERROR << "VerifyLocalCertOtherName: local cert otherName mismatch, cert otherName=" << otherName
                       << ", local nodeId=" << expectedNodeId << ", cert=" << certPath;
        return false;
    }
    UBSE_LOG_INFO << "VerifyLocalCertOtherName: local cert otherName=" << otherName
                  << " matches local nodeId=" << expectedNodeId;
    return true;
}

bool GetCertSanOtherName(void* cert, const char* oid, char* value, size_t bufferLen)
{
    if (cert == nullptr || oid == nullptr || value == nullptr || bufferLen == 0) {
        UBSE_LOG_ERROR << "GetCertSanOtherName: invalid input parameters";
        return false;
    }

    void* sanStack = GetSanStackFromCert(cert);
    if (sanStack == nullptr) {
        UBSE_LOG_ERROR << "GetCertSanOtherName: no SAN extension found";
        return false;
    }

    void* targetOid = ParseOidString(oid);
    if (targetOid == nullptr) {
        UBSE_LOG_ERROR << "GetCertSanOtherName: invalid OID string=" << oid;
        GENERAL_NAMES_free(static_cast<GENERAL_NAMES*>(sanStack));
        return false;
    }

    bool found = false;
    int count = OPENSSL_sk_num(static_cast<OPENSSL_STACK*>(sanStack));

    for (int i = 0; i < count; i++) {
        void* genName = OPENSSL_sk_value(static_cast<OPENSSL_STACK*>(sanStack), i);
        if (genName == nullptr) {
            continue;
        }

        int32_t extractRet = ExtractOtherNameValue(genName, targetOid, value, bufferLen);
        if (extractRet == 0) {
            found = true;
            break;
        }
    }

    GENERAL_NAMES_free(static_cast<GENERAL_NAMES*>(sanStack));
    ASN1_OBJECT_free(static_cast<ASN1_OBJECT*>(targetOid));

    return found;
}

void* GetSanStackFromCert(void* cert)
{
    X509* x509 = static_cast<X509*>(cert);
    void* sanStack = X509_get_ext_d2i(x509, NID_subject_alt_name, nullptr, nullptr);
    if (sanStack == nullptr) {
        UBSE_LOG_ERROR << "GetSanStackFromCert: no SAN extension found in certificate";
        return nullptr;
    }
    return sanStack;
}

void* ParseOidString(const char* oid)
{
    ASN1_OBJECT* targetOid = OBJ_txt2obj(oid, 1);
    if (targetOid == nullptr) {
        UBSE_LOG_ERROR << "ParseOidString: invalid OID string=" << oid;
        return nullptr;
    }
    return targetOid;
}

int32_t ExtractOtherNameValue(void* genName, void* targetOid, char* value, size_t bufferLen)
{
    GENERAL_NAME* generalName = static_cast<GENERAL_NAME*>(genName);
    ASN1_OBJECT* oidObj = static_cast<ASN1_OBJECT*>(targetOid);

    if (generalName->type != GEN_OTHERNAME) {
        return -1;
    }

    OTHERNAME* otherName = generalName->d.otherName;
    if (otherName == nullptr) {
        UBSE_LOG_ERROR << "ExtractOtherNameValue: otherName is null";
        return -1;
    }

    if (OBJ_cmp(otherName->type_id, oidObj) != 0) {
        return -1;
    }

    ASN1_TYPE* asn1Value = otherName->value;
    if (asn1Value == nullptr || asn1Value->type != V_ASN1_UTF8STRING) {
        UBSE_LOG_ERROR << "ExtractOtherNameValue: invalid ASN1_TYPE";
        return -1;
    }

    ASN1_STRING* asn1Str = asn1Value->value.asn1_string;
    if (asn1Str == nullptr) {
        UBSE_LOG_ERROR << "ExtractOtherNameValue: ASN1_STRING is null";
        return -1;
    }

    const unsigned char* strData = ASN1_STRING_get0_data(asn1Str);
    int strLen = ASN1_STRING_length(asn1Str);
    if (strData == nullptr || strLen <= 0) {
        UBSE_LOG_ERROR << "ExtractOtherNameValue: invalid string data";
        return -1;
    }

    return CopyStringValue(strData, strLen, value, bufferLen);
}

int32_t CopyStringValue(const unsigned char* strData, int strLen, char* value, size_t bufferLen)
{
    if (static_cast<size_t>(strLen) >= bufferLen) {
        UBSE_LOG_ERROR << "CopyStringValue: buffer too small, required=" << strLen + 1 << ", available=" << bufferLen;
        return -1;
    }

    errno_t memcpyRet = memcpy_s(value, bufferLen, strData, static_cast<size_t>(strLen));
    if (memcpyRet != EOK) {
        UBSE_LOG_ERROR << "CopyStringValue: memcpy failed";
        return -1;
    }

    value[strLen] = '\0';
    return 0;
}

} // namespace ubse::com
