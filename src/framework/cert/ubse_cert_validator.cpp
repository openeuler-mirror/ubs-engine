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

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
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
        UBSE_LOG_ERROR << "[CERT] Password file not found at path: " << path;
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
    UBSE_LOG_ERROR << "[CERT] Failed to read password from file: " << path;
    return {};
}

X509 *UbseSslValidator::LoadAndValidateCert(const char *path, const char *name)
{
    if (!UbseFileUtil::CheckFileExists(path)) {
        UBSE_LOG_ERROR << "[CERT] " << name << " file not found at path: " << path;
        return nullptr;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        UBSE_LOG_ERROR << "[CERT] Failed to open " << name << " file at path: " << path;
        return nullptr;
    }

    X509 *cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    fclose(fp);

    if (!cert) {
        UBSE_LOG_ERROR << "[CERT] Failed to parse PEM format for " << name << " at path: " << path;
        return nullptr;
    }

    // 检查证书有效期
    auto x509NotAfterPtr = X509_get0_notAfter(cert);
    if (x509NotAfterPtr == nullptr) {
        UBSE_LOG_ERROR << "[CERT] Failed to get expiration time for " << name;
        X509_free(cert);
        return nullptr;
    }
    if (X509_cmp_time(x509NotAfterPtr, nullptr) < 0) {
        UBSE_LOG_ERROR << "[CERT] " << name << " has expired at path: " << path;
        X509_free(cert);
        return nullptr;
    }

    auto x509NotBeforPtr = X509_get0_notBefore(cert);
    if (x509NotBeforPtr == nullptr) {
        UBSE_LOG_ERROR << "[CERT] Failed to get valid from time for " << name;
        X509_free(cert);
        return nullptr;
    }
    if (X509_cmp_time(x509NotBeforPtr, nullptr) > 0) {
        UBSE_LOG_ERROR << "[CERT] " << name << " is not valid at path: " << path;
        X509_free(cert);
        return nullptr;
    }
    return cert;
}

EVP_PKEY *UbseSslValidator::LoadAndValidatePrivateKey(const char *keyPath, const char *password, const char *name)
{
    if (!UbseFileUtil::CheckFileExists(keyPath)) {
        UBSE_LOG_ERROR << "[CERT] " << name << " file not found at path: " << keyPath;
        return nullptr;
    }
    FILE *fp = fopen(keyPath, "r");
    if (!fp) {
        UBSE_LOG_ERROR << "[CERT] Failed to open " << name << " file at path: " << keyPath;
        return nullptr;
    }
    ERR_clear_error();
    EVP_PKEY *pkey = PEM_read_PrivateKey(fp, nullptr, nullptr, const_cast<void *>(static_cast<const void *>(password)));
    if (!pkey) {
        int errorCode = ERR_get_error();
        UBSE_LOG_ERROR << "[CERT] Failed to parse " << name
                       << ". Incorrect password provided. sslErrorCode=" << errorCode << ". Check password at"
                       << UbseSSLConfig::PasswordFile << " and private key at" << UbseSSLConfig::ServerKeyFile;
        fclose(fp);
        return nullptr;
    }
    fclose(fp);
    return pkey;
}

bool UbseSslValidator::VerifyCertAndKeyMatch(X509 *cert, EVP_PKEY *pkey, const char *certName, const char *keyName)
{
    if (!cert || !pkey) {
        UBSE_LOG_ERROR << "[CERT] " << certName << "at " << UbseSSLConfig::ServerCertFile << " or " << keyName << "at"
                       << UbseSSLConfig::ServerCertFile << " is invalid";
        return false;
    }
    if (X509_check_private_key(cert, pkey) != 1) {
        UBSE_LOG_ERROR << "[CERT] " << certName << "at " << UbseSSLConfig::ServerCertFile << " and " << keyName << "at"
                       << UbseSSLConfig::ServerCertFile << " do not match";
        return false;
    }
    UBSE_LOG_INFO << "[CERT] " << certName << " and " << keyName << " match successfully";
    return true;
}

X509_STORE *UbseSslValidator::LoadAndValidateCaStore(const char *caPath)
{
    if (!UbseFileUtil::CheckFileExists(caPath)) {
        UBSE_LOG_ERROR << "[CERT] CA trust file not found at path: " << caPath;
        return nullptr;
    }

    X509_STORE *store = X509_STORE_new();
    if (!store) {
        UBSE_LOG_ERROR << "[CERT] Failed to create X509 certificate store at path: " << caPath;
        return nullptr;
    }

    if (X509_STORE_load_locations(store, caPath, nullptr) != 1) {
        UBSE_LOG_ERROR << "[CERT] Failed to load CA certificates from path: " << caPath;
        X509_STORE_free(store);
        return nullptr;
    }

    UBSE_LOG_INFO << "[CERT] Successfully loaded CA trust certificates from path: " << caPath;
    return store;
}

