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

#ifndef UBSE_CERT_CLI_IMPORT_H
#define UBSE_CERT_CLI_IMPORT_H
#include <ubse_common_def.h>
#include <filesystem>
#include <string>

namespace ubse::cli::cert {
using namespace common::def;
bool ImportCertSet(const std::string& serverCertPath, const std::string& trustCertPath,
                   const std::string& serverKeyPath, const std::string& caCrlPath, std::string& errMsg);
bool ImportCert(const std::string& serverCertPath, const std::string& trustCertPath, const std::string& serverKeyPath,
                std::string& errMsg);
bool ImportCaCrl(const std::string& caCrlPath, std::string& errMsg);
int DeleteCertificateFiles(std::string& errMsg);
UbseResult DeleteCertSet(std::string& errMsg);
bool CopyFileContents(const std::string& sourcePath, const std::string& destinationPath, std::string& errMsg);
std::string GetInteractiveInput(const std::string& prompt);
bool CheckFilePathValid(const std::string& filePath, bool isFile, std::string& errMsg);
} // namespace ubse::cli::cert
#endif // UBSE_CERT_CLI_IMPORT_H
