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

#ifndef UBSE_COM_CERT_VERIFY_H
#define UBSE_COM_CERT_VERIFY_H

#include <cstdint>
#include <string>

namespace ubse::com {

constexpr const char* CERT_DIR = "/var/lib/ubse/cert/";
constexpr const char* SERVER_CERT_FILENAME = "/var/lib/ubse/cert/server.pem";
constexpr const char* TRUST_CERT_FILENAME = "/var/lib/ubse/cert/trust.pem";
constexpr const char* CA_CRL_FILENAME = "/var/lib/ubse/cert/ca.crl";
constexpr const char* SERVER_KEY_FILENAME = "/var/lib/ubse/cert/server_key.pem";
constexpr const char* PASSWORD_FILENAME = "/var/lib/ubse/cert/key_pwd.txt";

constexpr const char* CERT_OTHER_NAME_OID = "1.3.6.1.4.1.2011.999.1";
constexpr size_t CERT_OTHER_NAME_MAX_LEN = 256;

// 校验本端导入的证书 otherName 是否等于本节点 nodeid。
// certPath 为本地证书文件路径（如 SERVER_CERT_FILENAME），expectedNodeId 为本节点 nodeid。
// 返回 true 表示一致，false 表示证书缺失 otherName 或与实际 nodeid 不符。
bool VerifyLocalCertOtherName(const std::string& certPath, const std::string& expectedNodeId);

void SetExpectedLocalNodeId(const std::string& nodeId);

bool GetLocalCertOtherName(const std::string& certPath, std::string& otherName);

int CertVerifyCallback(void* x509ctx, const char* crlPath);

bool GetCertSanOtherName(void* cert, const char* oid, char* value, size_t bufferLen);

void* GetSanStackFromCert(void* cert);

void* ParseOidString(const char* oid);

int32_t ExtractOtherNameValue(void* genName, void* targetOid, char* value, size_t bufferLen);

int32_t CopyStringValue(const unsigned char* strData, int strLen, char* value, size_t bufferLen);

} // namespace ubse::com

#endif // UBSE_COM_CERT_VERIFY_H