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
#include "ubse_cli_cert_cmd_reg.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_cert_cli_import.h"

namespace ubse::cli::reg {
UBSE_CLI_REGISTER_MODULE("CLI_CERT_MODULE", UbseCliRegCertModule);
using namespace ubse::cli::cert;
using namespace ubse::cli::framework;
using namespace ubse::common::def;
static const std::string SUCCESS_IMPORT = "Success to Import cert set";
static const std::string SUCCESS_DELETE = "Success to Delete cert set";
static const std::string SUCCESS_IMPORT_CRL = "Success to Import cert crl";
static const std::string FAILED_IMPORT = "failed import cert set";
static const std::string FAILED_DELETE = "Failed to Delete cert set";
static const std::string FAILED_IMPORT_CRL = "failed import cert crl";
static const std::string SERVER_CERT_DES =
    "A digital document that verifies the identity of an entity and binds a public key to it.";
static const std::string CA_CERT_DES = "A certificate issued by a trusted authority to validate authenticity and "
    "establish trust in digital communications.";
static const std::string SERVER_KEY_DES =
    "A confidential cryptographic key used for decryption, signing, and proving identity.";
static const std::string CA_CRL_DES =
    "A Certificate Revocation List (CRL) is a digitally signed list of certificates that have been revoked by a "
    "Certificate Authority (CA) before their scheduled expiration, serving as a critical security mechanism to "
    "invalidate compromised or untrusted digital certificates.";
static const std::string KEY_PW_DES =
    "A secret passphrase that encrypts and protects the private key from unauthorized access.";
static const std::string SERVER_CERT_PATH = "server-cert-file";
static const std::string CA_CERT_PATH = "ca-cert-file";
static const std::string SERVER_KEY_PATH = "server-key-file";
static const std::string CA_CRL_PATH = "ca-crl-file";
const std::string NO_SERVER_CERT_PATH =
    "ERROR: Missing the specific role. \"-s/--server-cert-file\" is required. Please "
    "try 'ubsectl --help' for more info.";
const std::string NO_TRUST_CERT_PATH = "ERROR: Missing the specific role. \"-c/--ca-cert-file\" is required. Please "
    "try 'ubsectl --help' for more info.";
const std::string NO_SERVER_KEY_PATH = "ERROR: Missing the specific role. \"-k/--server-key-file\" is required. Please "
    "try 'ubsectl --help' for more info.";
const std::string NO_CA_CRL = "ERROR: Missing the specific role. \"-k/--ca-crl-file\" is required. Please "
    "try 'ubsectl --help' for more info.";

std::shared_ptr<UbseCliResultEcho> UbseCliRegCertModule::UbseCliCertImportFunc([
    [maybe_unused]] const std::map<std::string, std::string> &params)
{
    auto server_cert_path = params.find(SERVER_CERT_PATH);
    auto trust_cert_path = params.find(CA_CERT_PATH);
    auto server_key_path = params.find(SERVER_KEY_PATH);
    auto ca_crl_path = params.find(CA_CRL_PATH);
    char *ca_crl_path_value = nullptr;
    if (server_cert_path == params.end()) {
        return UbseCliStringPromptReply(NO_SERVER_CERT_PATH);
    }
    if (trust_cert_path == params.end()) {
        return UbseCliStringPromptReply(NO_TRUST_CERT_PATH);
    }
    if (server_key_path == params.end()) {
        return UbseCliStringPromptReply(NO_SERVER_KEY_PATH);
    }
    if (ca_crl_path != params.end()) {
        ca_crl_path_value = const_cast<char *>(ca_crl_path->second.c_str());
    }
    UbseResult ret = UBSE_OK;
    ret = ImportCertSet(server_cert_path->second.c_str(), trust_cert_path->second.c_str(),
        server_key_path->second.c_str(), ca_crl_path_value);
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(FAILED_IMPORT);
    }
    return UbseCliStringPromptReply(SUCCESS_IMPORT);
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegCertModule::UbseCliCaCrlImportFunc([
    [maybe_unused]] const std::map<std::string, std::string> &params)
{
    auto ca_crl_path = params.find(CA_CRL_PATH);
    if (ca_crl_path == params.end()) {
        return UbseCliStringPromptReply(NO_CA_CRL);
    }
    UbseResult ret = UBSE_OK;
    ret = ImportCaCrl(ca_crl_path->second.c_str());
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(FAILED_IMPORT_CRL);
    }
    return UbseCliStringPromptReply(SUCCESS_IMPORT_CRL);
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegCertModule::UbseCliCertDeleteFunc([
    [maybe_unused]] const std::map<std::string, std::string> &params)
{
    UbseResult ret = UBSE_OK;
    ret = DeleteCertSet();
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(FAILED_DELETE);
    }
    return UbseCliStringPromptReply(SUCCESS_DELETE);
}

UbseCliCommandInfo UbseCliRegCertModule::UbseCliCreateCert()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("import")
        .UbseCliSetType("cert")
        .UbseCliAddOption("s", SERVER_CERT_PATH, SERVER_CERT_DES)
        .UbseCliAddOption("c", CA_CERT_PATH, CA_CERT_DES)
        .UbseCliAddOption("k", SERVER_KEY_PATH, SERVER_KEY_DES)
        .UbseCliAddOption("l", CA_CRL_PATH, CA_CRL_DES)
        .UbseCliSetFunc(UbseCliCertImportFunc);
    return builder.UbseCliBuild();
}

UbseCliCommandInfo UbseCliRegCertModule::UbseCliDeleteCert()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("delete").UbseCliSetType("cert").UbseCliSetFunc(UbseCliCertDeleteFunc);
    return builder.UbseCliBuild();
}

UbseCliCommandInfo UbseCliRegCertModule::UbseCliImportCaCrl()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("import")
        .UbseCliSetType("crl")
        .UbseCliAddOption("l", CA_CRL_PATH, CA_CRL_DES)
        .UbseCliSetFunc(UbseCliCaCrlImportFunc);
    return builder.UbseCliBuild();
}

void UbseCliRegCertModule::UbseCliSignUp()
{
    this->cmd.emplace_back(UbseCliCreateCert());
    this->cmd.emplace_back(UbseCliImportCaCrl());
    this->cmd.emplace_back(UbseCliDeleteCert());
}
} // namespace ubse::cli::reg