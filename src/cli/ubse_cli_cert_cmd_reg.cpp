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
static const std::string SUCCESS_IMPORT = "Certificates imported successfully";
static const std::string SUCCESS_REMOVE = "Certificates removed successfully";
static const std::string SUCCESS_CHANGE_CRL = "Certificate Revocation List changed successfully";
static const std::string FAILED_IMPORT = "Certificates import failed: ";
static const std::string FAILED_REMOVE = "Certificates removed failed: ";
static const std::string FAILED_NOT_EXIST = "Certificates do not exist";
static const std::string FAILED_CHANGE_CRL = "Certificate Revocation List change failed: ";
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
    std::string ca_crl_path_value{};
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
        ca_crl_path_value = ca_crl_path->second;
    }
    std::string errMsg{};
    if (!ImportCertSet(server_cert_path->second, trust_cert_path->second, server_key_path->second, ca_crl_path_value,
                       errMsg)) {
        return UbseCliStringPromptReply(FAILED_IMPORT + errMsg);
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
    std::string errMsg{};
    if (!ImportCaCrl(ca_crl_path->second, errMsg)) {
        return UbseCliStringPromptReply(FAILED_CHANGE_CRL + errMsg);
    }
    return UbseCliStringPromptReply(SUCCESS_CHANGE_CRL);
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegCertModule::UbseCliCertDeleteFunc([
    [maybe_unused]] const std::map<std::string, std::string> &params)
{
    UbseResult ret = UBSE_OK;
    std::string errMsg{};
    ret = DeleteCertSet(errMsg);
    if (ret == UBSE_ERROR_FILE_NOT_EXIST) {
        return UbseCliStringPromptReply(FAILED_NOT_EXIST);
    }
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(FAILED_REMOVE + errMsg);
    }
    return UbseCliStringPromptReply(SUCCESS_REMOVE);
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

UbseCliCommandInfo UbseCliRegCertModule::UbseCliRemoveCert()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("remove").UbseCliSetType("cert").UbseCliSetFunc(UbseCliCertDeleteFunc);
    return builder.UbseCliBuild();
}

UbseCliCommandInfo UbseCliRegCertModule::UbseCliChangeCaCrl()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("change")
        .UbseCliSetType("cert")
        .UbseCliAddOption("l", CA_CRL_PATH, CA_CRL_DES)
        .UbseCliSetFunc(UbseCliCaCrlImportFunc);
    return builder.UbseCliBuild();
}

void UbseCliRegCertModule::UbseCliSignUp()
{
    this->cmd_.emplace_back(UbseCliCreateCert());
    this->cmd_.emplace_back(UbseCliChangeCaCrl());
    this->cmd_.emplace_back(UbseCliRemoveCert());
}
} // namespace ubse::cli::reg