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

#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>

namespace ubse::cli::cert {
int8_t ImportCertSet(const char *serverCertPath, const char *trustCertPath, const char *serverKeyPath,
    const char *caCrlPath);
int8_t ImportCert(const char *serverCertPath, const char *trustCertPath, const char *serverKeyPath);
int8_t ImportCaCrl(const char *caCrlPath);
bool DeleteSingleFile(const std::filesystem::path &filePath);
int DeleteCertificateFiles();
int8_t DeleteCertSet();
int CopyFileContents(const std::string &sourcePath, const std::string &destinationPath);
std::string GetInteractiveInput(const std::string &prompt);
bool CheckFilePathValid(std::string filePath, bool isFile);
}
#endif // UBSE_CERT_CLI_IMPORT_H
