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
#include "ubse_cert_validator.h"

#include <fstream>

#include "ubse_cert_def.h"
#include "ubse_file_util.h"
#include "ubse_logger.h"

namespace ubse::cert {
using namespace ubse::utils;
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

SecureBuffer UbseSslValidator::LoadPasswordFromFile(const char *path)
{
    if (!UbseFileUtil::CheckFileExists(path)) {
        UBSE_LOG_ERROR << "Password file not found: " << path;
        return {};
    }
    std::ifstream f(path);
    std::string tmpValue;
    tmpValue.reserve(4096);
    if (f && std::getline(f, tmpValue)) {
        if (!tmpValue.empty() && tmpValue.back() == '\r') {
            tmpValue.pop_back();
        }
        SecureBuffer securePwd(tmpValue);
        SecureZeroMemory(tmpValue.data(), tmpValue.size());
        tmpValue.clear();
        return securePwd;
    }
    return {};
}

X509 *UbseSslValidator::LoadAndValidateCert(const char *path, const char *name)
{
    if (!UbseFileUtil::CheckFileExists(path)) {
        UBSE_LOG_ERROR << name << " file not found: " << path;
        return nullptr;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        UBSE_LOG_ERROR << "Failed to open " << name << ": " << path;
        return nullptr;
    }

    X509 *cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    fclose(fp);

    if (!cert) {
        UBSE_LOG_ERROR << "Failed to parse PEM " << name << ": " << path;
        return nullptr;
    }

    // 检查有效期
    auto x509NotAfterPtr = X509_get0_notAfter(cert);
    if (x509NotAfterPtr == nullptr) {
        X509_free(cert);
        return nullptr;
    }
    if (X509_cmp_time(x509NotAfterPtr, nullptr) < 0) {
        UBSE_LOG_ERROR << name << " has expired: " << path;
        X509_free(cert);
        return nullptr;
    }

    auto x509NotBeforPtr = X509_get0_notBefore(cert);
    if (x509NotBeforPtr == nullptr) {
        X509_free(cert);
        return nullptr;
    }
    if (X509_cmp_time(x509NotBeforPtr, nullptr) > 0) {
        UBSE_LOG_ERROR << name << " is not yet valid: " << path;
        X509_free(cert);
        return nullptr;
    }

    return cert;
}

EVP_PKEY *UbseSslValidator::LoadAndValidatePrivateKey(const char *keyPath, const char *password, const char *name)
{
    if (!UbseFileUtil::CheckFileExists(keyPath)) {
        UBSE_LOG_ERROR << name << " file not found: " << keyPath;
        return nullptr;
    }

    FILE *fp = fopen(keyPath, "r");
    if (!fp) {
        UBSE_LOG_ERROR << "Failed to open " << name << ": " << keyPath;
        return nullptr;
    }

    EVP_PKEY *pkey = PEM_read_PrivateKey(fp, nullptr, nullptr, const_cast<void *>(static_cast<const void *>(password)));
    fclose(fp);

    if (!pkey) {
        UBSE_LOG_ERROR << "Failed to parse " << name << ". Please check your server key password.";
        return nullptr;
    }

    return pkey;
}

bool UbseSslValidator::VerifyCertAndKeyMatch(X509 *cert, EVP_PKEY *pkey, const char *certName, const char *keyName)
{
    if (!cert || !pkey) {
        return false;
    }
    if (X509_check_private_key(cert, pkey) != 1) {
        UBSE_LOG_ERROR << certName << " and " << keyName << " do NOT match.";
        return false;
    }
    return true;
}

X509_STORE *UbseSslValidator::LoadAndValidateCaStore(const char *caPath)
{
    if (!UbseFileUtil::CheckFileExists(caPath)) {
        UBSE_LOG_ERROR << "CA trust file not found: " << caPath;
        return nullptr;
    }

    X509_STORE *store = X509_STORE_new();
    if (!store) {
        UBSE_LOG_ERROR << "Failed to create X509_STORE";
        return nullptr;
    }

    if (X509_STORE_load_locations(store, caPath, nullptr) != 1) {
        UBSE_LOG_ERROR << "Failed to load CA certificates from: " << caPath;
        X509_STORE_free(store);
        return nullptr;
    }

    return store;
}

bool UbseSslValidator::ValidateCRLIfExists()
{
    if (!UbseFileUtil::CheckFileExists(UbseSSLConfig::CrlFile)) {
        UBSE_LOG_WARN << "CRL file not found, skipping CRL validation.";
        return true; // CRL 可选
    }

    // 尝试加载 CRL
    FILE *fp = fopen(UbseSSLConfig::CrlFile, "r");
    if (!fp) {
        UBSE_LOG_ERROR << "Failed to open CRL file: " << UbseSSLConfig::CrlFile;
        return false;
    }

    X509_CRL *crl = PEM_read_X509_CRL(fp, nullptr, nullptr, nullptr);
    fclose(fp);

    if (!crl) {
        UBSE_LOG_ERROR << "Failed to parse CRL file: " << UbseSSLConfig::CrlFile;
        return false;
    }

    // 检查 CRL 是否过期
    auto resultPtr = X509_CRL_get0_nextUpdate(crl);
    if (resultPtr == nullptr) {
        X509_CRL_free(crl);
        return true;
    }
    if (X509_cmp_time(resultPtr, nullptr) < 0) {
        X509_CRL_free(crl);
        UBSE_LOG_ERROR << "CRL has expired: " << UbseSSLConfig::CrlFile;
        return false;
    }

    X509_CRL_free(crl);
    UBSE_LOG_INFO << "CRL file is valid.";
    return true;
}

bool UbseSslValidator::ValidateAll()
{
    UBSE_LOG_INFO << "Starting SSL certificate validation...";

    // 1. 加载服务端私钥密码
    SecureBuffer serverKeyPassword = LoadPasswordFromFile(UbseSSLConfig::PasswordFile);
    if (serverKeyPassword.size() == 0) {
        UBSE_LOG_ERROR << "ServerKeyPassword is empty!";
        return false;
    }
    // 2. 验证服务端证书 + 私钥
    std::unique_ptr<X509, decltype(&X509_free)> serverCert(
        LoadAndValidateCert(UbseSSLConfig::ServerCertFile, "Server certificate"), X509_free);
    if (!serverCert) {
        return false;
    }

    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> serverKey(
        LoadAndValidatePrivateKey(UbseSSLConfig::ServerKeyFile, serverKeyPassword.c_str(), "Server private key"),
        EVP_PKEY_free);
    if (!serverKey ||
        !VerifyCertAndKeyMatch(serverCert.get(), serverKey.get(), "Server certificate", "Server private key")) {
        return false;
    }

    // 3. 验证 CA 信任证书（用于 mTLS）
    std::unique_ptr<X509_STORE, decltype(&X509_STORE_free)> caStore(
        LoadAndValidateCaStore(UbseSSLConfig::TrustCertFile), X509_STORE_free);
    if (!caStore) {
        return false;
    }

    // 4. 验证 CRL（如果存在）
    if (!ValidateCRLIfExists()) {
        return false;
    }

    UBSE_LOG_INFO << "All SSL certificates and keys are valid.";
    return true;
}
} // namespace ubse::cert