bool UbseSslValidator::ValidateCRLIfExists()
{
    if (!UbseFileUtil::CheckFileExists(UbseSSLConfig::CrlFile)) {
        UBSE_LOG_WARN << "[CERT] CRL file not found at path: " << UbseSSLConfig::CrlFile << ", skipping CRL validation";
        return true; // CRL 可选
    }

    // 尝试加载 CRL
    FILE *fp = fopen(UbseSSLConfig::CrlFile, "r");
    if (!fp) {
        UBSE_LOG_ERROR << "[CERT] Failed to open CRL file at path: " << UbseSSLConfig::CrlFile;
        return false;
    }

    X509_CRL *crl = PEM_read_X509_CRL(fp, nullptr, nullptr, nullptr);
    fclose(fp);

    if (!crl) {
        UBSE_LOG_ERROR << "[CERT] Failed to parse CRL file at path: " << UbseSSLConfig::CrlFile;
        return false;
    }

    // 检查 CRL 是否过期
    auto resultPtr = X509_CRL_get0_nextUpdate(crl);
    if (resultPtr == nullptr) {
        UBSE_LOG_WARN << "[CERT] CRL has no next update time, skipping expiration check";
        X509_CRL_free(crl);
        return true;
    }
    if (X509_cmp_time(resultPtr, nullptr) < 0) {
        X509_CRL_free(crl);
        UBSE_LOG_ERROR << "[CERT] CRL has expired at path: " << UbseSSLConfig::CrlFile;
        return false;
    }

    X509_CRL_free(crl);
    UBSE_LOG_INFO << "[CERT] CRL file is valid at path: " << UbseSSLConfig::CrlFile;
    return true;
}

bool UbseSslValidator::ValidateAll()
{
    UBSE_LOG_INFO << "[CERT] Starting SSL certificate validation process...";

    // 1. 加载服务端私钥密码
    SecureBuffer serverKeyPassword = LoadPasswordFromFile(UbseSSLConfig::PasswordFile);
    if (serverKeyPassword.size() == 0) {
        UBSE_LOG_ERROR << "[CERT] Server private key password is empty or could not be loaded";
        return false;
    }
    // 2. 验证服务端证书 + 私钥
    std::unique_ptr<X509, decltype(&X509_free)> serverCert(
        LoadAndValidateCert(UbseSSLConfig::ServerCertFile, "Server certificate"), X509_free);
    if (!serverCert) {
        UBSE_LOG_ERROR << "[CERT] Invalid server certificate";
        return false;
    }

    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> serverKey(
        LoadAndValidatePrivateKey(UbseSSLConfig::ServerKeyFile, serverKeyPassword.c_str(), "Server private key"),
        EVP_PKEY_free);
    if (!serverKey ||
        !VerifyCertAndKeyMatch(serverCert.get(), serverKey.get(), "Server certificate", "Server private key")) {
        UBSE_LOG_ERROR << "[CERT] Server private key or certificate-key mismatch";
        return false;
    }

    // 3. 验证 CA 信任证书（用于 mTLS）
    std::unique_ptr<X509_STORE, decltype(&X509_STORE_free)> caStore(
        LoadAndValidateCaStore(UbseSSLConfig::TrustCertFile), X509_STORE_free);
    if (!caStore) {
        UBSE_LOG_ERROR << "[CERT] Invalid CA trust certificates";
        return false;
    }

    // 4. 验证 CRL（如果存在）
    if (!ValidateCRLIfExists()) {
        UBSE_LOG_ERROR << "[CERT] Invalid CRL.";
        return false;
    }
    return true;
}

bool UbseSslValidator::ConfigureCrlValidation(SSL_CTX *ctx)
{
    if (!UbseFileUtil::CheckFileExists(UbseSSLConfig::CrlFile)) {
        UBSE_LOG_WARN << "CRL file not found, skipping CRL validation.";
        return true;
    }
    X509_STORE *store = SSL_CTX_get_cert_store(ctx);
    if (!store) {
        UBSE_LOG_ERROR << "Failed to get certificate store from SSL context. path at" << UbseSSLConfig::CrlFile;
        return false;
    }
    FILE *fp = fopen(UbseSSLConfig::CrlFile, "r");
    if (!fp) {
        UBSE_LOG_ERROR << "Failed to open CRL file: " << UbseSSLConfig::CrlFile;
        return false;
    }
    ERR_clear_error();
    X509_CRL *crl = PEM_read_X509_CRL(fp, nullptr, nullptr, nullptr);
    fclose(fp);
    if (!crl) {
        int errorCode = ERR_get_error();
        UBSE_LOG_ERROR << "Failed to parse CRL file  at " << UbseSSLConfig::CrlFile << ", sslErrorCode: " << errorCode;
        return false;
    }
    if (X509_STORE_add_crl(store, crl) != 1) {
        int errorCode = ERR_get_error();
        UBSE_LOG_ERROR << "Failed to add CRL to certificate store at path: " << UbseSSLConfig::CrlFile
                       << ", sslErrorCode: " << errorCode;
        X509_CRL_free(crl);
        return false;
    }
    X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
    X509_CRL_free(crl);
    return true;
}
} // namespace ubse::cert